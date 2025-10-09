// Tessellation Evaluation Shader for Rational B-Spline Curve (type 126)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - degreeT (uniform int)
//   - numCtrlPointsT (uniform int)
//   - paramRangeT (uniform vec2)
// - EntityDependent (SSBO; std430)
//   - knotsT (float[] - binding=0)
//   - controlPointsWithWeightsT (vec4[] - binding=1)
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
#version 430 core
layout (isolines, equal_spacing, ccw) in;

// constants
const int MAX_DEGREE = 5;

// parameters
uniform int degreeT;
uniform int numCtrlPointsT;
uniform vec2 paramRangeT;  // [start, end]

// Use Shader Storage Buffer Objects (SSBOs) for large data arrays
layout(std430, binding = 0) buffer KnotBufferT {
    float knotsT[];
};
layout(std430, binding = 1) buffer ControlPointBufferT {
    vec4 controlPointsWithWeightsT[]; // vec4(x, y, z, weight)
};

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

#include "glsl/common/nurbs_curve_prop.glsl"

void main() {
    // Map gl_TessCoord.x (range [0, 1]) to the curve's parameter range [start, end]
    float t = paramRangeT.x + gl_TessCoord.x * (paramRangeT.y - paramRangeT.x);

    // Calculate the coordinates on the curve
    vec3 pos = computeNURBSCurvePoint(t);

    // Perform coordinate transformation and set the final vertex position
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
