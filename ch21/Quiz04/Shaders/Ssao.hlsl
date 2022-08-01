//Ssao

cbuffer cbSsao : register(b0)   //该常量缓冲区通过根描述符在Ssao中被传递绑定至b0
{
    float4x4 gProj; //相机投影矩阵
    float4x4 gInvProj;  //相机投影矩阵的逆矩阵
    float4x4 gProjTex;  //相机投影采样矩阵
    float4 gOffsetVectors[14];  //随机采样的周围向量
    float4 gBlurWeights[3]; //混合权重
    float2 gInvRenderTargetSize;    //渲染目标尺寸的倒数
    float gOcclusionRadius; //遮蔽半径
    float gOcclusionFadeStart;  //从何处开始遮蔽衰减
    float gOcclusionFadeEnd;    //到何距离时遮蔽衰减至0
    float gSurfaceEpsilon;  //距离小于该值, 我们认为处于同一平面，因此没有遮蔽
};

cbuffer cbRootConstants : register(b1)  //根常量们. 通过根常量在Ssao中被绑定至b1
{
    bool gHorizontalBlur;   //记录是否为水平混合
};

Texture2D gNormalMap : register(t0);    //绑定一些纹理. 直接指向的纹理无法通过常量缓冲区绑定. 法线贴图
Texture2D gDepthMap : register(t1); //深度贴图. 该贴图是在NormalMap的后面被一同绑定的. 可以参见SsaoApp::BuildSsaoRootSignature(), 其中TexTable0元素数量为2, 0为gNormalMap(t0), 1为gDepthMap(t1)
Texture2D gRandomVecMap : register(t2); //随机采样贴图

SamplerState gsamPointClamp : register(s0); //绑定几个静态采样器
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

static const int gSampleCount = 14; //这里的14和上面gOffsetVectors的数组长度严格一致

static const float2 gTexCoords[6] = //采样的纹理坐标. 我们假设传入顶点着色器的均为三角形列表，且顺时针旋转的三个顶点为正向. 6个顶点组成的两个三角形刚好组成了一个矩形，分别对应了左上部分的三角和右下部分的三角
{
    /*              u
     p1 o-----------------------> p2, p4
        |                        |
        |                        
        |                        |
       v|                        
        |                        |
        |                        
        |                        |
 p0, p3 v- - - - - - - - - - - -  p5
    */
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)    //我们的顶点着色器只要接受顶点ID即可. 剩下的，根据顶点ID在采样图中进行采样
{
    VertexOut vout;

    vout.TexC = gTexCoords[vid];    //根据当前的顶点，获取其uv采样坐标

    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f); //我们将uv坐标映射到[-1, 1]空间内, 让深度先为0,此即其对应的近切点的ndc坐标;同时需要注意在dx中，需要对y轴进行一次反转

    float4 ph = mul(vout.PosH, gInvProj);   //将ndc坐标转换到观察空间中中的近平面
    vout.PosV = ph.xyz / ph.w;  //我们需要注意要让坐标借助w进行归一化, 因为投影变换本身分为了线性和非线性两个部分, 这一步是为了去除非线性部分

    return vout;
}

float OcclusionFunction(float distZ)    //计量在距离为distZ的情况下,一个点p周围的点q对p的遮蔽程度
{
    float occlusion = 0.0f; //我们先让遮蔽率为0
    if (distZ > gSurfaceEpsilon)    //只有在距离大于我们认为处于同一平面的小误差时，我们才需要计算实际的遮蔽率
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength); //我们线性计算遮蔽率即可. 需要注意将其变换到[0, 1]范围内
    }
    return occlusion;
}

float NdcDepthToViewDepth(float z_ndc)  //根据ndc空间中的z值，计算观察空间中的深度值
{
    //z_ndc = A + B / viewZ, A为gProj[2][2], B为gProj[3][2]. 
    //则A = B / (z_ndc - A)
    //参见5.6.3.4, P162
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
    //在该方法中，我们以p为我们要计算遮蔽率的点, 其为当前观察空间中可看到的点，因此自然是距离相机最近的(pz即为DepthMap中对应点的最小值, n即为对应NormalMap中记录的对应点的发现)
    float3 n = normalize(gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);   //我们获取p点的法线.而最近点的法线我们在之前的计算中已经得出并存储在gNormalMap中(参见SsaoApp::DrawNormalsAndDepth). 法线是不能插值的!
    float pz = gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;   //求得在深度图中求得的对应点的深度值. 需要注意，由于在DrawNormalsAndDepth方法中我们在PS中保存的该值，因此需要将其从NDC空间变换回观察空间(法线本来就没插值)
    pz = NdcDepthToViewDepth(pz);

    float3 p = (pz / pin.PosV.z) * pin.PosV;    //根据相似三角形原理，以近平面上从观察点到近平面对应点的向量，求出p对应的实际向量

    float3 randVec = 2.0f * gRandomVecMap.SampleLevel(gsamLinearWrap, 4.0f * pin.TexC, 0.0f).rgb - 1.0f;    //我们对RandomVecMap进行采样，求得该贴图中存储的向量，并将其从[0,1]映射到[-1, +1]

    float occlusionSum = 0.0f;

    for (int i = 0; i < gSampleCount; ++i)  //进行采样
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);    //我们通过reflect方法，以及我们手动配置的14个偏移向量，得到随机分布的偏移向量们

        float flip = sign(dot(offset, n));  //我们获取offset和n的叉乘结果，从而检查偏移向量是否在(p, n)定义的平面后方,如果其在平面后方，则我们需要逆转方向，对前方进行采样

        float3 q = p + flip * gOcclusionRadius * offset;    //获取观察空间中的q, 因为p是观察空间中的了

        float4 projQ = mul(float4(q, 1.0f), gProjTex);  //对q进行投影，求出q对应的ndc空间坐标
        projQ /= projQ.w;

        float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r; //找到在从观察点到q点的方向上的最小深度值, 我们只需要对ndc空间坐标进行采样得到ndc空间中的深度，然后再将其转换到观察空间即可
        rz = NdcDepthToViewDepth(rz);

        float3 r = (rz / q.z) * q;  //和p一样，我们求出q方向上最接近观察点的点r

        float distZ = p.z - r.z;    //求出p和r的深度差

        //Quiz2104. 要去掉自相交遮蔽, 我们只需要去掉将n裁剪为最小值为0的步骤即可
        //float dp = max(dot(n, normalize(r - p)), 0.0f); //dp计量了r和p的法线方向的重合程度，重合程度越高，自然遮蔽率也就越高. 我们需要将其限制到[0, 1]范围内
        //float occlusion = dp * OcclusionFunction(distZ);    //求出在该深度差下的遮蔽率
        float occlusion = OcclusionFunction(distZ);    //求出在该深度差下的遮蔽率

        occlusionSum += occlusion;  //累加遮蔽率
    }

    occlusionSum /= gSampleCount;   //求出平均遮蔽率(将其映射到[0, 1]范围之间)

    float access = 1.0f - occlusionSum; //可达率与遮蔽率的和为1

    return saturate(pow(access, 6.0f)); //通过幂次，我们可以让SSAO采样结果对比度更高，从而让其更有戏剧性
}