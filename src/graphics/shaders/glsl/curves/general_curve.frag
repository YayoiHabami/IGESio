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
}
