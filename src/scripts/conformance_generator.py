#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2024, The Khronos Group Inc.
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

from typing import List, Tuple
from automatic_source_generator import AutomaticSourceOutputGenerator, write
from interaction_profile_processor import AvailabilitySymbols, InteractionProfileProcessor, FrozenAvailability
from jinja_helpers import JinjaTemplate, make_jinja_environment

VALID_FOR_NULL_INSTANCE = set((
    'xrEnumerateInstanceExtensionProperties',
    'xrEnumerateApiLayerProperties',
    'xrCreateInstance',
    'xrInitializeLoaderKHR'
))


class ConformanceGenerator(AutomaticSourceOutputGenerator):
    """Generate conformance source using XML element attributes from registry"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.env = make_jinja_environment(file_with_templates_as_sibs=__file__)
        self.interaction_profiles = InteractionProfileProcessor()

    def outputGeneratedAuthorNote(self):
        pass

    # Override the base class header warning so the comment indicates this file.
    #   self            the AutomaticSourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        # File Comment
        generated_warning = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See conformance_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the ConformanceLayerHeaderGenerator object
    #   gen_opts        the ConformanceLayerHeaderGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        self.template = JinjaTemplate(
            self.env, f"template_{genOpts.filename}")

    def extensionReturnCodesForCommand(self, cur_cmd):
        assert self.registry
        return (x for x
                in self.registry.commandextensionerrors + self.registry.commandextensionsuccesses
                if x.command == cur_cmd.name)

    def allReturnCodesForCommand(self, cur_cmd):
        return cur_cmd.return_values + list(self.extensionReturnCodesForCommand(cur_cmd))

    def beginFeature(self, interface, emit):
        super().beginFeature(interface, emit)

        assert self.registry
        self.interaction_profiles.process_feature(self.registry.tree, interface, emit)

    def _compute_avail_symbols(self) -> List[Tuple[str, FrozenAvailability]]:
        temp = AvailabilitySymbols()
        for profile in self.interaction_profiles.interaction_profiles.values():
            temp.add(profile.availability)
            for component in profile.components.values():
                temp.add(component.availability)
        return temp.make_frozen()

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the ConformanceLayerBaseGenerator object
    def endFile(self):
        self.interaction_profiles.process_deferred()
        avail_syms = self._compute_avail_symbols()
        sorted_cmds = self.core_commands + self.ext_commands
        file_data = self.template.render(
            gen=self,
            registry=self.registry,
            null_instance_ok=VALID_FOR_NULL_INSTANCE,
            sorted_cmds=sorted_cmds,
            interaction_profiles=self.interaction_profiles.interaction_profiles,
            availabilities=avail_syms)
        write(file_data, file=self.outFile)

        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)
