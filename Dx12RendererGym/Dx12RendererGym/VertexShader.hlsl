struct VS_INPUT
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normalLocal : NORMAL;
};

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


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = mul(float4(input.pos, 1.0f), wvpMat);
    output.worldPos = mul(float4(input.pos, 1.0f), wMat).xyz;
    output.normalWorld = mul(input.normalLocal, (float3x3)wMat);
    output.texCoord = input.texCoord;
    return output;
}