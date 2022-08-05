#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

struct ObjectConstants	//每个物体的常量. 一个物体拥有从局部空间变换到世界空间中的变换矩阵, 以及对每个顶点进行纹理采样的纹理采样矩阵. 同时, 我们假定了每个物体只有一个材质, 其通过材质索引进行索引
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT MaterialIndex;
	UINT ObjPad0;
	UINT ObjPad1;
	UINT ObjPad2;
};

struct SkinnedConstants	//蒙皮材质的常量. 在蒙皮的材质中, 其有最多96个骨骼.
{
	DirectX::XMFLOAT4X4 BoneTransforms[96];
};

struct PassConstants	//Pass常量. 每个Pass对应了一个相机. 在该常量中, 包含了观察矩阵(从世界空间变换到观察空间)、投影矩阵、上两个矩阵的联立与其逆矩阵, 从世界空间中到阴影图采样的变换矩阵, 观察点位置, 渲染目标的尺寸与其倒数, 相机的NearZ和Farz, 以及环境自发光、光源们、时间
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	Light Lights[MaxLights];
};

struct SsaoConstants	//Ssao常量. Ssao中记录了投影矩阵及其逆矩阵，一个方便运算的投影采样矩阵. 我们需要使用投影矩阵来将采样得到的深度变换到观察空间并计算出观察空间中最近的点. 此外, 我们还需要扰动向量, 混合权重们, 以及渲染对象大小的倒数, 同时, 我们也要记录遮蔽的最小最大距离、判断是否为一个表面的偏差、遮蔽率半径
{
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ProjTex;
	DirectX::XMFLOAT4 OffsetVector[14];

	DirectX::XMFLOAT4 BlurWeights[3];
	
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05;
};

struct MaterialData	//材质常量. 材质包含了漫反射,用于计算高光反射的R0,粗糙度, 材质到纹理的变换矩阵, 纹理图和法线图的偏移
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01, 0.01f, 0.01f };
	float Roughness = 0.5f;	//要看清我们怎么组织数据的. 3+1 == 4

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT NormalMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
};

struct Vertex	//顶点. 一个顶点包含了位置，法线，纹理坐标，和切线
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
};

struct SkinnedVertex	//蒙皮的顶点. 蒙皮的顶点在正常顶点的基础上还多了骨骼和骨骼的权重
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
	DirectX::XMFLOAT3 BoneWeights;	//这里只需要记录3个, 因为4个骨骼的权重之和为1
	BYTE BoneIndices[4];	//骨骼下标. 由于骨骼不多(至多96)，因此我们可以以更小的数值范围进行索引
};

class FrameResource	//帧资源. 真正的, 需要作为每帧的资源被提交到GPU的资源的列表
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedConstants>> SkinnedCB = nullptr;
	std::unique_ptr<UploadBuffer<SsaoConstants>> SsaoCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

	UINT64 Fence = 0;
};

