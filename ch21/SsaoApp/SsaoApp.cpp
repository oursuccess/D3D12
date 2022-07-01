//SsaoApp.cpp forked from Frank Luna.

#include "../../QuizCommonHeader.h"
#include "FrameResource.h"
#include "ShadowMap.h"
#include "Ssao.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;	//帧资源数量。  我们让CPU比GPU超前3个帧资源，从而实现更好的并行，让双方不再需要单流水线等待

//渲染对象。 每个材质都有自己的渲染对象。 在渲染项中，记录了其对应的世界矩阵、材质变换矩阵、脏标记(用来进行提前剔除)
//其对应的物体常量缓冲区的下标、其对应的材质的地质、其对应的几何的地质、其几何的拓扑结构、其所干预的顶点在几何中的顶点的起始index和总的index数量，以及这些index对应的顶点的基准偏移量
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMFLOAT4X4 World = MathHelper::Identity4x4();	//世界矩阵， 从局部空间转换到世界空间

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();	//纹理采样矩阵，用来对纹理进行采样

	int NumFramesDirty = gNumFrameResources;	//脏标记，若为0，则我们可以不更新

	UINT ObjCBIndex = -1;	//其对应的物体在常量缓冲区中的下标

	Material* Mat = nullptr;	//其对应的材质
	MeshGeometry* Geo = nullptr;	//其对应的几何。 几何中包含了顶点缓冲区和索引缓冲区

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	//该几何的拓扑。 为什么拓扑放在了这儿，而非放在了几何中？ 因为同样的顶点可以表示不同的内容；不同的格式可以推入同一个几何中，到时候只需要根据索引和顶点基准值进行采样即可

	UINT IndexCount = 0;	//分别为索引数量、起始索引位置、顶点基准值
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

//用来记录我们的渲染层。 不同层可以存储不同部分，并采用不同的shader执行
enum class RenderLayer : int
{
	Opaque = 0,
	Debug,
	Sky,
	Count
};

class SsaoApp : public D3DApp
{
public:
	SsaoApp(HINSTANCE hInstance);
	SsaoApp(const SsaoApp& rhs) = delete;
	SsaoApp& operator=(const SsaoApp& rhs) = delete;
	~SsaoApp();

	virtual bool Initialize() override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;
	virtual void 
};
