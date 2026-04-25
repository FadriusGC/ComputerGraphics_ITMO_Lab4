struct PSIn
{
    float4 PositionH : SV_POSITION;
    float2 Uv : TEXCOORD;
    float4 Color : COLOR0;
};

float4 PS(PSIn input) : SV_TARGET
{
    float2 centered = input.Uv * 2.0 - 1.0;
    float dist = length(centered);
    float sdf = 1.0 - smoothstep(0.85, 1.0, dist);
    clip(sdf - 0.02);
    return float4(input.Color.rgb, 1.0);
}
