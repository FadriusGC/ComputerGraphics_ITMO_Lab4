struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gDepth : register(t2);
Texture2DArray gShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gsamShadow : register(s1);

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_SPOT = 2;
static const uint MAX_LIGHTS = 100;
static const uint CASCADE_COUNT = 3;

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
    float4x4 gShadowTransforms[CASCADE_COUNT];
    GpuLight gLights[MAX_LIGHTS];
};

float3 ReconstructWorldPos(float2 uv, float depth) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 worldPos = mul(ndc, gInvViewProj);
    return worldPos.xyz / max(worldPos.w, 1e-6f);
}

float CalcShadowFactor(float3 worldPos) {
    uint cascade = CASCADE_COUNT;
    float4 shadowPosH = 0.0f;
    [unroll]
    for (uint i = 0; i < CASCADE_COUNT; ++i) {
        float4 testPos = mul(float4(worldPos, 1.0f), gShadowTransforms[i]);
        testPos.xyz /= testPos.w;
        if (testPos.x >= 0.0f && testPos.x <= 1.0f &&
            testPos.y >= 0.0f && testPos.y <= 1.0f &&
            testPos.z >= 0.0f && testPos.z <= 1.0f) {
            cascade = i;
            shadowPosH = testPos;
            break;
        }
    }
    if (cascade >= CASCADE_COUNT) return 1.0f;

    uint w, h, elements;
    gShadowMap.GetDimensions(w, h, elements);
    float dx = 1.0f / (float)w;
    float2 offsets[9] = {
        float2(-dx,-dx), float2(0,-dx), float2(dx,-dx),
        float2(-dx,0), float2(0,0), float2(dx,0),
        float2(-dx,dx), float2(0,dx), float2(dx,dx)
    };
    float lit = 0;
    [unroll] for (int i=0;i<9;++i) {
        lit += gShadowMap.SampleCmpLevelZero(gsamShadow, float3(shadowPosH.xy + offsets[i], cascade), shadowPosH.z).r;
    }
    return lit / 9.0f;
}

float3 EvaluateLight(uint lightType, GpuLight light, float3 worldPos, float3 normal, float3 viewDir, float roughness, float shadowFactor) {
    float3 radiance = light.ColorAndIntensity.rgb * light.ColorAndIntensity.a;
    float3 L = 0.0f;
    float attenuation = 1.0f;

    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        L = normalize(-light.DirectionAndType.xyz);
    } else {
        float3 lightPos = light.PositionWorldAndRange.xyz;
        float3 toLight = lightPos - worldPos;
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
            float spot = saturate((cosTheta - outerCos) / max(innerCos - outerCos, 1e-4f));
            attenuation *= spot;
        }
    }

    float NdotL = saturate(dot(normal, L));
    float3 diffuse = radiance * NdotL * attenuation;

    float3 halfVec = normalize(L + viewDir);
    float specPower = lerp(64.0f, 4.0f, saturate(roughness));
    float specStrength = lerp(0.25f, 0.04f, saturate(roughness));
    float spec = pow(saturate(dot(normal, halfVec)), specPower) * attenuation;
    float3 specular = radiance * spec * specStrength;

    float directionalShadow = (lightType == LIGHT_TYPE_DIRECTIONAL) ? shadowFactor : 1.0f;
    return directionalShadow * (diffuse + specular);
}

float4 PS(PS_INPUT input) : SV_Target {
    float4 albedo = gAlbedo.Sample(gSampler, input.TexC);
    float4 normalSample = gNormal.Sample(gSampler, input.TexC);
    float3 encodedNormal = normalSample.xyz;
    float depth = gDepth.Sample(gSampler, input.TexC).r;

    float3 normal = normalize(encodedNormal * 2.0f - 1.0f);
    float roughness = saturate(normalSample.a);
    float3 worldPos = ReconstructWorldPos(input.TexC, depth);
    float3 viewDir = normalize(gCameraPosition.xyz - worldPos);
    float shadowFactor = CalcShadowFactor(worldPos);

    float3 color = albedo.rgb * 0.05f;
    uint lightCount = min((uint)gLightCount.x, MAX_LIGHTS);

    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        uint lightType = (uint)gLights[i].DirectionAndType.w;
        color += albedo.rgb * EvaluateLight(lightType, gLights[i], worldPos, normal, viewDir, roughness, shadowFactor);
    }

    return float4(saturate(color), albedo.a);
}