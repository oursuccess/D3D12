#pragma once

#include "../../QuizCommonHeader.h"

struct ObjectConstants	//每个对象拥有的常量
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();	//其世界变换矩阵. 用于将物体从局部空间变换到世界空间
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();	//其所有顶点对应的纹理采样变换矩阵.
	UINT MaterialIndex;	//其材质在对应缓冲区中的偏移量
	UINT ObjPad0;	//用于保证对齐
	UINT ObjPad1;
	UINT ObjPad2;
};

struct PassConstants	//帧常量. Pass存储了记录到一个命令列表(提交前)的(从Begin到End的)命令
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();	//观察矩阵。 用于从世界空间转换到观察空间。 一个Pass只有一个相机
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();	//观察矩阵的逆矩阵. 从观察空间变换回世界空间中
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();	//投影矩阵. 用于从观察空间变换到投影空间. 但是需要注意的是投影变换分为两部分，一部分为线性的矩阵变换，另一部分为非线性的除法(除以w)
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();	//投影矩阵的逆矩阵. 同样分为线性和非线性两部分
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();	//观察投影矩阵. 由ViewXProj得到. 这里左View是因为我们使用的是坐标/向量在左边写为行向量的方式.
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();	//观察投影矩阵的逆矩阵
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };	//观察点的位置
	float cbPerObjectPad1 = 0.0f;	//为了进行对齐而添加的填充
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };	//渲染对象大小. 以像素计
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };	//渲染对象大小的逆大小
	float NearZ = 0.0f;	//相机的近切距离. 低于此距离的不会被观察到
	float FarZ = 0.0f;	//相机的远切距离. 高于此距离的不会被观察到
	float TotalTime = 0.0f;	//App运行至今的总时长
	float DeltaTime = 0.0f;	//当前帧的时长

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };	//环境光. 环境光当然是相同的

	Light Lights[MaxLights];	//光源们. 光源们按照平行光-点光源-聚光灯的顺序排列
};

struct MaterialData	//材质的数据
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };	//漫反射颜色
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };	//R0值. R0用在菲涅尔效应的计算中. R(θ) = R0 + (1 - R0) * (1 - cosθ)^5. 其中cosθ即为h dot L(在h和L都为单位向量的情况下). 其中h为normalize(v + L). 因为只有法线刚好正好处在v和L中间的平面才有可能将入射的光线刚好反射到观察点. 根据该公式, 我们可以发现，反射率随着观察向量和法线之间夹角的增加而快速增加
	float Roughness = 64.0f;	//粗糙度. 粗糙度用于模拟表面的粗糙程度. 粗糙度越大，则镜面反射越发散，但反射区域的平均光强越低(能量守恒). 我们以(m + 8) / 8 * (cosθ) ^ m 来模拟微表面的发散情况. 其中(m+8)/8用于能量守恒, 而m为(1 - Roughness) * 256.0f, 而cosθ可以通过n dot h来计算获得

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();	//材质的偏移量. 这里记录的是顶点的uv坐标先经过对象uv采样矩阵的变换，再经过材质的uv采样矩阵的变换，得到最终的纹理采样值. 需要注意的是纹理坐标变换需要在顶点着色器中完成, 从而在光栅化阶段中正确插值出每个像素点对应的纹理坐标

	UINT DiffuseMapIndex = 0;	//材质对应的漫反射贴图在缓冲区中的偏移量
	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

struct Vertex	//顶点的数据
{
	DirectX::XMFLOAT3 Pos;	//顶点位置
	DirectX::XMFLOAT3 Normal;	//顶点法线
	DirectX::XMFLOAT2 TexC;	//顶点对应的uv采样坐标
};

struct FrameResource	//帧资源. 帧资源中包含了帧常量、帧所需要的对象对应的常量、帧所需要的材质的常量，并将这些常量都上传到GPU中以使其可用
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);	//要创建帧资源，我们需要首先明确帧数量、物体数量和材质数量，才能为其分配出合理的缓存空间
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;	//命令分配器. 每个帧都需要自己的命令分配器来将自己的命令暂时缓存，直到GPU使用完毕后才能将命令列表中对应的命令重置

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;	//每种常量对应的上传缓冲区. 每个缓冲区中记录了自己的元素数量
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

	UINT64 Fence = 0;	//围栏值，用于确认GPU是否已经使用完毕,并解除了对该帧资源的依赖
};

