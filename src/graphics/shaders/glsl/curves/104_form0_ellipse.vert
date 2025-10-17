// Tesselation Evaluation Shader for Ellipse (type 104, form 0)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - Entity Dependent:
//   - center (uniform vec3)
//   - radiusX, radiusY (uniform float)
//   - startAngle (uniform float)  // [rad]
//   - endAngle (uniform float)    // [rad], endAngle > startAngle
//   - segments (uniform int)      // number of segments
// - General:
//   - model (uniform mat4)
//   - view (uniform mat4)
//   - projection (uniform mat4)
#version 400 core

// parameters
uniform vec3 center;
uniform float radiusX;
uniform float radiusY;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// segmentation
uniform int segments;
void main() {
    float t = float(gl_VertexID) / float(segments);
    float angle = startAngle + t * (endAngle - startAngle);

    vec4 pos = vec4(center.x + radiusX * cos(angle),
                    center.y + radiusY * sin(angle),
                    center.z, 1.0);
    gl_Position = projection * view * model * pos;
}
