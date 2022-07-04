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
	SsaoApp(HINSTANCE hInstance);	//构造函数. 我们不允许拷贝构造函数
	SsaoApp(const SsaoApp& rhs) = delete;
	SsaoApp& operator=(const SsaoApp& rhs) = delete;
	~SsaoApp();

	virtual bool Initialize() override;	//初始化，执行必要的资源、命令、PSO等的创建

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;	//创建渲染目标和深度模板视图所用的描述符堆。 由于我们实现阴影图和Ssao效果，因此需要特意重写该方法
	virtual void OnResize() override;	//当分辨率变化时响应，我们更新分辨率、阴影图分辨率，Ssao分辨率等
	virtual void Update(const GameTimer& gt) override;	//更新。 每帧更新时，我们更新相机、场景内动态物体，然后进行绘制调用
	virtual void Draw(const GameTimer& gt) override;	//绘制调用。 绘制调用时，我们根据命令列表首先将缓冲区清空，然后依次按照顺序进行各种效果的绘制调用

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;	//下面三个方法分别为当鼠标按下、鼠标抬起、鼠标移动时触发。 我们在其中进行玩家鼠标操作的响应。
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);	//当玩家按下按键时触发。 我们在其中进行玩家键盘输入的响应。
	void AnimateMaterials(const GameTimer& gt);	//动态更新材质。 需要更新的材质在此处更新
	void UpdateObjectCBs(const GameTimer& gt);	//更新物体的常量缓冲区。 如物体的世界矩阵等
	void UpdateMaterialBuffer(const GameTimer& gt);	//更新材质缓冲区。 如动态材质等。
	void UpdateShadowTransform(const GameTimer& gt);	//更新阴影的变换矩阵。 我们让阴影随着光源而变化
	void UpdateMainPassCB(const GameTimer& gt);	//进行Pass常量缓冲区的更新。 如观察点、投影矩阵等
	void UpdateShadowPassCB(const GameTimer& gt);	//进行阴影Pass常量缓冲区的更新。 如进行阴影贴图的绘制
	void UpdateSsaoCB(const GameTimer& gt);	//进行Ssao常量缓冲区的更新。 如进行Ssao图的绘制

	void LoadTextures();	//加载贴图. 贴图是材质的一部分。
	void BuildRootSignature();	//构建根签名。 根描述符、描述符表、根常量需要在根签名中绑定
	void BuildSsaoRootSignature();	//构建Ssao所需的根签名
	void BuildDescriptorHeaps();	//构建描述符堆。贴图资源所需的描述符在这里创建
	void BuildShadersAndInputLayout();	//构建Shader代码和输入描述布局。
	void BuildShapeGeometry();	//构建几何
	void BuildSkullGeometry();	//构建骷髅头
	void BuildPSOs();	//构建渲染状态对象
	void BuildFrameResources();	//构建帧资源们. 帧资源数量越多，我们需要推入的材质等也就越多
	void BuildMaterials();	//构建材质. 材质包含了纹理、基础颜色、R0、粗糙度、资源的位置等
	void BuildRenderItems();	//构建渲染项. 渲染项包含了世界矩阵、采样矩阵、物体对象在资源中的便宜、其材质、其几何、其顶点数量、其索引起始量等
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);	//根据传入的渲染项依次绘制渲染项. 将绘制命令放在命令列表中
	void DrawSceneToShadowMap();	//根据场景绘制阴影贴图(从光源看的深度图)
	void DrawNormalsAndDepth();	//绘制法线和深度图(从相机看的)

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;	//分别为获取对应的着色器资源视图、深度模板视图、渲染对象视图的方法
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();	//获取静态采样器

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;	//存储帧资源们
	FrameResource* mCurrFrameResource = nullptr;	//当前的帧资源，及其序号
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;	//存放根签名
	ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;	//存放Ssao所需的根签名

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;	//存放着色器资源描述符堆

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;	//分别存放几何、材质、贴图、Shader、流水线状态对象们, 均为hash
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;	//输入描述布局列表

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;	//所有的渲染项们

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];	//存放不同层级的渲染项们

	UINT mSkyTexHeapIndex = 0;	//分别存储天空纹理、阴影纹理、Ssao所需的描述符堆、Ssao的遮蔽率纹理在堆中的偏移
	UINT mShadowMapHeapIndex = 0;
	UINT mSsaoHeapIndexStart = 0;
	UINT mSsaoAmbientMapIndex = 0;

	UINT mNullCubeSrvIndex = 0;	//这是什么? 
	UINT mNullTexSrvIndex1 = 0;
	UINT mNullTexSrvIndex2 = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;	//这是什么?

	PassConstants mMainPassCB;	//主Pass所需的帧常量
	PassConstants mShadowPassCB;	//阴影Pass所需的帧常量

	Camera mCamera;	//当前相机

	std::unique_ptr<ShadowMap> mShadowMap;	//存储阴影图
	std::unique_ptr<Ssao> mSsao;	//存储Ssao

	DirectX::BoundingSphere	mSceneBounds;	//场景的包围球

	float mLightNearZ = 0.0f;	//光源的nearZ和farZ. 用于决定光的衰减
	float mLightFarZ = 0.0f;	
	XMFLOAT3 mLightPosW;	//光的世界坐标
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();	//光的观察方向
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();	//光的投影方向
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();	//阴影的变换坐标

	float mLightRotationAngle = 0.0f;	//光源当前的角度
	XMFLOAT3 mBaseLightDirections[3] = {	//三点布光系统，下面记录的是光的方向
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),	//从左前方向下方的主光源
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),	//从右前方向下方的副光
		XMFLOAT3(0.0f, -0.707f, -0.707f),	//从后上方向前、下方的补光
	};
	XMFLOAT3 mRotatedLightDirections[3];	//旋转后的光源方向

	POINT mLastMousePos;	//记录上一次的鼠标位置
};

SsaoApp::SsaoApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
	//我们已然知道场景包围球的中心店和其半径
	//若要通用性的话，我们需要遍历整个世界空间下所有顶点的坐标，然后计算出包围球
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

SsaoApp::~SsaoApp()
{
	//若d3dDevice不为空，则我们需要等待命令队列执行完成，从而防止退出时还有未执行完毕、资源也未备好的命令，导致退出时报错
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool SsaoApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));		//首先将命令列表重置为空，从而为我们的初始化做准备

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);	//设置相机位置. 这里是左手坐标系, 即相机从斜上方向场景中看. Z值并非0，而是-15

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);	//我们创建一个2048*2048分辨率的阴影贴图

	mSsao = std::make_unique<Ssao>(md3dDevice.Get(), mCommandList.Get(), mClientWidth, mClientHeight);	//我们创建一个分辨率和当前屏幕分辨率相同的Ssao

	LoadTextures();	//加载纹理
	BuildRootSignature();	//创建根签名. 从而为后面的资源绑定做准备
	BuildSsaoRootSignature();	//创建Ssao所需的根签名. 同样为后面的资源绑定做准备
	BuildDescriptorHeaps();	//创建描述符堆.我们将纹理资源绑定到描述符堆中. 而描述符堆则在根签名中定义
	BuildShadersAndInputLayout();	//加载shader代码和输入描述布局.
	BuildShapeGeometry();	//创建几何
	BuildSkullGeometry();	//创建骷髅头几何
	BuildMaterials();	//创建材质们. 材质包含了纹理、粗糙度、R0等
	BuildRenderItems();	//创建渲染项. 渲染项里包含了材质、几何、PRIMITIVE_TYPE、顶点数量等
	BuildFrameResources();	//创建帧资源. 帧资源包含了渲染项和实际材质
	BuildPSOs();	//创建流水线状态对象. 流水线状态对象要求我们设置好了根签名、材质、Shader代码、输入描述布局等

	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());	//为Ssao传入必须的Ssao和SsaoBlur流水线状态对象

	ThrowIfFailed(mCommandList->Close());	//在初始化完成后，开始根据命令列表执行命令队列
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);	

	FlushCommandQueue();	//等待初始化完成

	return true;
}

void SsaoApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;	//创建描述符堆,用于描述渲染对象
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;	//渲染对象要多3个，其中1个为屏幕法线图，还有2个为遮蔽率图. 这些都是Ssao要用的. 原本所需的是SwapChainBufferCount
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;	//其类型自然为渲染目标
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	//没什么特殊标记
	rtvHeapDesc.NodeMask = 0;	//同样没有节点标记
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));	//根据描述符堆说明创建实际的描述符堆

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;	//为了ShadowMap，增加一个用于描述深度/模板视图的描述符堆描述
	dsvHeapDesc.NumDescriptors = 2;	//原本需要一个深度/模板视图(参见D3DApp::CreateRtvAndDsvDescriptorHeap()), 现在又增加了1个
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	//其类型自然为DSV
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;	//同样没有特殊标记
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));	//根据说明创建实际的描述符堆
}

void SsaoApp::OnResize()
{
	D3DApp::OnResize();	//首先调用基类的OnResize方法. 基类的方法中调整了窗口分辨率、裁剪矩形, 并对分辨率进行了保存

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);		//重新构建相机视角. 其第一个参数为FOV

	if (mSsao != nullptr)
	{
		mSsao->OnResize(mClientWidth, mClientHeight);	//若Ssao启用，则同时调用Ssao的OnResize方法

		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());	//同样的我们让Ssao重新构建描述符. 之所以要传入mDepthStencilBuffer，是因为我们在App中还需要该缓冲区
	}
}

void SsaoApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);	//首先响应玩家的输入

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;	//每次步进一个FrameResourceIndex
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)	//如果当前帧资源的围栏值不为0，且GPU尚未处理到该Frame，则无限等待. 
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));	//在GPU处理该FrameResource后，会将当前Fence值设置为当前帧的Fence值
		WaitForSingleObject(eventHandle, INFINITE);	//无限等待直至该事件被触发(此时Fence值也将被设置)
		CloseHandle(eventHandle);	//事件触发后，关闭事件
	}

	mLightRotationAngle += 0.1f * gt.DeltaTime();	//让光旋转. 每秒旋转1/10. 每帧旋转时间乘以该值

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);	//根据该角度构建绕y轴旋转的矩阵
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);	//旋转一个向量，因此我们使用TransformNormal方法. 若用Transform，则默认为旋转点
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	AnimateMaterials(gt);	//材质的动画
	UpdateObjectCBs(gt);	//更新物体常量缓冲区
	UpdateMaterialBuffer(gt);	//更新材质缓冲区
	UpdateShadowTransform(gt);	//更新阴影的变换矩阵
	UpdateMainPassCB(gt);	//更新主帧的常量缓冲区
	UpdateShadowPassCB(gt);	//更新阴影图的常量缓冲区
	UpdateSsaoCB(gt);	//更新Ssao的常量缓冲区
}

void SsaoApp::Draw(const GameTimer& gt)
{
}

void SsaoApp::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void SsaoApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void SsaoApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void SsaoApp::OnKeyboardInput(const GameTimer& gt)
{
}

void SsaoApp::AnimateMaterials(const GameTimer& gt)
{
}

void SsaoApp::UpdateObjectCBs(const GameTimer& gt)
{
}

void SsaoApp::UpdateMaterialBuffer(const GameTimer& gt)
{
}

void SsaoApp::UpdateShadowTransform(const GameTimer& gt)
{
}

void SsaoApp::UpdateMainPassCB(const GameTimer& gt)
{
}

void SsaoApp::UpdateShadowPassCB(const GameTimer& gt)
{
}

void SsaoApp::UpdateSsaoCB(const GameTimer& gt)
{
}

void SsaoApp::LoadTextures()
{
}

void SsaoApp::BuildRootSignature()
{
}

void SsaoApp::BuildSsaoRootSignature()
{
}

void SsaoApp::BuildDescriptorHeaps()
{
}

void SsaoApp::BuildShadersAndInputLayout()
{
}

void SsaoApp::BuildShapeGeometry()
{
}

void SsaoApp::BuildSkullGeometry()
{
}

void SsaoApp::BuildPSOs()
{
}

void SsaoApp::BuildFrameResources()
{
}

void SsaoApp::BuildMaterials()
{
}

void SsaoApp::BuildRenderItems()
{
}

void SsaoApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}

void SsaoApp::DrawSceneToShadowMap()
{
}

void SsaoApp::DrawNormalsAndDepth()
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SsaoApp::GetCpuSrv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SsaoApp::GetGpuSrv(int index) const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SsaoApp::GetDsv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SsaoApp::GetRtv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SsaoApp::GetStaticSamplers()
{
	return std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7>();
}
