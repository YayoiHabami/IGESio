// Fragment Shader for Rational B-Spline Surface (type 128)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - mainColor (uniform vec4)
//   - ambientStrength (uniform float)
//   - specularStrength (uniform float)
//   - shininess (uniform int)
// - General:
//   - lightPos_WorldSpace (uniform vec3)
//   - lightColor (uniform vec4)
//   - viewPos_WorldSpace (uniform vec3)
#version 430 core
out vec4 FragColor;

// Data received from the Tessellation Evaluation Shader (interpolated)
in vec3 fragPos_WorldSpace;
in vec3 fragNormal_WorldSpace;

// Material properties
uniform vec4 mainColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform int shininess;

// Light properties
uniform vec3 lightPos_WorldSpace;
uniform vec4 lightColor;

// Camera properties
uniform vec3 viewPos_WorldSpace;

void main() {
    // --- Blinn-Phong Lighting Model ---

    // Ambient component
    vec4 ambient = ambientStrength * lightColor;

    // Diffuse component
    vec3 norm = normalize(fragNormal_WorldSpace);
    vec3 lightDir = normalize(lightPos_WorldSpace - fragPos_WorldSpace);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diff * lightColor;

    // Specular component
    vec3 viewDir = normalize(viewPos_WorldSpace - fragPos_WorldSpace);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    vec4 specular = specularStrength * spec * lightColor;

    // Combine components and apply the object's color
    vec4 result = (ambient + diffuse + specular);
    FragColor = result * mainColor;
}