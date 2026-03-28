struct VS_OUTPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
};

struct HS_CONSTANT_DATA_OUTPUT {
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

struct DS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
};

cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessellationParams;
    float4 gWaveParams;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float gHasNormalMap;
    float gHasDisplacementMap;
    float gHasRoughnessMap;
    float gDisplacementScale;
    float4x4 gTexTransform;
};

Texture2D gDisplacementMap : register(t2);
SamplerState gSampler : register(s0);

[domain("tri")]
DS_OUTPUT DS(HS_CONSTANT_DATA_OUTPUT input,
             float3 bary : SV_DomainLocation,
             const OutputPatch<VS_OUTPUT, 3> patch) {
    DS_OUTPUT output;

    float3 localPos = patch[0].Pos * bary.x + patch[1].Pos * bary.y + patch[2].Pos * bary.z;
    float3 localNormal = normalize(patch[0].Normal * bary.x + patch[1].Normal * bary.y + patch[2].Normal * bary.z);
    float3 localTangent = normalize(patch[0].Tangent * bary.x + patch[1].Tangent * bary.y + patch[2].Tangent * bary.z);
    float3 localBitangent = normalize(patch[0].Bitangent * bary.x + patch[1].Bitangent * bary.y + patch[2].Bitangent * bary.z);
    float2 texC = patch[0].TexC * bary.x + patch[1].TexC * bary.y + patch[2].TexC * bary.z;

     if (gWaveParams.x > 0.0f) { //немного поясню формулу для допа
        float wavePhase = gWaveParams.w * gWaveParams.z;//фаза = время на скорость 
        float waveSpatial = (localPos.x + localPos.z) * gWaveParams.y; //это пространственная координата. Волна распространяется в плоскости XZ
        float waveOffset = sin(waveSpatial + wavePhase) * gWaveParams.x; //итоговое смещение вдоль нормали, умноженное на амплитуду.
        localPos += localNormal * waveOffset;
    }

    float4 worldPos = mul(float4(localPos, 1.0f), gWorld);
    output.Pos = mul(worldPos, gWorldViewProj);
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(mul(float4(localNormal, 0.0f), gWorld).xyz);
    output.Tangent = normalize(mul(float4(localTangent, 0.0f), gWorld).xyz);
    output.Bitangent = normalize(mul(float4(localBitangent, 0.0f), gWorld).xyz);
    output.TexC = texC;

    return output;
}
