struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
};

struct PS_OUTPUT {
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float gHasNormalMap;
    float3 gPadding;
    float4x4 gTexTransform;
};

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
SamplerState gSampler : register(s0);

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    float2 transformedTexC = mul(float4(input.TexC, 0.0f, 1.0f), gTexTransform).xy;
    float4 texColor = gDiffuseMap.Sample(gSampler, transformedTexC);

    output.Albedo = float4(gDiffuseAlbedo.rgb * texColor.rgb, gDiffuseAlbedo.a * texColor.a);

    float3 worldNormal = normalize(input.Normal);
    if (gHasNormalMap > 0.5f) {
        float3 tangent = normalize(input.Tangent);
        float3 bitangent = normalize(input.Bitangent);
        float3x3 tbn = float3x3(tangent, bitangent, worldNormal);

        float3 mapNormal = gNormalMap.Sample(gSampler, transformedTexC).xyz * 2.0f - 1.0f;
        worldNormal = normalize(mul(mapNormal, tbn));
    }

    output.Normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    return output;
}
