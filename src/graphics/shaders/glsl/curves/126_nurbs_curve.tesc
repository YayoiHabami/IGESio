// Tessellation Control Shader for Rational B-Spline Curve (type 126)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - Entity Dependent (uniform):
//   - numRefPoints (uniform int)
// - Entity Dependent (SSBO; std430):
//   - refPoints (vec3[])
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
//   - viewportSize (uniform vec2)
#version 430 core
layout (vertices = 1) out;

// constants
const int pxPerSegment = 2;
const float maxSegments = 100.0;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// viewport
uniform vec2 viewportSize;

// reference points (points on the curve)
uniform int numRefPoints;
layout(std430, binding = 2) buffer RefPoints {
    vec3 refPoints[];
};

void main() {
    // to avoid compiler warnings
    gl_InvocationID;

    // Approximate the curve length by summing the distances between consecutive points
    mat4 pvm = projection * view * model;
    float totalScreenDistance = 0.0;

    // Convert the first point to screen coordinates
    vec4 clipPos = pvm * vec4(refPoints[0], 1.0);
    vec2 prevScreenPos = (clipPos.xy / clipPos.w + 1.0) / 2.0 * viewportSize;

    // Process subsequent points and accumulate distances between each point
    for (int i = 1; i < numRefPoints; ++i) {
        clipPos = pvm * vec4(refPoints[i], 1.0);
        vec2 currentScreenPos = (clipPos.xy / clipPos.w + 1.0) / 2.0 * viewportSize;
        totalScreenDistance += distance(prevScreenPos, currentScreenPos);
        prevScreenPos = currentScreenPos;
    }

    // Determine the number of segments based on the distance
    float segments = clamp(totalScreenDistance / pxPerSegment, 1.0, maxSegments);

    // Set tessellation level
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = segments;
}
