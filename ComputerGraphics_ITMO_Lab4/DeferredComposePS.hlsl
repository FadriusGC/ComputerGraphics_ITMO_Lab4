struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gAlbedo : register(t0);            // rgb = base color (linear), a = metallic
Texture2D gNormal : register(t1);            // xyz = world normal, a = roughness
Texture2D gDepth : register(t2);
Texture2DArray gShadowMap : register(t3);
TextureCube gIrradianceMap : register(t4);       // IBL diffuse irradiance
TextureCube gPrefilteredEnvMap : register(t5);   // IBL specular (roughness mips)
Texture2D gBrdfLUT : register(t6);               // IBL split-sum BRDF integration
SamplerState gSampler : register(s0);
SamplerComparisonState gsamShadow : register(s1);
SamplerState gSamplerLinearClamp : register(s2); // IBL / LUT sampling (clamped)

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_SPOT = 2;
static const uint MAX_LIGHTS = 100;
static const uint CASCADE_COUNT = 3;
static const float PI = 3.14159265359f;

struct GpuLight {
    float4 PositionWorldAndRange;
    float4 DirectionAndType;
    float4 ColorAndIntensity;
    float4 Params;
};

cbuffer cbCompose : register(b0) {
    float4x4 gInvViewProj;
    float4 gCameraPosition;
    float4 gScreenSize;
    float4 gLightCount;
    float4 gCascadeSplits;
    float4 gPostProcessParams; // x: exposure, y: gamma, z: enableHdr, w: enableGammaCorrection
    float4 gMonitorEffectParams; // x: enableMonitorEffect, y: totalTime
    float4 gIblParams; // x: prefiltered max mip, y: IBL intensity, z: enable IBL, w: ao
    float4x4 gShadowTransforms[CASCADE_COUNT];
    float4x4 gLightViewProj[CASCADE_COUNT];
    GpuLight gLights[MAX_LIGHTS];
};

float3 ReconstructWorldPos(float2 uv, float depth, out float linearDepth) {
    float x = uv.x * 2.0f - 1.0f;
    float y = 1.0f - uv.y * 2.0f;

    float4 ndcPos = float4(x, y, depth, 1.0f);
    float4 worldPos = mul(ndcPos, gInvViewProj);

    float3 world = worldPos.xyz / worldPos.w;
    linearDepth = distance(world, gCameraPosition.xyz);
    return world;
}

float CalcShadowFactor(float3 worldPos, float3 normalW, float3 lightDirW, float pixelDepth) {
    uint cascade = 0;
    [unroll]
    for (uint i = 0; i < CASCADE_COUNT - 1; ++i) {
        if (pixelDepth > gCascadeSplits[i]) {
            cascade = i + 1;
        }
    }
    cascade = min(cascade, CASCADE_COUNT - 1);
    float4 shadowPosH = mul(float4(worldPos, 1.0f), gLightViewProj[cascade]);

    shadowPosH.xyz /= shadowPosH.w;
    float2 shadowTexC = shadowPosH.xy * 0.5f + 0.5f;
    shadowTexC.y = 1.0f - shadowTexC.y;

    if (shadowTexC.x < 0.0f || shadowTexC.x > 1.0f ||
        shadowTexC.y < 0.0f || shadowTexC.y > 1.0f ||
        shadowPosH.z < 0.0f || shadowPosH.z > 1.0f) {
        return 1.0f;
    }

    uint w, h, elements;
    gShadowMap.GetDimensions(w, h, elements);
    float2 texelSize = 1.0f / float2((float)w, (float)h);
    float2 offsets[9] = {
        float2(-1.0f,-1.0f), float2(0.0f,-1.0f), float2(1.0f,-1.0f),
        float2(-1.0f, 0.0f), float2(0.0f, 0.0f), float2(1.0f, 0.0f),
        float2(-1.0f, 1.0f), float2(0.0f, 1.0f), float2(1.0f, 1.0f)
    };

    float ndotl = saturate(dot(normalW, lightDirW));
    float bias = max(0.00035f * (1.0f - ndotl), 0.00008f);
    float compareDepth = shadowPosH.z - bias;

    float lit = 0;
    [unroll] for (int i=0;i<9;++i) {
        lit += gShadowMap.SampleCmpLevelZero(gsamShadow, float3(shadowTexC + offsets[i] * texelSize, cascade), compareDepth).r;
    }
    return lit / 9.0f;
}


// Cook-Torrance PBR

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;       // Disney reparam: alpha = roughness^2
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-7f);
}

float GeometrySchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0f - k) + k);
}

// k is supplied by the caller: direct light -> (r+1)^2/8, IBL -> r^2/2.
float GeometrySmith(float3 N, float3 V, float3 L, float k) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    float3 r = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (r - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Direct radiance contribution of a single analytic light.
float3 EvaluateLightPBR(GpuLight light, float3 N, float3 V, float3 worldPos,
                        float3 albedo, float metallic, float roughness,
                        float3 F0, float shadowFactor) {
    uint lightType = (uint)light.DirectionAndType.w;
    float3 radiance = light.ColorAndIntensity.rgb * light.ColorAndIntensity.a;
    float3 L = 0.0f;
    float attenuation = 1.0f;

    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        L = normalize(-light.DirectionAndType.xyz);
    } else {
        float3 toLight = light.PositionWorldAndRange.xyz - worldPos;
        float dist = length(toLight);
        if (dist <= 1e-4f) return 0.0f;
        L = toLight / dist;
        float range = max(light.PositionWorldAndRange.w, 1e-3f);
        float rangeFade = saturate(1.0f - dist / range);
        attenuation = rangeFade * rangeFade;
        if (lightType == LIGHT_TYPE_SPOT) {
            float3 spotDir = normalize(-light.DirectionAndType.xyz);
            float cosTheta = dot(L, spotDir);
            float innerCos = light.Params.x;
            float outerCos = light.Params.y;
            attenuation *= saturate((cosTheta - outerCos) / max(innerCos - outerCos, 1e-4f));
        }
    }

    float NdotL = max(dot(N, L), 0.0f);
    if (NdotL <= 0.0f) return 0.0f;
    radiance *= attenuation;

    float3 H = normalize(V + L);
    float k = roughness + 1.0f;
    k = (k * k) / 8.0f;                       // direct-light geometry term
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, k);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 numerator = D * G * F;
    float denom = 4.0f * max(dot(N, V), 0.0f) * NdotL + 1e-4f;
    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);
    float3 diffuse = kD * albedo / PI;

    float shadow = (lightType == LIGHT_TYPE_DIRECTIONAL) ? shadowFactor : 1.0f;
    return (diffuse + specular) * radiance * NdotL * shadow;
}

float Hash2(float2 p) //generit psevdosluchaemiy shum
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.2831f);
    p3 += dot(p3, p3.yzx + 19.19f);
    return frac((p3.x + p3.y) * p3.z);
}

//monitor post process ezhzhe
float3 ApplyMonitorEffect(float2 uv, float3 sceneColor)
{
    float2 resolution = gScreenSize.xy;
    float t = gMonitorEffectParams.y;

    float2 V = 1.0f - 2.0f * uv;
    float3 result = float3(0.0f, 0.1f, 0.2f);
    result += sceneColor;
    result += 0.06f * Hash2(
        float2(t + V.x * 1462.439f, t + V.y * 297.185f)
    );
    result *= 1.25f * (1.0f - smoothstep(0.1f, 1.8f, length(V * V)));
    float scanline = 0.90f + 0.10f * sin(uv.y * resolution.y * 0.5f);
    result *= scanline;
    return saturate(result);
}

float FishEyeCorrection(float fov, float2 uv) {
    float z = 1.0f / tan(fov * 0.5f);
    float xyLen = length(uv);
    if (xyLen < 1e-5f) {
        return 1.0f;
    }
    float b = atan2(xyLen, z);
    float k = 2.0f * b / (xyLen * fov);
    return k;
}

float2 DistortFishEyeUV(float2 uv01) {
    float2 uv = uv01 * 2.0f - 1.0f;
    float aspect = gScreenSize.x / gScreenSize.y;
    uv.y /= aspect;
    const float fov = 120.0f * PI / 180.0f;
    float k = FishEyeCorrection(fov, uv);
    float2 newUv = uv / max(k, 1e-5f);
    newUv.y *= aspect;
    return (newUv + 1.0f) * 0.5f;
}

float Bayer4x4(int2 pixelPos)
{
    static const float bayer[16] =
    {
         0.0f,  8.0f,  2.0f, 10.0f,
        12.0f,  4.0f, 14.0f,  6.0f,
         3.0f, 11.0f,  1.0f,  9.0f,
        15.0f,  7.0f, 13.0f,  5.0f
    };
    int x = pixelPos.x & 3;
    int y = pixelPos.y & 3;
    return bayer[y * 4 + x] / 16.0f;
}

float3 ApplyDithering(float3 color, float2 uv)
{
    float2 screenPos = uv * gScreenSize.xy;
    float threshold = Bayer4x4(int2(screenPos));
    float ditherStrength = 50.0f / 255.0f;
    color += (threshold - 0.5f) * ditherStrength;
    float colorLevels = 8.0f;
    color = floor(color * colorLevels) / colorLevels;
    return saturate(color);
}

float4 PS(PS_INPUT input) : SV_Target {
    float2 sampleUv = input.TexC;
    if (gMonitorEffectParams.z > 0.5f) {
        sampleUv = DistortFishEyeUV(input.TexC);
    }

    if (sampleUv.x < 0.0f || sampleUv.x > 1.0f || sampleUv.y < 0.0f || sampleUv.y > 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float4 albedoSample = gAlbedo.Sample(gSampler, sampleUv);
    float4 normalSample = gNormal.Sample(gSampler, sampleUv);
    float depth = gDepth.Sample(gSampler, sampleUv).r;
    if (depth >= 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float3 albedo = albedoSample.rgb;            // already linear from the G-buffer
    float metallic = saturate(albedoSample.a);
    float roughness = saturate(normalSample.a);
    float3 N = normalize(normalSample.xyz * 2.0f - 1.0f);

    float pixelDepth = 0.0f;
    float3 worldPos = ReconstructWorldPos(sampleUv, depth, pixelDepth);
    float3 V = normalize(gCameraPosition.xyz - worldPos);
    float NdotV = max(dot(N, V), 0.0f);

    // reflectivity 0.4
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 dirLightDir = normalize(-gLights[0].DirectionAndType.xyz);
    float shadowFactor = CalcShadowFactor(worldPos, N, dirLightDir, pixelDepth);

    //Direct lighting
    float3 Lo = 0.0f;
    uint lightCount = min((uint)gLightCount.x, MAX_LIGHTS);
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        Lo += EvaluateLightPBR(gLights[i], N, V, worldPos, albedo, metallic,
                               roughness, F0, shadowFactor);
    }

    // Indirect lighting (IBL ambient + reflections)
    float ao = gIblParams.w > 0.0f ? gIblParams.w : 1.0f;
    float3 ambient;
    if (gIblParams.z > 0.5f) {
        float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metallic);

        float3 irradiance = gIrradianceMap.Sample(gSamplerLinearClamp, N).rgb;
        float3 diffuseIBL = irradiance * albedo;

        float3 R = reflect(-V, N);
        float maxMip = max(gIblParams.x, 1.0f);
        float3 prefiltered =
            gPrefilteredEnvMap.SampleLevel(gSamplerLinearClamp, R, roughness * maxMip).rgb;
        float2 brdf = gBrdfLUT.Sample(gSamplerLinearClamp, float2(NdotV, roughness)).rg;
        float3 specularIBL = prefiltered * (F * brdf.x + brdf.y);

        ambient = (kD * diffuseIBL + specularIBL) * ao * gIblParams.y;
    } else {
        ambient = albedo * 0.03f * ao; // fallback constant ambient if IBL is off
    }

    float3 color = ambient + Lo;
    float3 finalColor = max(color, 0.0f);

    float exposure = max(gPostProcessParams.x, 0.0001f);
    float gamma = max(gPostProcessParams.y, 0.0001f);
    bool enableHdr = (gPostProcessParams.z > 0.5f);
    bool enableGammaCorrection = (gPostProcessParams.w > 0.5f);

    if (enableHdr) {
        finalColor = 1.0f - exp(-finalColor * exposure);
    }
    if (enableGammaCorrection) {
        finalColor = pow(saturate(finalColor), 1.0f / gamma);
    }
    if (gMonitorEffectParams.x > 0.5f) {
        finalColor = ApplyMonitorEffect(input.TexC, finalColor);
    }
    if (gMonitorEffectParams.w > 0.5f) {
        finalColor = ApplyDithering(finalColor, input.TexC);
    }

    return float4(finalColor, 1.0f);
}
