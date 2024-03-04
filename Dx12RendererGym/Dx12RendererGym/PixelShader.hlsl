Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 worldPos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normalWorld : NORMAL;
};



cbuffer ConstantBuffer : register(b0)
{
    float4x4 wMat;
    float4x4 wvpMat;
    float3 cameraPos;
};

struct PointLightData
{
    float3 diffuseColor;
    float3 specularColor;
    float3 position;
    float specularPower;
    float innerRadius;
    float outerRadius;
    bool enabled;
};

cbuffer LIGHTING : register(b1)
{
    float3 ambientLight;
    PointLightData pointLights[3];
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    //return float4(1.f, 1.f, 1.f, 1.f);
    float4 color = t1.Sample(s1, input.texCoord);
    
    float3 phong = ambientLight;
    
    float3 N = normalize(input.normalWorld);
    float3 V = cameraPos - input.worldPos;
    V = normalize(V);
    
    for (int i = 0; i < 3; i++)
    {
        if (!pointLights[i].enabled)
        {
            continue;
        }

        float3 L = pointLights[i].position - input.worldPos;
        L = normalize(L);
        float NdotL = dot(N, L);
        
        float3 R = reflect(-L, N);
        
        if (NdotL > 0)
        {
            float dist = distance(input.worldPos, pointLights[i].position);
            float sstep = smoothstep(pointLights[i].innerRadius, pointLights[i].outerRadius, dist);
            
            float3 diffuseColor = lerp(pointLights[i].diffuseColor, float3(0, 0, 0), sstep);
            phong += (diffuseColor * NdotL);

            
            
            float RdotV = dot(R, V);
            phong += pointLights[i].specularColor * pow(max(0, RdotV), pointLights[i].specularPower);

        }

    }
    float4 finalPhong = float4(phong, 0);
    
    return color * saturate(finalPhong);
    
}