// Fragment Shader for General Surface Rendering
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - mainColor (uniform vec4)
//   - ambientStrength (uniform float)
//   - specularStrength (uniform float)
//   - shininess (uniform int)
//   - useTexture (uniform bool)
//   - textureSampler (uniform sampler2D) - texture unit should be set to 0
// - General:
//   - lightPos_WorldSpace (uniform vec3)
//   - lightColor (uniform vec4)
//   - viewPos_WorldSpace (uniform vec3)
//
// THIS SHADER REQUIRES THE FOLLOWING INPUTS FROM THE PREVIOUS STAGE:
// - fragPos_WorldSpace (in vec3)
// - fragNormal_WorldSpace (in vec3)
// - fragTexCoord (in vec2) - texture coordinates (u, v)
#version 400 core
out vec4 FragColor;

in vec3 fragPos_WorldSpace;
in vec3 fragNormal_WorldSpace;
in vec2 fragTexCoord;

// Material properties
uniform vec4 mainColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform int shininess;
uniform bool useTexture;
uniform sampler2D textureSampler;

// Light properties
uniform vec3 lightPos_WorldSpace;  // Direction (for directional light) or position (for point light)
uniform vec3 lightAttenuation;     // C, L, Q coefficients for point light
uniform vec4 lightColor;

// Camera properties
uniform vec3 viewPos_WorldSpace;

void main() {
    // --- Blinn-Phong Lighting Model ---

    // Determine light type
    vec3 lightDir;
    float attenuation = 1.0;
    if (lightAttenuation == vec3(0.0)) {
        // Directional light
        lightDir = normalize(-lightPos_WorldSpace);
    } else {
        // Point light
        vec3 toLight = lightPos_WorldSpace - fragPos_WorldSpace;
        float distance = length(toLight);
        lightDir = normalize(toLight);
        attenuation = 1.0 / (lightAttenuation.x +
                             lightAttenuation.y * distance +
                             lightAttenuation.z * distance * distance);
    }

    // Ambient component
    vec4 ambient = ambientStrength * lightColor;

    // Diffuse component
    vec3 norm = normalize(fragNormal_WorldSpace);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diff * lightColor;

    // Specular component
    vec3 viewDir = normalize(viewPos_WorldSpace - fragPos_WorldSpace);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    vec4 specular = specularStrength * spec * lightColor;

    // Get surface color
    vec4 surfaceColor = mainColor;
    if (useTexture) {
        // Overlay texture color using alpha blending
        vec4 texColor = texture(textureSampler, fragTexCoord);
        surfaceColor.rgb = (1.0 - texColor.a) * surfaceColor.rgb + texColor.a * texColor.rgb;
    }

    // Combine components and apply the object's color
    vec3 lighting = (ambient.rgb + (diffuse.rgb + specular.rgb) * attenuation);
    vec3 finalColor = lighting * surfaceColor.rgb;
    FragColor = vec4(finalColor, surfaceColor.a);
}
