#!/usr/bin/env python3
#
# Copyright (c) 2018-2019 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Ryan Pavlik <ryan.pavlik@collabora.com>
#
# Purpose:      This file performs some basic checks of the custom macros
#               used in the AsciiDoctor source for the spec, especially
#               related to the validity of the entities linked-to.

from pathlib import Path

from reg import Registry
from spec_tools.entity_db import EntityDatabase
from spec_tools.macro_checker import MacroChecker
from spec_tools.macro_checker_file import BlockType, MacroCheckerFile
from spec_tools.main import checkerMain
from spec_tools.shared import MessageId

###
# "Configuration" constants

EXTRA_DEFINES = ('XRAPI_ATTR', 'XRAPI_CALL', 'XRAPI_PTR', 'XR_NO_STDINT_H')

# These are marked with the code: macro
SYSTEM_TYPES = set(('void', 'char', 'float', 'size_t',
                    'intptr_t', 'uintptr_t',
                    'int8_t', 'uint8_t',
                    'int16_t', 'uint16_t',
                    'int32_t', 'uint32_t',
                    'int64_t', 'uint64_t'))

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_DISABLED_MESSAGES = set((MessageId.REFPAGE_MISSING,))

CWD = Path('.').resolve()


class XREntityDatabase(EntityDatabase):
    """OpenXR-specific subclass of EntityDatabase."""

    def makeRegistry(self):
        root = Path(__file__).resolve().parent.parent.parent
        registryFile = str(root / 'specification/registry/xr.xml')
        registry = Registry()
        registry.loadFile(registryFile)
        return registry

    def getNamePrefix(self):
        return "xr"

    def getPlatformRequires(self):
        return "openxr_platform_defines"

    def getSystemTypes(self):
        return SYSTEM_TYPES

    def populateMacros(self):
        # TODO: What about flag wildcards? There are a few such uses...
        self.addMacro('elink', ('enums', 'flags',), link=True)
        self.addMacro('basetype', ('basetypes',), link=True)

    def populateEntities(self):
        # These are not mentioned in the XML
        for name in EXTRA_DEFINES:
            self.addEntity(name, 'dlink', category='configdefines',
                           generates=False)

    def handleType(self, name, info, requires):
        """Extend superclass implementation for OpenXR bitmasks."""
        cat = info.elem.get('category')

        if cat == 'bitmask':
            # OpenXR uses elink, not tlink, for flags.
            self.addEntity(
                name,
                'elink',
                elem=info.elem,
                category='flags')
        else:
            super().handleType(name, info, requires)


class XRMacroCheckerFile(MacroCheckerFile):
    """OpenXR-specific subclass of MacroCheckerFile."""

    def processBlockOpen(self, block_type, context=None, delimiter=None):
        """Do any block-type-specific processing and push the new block.

        Extends the superclass to warn if a ref-page-like block is opened
        without a ref page tag.

        Must call self.pushBlock().

        Called by self.processBlockDelimiter().
        """
        # OpenXR only uses '--' blocks for ref pages.
        if block_type == BlockType.REF_PAGE_LIKE and \
                not self.prev_line_ref_page_tag:

            self.error(MessageId.REFPAGE_BLOCK,
                       ["Found a line containing only -- outside of a reference page block, not preceded by a reference page tag,",
                        "Pretending there was one and opening refpage block for unknown entity anyway, for more readable messages."],
                       group=None)

            self.current_ref_page = '?missing-refpage-tag?'
            self.prev_line_ref_page_tag = None
            self.in_ref_page = True
            # Manually pushing the block here, to force a refpage block.
            # Not going to superclass method in this case.
            self.pushBlock(block_type, refpage=self.current_ref_page,
                           context=context, delimiter=delimiter)
            return

        # Everything else about block opening is standard.
        super().processBlockOpen(block_type, context=context,
                                 delimiter=delimiter)

    def computeExpectedRefPageFromInclude(self, entity):
        """Compute the expected ref page entity based on an include entity name."""
        return entity


def makeMacroChecker(enabled_messages):
    """Create a correctly-configured MacroChecker instance."""
    entity_db = XREntityDatabase()
    return MacroChecker(enabled_messages, entity_db, XRMacroCheckerFile, ROOT)


if __name__ == '__main__':
    available_messages = set(MessageId)
    # LEGACY messages are Vulkan-only.
    available_messages.remove(MessageId.LEGACY)

    default_enabled_messages = available_messages.difference(
        DEFAULT_DISABLED_MESSAGES)

    all_docs = sorted((str(fn)
                       for fn in (ROOT / 'specification/sources/').glob('**/*.adoc')
                       if "styleguide" not in str(fn)))

    checkerMain(default_enabled_messages, makeMacroChecker,
                all_docs, available_messages=available_messages)
