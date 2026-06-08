struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexC : TEXCOORD;
};

// G-buffer layout (deferred PBR, metallic-roughness workflow):
//   RT0 (R8G8B8A8_UNORM):  rgb = base color (linear), a = metallic
//   RT1 (R16G16B16A16F):   xyz = world normal (encoded *0.5+0.5), a = roughness
struct PS_OUTPUT {
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float gHasNormalMap;
    float gHasDisplacementMap;
    float gHasRoughnessMap;
    float gDisplacementScale;
    float gMetallic;
    float gHasMetallicMap;
    float2 gMatPadding;
    float4x4 gTexTransform;
};

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDisplacementMap : register(t2);
Texture2D gRoughnessMap : register(t3);
Texture2D gMetallicMap : register(t4);
SamplerState gSampler : register(s0);

PS_OUTPUT PS(PS_INPUT input) {
    PS_OUTPUT output;

    float2 transformedTexC = mul(float4(input.TexC, 0.0f, 1.0f), gTexTransform).xy;
    float4 texColor = gDiffuseMap.Sample(gSampler, transformedTexC);
    float alpha = gDiffuseAlbedo.a * texColor.a;
    clip(alpha - 0.1f);

    // Base color textures are authored in sRGB; bring them to linear for PBR.
    float3 baseColorLinear = pow(saturate(texColor.rgb), 2.2f);
    float3 albedo = gDiffuseAlbedo.rgb * baseColorLinear;

    float3 worldNormal = normalize(input.Normal);
    if (gHasNormalMap > 0.5f) {
        float3 tangent = normalize(input.Tangent);
        float3 bitangent = normalize(input.Bitangent);
        float3x3 tbn = float3x3(tangent, bitangent, worldNormal);
        float3 mapNormal = gNormalMap.Sample(gSampler, transformedTexC).xyz * 2.0f - 1.0f;
        worldNormal = normalize(mul(mapNormal, tbn));
    }

    float roughness = saturate(gRoughness);
    if (gHasRoughnessMap > 0.5f) {
        roughness = saturate(gRoughnessMap.Sample(gSampler, transformedTexC).r);
    }

    float metallic = saturate(gMetallic);
    if (gHasMetallicMap > 0.5f) {
        metallic = saturate(gMetallicMap.Sample(gSampler, transformedTexC).r);
    }

    output.Albedo = float4(albedo, metallic);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, roughness);
    return output;
}
