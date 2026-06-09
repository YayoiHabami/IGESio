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

// Light properties (must match kMaxLights on the C++ side)
#define MAX_LIGHTS 8
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];   // Direction (directional) or position (point)
uniform vec3 lightAttenuations[MAX_LIGHTS];  // C, L, Q coefficients (zero => directional)
uniform vec4 lightColors[MAX_LIGHTS];

// Camera properties
uniform vec3 viewPos_WorldSpace;

// Constant environment color reflected by the ambient term (cheap IBL stand-in)
uniform vec3 ambientColor;

const float PI = 3.14159265359;

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

// Fresnel reflectance with roughness (for the ambient/IBL specular term)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float rough) {
    return F0 + (max(vec3(1.0 - rough), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cook-Torrance outgoing radiance contributed by a single light
vec3 CalcLight(int i, vec3 N, vec3 V, vec3 albedo, float rough, vec3 F0) {
    // Determine light type (direction and distance attenuation)
    vec3 L;
    float attenuation = 1.0;
    if (lightAttenuations[i] == vec3(0.0)) {
        // Directional light
        L = normalize(-lightPositions[i]);
    } else {
        // Point light
        vec3 toLight = lightPositions[i] - fragPos_WorldSpace;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = 1.0 / (lightAttenuations[i].x +
                             lightAttenuations[i].y * dist +
                             lightAttenuations[i].z * dist * dist);
    }
    vec3 H = normalize(V + L);
    vec3 radiance = pow(lightColors[i].rgb, vec3(2.2)) * attenuation;

    // Cook-Torrance specular BRDF
    vec3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, rough);
    float Gg  = GeometrySmith(N, V, L, rough);
    vec3 specular = (NDF * Gg * F)
                  / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4);

    // Diffuse weight (energy conservation; metals have no diffuse)
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // --- Cook-Torrance BRDF (multiple lights) ---

    // Clamp roughness to avoid a singularity in the NDF at roughness = 0
    float rough = max(roughness, 0.04);

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
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Accumulate outgoing radiance over all active lights
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < numLights; ++i) {
        Lo += CalcLight(i, N, V, albedo, rough, F0);
    }

    // Ambient from a constant environment color (cheap IBL stand-in):
    // dielectric diffuse + metallic specular reflection (grazing-aware Fresnel)
    float NdotV = max(dot(N, V), 0.0);
    vec3 Fa  = FresnelSchlickRoughness(NdotV, F0, rough);
    vec3 kDa = (1.0 - Fa) * (1.0 - metallic);
    vec3 ambient = (kDa * albedo + Fa) * ambientColor * ao;
    vec3 color = ambient + Lo;

    // Tone mapping (HDR -> LDR) and gamma correction (linear -> sRGB)
    vec3 mapped = ACESFilm(color);
    FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), surfaceColor.a);
}
