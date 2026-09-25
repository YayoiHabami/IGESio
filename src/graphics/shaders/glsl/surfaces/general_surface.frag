// Fragment Shader for General Surface Rendering (Cook-Torrance PBR)
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - EntityDependent (uniform):
//   - mainColor (uniform vec4) - base color (albedo), authored in sRGB
//   - roughness (uniform float) - surface roughness [0, 1]
//   - metallic (uniform float) - metalness [0, 1]
//   - ao (uniform float) - ambient occlusion [0, 1]
//   - useTexture (uniform bool)
//   - textureSampler (uniform sampler2D) - texture unit should be set to 0
// - General:
//   - numLights (uniform int) - number of active lights ([0, MAX_LIGHTS])
//   - lightPositions (uniform vec3[MAX_LIGHTS]) - direction or position
//   - lightAttenuations (uniform vec3[MAX_LIGHTS]) - C, L, Q (zero => directional)
//   - lightColors (uniform vec4[MAX_LIGHTS])
//   - viewPos_WorldSpace (uniform vec3)
//   - ambientColor (uniform vec3) - constant environment color for ambient
// (roughness/metallic/ao and the General uniforms are declared in
//  glsl/surfaces/pbr_shading.glsl)
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

// Base color and texture
uniform vec4 mainColor;
uniform bool useTexture;
uniform sampler2D textureSampler;

#include "glsl/surfaces/pbr_shading.glsl"

void main() {
    // Normal (two-sided) and view direction
    vec3 N = normalize(fragNormal_WorldSpace);
    if (!gl_FrontFacing) N = -N;  // Flip the normal on back faces (two-sided)
    vec3 V = normalize(viewPos_WorldSpace - fragPos_WorldSpace);

    // Base color (albedo): overlay texture in sRGB space, then linearize
    vec4 surfaceColor = mainColor;
    if (useTexture) {
        // Overlay texture color using alpha blending
        vec4 texColor = texture(textureSampler, fragTexCoord);
        surfaceColor.rgb = (1.0 - texColor.a) * surfaceColor.rgb + texColor.a * texColor.rgb;
    }
    vec3 albedo = pow(surfaceColor.rgb, vec3(2.2));

    // Cook-Torrance BRDF (multiple lights + ambient), then tone mapping and gamma
    vec3 color = ShadePBR(N, V, fragPos_WorldSpace, albedo);
    FragColor = vec4(ToneMapGamma(color), surfaceColor.a);
}
