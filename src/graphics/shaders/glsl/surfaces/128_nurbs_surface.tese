// Tessellation Evaluation Shader for Rational B-Spline Surface (type 128)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - degreeU, degreeV (uniform int)
//   - numCtrlPointsU, numCtrlPointsV (uniform int)
//   - paramRangeU, paramRangeV (uniform vec2)
// - EntityDependent (SSBO; std430):
//   - knotsU (float[] - binding=0)
//   - knotsV (float[] - binding=1)
//   - controlPointsWithWeights (vec4[] - binding=2)
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
#version 430 core
layout (quads, equal_spacing, ccw) in;

// constants
const int MAX_DEGREE = 5;

// NURBS surface definition
uniform int degreeU, degreeV;
uniform int numCtrlPointsU, numCtrlPointsV;
uniform vec2 paramRangeU;
uniform vec2 paramRangeV;

// Use Shader Storage Buffer Objects (SSBOs) for large data arrays
layout(std430, binding = 0) buffer KnotBufferU {
    float knotsU[];
};
layout(std430, binding = 1) buffer KnotBufferV {
    float knotsV[];
};
layout(std430, binding = 2) buffer ControlPointBuffer {
    vec4 controlPointsWithWeights[]; // vec4(x, y, z, weight)
};

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Data to pass to Fragment Shader
out vec3 fragPos_WorldSpace;
out vec3 fragNormal_WorldSpace;
out vec2 fragTexCoord;

#include "glsl/common/nurbs_surface_prop.glsl"



void main() {
    // Map gl_TessCoord (range [0, 1]) to the surface's parameter range
    float u = paramRangeU.x + gl_TessCoord.x * (paramRangeU.y - paramRangeU.x);
    float v = paramRangeV.x + gl_TessCoord.y * (paramRangeV.y - paramRangeV.x);
    // Pass normalized (u, v) for texture mapping
    fragTexCoord = vec2((u - paramRangeU.x) / (paramRangeU.y - paramRangeU.x),
                        (v - paramRangeV.x) / (paramRangeV.y - paramRangeV.x));

    // Calculate the position and normal in object space
    vec3 pos_ObjectSpace, normal_ObjectSpace;
    computeNURBSSurfaceProperties(u, v, pos_ObjectSpace, normal_ObjectSpace);

    // Transform position and normal to world space for lighting calculations
    fragPos_WorldSpace = vec3(model * vec4(pos_ObjectSpace, 1.0));
    // Use the inverse transpose of the model matrix for the normal
    // This correctly handles non-uniform scaling
    fragNormal_WorldSpace = mat3(transpose(inverse(model))) * normal_ObjectSpace;

    // Final vertex position for rasterization
    gl_Position = projection * view * vec4(fragPos_WorldSpace, 1.0);
}