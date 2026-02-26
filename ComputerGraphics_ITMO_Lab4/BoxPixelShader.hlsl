struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
};

cbuffer cbLight : register(b1) {
    float4 gLightPosition;
    float4 gLightColor;
    float4 gCameraPosition;
};

cbuffer cbMaterial : register(b2) {
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
};

// Текстура и сэмплер
Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

float4 PS(PS_INPUT input) : SV_Target {
    // Цвет из текстуры
    float4 texColor = gDiffuseMap.Sample(gSampler, input.TexC);
    
    // Модулируем диффузный цвет материала текстурой
    float3 albedo = gDiffuseAlbedo.rgb * texColor.rgb;
    float alpha = gDiffuseAlbedo.a * texColor.a;

    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(gLightPosition.xyz - input.WorldPos);
    
    float diffuse = max(dot(normal, lightDir), 0.0f);
    float3 diffuseColor = diffuse * gLightColor.rgb * albedo;
    
    float3 ambient = 0.1f * gLightColor.rgb * albedo;
    
    float3 viewDir = normalize(gCameraPosition.xyz - input.WorldPos);
    float3 halfVec = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfVec), 0.0f), 32.0f);
    float3 specularColor = specular * gLightColor.rgb * gFresnelR0;
    
    float3 finalColor = ambient + diffuseColor + specularColor;
    return float4(finalColor, alpha);
}