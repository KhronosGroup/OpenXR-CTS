// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "Geometry.h"

#include <openxr/openxr.h>

namespace Geometry
{

    XrVector3f rotateAxes(XrVector3f input, int axis)
    {
        while (axis > 0) {
            auto tmp = input.z;
            input.z = input.y;
            input.y = input.x;
            input.x = tmp;
            axis--;
        }

        return input;
    }

    AxisIndicator::AxisIndicator() : count(0)
    {
        constexpr int axes = 3;
        constexpr int verticesPerAxis = 30;
        constexpr int totalVertices = verticesPerAxis * axes;

        constexpr float thickness = 0.1f;

        // For each axis, create a copy of the cube mesh missing the -x face.
        // The +x face will be at 1.0, and the -x faces will be mitered together.
        // Each axis is colored and rotated differently, but otherwise identical.
        // Creating this from the cube vertices lets us avoid writing an array of
        // 90 vertices (and indices), and should be easier to tweak in the future.
        for (unsigned short i = 0; i != totalVertices; ++i) {
            int axis = i / verticesPerAxis;
            int index = i % verticesPerAxis;

            XrVector3f colors[axes] = {Red, Green, Blue};
            XrVector3f color = colors[axis];

            Vertex vertex = c_cubeVertices[index + 6];  // skip -x face
            vertex.Color = color;
            vertex.Position.x *= thickness;
            vertex.Position.y *= thickness;
            vertex.Position.z *= thickness;

            if (vertex.Position.x > 0) {
                // +x vertex, end of the arm, send x to +1
                vertex.Position.x = 1.0;
            }
            else if (vertex.Position.y > 0 || vertex.Position.z > 0) {
                // make room for another axis
                vertex.Position.x = -vertex.Position.x;
            }

            // rotate the x to whichever axis we are on
            vertex.Position = rotateAxes(vertex.Position, axis);

            count += 1;
            indices[i] = i;
            vertices[i] = vertex;
        }
    }

    const AxisIndicator& AxisIndicator::GetInstance()
    {
        static const AxisIndicator instance;
        return instance;
    }
}  // namespace Geometry
