#!/usr/bin/env python3
# Copyright 2023-2024, The Khronos Group, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import os
import subprocess
from pathlib import Path
from dataclasses import dataclass, field
import re
import sys
from typing import Dict, List, Optional, Tuple

_PBR_DIR = Path(__file__).parent.resolve()

_SHADER = "PbrPixelShader_glsl.frag"

_GL_TYPES = {
    0x8B51: "GL_FLOAT_VEC3",
    0x1405: "GL_UNSIGNED_INT",  # 32 bit (4 bytes)
    0x1406: "GL_FLOAT",  # 32 bit (4 bytes)
}


@dataclass
class GlslObjectReflectionData:
    name: str
    offset: int
    data_type: int  # typically shown in hex
    size: int
    index: int
    binding: int
    stages: int
    counter: Optional[int] = None
    num_members: Optional[int] = None
    array_stride: Optional[int] = None
    top_level_array_stride: Optional[int] = None


def parse_reflection_line(line: str) -> GlslObjectReflectionData:
    """
    Process a single object reflection dump line from glslangValidator.

    Parses the output of TObjectReflection::dump(), effectively.

    See:
    https://github.com/KhronosGroup/glslang/blob/db4d6f85afb8cf6aa404a141855f556d172c1ed2/glslang/MachineIndependent/reflection.cpp#L1092
    """
    name, _, all_fields = line.strip().partition(": ")
    if all_fields is None:
        raise RuntimeError(
            f"Could not parse line as a reflection data line: {all_fields}"
        )
    # field_list = []
    # fields = {name: val for name, val in field_list}
    fields = dict(field.split(" ") for field in all_fields.split(", "))

    # These fields are always printed - see TObjectReflection::dump
    # So, let's parse them.

    offset = int(fields["offset"])

    # parse hex
    data_type = int(fields["type"], 16)

    size = int(fields["size"])

    index = int(fields["index"])

    binding = int(fields["binding"])

    stages = int(fields["stages"])
    data = GlslObjectReflectionData(
        name=name,
        offset=offset,
        data_type=data_type,
        size=size,
        index=index,
        binding=binding,
        stages=stages,
    )

    # These are not always printed
    counter = fields.get("counter")
    if counter is not None:
        data.counter = int(counter)

    num_members = fields.get("numMembers")
    if num_members is not None:
        data.num_members = int(num_members)

    array_stride = fields.get("arrayStride")
    if array_stride is not None:
        data.array_stride = int(array_stride)

    top_level_array_stride = fields.get("topLevelArrayStride")
    if top_level_array_stride is not None:
        data.top_level_array_stride = int(top_level_array_stride)

    return data


def process_reflection_data_section(
    reflection_lines: List[str], heading: str
) -> Dict[str, GlslObjectReflectionData]:
    """
    Process a section of reflection data from glslangValidator.

    Effectively handles a single section (loop) from TReflection::dump()

    See:
    https://github.com/KhronosGroup/glslang/blob/db4d6f85afb8cf6aa404a141855f556d172c1ed2/glslang/MachineIndependent/reflection.cpp#L1222
    """
    start_idx = reflection_lines.index(heading)
    end_idx = None
    try:
        end_idx = reflection_lines.index("", start_idx + 1)
    except ValueError:
        # don't care, last section
        pass
    if end_idx is None:
        section_lines = reflection_lines[start_idx + 1 :]
    else:
        section_lines = reflection_lines[start_idx + 1 : end_idx - 1]
    ret = {}
    for line in section_lines:
        data = parse_reflection_line(line)
        ret[data.name] = data
    return ret


@dataclass
class ReflectionData:
    uniforms: Dict[str, GlslObjectReflectionData] = field(default_factory=dict)


def compile_and_get_reflection_data(
    shader_source: Path, glslangvalidator_cmd: str = "glslangValidator"
) -> ReflectionData:
    output = subprocess.check_output(
        [glslangvalidator_cmd, "-V", str(shader_source), "-q"]
    )

    ret = ReflectionData()

    lines = [line.strip().decode(encoding="utf-8") for line in output.splitlines()]
    start_uniform = lines.index("Uniform reflection:")
    end_uniform = lines.index("", start_uniform + 1)
    uniform_lines = lines[start_uniform + 1 : end_uniform - 1]
    for line in uniform_lines:
        data = parse_reflection_line(line)
        ret.uniforms[data.name] = data

    return ret


@dataclass
class COffsetData:
    structure_type: str
    member_name: str
    offset: int


_RE_OFFSET_ASSERTS = re.compile(
    r"offsetof\((?P<structure_type>[A-Za-z]+), (?P<member_name>[A-Za-z]+)\) == (?P<offset>[0-9]+),"
)


def parse_c_offsets(source: Path) -> Dict[Tuple[str, str], COffsetData]:
    ret = {}
    with open(source, "r", encoding="utf-8") as fp:
        for line in fp:
            m = _RE_OFFSET_ASSERTS.search(line)
            if m:
                structure_type = m.group("structure_type")
                member_name = m.group("member_name")
                offset = int(m.group("offset"))
                ret[(structure_type, member_name)] = COffsetData(
                    structure_type=structure_type,
                    member_name=member_name,
                    offset=offset,
                )
    return ret


def compare_glsl_and_c(
    shader_data: ReflectionData,
    source_data: Dict[Tuple[str, str], COffsetData],
    glsl_qualified_member: str,
    c_structure: str,
    c_member: str,
):
    uniform_data = shader_data.uniforms.get(glsl_qualified_member)
    if uniform_data is None:
        raise RuntimeError(
            f"Could not find qualified GLSL name {glsl_qualified_member} in reflection data!"
        )
    c_data = source_data.get((c_structure, c_member))
    if c_data is None:
        raise RuntimeError(
            f"Could not find parsed source code offset assertion for type {c_structure}, member name {c_member}"
        )

    glsl_offset = shader_data.uniforms[glsl_qualified_member].offset
    c_offset = c_data.offset
    gl_type_enum = shader_data.uniforms[glsl_qualified_member].data_type
    # print(f"GLSL: datatype: {gl_type_enum:04x}")
    gl_type_name = _GL_TYPES[gl_type_enum]

    print(
        f"GLSL: offset {glsl_offset:03d} {glsl_qualified_member} (datatype: {gl_type_name})"
    )
    print(f"C:    offset {c_offset:03d} {c_structure}.{c_member}")
    if glsl_offset != c_offset:
        raise RuntimeError("Mismatch!")


if __name__ == "__main__":
    shader = _PBR_DIR / "Shaders" / _SHADER

    if len(sys.argv) > 1:
        glslangValidator = sys.argv[1]
    else:

    # glslangValidator = os.getenv("GLSLANGVALIDATOR")
    # if not glslangValidator:
        glslangValidator = "glslangValidator"

    shader_data = compile_and_get_reflection_data(shader, glslangValidator)

    source_data = parse_c_offsets(_PBR_DIR / "GlslBuffers.h")

    compare_glsl_and_c(
        shader_data,
        source_data,
        "type_SceneBuffer.EyePosition",
        "SceneConstantBuffer",
        "EyePosition",
    )
    compare_glsl_and_c(
        shader_data,
        source_data,
        "type_SceneBuffer.LightDirection",
        "SceneConstantBuffer",
        "LightDirection",
    )
    compare_glsl_and_c(
        shader_data,
        source_data,
        "type_SceneBuffer.LightColor",
        "SceneConstantBuffer",
        "LightDiffuseColor",
    )
    compare_glsl_and_c(
        shader_data,
        source_data,
        "type_SceneBuffer.NumSpecularMipLevels",
        "SceneConstantBuffer",
        "NumSpecularMipLevels",
    )
