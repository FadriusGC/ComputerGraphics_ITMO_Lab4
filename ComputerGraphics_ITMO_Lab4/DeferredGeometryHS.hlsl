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

cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gCameraPosition;
    float4 gTessellationParams; // x=minDist, y=maxDist, z=maxTess, w=minTess
};

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
    InputPatch<VS_OUTPUT, 3> patch,
    uint patchID : SV_PrimitiveID) {
    HS_CONSTANT_DATA_OUTPUT output;

    float3 p0 = mul(float4(patch[0].Pos, 1.0f), gWorld).xyz;
    float3 p1 = mul(float4(patch[1].Pos, 1.0f), gWorld).xyz;
    float3 p2 = mul(float4(patch[2].Pos, 1.0f), gWorld).xyz;
    float3 center = (p0 + p1 + p2) / 3.0f;

    float distanceToCamera = distance(center, gCameraPosition.xyz);
    float minDist = gTessellationParams.x;
    float maxDist = gTessellationParams.y;
    float maxTess = gTessellationParams.z;
    float minTess = gTessellationParams.w;

    float factor = saturate((maxDist - distanceToCamera) / max(maxDist - minDist, 1e-4f));
    float tessLevel = lerp(minTess, maxTess, factor);

    output.EdgeTess[0] = tessLevel;
    output.EdgeTess[1] = tessLevel;
    output.EdgeTess[2] = tessLevel;
    output.InsideTess = tessLevel;

    return output;
}

[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
VS_OUTPUT HS(InputPatch<VS_OUTPUT, 3> patch,
             uint i : SV_OutputControlPointID,
             uint patchID : SV_PrimitiveID) {
    return patch[i];
}
