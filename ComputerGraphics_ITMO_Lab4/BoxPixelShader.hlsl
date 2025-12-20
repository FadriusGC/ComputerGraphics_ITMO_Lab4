struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

cbuffer cbLight : register(b1) {
    float4 gLightPosition;
    float4 gLightColor;
    float4 gCameraPosition;
};

float4 PS(PS_INPUT input) : SV_Target {
    // Нормализуем нормаль
    float3 normal = normalize(input.Normal);
    
    // Направление света
    float3 lightDir = normalize(gLightPosition.xyz - input.WorldPos);
    
    // Диффузное освещение
    float diffuse = max(dot(normal, lightDir), 0.0f);
    float3 diffuseColor = diffuse * gLightColor.rgb * input.Color.rgb;
    
    // Амбиентное освещение
    float3 ambient = 0.1f * gLightColor.rgb * input.Color.rgb;
    
    // Специфическое освещение (Blinn-Phong)
    float3 viewDir = normalize(gCameraPosition.xyz - input.WorldPos);
    float3 halfDir = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfDir), 0.0f), 32.0f);
    float3 specularColor = specular * gLightColor.rgb * 0.5f;
    
    // Итоговый цвет
    float3 finalColor = ambient + diffuseColor + specularColor;
    return float4(finalColor, 1.0f);
}