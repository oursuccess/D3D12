//copy of BezierTessellation by Frank Luna, ch14

#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);

SamplerState gSamPoint : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;

    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosL = vin.PosL;

    return vout;
}

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 16> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;

    pt.EdgeTess[0] = 25;
    pt.EdgeTess[1] = 25;
    pt.EdgeTess[2] = 25;
    pt.EdgeTess[3] = 25;

    pt.InsideTess[0] = 25;
    pt.InsideTess[1] = 25;
    return pt;
}

struct HullOut
{
    float3 PosL : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 16> p, uint i : SV_OutputControlPointID,
    uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    return hout;
}

struct DomainOut
{
    float4 PosH : SV_Position;
    float3 PosW : Position;
    float3 NormalW : NORMAL;
};

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;
    return float4(invT * invT * invT, 3.0f * t * invT * invT,
        3.0f * t * t * invT, t * t * t);
}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum = basisV.x * (basisU.x * bezpatch[0].PosL + basisU.y * bezpatch[1].PosL + basisU.z * bezpatch[2].PosL + basisU.w * bezpatch[3].PosL);
    sum += basisV.y * (basisU.x*bezpatch[4].PosL  + basisU.y*bezpatch[5].PosL  + basisU.z*bezpatch[6].PosL  + basisU.w*bezpatch[7].PosL );
    sum += basisV.z * (basisU.x*bezpatch[8].PosL  + basisU.y*bezpatch[9].PosL  + basisU.z*bezpatch[10].PosL + basisU.w*bezpatch[11].PosL);
    sum += basisV.w * (basisU.x*bezpatch[12].PosL + basisU.y*bezpatch[13].PosL + basisU.z*bezpatch[14].PosL + basisU.w*bezpatch[15].PosL);
    return sum;
}

float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;
    return float4(-3 * invT * invT, 3 * invT * invT - 6 * t * invT,
        5 * t * invT - 3 * t * t, 3 * t * t);
}

[domain("quad")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation,
    const OutputPatch<HullOut, 16> bezPatch)
{
    DomainOut dout;

    float4 basisU = BernsteinBasis(uv.x);
    float4 basisV = BernsteinBasis(uv.y);

    float3 p = CubicBezierSum(bezPatch, basisU, basisV);

    float4 posW = mul(float4(p, 1.0f), gWorld);
    dout.PosH = mul(posW, gViewProj);

    //添加PosW和NormalW
    dout.PosW = posW.xyz;
    float4 dBasisU = dBernsteinBasis(uv.x);
    float4 dBasisV = dBernsteinBasis(uv.y);
    float3 dpdu = CubicBezierSum(bezPatch, dBasisU, basisV);
    float3 dpdv = CubicBezierSum(bezPatch, basisU, dBasisV);
    dout.NormalW = cross(dpdu, dpdv);

    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    //实现光照效果
    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye;

    float4 ambient = gAmbientLight * gDiffuseAlbedo;
    const float shininess = 1.0f - gRoughness;
    Material mat = { gDiffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;
    litColor.a = gDiffuseAlbedo.a;

    //return float4(1.0f, 1.0f, 1.0f, 1.0f);
    return litColor;
}
