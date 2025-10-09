// Tessellation Control Shader for Rational B-Spline Surface (type 128)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - numRefPointsU, numRefPointsV (uniform int)
// - EntityDependent (SSBO; std430):
//   - refPoints (vec3[] - binding=3)
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
//   - viewportSize (uniform vec2)
#version 430 core
layout (vertices = 1) out;

// constants
const int pxPerSegment = 10;
const float maxSegments = 100.0;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// viewport
uniform vec2 viewportSize;

// reference points (grid of points on the surface)
uniform int numRefPointsU;
uniform int numRefPointsV;
layout(std430, binding = 3) buffer RefPointsBuffer {
    vec3 refPoints[];
};

// Helper to project a point to 2D screen space
vec2 projectToScreen(vec3 pos, mat4 pvm) {
    vec4 clipPos = pvm * vec4(pos, 1.0);
    return (clipPos.xy / clipPos.w + 1.0) / 2.0 * viewportSize;
}

void main() {
    gl_InvocationID; // to avoid compiler warnings
    mat4 pvm = projection * view * model;

    // Approximate the screen-space size in U and V directions
    float totalDistU = 0.0;
    for (int j = 0; j < numRefPointsV; ++j) {
        vec2 prevScreenPos = projectToScreen(refPoints[j], pvm);
        for (int i = 1; i < numRefPointsU; ++i) {
            vec2 currentScreenPos = projectToScreen(refPoints[i * numRefPointsV + j], pvm);
            totalDistU += distance(prevScreenPos, currentScreenPos);
            prevScreenPos = currentScreenPos;
        }
    }

    float totalDistV = 0.0;
    for (int i = 0; i < numRefPointsU; ++i) {
        vec2 prevScreenPos = projectToScreen(refPoints[i * numRefPointsV], pvm);
        for (int j = 1; j < numRefPointsV; ++j) {
            vec2 currentScreenPos = projectToScreen(refPoints[i * numRefPointsV + j], pvm);
            totalDistV += distance(prevScreenPos, currentScreenPos);
            prevScreenPos = currentScreenPos;
        }
    }

    float avgDistU = totalDistU / numRefPointsV;
    float avgDistV = totalDistV / numRefPointsU;

    // Determine segments based on average screen distance
    float segmentsU = clamp(avgDistU / pxPerSegment, 1.0, maxSegments);
    float segmentsV = clamp(avgDistV / pxPerSegment, 1.0, maxSegments);

    // Set tessellation levels for a quad patch
    gl_TessLevelOuter[0] = segmentsV;
    gl_TessLevelOuter[1] = segmentsU;
    gl_TessLevelOuter[2] = segmentsV;
    gl_TessLevelOuter[3] = segmentsU;
    gl_TessLevelInner[0] = segmentsU;
    gl_TessLevelInner[1] = segmentsV;
}
