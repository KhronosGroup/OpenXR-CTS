#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2022, The Khronos Group Inc.
# Copyright (c) 2017-2019 Valve Corporation
# Copyright (c) 2017-2019 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from automatic_source_generator import AutomaticSourceOutputGenerator, write
from jinja_helpers import JinjaTemplate, make_jinja_environment

# The following commands should not be generated for the layer
MANUALLY_DEFINED_IN_LAYER = [
    'xrCreateInstance',
]


def make_ext_variable_name(extName):
    return extName.lower()[3:]


def make_environment():
    env = make_jinja_environment(file_with_templates_as_sibs=__file__)
    env.filters['make_ext_variable_name'] = make_ext_variable_name
    return env


class ConformanceLayerGenerator(AutomaticSourceOutputGenerator):
    """Generate conformance layer source using XML element attributes from registry"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.env = make_environment()

    def outputGeneratedAuthorNote(self):
        pass

    # Override the base class header warning so the comment indicates this file.
    #   self            the AutomaticSourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        # File Comment
        generated_warning = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See conformance_layer_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the ConformanceLayerHeaderGenerator object
    #   gen_opts        the ConformanceLayerHeaderGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        self.template = JinjaTemplate(self.env, "template_{}".format(genOpts.filename))

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the ConformanceLayerBaseGenerator object
    def endFile(self):
        sorted_cmds = self.core_commands + self.ext_commands
        skip_hooks = set(self.no_trampoline_or_terminator).union(
            set(MANUALLY_DEFINED_IN_LAYER))
        file_data = self.template.render(
                gen=self,
                registry=self.registry,
                sorted_cmds=sorted_cmds,
                skip_hooks=skip_hooks)
        write(file_data, file=self.outFile)

        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)
