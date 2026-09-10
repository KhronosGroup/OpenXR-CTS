// Copyright 2024-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

import {
  removeBreakNamespace,
  removeDrawioAttrs,
  fixFonts,
} from "./svgo-plugins.mjs";


export default {
  multipass: true,
  plugins: [
    fixFonts,
    "preset-default",
    fixFonts,
    removeDrawioAttrs,
    removeBreakNamespace,
  ],
  overrides: {
    fixFonts: {
      // leave Helvetica alone
      regBadFonts: /('Liberation Sans')|Arial|Arimo|ArimoMT/g,
    },
  },
};
