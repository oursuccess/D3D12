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
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;
	UINT mSsaoHeapIndexStart = 0;
	UINT mSsaoAmbientMapIndex = 0;

	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex1 = 0;
	UINT mNullTexSrvIndex2 = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	PassConstants mMainPassCB;
	PassConstants mShadowPassCB;

	UINT mSkinnedSrvHeapStart = 0;
	std::string mSkinnedModelFilename = "Models\\soldier.m3d";
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;

	Camera mCamera;

	std::unique_ptr<ShadowMap> mShadowMap;
	std::unique_ptr<Ssao> mSsao;
	
	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;

	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f),
	};

	XMFLOAT3 mRotatedLightDirections[3];

	POINT mLastMousePos;
};
