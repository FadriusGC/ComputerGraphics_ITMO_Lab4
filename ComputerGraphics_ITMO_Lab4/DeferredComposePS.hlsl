struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gDepth : register(t2);
Texture2DArray gShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowCmp : register(s1);

#define MAX_LIGHTS 64
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_SPOT 2

struct GpuLight {
    float4 PositionWorldAndRange;
    float4 DirectionAndType;
    float4 ColorAndIntensity;
    float4 Params;
};

cbuffer CB_Compose : register(b0) {
    float4x4 gInvViewProj;
    float4 gCameraPosition;
    float4 gScreenSize;
    float4 gCameraForward;
    float4 gLightCount;
    float4 gCascadeSplits;
    float4 gShadowTexelSize;
    float4x4 gLightViewProj[3];
    GpuLight gLights[MAX_LIGHTS];
};

float3 ReconstructWorldPos(float2 uv, float depth) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 world = mul(ndc, gInvViewProj);
    return world.xyz / max(world.w, 1e-6f);
}

float ComputeShadowCSM(float3 worldPos, float3 normal, float3 lightDir) {
    float3 viewVec = worldPos - gCameraPosition.xyz;
    float z = abs(dot(viewVec, normalize(gCameraForward.xyz)));

    int cascade = (z < gCascadeSplits.x)
                      ? 0
                      : ((z < gCascadeSplits.y) ? 1 : 2);

    float4 lightClip = mul(float4(worldPos, 1.0f), gLightViewProj[cascade]);
    float3 shadowTex = lightClip.xyz / lightClip.w;
    shadowTex.xy = shadowTex.xy * 0.5f + 0.5f;
    shadowTex.y = 1.0f - shadowTex.y;

    if (shadowTex.x < 0.0f || shadowTex.x > 1.0f || shadowTex.y < 0.0f ||
        shadowTex.y > 1.0f || shadowTex.z < 0.0f || shadowTex.z > 1.0f) {
        return 1.0f;
    }

    float ndotl = saturate(dot(normal, -lightDir));
    float bias = max(0.0035f * (1.0f - ndotl), 0.0010f);
    float2 texel = gShadowTexelSize.xy;

    float visibility = 0.0f;
    [unroll]
    for (int x = -1; x <= 1; ++x) {
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            float2 uv = shadowTex.xy + float2(x, y) * texel;
            visibility += gShadowMap.SampleCmpLevelZero(
                gShadowCmp, float3(uv, cascade), shadowTex.z - bias);
        }
    }

    return visibility / 9.0f;
}

float3 EvaluateLight(
    uint type,
    GpuLight light,
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float roughness) {
    float3 lightDir = 0.0f;
    float attenuation = 1.0f;

    if (type == LIGHT_TYPE_DIRECTIONAL) {
        lightDir = normalize(-light.DirectionAndType.xyz);
    } else {
        float3 toLight = light.PositionWorldAndRange.xyz - worldPos;
        float distanceToLight = length(toLight);

        if (distanceToLight <= 1e-4f) {
            return 0.0f.xxx;
        }

        lightDir = toLight / distanceToLight;
        float range = max(light.PositionWorldAndRange.w, 1e-3f);
        float rangeFactor = saturate(1.0f - distanceToLight / range);
        attenuation = rangeFactor * rangeFactor;

        if (type == LIGHT_TYPE_SPOT) {
            float3 spotDirection = normalize(-light.DirectionAndType.xyz);
            float cosineAngle = dot(lightDir, spotDirection);
            attenuation *= saturate(
                (cosineAngle - light.Params.y) /
                max(light.Params.x - light.Params.y, 1e-4f));
        }
    }

    float3 halfway = normalize(lightDir + viewDir);
    float ndotl = saturate(dot(normal, lightDir));
    float specExp = lerp(64.0f, 8.0f, roughness);
    float specular = pow(saturate(dot(normal, halfway)), specExp);

    float shadow = 1.0f;
    if (type == LIGHT_TYPE_DIRECTIONAL) {
        shadow = ComputeShadowCSM(worldPos, normal, lightDir);
    }

    float3 lightColor =
        light.ColorAndIntensity.rgb * light.ColorAndIntensity.w;
     return (ndotl + specular) * attenuation * shadow * lightColor;
}

float4 PS(PS_INPUT input) : SV_Target {
    float4 albedo = gAlbedo.Sample(gSampler, input.TexC);
    float4 normalSample = gNormal.Sample(gSampler, input.TexC);
    float depth = gDepth.Sample(gSampler, input.TexC).r;

    float3 normal = normalize(normalSample.xyz * 2.0f - 1.0f);
    float roughness = saturate(normalSample.a);
    float3 worldPos = ReconstructWorldPos(input.TexC, depth);
    float3 viewDir = normalize(gCameraPosition.xyz - worldPos);

    float3 color = albedo.rgb * 0.05f;
    uint lightCount = min((uint)gLightCount.x, MAX_LIGHTS);
    [loop]
    for (uint lightIndex = 0; lightIndex < lightCount; ++lightIndex) {
        color += albedo.rgb * EvaluateLight(
            (uint)gLights[lightIndex].DirectionAndType.w,
            gLights[lightIndex],
            worldPos,
            normal,
            viewDir,
            roughness);
    }

    color = color / (1.0f + color);
    return float4(saturate(color), albedo.a);
}