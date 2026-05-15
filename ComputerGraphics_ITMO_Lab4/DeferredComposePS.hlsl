struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gDepth : register(t2);
Texture2DArray<float> gShadowMap : register(t3);

SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_SPOT = 2;
static const uint MAX_LIGHTS = 8;

struct GpuLight {
    float4 PositionWorldAndRange;
    float4 DirectionAndType;
    float4 ColorAndIntensity;
    float4 Params;
};

cbuffer cbCompose : register(b0) {
    float4x4 gInvViewProj;
    float4x4 gView;
    float4 gCameraPosition;
    float4 gScreenSize;
    float4 gCascadeSplits;
    float4x4 gShadowViewProj[4];
    float4 gShadowParams;
    float4 gLightCount;
    GpuLight gLights[100];
};

float3 ReconstructWorldPos(float2 uv, float d) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, d, 1.0f);
    float4 w = mul(ndc, gInvViewProj);
    return w.xyz / max(w.w, 1e-6f);
}

uint GetCascadeIndex(float viewDepth) {
    uint i = 0;
    if (viewDepth > gCascadeSplits.x) i = 1;
    if (viewDepth > gCascadeSplits.y) i = 2;
    if (viewDepth > gCascadeSplits.z) i = 3;
    return min(i, max((uint)gShadowParams.y, 1u) - 1u);
}

float GetShadowPCF(float3 worldPos, uint ci) {
    float4 sh = mul(float4(worldPos, 1.0f), gShadowViewProj[ci]);
    sh.xyz /= max(sh.w, 1e-6f);

    float2 uv = float2(sh.x * 0.5f + 0.5f, -sh.y * 0.5f + 0.5f);
    float depth = sh.z;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f) {
        return 1.0f;
    }

    float tex = 1.0f / max(gShadowParams.x, 1.0f);
    float s = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 offset = float2((float)x, (float)y) * tex;
            s += gShadowMap.SampleCmpLevelZero(gShadowSampler, float3(uv + offset, ci), depth);
        }
    }

    return s / 9.0f;
}

float GetShadowFactor(float3 wp) {
    if ((uint)gShadowParams.z == 0 || (uint)gShadowParams.y == 0) {
        return 1.0f;
    }
    float4 vp = mul(float4(wp, 1.0f), gView);
    return GetShadowPCF(wp, GetCascadeIndex(abs(vp.z)));
}

float3 EvaluateLight(uint t, GpuLight Ld, float3 wp, float3 n, float3 v, float rough) {
    float3 r = Ld.ColorAndIntensity.rgb * Ld.ColorAndIntensity.a;
    float3 L = 0.0f;
    float att = 1.0f;

    if (t == LIGHT_TYPE_DIRECTIONAL) {
        L = normalize(-Ld.DirectionAndType.xyz);
        att *= GetShadowFactor(wp);
    } else {
        float3 to = Ld.PositionWorldAndRange.xyz - wp;
        float d = length(to);
        if (d <= 1e-4f) {
            return 0.0f;
        }

        L = to / d;
        float range = max(Ld.PositionWorldAndRange.w, 1e-3f);
        float rf = saturate(1.0f - d / range);
        att = rf * rf;

        if (t == LIGHT_TYPE_SPOT) {
            float3 sd = normalize(-Ld.DirectionAndType.xyz);
            float ct = dot(L, sd);
            float spot = saturate((ct - Ld.Params.y) / max(Ld.Params.x - Ld.Params.y, 1e-4f));
            att *= spot;
        }
    }

    float ndl = saturate(dot(n, L));
    float3 diff = r * ndl * att;
    float3 h = normalize(L + v);
    float sp = pow(saturate(dot(n, h)), lerp(64.0f, 4.0f, saturate(rough))) * att;
    return diff + r * sp * lerp(0.25f, 0.04f, saturate(rough));
}

float4 PS(PS_INPUT i) : SV_Target {
    float4 a = gAlbedo.Sample(gSampler, i.TexC);
    float4 ns = gNormal.Sample(gSampler, i.TexC);
    float d = gDepth.Sample(gSampler, i.TexC).r;
    float3 n = normalize(ns.xyz * 2.0f - 1.0f);
    float rough = saturate(ns.a);
    float3 wp = ReconstructWorldPos(i.TexC, d);
    float3 v = normalize(gCameraPosition.xyz - wp);
    float3 c = a.rgb * 0.05f;

    uint lc = min((uint)gLightCount.x, MAX_LIGHTS);
    [loop]
    for (uint k = 0; k < lc; ++k) {
        uint lightType = (uint)gLights[k].DirectionAndType.w;
        c += a.rgb * EvaluateLight(lightType, gLights[k], wp, n, v, rough);
    }

    return float4(saturate(c), a.a);
}