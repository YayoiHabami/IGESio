// Tesselation Evaluation Shader for Circular Arc (type 100)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - Entity Dependent:
//   - center (uniform vec3)
//   - radius (uniform float)
//   - startAngle (uniform float)  // [rad]
//   - endAngle (uniform float)    // [rad], endAngle > startAngle
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
#version 400 core
layout (isolines, equal_spacing, ccw) in;

// parameters
uniform vec3 center;
uniform float radius;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    float t = gl_TessCoord.x;  // [0.0, 1.0]
    float angle = startAngle + t * (endAngle - startAngle);

    vec4 pos = vec4(center.x + radius * cos(angle),
                    center.y + radius * sin(angle),
                    center.z, 1.0);
    gl_Position = projection * view * model * pos;
}
