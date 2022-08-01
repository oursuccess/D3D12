#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

//渲染每个物体时统一的常量
struct ObjectConstants
{
	//世界变换矩阵
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	//纹理采样矩阵
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	//材质的序号
	UINT MaterialIndex;
	//用于对齐
	UINT ObjPad0;
	UINT ObjPad1;
	UINT ObjPad2;
};

//每帧共用的常量
struct PassConstants
{
	//观察矩阵。 因为相机是同一个，因此不可能变
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	//观察矩阵的逆矩阵
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	//投影矩阵。 根据相机的视锥体而分为正交投影和透视投影两种。 在应用投影变换后，我们可以把物体坐标的x,y,z都变换到[-w, w]范围内，从而允许我们将其转化为NDC坐标，并进行视锥体剔除
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	//投影矩阵的逆矩阵
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	//观察矩阵和投影矩阵联立。 叉乘该矩阵后，将可以将世界坐标转换到投影坐标中
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	//观察投影矩阵的逆矩阵
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	//观察投影矩阵和纹理采样矩阵联立。这里采样的是阴影深度图 叉乘该矩阵后，可以直接得到该坐标对应的可视深度
	DirectX::XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
	//阴影图的采样矩阵。 使用该矩阵在标准设备坐标系中对阴影图进行采样
	DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	//相机(观察点)位置
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	//对齐
	float cbPerObjectPad1 = 0.0f;
	//渲染目标大小
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	//渲染目标大小的倒数
	DirectX::XMFLOAT2 InvRenderTargetSize{ 0.0f, 0.0f };
	//相机的NearZ
	float NearZ = 0.0f;
	//相机的FarZ
	float FarZ = 0.0f;
	//总时间和当前帧时间
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	//环境光强
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	//光源们
	Light Lights[MaxLights];
};

//用于实现环境光遮蔽的常量
struct SsaoConstants
{
	//光源的投影矩阵
	DirectX::XMFLOAT4X4 Proj;
	//光源投影矩阵的逆矩阵
	DirectX::XMFLOAT4X4 InvProj;
	//光源的投影矩阵和纹理采样矩阵的联立。用于直接从光源坐标系下对阴影图进行采样
	DirectX::XMFLOAT4X4 ProjTex;
	//相对于当前采样点的随机发散点r
	DirectX::XMFLOAT4 OffsetVectors[14];

	//用于模糊的权重
	DirectX::XMFLOAT4 BlurWeights[3];

	//渲染目标尺寸的倒数
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	//用于在观察空间中进行遮挡检测的相关参数, 分别为半径，近距离，远距离，以及判断是否在同一平面的距离
	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;
};

struct MaterialData
{
	//默认的漫反射系数
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	//用于计算高光反射的R0
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	//粗糙度
	float Roughness = 0.5f;

	//材质变换矩阵，用于材质采样
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	//记录漫反射贴图和法线纹理图在材质缓冲区中的下标
	UINT DiffuseMapIndex = 0;
	UINT NormalMapIndex = 0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

struct Vertex
{
	//顶点位置
	DirectX::XMFLOAT3 Pos;
	//顶点法线
	DirectX::XMFLOAT3 Normal;
	//顶点材质坐标
	DirectX::XMFLOAT2 TexC;
	//顶点切线
	DirectX::XMFLOAT3 TangentU;
};

class FrameResource
{
public:
	//构建帧资源. 需要的参数有当前设备， 当前帧的渲染过程数量、物体数量、材质数量
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//命令分配器
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	//分别用于存储当前渲染过程、当前渲染物体、当前的环境光遮蔽所需要的常量的上传堆
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<SsaoConstants>> SsaoCB = nullptr;

	//用于当前材质所需数据的上传堆
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

	//栅，用于表示该帧资源是否已经由GPU释放
	UINT Fence = 0;
};

