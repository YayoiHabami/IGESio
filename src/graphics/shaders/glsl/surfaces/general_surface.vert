// Vertex Shader for General Surface Rendering
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (VAO):
//   - aPos (layout location = 0) - vec3
//   - aNorm (layout location = 1) - vec3
//   - aTexCoord (layout location = 2) - vec2
// - General:
//   - lightPos_WorldSpace (uniform vec3)
//   - lightColor (uniform vec4)
//   - viewPos_WorldSpace (uniform vec3)
#version 400 core
layout(location = 0) in vec3 aPos;      // Vertex position
layout(location = 1) in vec3 aNorm;     // Vertex normal
layout(location = 2) in vec2 aTexCoord; // Vertex texture coordinates

out vec3 fragPos_WorldSpace;
out vec3 fragNormal_WorldSpace;
out vec2 fragTexCoord;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Transform vertex position to world space
    fragPos_WorldSpace = vec3(model * vec4(aPos, 1.0));
    // Transform normal to world space
    fragNormal_WorldSpace = mat3(transpose(inverse(model))) * aNorm;

    // Final vertex position in clip space
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    // Pass through texture coordinates
    fragTexCoord = aTexCoord;
}
