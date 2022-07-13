//Sky

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION; //与Default中，少了一个Tangent. 因为我们在天空盒中不需要
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;  //系统值，在PS中必须传入该值. 用于记录投影变换后的位置
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosL = vin.PosL;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    posW.xyz += gEyePosW;   //让天空盒永远以观察点为中心

    vout.PosH = mul(posW, gViewProj).xyww;  //不需要z值，或者让其深度值永远为1(最远的)

    return vout;
}

float4 PS(VertexOut pin) : SV_Target    //返回的即为对应像素
{
    return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}