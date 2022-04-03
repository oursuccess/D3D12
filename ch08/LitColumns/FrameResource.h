#pragma once
//FrameResource用于描述每帧所需的资源数据

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

//每个物体在更新时需要的常量数据。该数据在物体变化时更新
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	//第8章新添加的内容。材质偏移
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

//每帧需要更新的常量数据，不因单个物体而改变
struct PassConstants
{
	//观察矩阵。将物体从世界空间转换到观察空间
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	//观察矩阵的逆矩阵。将物体从观察空间转换回世界空间
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	//投影矩阵。将物体从观察空间转换到裁剪空间(投影)
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	//投影矩阵的逆矩阵。将物体从裁剪空间转换回观察空间
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	//观察投影矩阵。将物体直接从世界空间转换到裁剪空间
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	//观察投影矩阵的逆矩阵。将物体直接从裁剪空间转换回世界空间
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	//观察者的位置(在这里大概率就是相机位置。如果是用于阴影映射纹理生成，则我们可能用的是光源位置)
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	//用于pad。满足最小的字节对齐，从而避免c++的自动对齐机制可能导致的CPU--GPU之间资源位置不对应(偏移)问题
	float cbPerObjectPad = 0.0f;
	//渲染目标缓冲区大小
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	//渲染缓冲区大小的倒数
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	//近裁剪平面到相机的距离
	float NearZ = 0.0f;
	//远剪裁平面到相机的距离
	float FarZ = 0.0f;
	//运行到现在的总时间
	float TotalTime = 0.0f;
	//上一帧到这一帧的时间
	float DeltaTime = 0.0f;

	//第8章新添加的内容
	//漫反射光照
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	//最大的光线数量。 MaxLights在d3dUtil中定义
	Light Lights[MaxLights];
};

//用于描述一个顶点所需的数据
struct Vertex
{
	//顶点位置(模型空间内的位置)
	DirectX::XMFLOAT3 Pos;
	//法线。这是为了能够导入骷髅头
	DirectX::XMFLOAT4 Normal;
};

//存储了一帧中CPU构建命令列表(CommandList)所需的资源
struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//每个帧资源都需要自己独立的命令分配器、帧常量数据和物体常量数据
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	//第8章新添加的内容。每个帧资源还都有一个自己的材质常量
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	//围栏。用于保证当我们使用该帧资源时，该资源一定已经被GPU使用完毕了。
	UINT64 Fence = 0;
};
