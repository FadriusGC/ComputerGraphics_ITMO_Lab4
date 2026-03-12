struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
SamplerState gSampler : register(s0);

float4 PS(PS_INPUT input) : SV_Target {
    float4 albedo = gAlbedo.Sample(gSampler, input.TexC);
    return albedo;
}
