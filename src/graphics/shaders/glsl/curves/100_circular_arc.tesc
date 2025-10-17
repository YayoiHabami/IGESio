// Tessellation Control Shader for Circular Arc (type 100)
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
//   - viewportSize (uniform vec2)
#version 400 core
layout (vertices = 1) out;

// parameters
uniform vec3 center;
uniform float radius;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle
const int pxPerSegment = 2;
const float maxSegments = 100.0;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// viewport
uniform vec2 viewportSize;

void main() {
    gl_InvocationID;  // to avoid compiler warnings

    vec4 startPosW = vec4(center.x + radius * cos(startAngle),
                          center.y + radius * sin(startAngle), center.z, 1.0);
    vec4 endPosW = vec4(center.x + radius * cos(endAngle),
                        center.y + radius * sin(endAngle), center.z, 1.0);
    float midAngle = (startAngle + endAngle) / 2.0;
    vec4 midPosW = vec4(center.x + radius * cos(midAngle),
                        center.y + radius * sin(midAngle), center.z, 1.0);

    // World -> Clip coordinates
    mat4 pvm = projection * view * model;
    vec4 startPosClip = pvm * startPosW;
    vec4 endPosClip = pvm * endPosW;
    vec4 midPosClip = pvm * midPosW;

    // Clip -> Screen coordinates
    vec2 startPosScreen = (startPosClip.xy / startPosClip.w + 1.0) / 2.0 * viewportSize;
    vec2 endPosScreen = (endPosClip.xy / endPosClip.w + 1.0) / 2.0 * viewportSize;
    vec2 midPosScreen = (midPosClip.xy / midPosClip.w + 1.0) / 2.0 * viewportSize;

    // Calculate the number of segments
    float dist = distance(startPosScreen, midPosScreen)
               + distance(midPosScreen, endPosScreen);
    float segments = clamp(dist / pxPerSegment, 1.0, maxSegments);

    // Set tessellation level
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = segments;
}
