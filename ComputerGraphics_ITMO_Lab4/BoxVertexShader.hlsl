struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float gTime;
    float gScaleFactor;
    float2 gPadding;
};

cbuffer cbLight : register(b1) {
    float4 gLightPosition;
    float4 gLightColor;
    float4 gCameraPosition;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    
    float3 targetDirection = normalize(float3(1.0f, 1.0f, 1.0f));
    float3 currentDirection = normalize(input.Pos);
    
    float sravDot = dot(currentDirection, targetDirection);
    
    float animationForce = sin(gTime * 2.0f) * 0.5f;
    
    float3 animatedPosition = input.Pos + currentDirection * animationForce;
    
    float3 finalPosition = lerp(input.Pos, animatedPosition, sravDot);

    float4 worldPos = mul(float4(finalPosition, 1.0f), gWorld);
    output.Pos = mul(worldPos, gWorldViewProj);
    output.WorldPos = worldPos.xyz;
    
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), gWorld).xyz);
    output.Color = input.Color;
    
    return output;
}