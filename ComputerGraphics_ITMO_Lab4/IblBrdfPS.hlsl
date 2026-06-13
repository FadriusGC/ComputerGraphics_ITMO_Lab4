// Self-contained (no #include). Split-sum BRDF integration (lecture slide 77).
static const float PI = 3.14159265359f;

cbuffer BakeParams : register(b0) {
    uint  gFaceIndex;   // cube face being rendered (0..5)
    float gRoughness;   // prefilter roughness for this mip
    float gPad0;
    float gPad1;
};

struct VSOut {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Cube face texel -> world direction (Direct3D cube convention).
// Face order: 0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z. uv in [0,1], v = 0 at top.
float3 FaceDir(uint face, float2 uv) {
    float a = 2.0f * uv.x - 1.0f;
    float b = 2.0f * uv.y - 1.0f;
    float3 d;
    if (face == 0)      d = float3( 1.0f,    -b,    -a);
    else if (face == 1) d = float3(-1.0f,    -b,     a);
    else if (face == 2) d = float3(   a,  1.0f,     b);
    else if (face == 3) d = float3(   a, -1.0f,    -b);
    else if (face == 4) d = float3(   a,    -b,  1.0f);
    else                d = float3(  -a,    -b, -1.0f);
    return normalize(d);
}

// Analytic HDR sky used as the source environment (no external asset needed).
float3 SampleEnvironment(float3 dir) {
    float y = clamp(dir.y, -1.0f, 1.0f);
    float3 zenith  = float3(0.18f, 0.32f, 0.55f) * 1.7f;
    float3 horizon = float3(0.60f, 0.58f, 0.60f) * 1.5f;
    float3 ground  = float3(0.13f, 0.11f, 0.10f);
    float t = saturate(y);
    float3 sky = lerp(horizon, zenith, t);
    float gt = saturate(-y * 2.0f);
    float3 grnd = lerp(horizon * 0.55f, ground, gt);
    float3 env = (y >= 0.0f) ? sky : grnd;
    float3 sunDir = normalize(float3(0.45f, 0.72f, 0.30f));
    float sd = clamp(dot(dir, sunDir), -1.0f, 1.0f);
    float disk = saturate((sd - 0.9975f) / (1.0f - 0.9975f));
    float3 sun = disk * float3(14.0f, 12.5f, 10.0f);
    float3 glow = pow(saturate(sd), 80.0f) * float3(1.6f, 1.3f, 1.0f);
    return env + sun + glow;
}

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint N) {
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// Geometry term with the IBL k = roughness^2 / 2 (lecture slide 78).
float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
    float k = (roughness * roughness) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}
float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX_IBL(NdotV, roughness) *
           GeometrySchlickGGX_IBL(NdotL, roughness);
}

float2 IntegrateBRDF(float NdotV, float roughness) {
    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0.0f;
    V.z = NdotV;

    float A = 0.0f;
    float B = 0.0f;
    float3 N = float3(0.0f, 0.0f, 1.0f);
    const uint SAMPLE_COUNT = 1024u;
    [loop] for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        if (NdotL > 0.0f) {
            float G = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-6f);
            float Fc = pow(1.0f - VdotH, 5.0f);
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}

float4 PS(VSOut input) : SV_Target {
    float NdotV = max(input.TexC.x, 1e-3f);
    float roughness = input.TexC.y;
    float2 result = IntegrateBRDF(NdotV, roughness);
    return float4(result, 0.0f, 0.0f);
}
