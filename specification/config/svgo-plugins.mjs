// Copyright 2024-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

const regBadFontsDefault = /('Liberation Sans')|Helvetica|Arial|Arimo|ArimoMT/g;

export const fixFonts = {
  name: "fixFonts",
  params: {
    regBadFonts: regBadFontsDefault,
    replacement: "sans-serif",
  },
  fn: (ast, params, info) => {
    const { regBadFonts = regBadFontsDefault, replacement = "sans-serif" } =
      params;
    return {
      element: {
        enter: (node) => {
          if (node.attributes.style != null) {
            node.attributes.style = node.attributes.style.replace(
              regBadFonts,
              replacement,
            );
          }
          if (node.attributes["font-family"] != null) {
            node.attributes["font-family"] = node.attributes[
              "font-family"
            ].replace(regBadFonts, replacement);
          }
        },
      },
    };
  },
};

export const removeBreakNamespace = {
  name: "removeBreakNamespace",
  fn: () => {
    return {
      element: {
        enter: (node, parentNode) => {
          if (node.name == "xhtml:br") {
            node.name = "br";
          }
        },
      },
    };
  },
};

export const removeDrawioAttrs = {
  name: "removeAttrs",
  params: {
    attrs: ["xhtml:div_data-drawio-colors"],
    elemSeparator: "_",
  },
};

export const removeForeignObject = {
  name: "removeAttrs",
  params: {
    attrs: ["xhtml:div_data-drawio-colors"],
    elemSeparator: "_",
  },
  fn: () => {
    return {
      element: {
        enter: (node, parentNode) => {
          if (node.name == "foreignObject") {
            parentNode.children = parentNode.children.filter(
              (child) => child !== node,
            );
          }
        },
      },
    };
  },
};
