//SkinnedMeshApp copy from DX12, chapter24, by Frank Luna

#include "../../d3d12book-master/Common/d3dApp.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"
#include "../../d3d12book-master/Common/GeometryGenerator.h"
#include "../../d3d12book-master/Common/Camera.h"
#include "FrameResource.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "SkinnedData.h"
#include "LoadM3d.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct SkinnedModelInstance	//一个可运动的模型的实例
{
	SkinnedData* SkinnedInfo = nullptr;	//其中包含了其蒙皮的信息
	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;	//以及其绑定到每个骨骼的顶点从模型空间经过该骨骼运动后的最终的变换
	std::string ClipName;	//动画片段的名称. 其实我们的m3d文件中只有一个动画
	float TimePos = 0.0f;	//时间戳

	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName)) TimePos = 0.0f;
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

struct RenderItem	//渲染项, 每个应用中的渲染项都可能是不同的
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMFLOAT4X4 World = MathHelper::Identity4x4();	//一个渲染项中一定包含了模型到世界的变化矩阵, 模型顶点到纹理的采样矩阵
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	int NumFramesDirty = gNumFrameResources;	//脏标记用于我们仅更新需要更新的资源

	UINT ObjCBIndex = -1;	//渲染项对应了一个对象, 我们通过其常量缓冲区索引来索引之

	Material* Mat = nullptr;	//渲染项也一定有一个材质，有一个几何
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	//其默认拓扑我们设置为三角形列表

	UINT IndexCount = 0;	//我们需要记录其索引数量，起始索引位置，起始顶点位置
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	UINT SkinnedCBIndex = 0;	//其蒙皮的常量缓冲区的位置

	SkinnedModelInstance* SkinnedModelInst = nullptr;	//其对应的蒙皮模型的实例
};

enum class RenderLayer : int
{
	Opaque = 0,
	SkinnedOpaque,
	Debug,
	Sky,
	Count
};

class SkinnedMeshApp : public D3DApp
{
public:
	SkinnedMeshApp(HINSTANCE hInstance);	//构造函数
	SkinnedMeshApp(const SkinnedMeshApp& rhs) = delete;
	SkinnedMeshApp& operator=(const SkinnedMeshApp& rhs) = delete;
	~SkinnedMeshApp();	//析构函数

	virtual bool Initialize() override;	//初始化

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;	//创建Rtv和Dsv描述符堆, 由于我们需要引入Ssao和阴影，因此需要额外的渲染目标与深度/模板缓冲区
	virtual void OnResize() override;	//当视口大小变更时响应
	virtual void Update(const GameTimer& gt) override;	//更新方法， 每帧调用
	virtual void Draw(const GameTimer& gt) override;	//绘制方法, 每帧调用

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;	//鼠标按下/抬起/移动时的响应
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);	//按下按键时的响应
	void AnimateMaterials(const GameTimer& gt);	//动画
	void UpdateObjectCBs(const GameTimer& gt);	//更新物体常量缓冲区
	void UpdateSkinnedCBs(const GameTimer& gt);	//更新蒙皮顶点的常量缓冲区
	void UpdateMaterialBuffer(const GameTimer& gt);	//更新材质缓冲区
	void UpdateShadowTransform(const GameTimer& gt);	//更新阴影变换矩阵
	void UpdateMainPassCB(const GameTimer& gt);	//更新主Pass的常量缓冲区
	void UpdateShadowPassCB(const GameTimer& gt);	//更新阴影Pass的常量缓冲区
	void UpdateSsaoCB(const GameTimer& gt);	//更新Ssao的常量缓冲区

	void LoadTextures();	//加载纹理
	void BuildRootSignature();	//构建根签名
	void BuildSsaoRootSignature();	//构建Ssao的根签名
	void BuildDescriptorHeaps();	//构建描述符堆
	void BuildShadersAndInputLayout();	//构建Shader和输入描述布局
	void BuildShapeGeometry();	//构建形状
	void LoadSkinnedModel();	//构建蒙皮模型
	void BuildPSOs();	//构建流水线状态对象们
	void BuildFrameResources();	//构建帧资源们
	void BuildMaterials();	//构建材质们
	void BuildRenderItems();	//构建渲染项们
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);	//绘制渲染项们
	void DrawSceneToShadowMap();	//将场景绘制到阴影图中
	void DrawNormalsAndDepth();	//绘制法线和深度图
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;	//获取指定索引的Srv在CPU侧的句柄
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;	//获取指定索引的Srv在GPU侧的句柄
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;	//获取指定索引的Dsv的句柄. DSV一定是在CPU侧获取的, 然后在绘制调用时将其传递给命令列表
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;	//获取指定索引的Rtv的句柄, RTV一定是在CPU侧获取的, 然后在绘制调用时将其传递给命令列表

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();	//获取静态采样器们. 我们在这里定义了7个静态采样器

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;	//存储帧资源们
	FrameResource* mCurrFrameResource = nullptr;	//当前使用的帧资源
	int mCurrFrameResourceIndex = 0;	//记录当前使用的帧资源的下标

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;	//主Pass的根签名
	ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;	//SsaoPass的根签名

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;	//srv的描述符堆

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;	//分别记录了几何，材质，纹理，Shader和PSO
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;	//通常顶点的输入布局描述
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;	//动画的输入布局描述. 动画中添加了骨骼和权重, 因此我们需要向VS传入更多数据和语义

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;	//记录所有的渲染项
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];	//记录每个层级中的渲染项

	UINT mSkyTexHeapIndex = 0;	//记录了天空纹理在描述符堆中的索引
	UINT mShadowMapHeapIndex = 0;	//记录了阴影图在描述符堆中的索引
	UINT mSsaoHeapIndexStart = 0;	//记录了Ssao图在描述符堆中的索引的起始位置
	UINT mSsaoAmbientMapIndex = 0;	//记录了Ssao遮蔽率图在描述符堆中的索引

	UINT mNullCubeSrvIndex = 0;	//下面是几个我们不会真正使用，仅仅是为了绑定资源而创建的空Srv的索引， 分别是一个立方体图着色器资源，两个正常纹理着色器资源
	UINT mNullTexSrvIndex1 = 0;
	UINT mNullTexSrvIndex2 = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;	//记录了我们需要将空Srv绑定到的GPU中的位置

	PassConstants mMainPassCB;	//主Pass对应的帧常量们
	PassConstants mShadowPassCB;	//阴影Pass对应的帧常量们

	UINT mSkinnedSrvHeapStart = 0;	//动画对应的着色器资源在描述符堆中的索引的起始位置
	std::string mSkinnedModelFilename = "Models\\soldier.m3d";	//动画模型的相对路径与名称
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;	//指向根据上面的动画模型而创建的蒙皮模型的实例的指针
	SkinnedData mSkinnedInfo;	//记录了我们的动画模型对应的蒙皮、动画信息. 其包含了动画起始、结束时间、骨骼数量、模型空间到骨骼空间的变换矩阵、所有动画的信息
	std::vector<M3DLoader::Subset> mSkinnedSubsets;	//记录了我们动画模型的submesh们
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;	//记录了我们动画模型的每个submesh对应的材质
	std::vector<std::string> mSkinnedTextureNames;	//我们在动画模型每个submesh中需要的纹理的名称

	Camera mCamera;	//本app使用的相机

	std::unique_ptr<ShadowMap> mShadowMap;	//本app使用的阴影图。这里我们假定了阴影图只有一个
	std::unique_ptr<Ssao> mSsao;	//本App使用的Ssao效果
	
	DirectX::BoundingSphere mSceneBounds;	//包围了场景中所有物体的包围球

	float mLightNearZ = 0.0f;	//相机的NearZ
	float mLightFarZ = 0.0f;	//相机的FarZ
	XMFLOAT3 mLightPosW;	//光源的位置
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();	//光源的观察矩阵. 用于将坐标从世界空间变换到相机的观察空间
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();	//光源的投影矩阵. 用于将坐标从相机的观察空间变换到NDC空间
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();	//阴影的变换矩阵. 用于直接将世界空间中的坐标变换到阴影采样纹理坐标

	float mLightRotationAngle = 0.0f;	//光源当前的旋转角度

	XMFLOAT3 mBaseLightDirections[3] = {	//我们使用了三点布光系统.
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),	//左前方的光源向右下方照射
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),	//右前方的光源向左下方照射
		XMFLOAT3(0.0f, -0.707f, -0.707f),	//上方的补光灯向前下方照射
	};

	XMFLOAT3 mRotatedLightDirections[3];	//在计算过旋转角度后，每个光源当前的方向

	POINT mLastMousePos;	//记录了鼠标上次的位置. 用于实现鼠标滑动的效果
};

SkinnedMeshApp::SkinnedMeshApp(HINSTANCE hInstance)
{
}

SkinnedMeshApp::~SkinnedMeshApp()
{
}

bool SkinnedMeshApp::Initialize()
{
	return false;
}

void SkinnedMeshApp::CreateRtvAndDsvDescriptorHeaps()
{
}

void SkinnedMeshApp::OnResize()
{
}

void SkinnedMeshApp::Update(const GameTimer& gt)
{
}

void SkinnedMeshApp::Draw(const GameTimer& gt)
{
}

void SkinnedMeshApp::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void SkinnedMeshApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void SkinnedMeshApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void SkinnedMeshApp::OnKeyboardInput(const GameTimer& gt)
{
}

void SkinnedMeshApp::AnimateMaterials(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateObjectCBs(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateSkinnedCBs(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateMaterialBuffer(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateShadowTransform(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateMainPassCB(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateShadowPassCB(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateSsaoCB(const GameTimer& gt)
{
}

void SkinnedMeshApp::LoadTextures()
{
}

void SkinnedMeshApp::BuildRootSignature()
{
}

void SkinnedMeshApp::BuildSsaoRootSignature()
{
}

void SkinnedMeshApp::BuildDescriptorHeaps()
{
}

void SkinnedMeshApp::BuildShadersAndInputLayout()
{
}

void SkinnedMeshApp::BuildShapeGeometry()
{
}

void SkinnedMeshApp::LoadSkinnedModel()
{
}

void SkinnedMeshApp::BuildPSOs()
{
}

void SkinnedMeshApp::BuildFrameResources()
{
}

void SkinnedMeshApp::BuildMaterials()
{
}

void SkinnedMeshApp::BuildRenderItems()
{
}

void SkinnedMeshApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}

void SkinnedMeshApp::DrawSceneToShadowMap()
{
}

void SkinnedMeshApp::DrawNormalsAndDepth()
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetCpuSrv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetGpuSrv(int index) const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetDsv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetRtv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SkinnedMeshApp::GetStaticSamplers()
{
	return std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7>();
}
