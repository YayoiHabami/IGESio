// Fragment Shader for General Curves
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - General:
//   - mainColor (uniform vec4)
#version 400 core

out vec4 FragColor;
uniform vec4 mainColor;

void main() {
    FragColor = mainColor;

    // Slightly adjust depth to avoid z-fighting
    // assuming curves are rendered on top of surfaces
    gl_FragDepth = gl_FragCoord.z - 0.0001;
}
