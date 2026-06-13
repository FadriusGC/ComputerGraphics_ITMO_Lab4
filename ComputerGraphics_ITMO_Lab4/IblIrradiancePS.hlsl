// Self-contained (no #include). Cosine convolution (lecture slides 48-50).
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

TextureCube gEnvMap : register(t0);
SamplerState gSampler : register(s0);

float4 PS(VSOut input) : SV_Target {
    float3 N = normalize(FaceDir(gFaceIndex, input.TexC));

    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f)
                                  : float3(1.0f, 0.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    float nrSamples = 0.0f;
    const float sampleDelta = 0.025f;
    // [loop] forbids unrolling: this loop has ~16k iterations and FXC would
    // otherwise try to fully unroll it and fail (error X3511).
    [loop] for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta) {
        [loop] for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta) {
            float3 tangentSample = float3(sin(theta) * cos(phi),
                                          sin(theta) * sin(phi),
                                          cos(theta));
            float3 sampleVec = tangentSample.x * right +
                               tangentSample.y * up +
                               tangentSample.z * N;
            irradiance += gEnvMap.SampleLevel(gSampler, sampleVec, 0).rgb *
                          cos(theta) * sin(theta);
            nrSamples += 1.0f;
        }
    }
    irradiance = PI * irradiance / max(nrSamples, 1.0f);
    return float4(irradiance, 1.0f);
}
