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

// Material properties (PBR)
uniform vec4 mainColor;
uniform float roughness;
uniform float metallic;
uniform float ao;
uniform bool useTexture;
uniform sampler2D textureSampler;

// Light properties
uniform vec3 lightPos_WorldSpace;  // Direction (for directional light) or position (for point light)
uniform vec3 lightAttenuation;     // C, L, Q coefficients for point light
uniform vec4 lightColor;

// Camera properties
uniform vec3 viewPos_WorldSpace;

const float PI = 3.14159265359;
// Constant ambient factor (placeholder until IBL is introduced)
const float kAmbient = 0.1;

// ACES filmic tone mapping (Narkowicz approximation): HDR -> LDR
vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Normal distribution function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float rough) {
    float a2 = pow(rough, 4.0);
    float NdotH = max(dot(N, H), 0.0);
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Geometry function (Schlick-GGX, single direction)
float GeometrySchlickGGX(float NdotX, float rough) {
    float k = pow(rough + 1.0, 2.0) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

// Geometry function (Smith: combines view and light directions)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float rough) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), rough)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), rough);
}

// Fresnel reflectance (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // --- Cook-Torrance BRDF ---

    // Determine light type (direction and distance attenuation)
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

    // Clamp roughness to avoid a singularity in the NDF at roughness = 0
    float rough = max(roughness, 0.04);

    // Normal (two-sided), view, light and halfway vectors
    vec3 N = normalize(fragNormal_WorldSpace);
    if (!gl_FrontFacing) N = -N;  // Flip the normal on back faces (two-sided)
    vec3 V = normalize(viewPos_WorldSpace - fragPos_WorldSpace);
    vec3 L = lightDir;
    vec3 H = normalize(V + L);

    // Base color (albedo): overlay texture in sRGB space, then linearize
    vec4 surfaceColor = mainColor;
    if (useTexture) {
        // Overlay texture color using alpha blending
        vec4 texColor = texture(textureSampler, fragTexCoord);
        surfaceColor.rgb = (1.0 - texColor.a) * surfaceColor.rgb + texColor.a * texColor.rgb;
    }
    vec3 albedo = pow(surfaceColor.rgb, vec3(2.2));
    vec3 radiance = pow(lightColor.rgb, vec3(2.2)) * attenuation;

    // Cook-Torrance specular BRDF
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, rough);
    float Gg  = GeometrySmith(N, V, L, rough);
    vec3 specular = (NDF * Gg * F)
                  / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4);

    // Diffuse weight (energy conservation; metals have no diffuse)
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    // Outgoing radiance contributed by this light
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // Constant ambient term (placeholder for IBL), modulated by AO
    vec3 ambient = kAmbient * albedo * ao;
    vec3 color = ambient + Lo;

    // Tone mapping (HDR -> LDR) and gamma correction (linear -> sRGB)
    vec3 mapped = ACESFilm(color);
    FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), surfaceColor.a);
}
