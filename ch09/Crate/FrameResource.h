#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

//物体需要的常量
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

//每个Pass需要的常量
struct PassConstants
{
	//观察矩阵和观察矩阵的逆矩阵。观察矩阵将物体从世界空间转换到观察空间
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	//投影矩阵和投影矩阵的逆矩阵。投影矩阵将物体从观察空间转换到投影空间
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	//观察投影矩阵和其逆矩阵。观察投影矩阵将物体从世界空间直接转换到投影空间
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();

	//观察点的位置。相机所在的位置
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	//对齐用的填充
	float cbPerObjectPad1 = 0.0f;
	//缓冲区大小和其倒数
	DirectX::XMFLOAT2 RenderTargetSize{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize{ 0.0f, 0.0f };
	//近裁剪距离和远裁剪距离
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	//总时间和当前帧的时间
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	//环境光的颜色
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	//光照们
	Light Lights[MaxLights];
};

//顶点属性
struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	//贴图位置
	DirectX::XMFLOAT2 TexC;
};

struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//每个帧资源都需要一个命令分配器和相应的缓冲区
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	//围栏值
	UINT64 Fence = 0;
};

