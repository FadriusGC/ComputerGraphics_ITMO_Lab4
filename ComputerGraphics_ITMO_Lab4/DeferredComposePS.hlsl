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

float3 ReconstructWorldPos(float2 uv, float depth, out float linearDepth) {
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    float4 ndcPos = float4(x, y, depth, 1.0f);
    float4 worldPos = mul(ndcPos, gInvViewProj);
    
    linearDepth = abs(1.0f / worldPos.w); // Изменено на более стабильный вариант
    return worldPos.xyz / worldPos.w;
}

float CalcShadowFactor(float3 worldPos, float pixelDepth) {
    uint cascade = 0;
    // Логика выбора каскада
    [unroll]
    for (uint i = 0; i < CASCADE_COUNT - 1; ++i) {
        if (pixelDepth > gCascadeSplits[i]) {
            cascade = i + 1;
        }
    }
    cascade = min(cascade, CASCADE_COUNT - 1);

    float4 shadowPosH = mul(float4(worldPos, 1.0f), gShadowTransforms[cascade]);
    shadowPosH.xyz /= shadowPosH.w;

    //Добавлен BIAS (0.002f)
    float bias = 0.002f;
    
    // Используем упрощенный семплинг для проверки
    return gShadowMap.SampleCmpLevelZero(gsamShadow, float3(shadowPosH.xy, cascade), shadowPosH.z - bias);
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
    float spec = pow(saturate(dot(normal, halfVec)), lerp(64.0f, 4.0f, saturate(roughness))) * attenuation;
    float3 specular = radiance * spec * lerp(0.25f, 0.04f, saturate(roughness));

    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        diffuse *= shadowFactor;
        specular *= shadowFactor;
    }

    return diffuse + specular;
}

float4 PS(PS_INPUT input) : SV_Target {
    float4 normalSample = gNormal.Sample(gSampler, input.TexC);
    float depth = gDepth.Sample(gSampler, input.TexC).r;
    
    if (depth >= 1.0f) discard; // Оптимизация пустого пространства

    float4 albedo = gAlbedo.Sample(gSampler, input.TexC);
    float3 normal = normalize(normalSample.xyz * 2.0f - 1.0f);
    float roughness = saturate(normalSample.a);
    
    float pixelDepth;
    float3 worldPos = ReconstructWorldPos(input.TexC, depth, pixelDepth);
    float3 viewDir = normalize(gCameraPosition.xyz - worldPos);
    
    float shadowFactor = CalcShadowFactor(worldPos, pixelDepth);
    //if (shadowFactor < 1.0f) return float4(1.0, 0.0, 0.0, 1.0);
    float3 color = albedo.rgb * 0.05f; // Ambient
    uint lightCount = min((uint)gLightCount.x, MAX_LIGHTS);

    for (uint i = 0; i < lightCount; ++i) {
        uint lightType = (uint)gLights[i].DirectionAndType.w;
        color += albedo.rgb * EvaluateLight(lightType, gLights[i], worldPos, normal, viewDir, roughness, shadowFactor);
    }

    return float4(saturate(color), albedo.a);
}