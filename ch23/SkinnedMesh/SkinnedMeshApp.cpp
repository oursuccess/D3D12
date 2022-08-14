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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);	//若开启了Debug, 则打开内存检查
#endif

	try 
	{
		SkinnedMeshApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

SkinnedMeshApp::SkinnedMeshApp(HINSTANCE hInstance) :
	D3DApp(hInstance)
{
	//由于场景是静态的, 因此我们预先就知道了最小包围球的中心和半径. 正常我们需要实时计算
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

SkinnedMeshApp::~SkinnedMeshApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();	//等待命令队列执行完毕才能退出, 防止GPU执行到特定指令时发现CPU中的资源已经被释放，从而导致退出时报错
}

bool SkinnedMeshApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));	//如果我们无法将命令列表分配器重置, 则直接报错

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);	//设置相机的初始位置. 在y2.0f, z为-15.0f的位置

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);	//初始化阴影图管理类, 其分辨率为2048*2048
	mSsao = std::make_unique<Ssao>(md3dDevice.Get(), mCommandList.Get(), mClientWidth, mClientHeight);	//初始化Ssao管理类. 其分辨率为视口大小. 由于Ssao绘制时需要命令列表, 因此我们将命令列表传过去

	LoadSkinnedModel();	//读取动画模型
	LoadTextures();	//读取纹理
	BuildRootSignature();	//构建根签名
	BuildSsaoRootSignature();	//构建Ssao的根签名
	BuildDescriptorHeaps();	//构建描述符堆
	BuildShadersAndInputLayout();	//构建Shader和输入描述布局
	BuildShapeGeometry();	//构建几何
	BuildMaterials();	//构建材质. 材质构建前需要先构建纹理
	BuildRenderItems();	//构建渲染项. 渲染项中包含了Geo, Mat
	BuildFrameResources();	//构建帧资源. 帧资源中存储了每个渲染项的常量资源，每个材质的资源, 因此依赖于BuildRenderItems和BuildMaterials
	BuildPSOs();	//构建流水线状态对象. 流水线状态构建时，我们需要设置其shader,输入描述布局,根签名,混合模式等. 因此其依赖于BuildShadersAndInputLayout, BuildRootSignature

	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);	//在上面的构建中, 可能用命令列表执行了一些命令, 我们将这些命令转为命令队列并执行

	FlushCommandQueue();	//等待命令执行完毕

	return true;
}

void SkinnedMeshApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;	//有一个屏幕法线贴图, 两个遮蔽率贴图, 因此+3
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	//其Flag为None
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;	//其类型为RTV
	rtvHeapDesc.NodeMask = 0;	//其没有任何Mask
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));	//根据描述创建rtv描述符堆. 并将其存储到mRtvHeap

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;	//相机正常的遮挡剔除需要一个深度图, 我们还需要一个用于阴影实现(从光源观察)的DSV
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));	//根据描述创建dsv描述符堆, 并将其存储到DsvHeap
}

void SkinnedMeshApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);	//更新相机的长宽比

	if (mSsao != nullptr)
	{
		mSsao->OnResize(mClientWidth, mClientHeight);	//先通知Ssao更新分辨率
		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());	//然后通知Ssao重建描述符堆. 我们需要将深度/模板对应的缓冲区传递过去, 从而Ssao才能正常绑定DSV
	}
}

void SkinnedMeshApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);	//先响应玩家的输入

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;	//步进帧资源
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)	//在两种情况下, 我们可以确信当前帧资源是空闲的: 1.从未使用过(Fence为0); 2.GPU已经使用完毕(Fence已经被更新为比mFence的CompletedValue更大的值)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);	//创建一个event句柄
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));	//当CurrFrameResource的Fence被更新时, 我们认为该帧资源使用完成了, 此时可以触发事件(eventHandle)
		WaitForSingleObject(eventHandle, INFINITE);	//我们等待eventHandle被触发, 等待时间为无限大. 因为我们此时没有其它可做的事
		CloseHandle(eventHandle);	//当事件被触发时, 我们可以直接关闭事件Handle
	}

	mLightRotationAngle += 0.1f * gt.DeltaTime();	//光源当前的旋转角度. 我们让光源直接匀速旋转即可

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);	//其旋转是相对于y轴进行的. 我们可以由角度得到对应的变换矩阵
	for (int i = 0; i < 3; ++i)	//我们的光源共有3个
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);	//我们准备让光源进行旋转, 首先我们需要取出没有旋转时的光照方向(这是个Float3, 但是为了计算, 我们将其转为Vector)
		lightDir = XMVector3TransformNormal(lightDir, R);	//我们让光照方向根据变换矩阵进行变换, 得到最新的方向
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);	//我们将最新的光源方向存入到旋转后的光照方向数组
	}

	AnimateMaterials(gt);	//然后我们依次更新材质、物体、动画物体、材质缓冲区、阴影采样矩阵、MainPass的常量缓冲区、阴影Pass的常量缓冲区、Ssao的常量缓冲区
	UpdateObjectCBs(gt);
	UpdateSkinnedCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);
	UpdateSsaoCB(gt);
}

void SkinnedMeshApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;	//取出命令分配器, 并准备重置
	ThrowIfFailed(cmdListAlloc->Reset());	//添加保护

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));	//将命令列表重置, 重置为cmdListAlloc作为分配器, 初始PSO为opaque

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };	//获取srv的描述符堆. 我们将其获取为一个数组!
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);	//我们利用上面的数组, 让命令列表设置描述符堆

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());	//我们设置渲染的根签名为RootSignature

	//开始ShadowMap的绘制
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();	//获取所有材质的缓冲区
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());	//对于结构化的缓冲区, 我们可以直接将其绑定为一个根描述符(RootShaderResourceView). 我们的根描述符表的3绑定了材质
	mCommandList->SetGraphicsRootDescriptorTable(4, mNullSrv);	//我们以NullSrv设置到4对应的根描述符表上. 表示Shadow绘制时不需要这个位置对应的Srv
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//我们将所有真正用到的纹理都直接绑定到5对应的根描述符表上. 这才是纹理对应的根参数索引
	DrawSceneToShadowMap();	//绘制阴影图

	//开始深度/法线图的绘制
	DrawNormalsAndDepth();

	//开始Ssao的绘制
	mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());	//将根签名变更为Ssao
	mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 2);	//我们让Ssao管理类自行计算Ssao. 其混合次数为2

	//MainPass的绘制
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());	//重新设置根参数3, 其指定了所有材质, 是一个根描述符

	mCommandList->RSSetViewports(1, &mScreenViewport);	//设置栅格化阶段的视口
	mCommandList->RSSetScissorRects(1, &mScissorRect);	//设置栅格化阶段的裁剪矩形

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));	//我们将后台缓冲区的资源状态从只读变更为渲染目标

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::SteelBlue, 0, nullptr);	//我们将CurrentBackBufferView这一RTV描述符对应的格式，来将其指定的资源重置，重置为颜色为深蓝，没有裁剪矩形
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);	//我们将Depth/Stencil描述符对应的资源重置, 将DEPTH和STENCIL的FLAG都清空，将深度均改为1，将STENCIL改为0, 且没有裁剪矩形

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());	//我们设置输出合并阶段的渲染目标. 渲染目标为1, 其为我们的CurrentBackBufferView指定的格式与资源, 我们同时声明了所有渲染目标共用根描述符, 深度与模板信息在DepthStencilView中写入
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//我们将所有真正用到的纹理都直接绑定到5对应的根描述符表上, 5就是纹理对应的根参数索引
	auto passCB = mCurrFrameResource->PassCB->Resource();	//获取当前的帧常量缓冲区
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());	//我们设置帧常量, 其绑定在根参数为2的位置, 同样的, 其为结构化常量，因此用根描述符即可

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvUavDescriptorSize);	//获取对应天空纹理的描述符
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);	//将天空纹理绑定到根参数4的位置. 由于是纹理, 因此我们必须绑定到根描述符表上

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());	//我们开始绘制不同的渲染项. 每次绘制时, 我们转向对应的PSO, 然后进行绘制调用. 绘制顺序为: opaque, skinnedOpaque, debug, sky
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Debug]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	//绘制调用完成后, 我们将后台缓冲区的状态从渲染目标变回只读
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	//然后, 我们可以关闭命令列表, 并执行命令队列
	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));	//将后台缓冲区与前台显示区域交换. 无需交换等待时间, 没有额外flag
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;	//更新当前被视为后台的Buffer

	mCurrFrameResource->Fence = ++mCurrentFence;	//增加当前帧资源的Fence值,并递增mCurrentFence. 我们先设置mCurrFrameResource的Fence, 表示我们期望的Fence值为x

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);	//当命令队列中的所有命令都执行完毕时, 将mFence更新为mCurrentFence, 从而发出信号, 通知CPU. 即当命令执行完毕时, mFence的值才会变为x. 此时我们可以确认, CurrFrameResource可以释放了
}

void SkinnedMeshApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;	//更新鼠标的位置
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);	//当玩家拖动屏幕时, 我们将画面定格
}

void SkinnedMeshApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();	//接触画面的定格
}

void SkinnedMeshApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)	//只有在按下左键的时候, 我们才移动相机
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));	//计算x上的移动量
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));	//计算y上的移动量

		//Picth: 绕着z轴旋转(上下朝向变化, 上看下看); Yaw: 绕着y轴旋转(角色上方向不变，左转右转); Roll： 绕着x轴旋转(上方向变化, 左偏右偏, 通常只有3D自由移动的物体如飞机才有该方向的旋转)
		mCamera.Pitch(dy);	//绕着z轴的旋转，其自然为y方向的移动量
		mCamera.RotateY(dx);	//绕着y轴的旋转, 其自然为x方向的移动量
	}

	mLastMousePos.x = x;	//记录相机位置
	mLastMousePos.y = y;
}

void SkinnedMeshApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)	//W和S控制前后走(Walk), 我们让移动幅度与时间dt相关
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)	//A和D控制左右移动(Strafe)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();	//相机移动后, 自然需要更新其观察矩阵. 因为其位置相对于世界移动了!
}

void SkinnedMeshApp::AnimateMaterials(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();	//所有物体的常量都存放在当前帧资源的ObjectCB中
	for (auto& e : mAllRitems)	//遍历所有有脏标记的物体并更新其对应的物体常量即可. 物体常量中有材质索引、世界坐标、纹理采样坐标
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));	//分别更新世界矩阵、纹理采样矩阵、材质索引
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);	//我们需要将新的物体常量复制到物体常量缓冲区中的指定位置

			--e->NumFramesDirty;
		}
	}
}

void SkinnedMeshApp::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();	//动画物体的常量缓冲区位于帧资源的SkinnedCB中

	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());	//我们需要更新当前的动画

	SkinnedConstants skinnedConstants;
	std::copy(std::begin(mSkinnedModelInst->FinalTransforms), std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);	//我们将动画运算后的顶点采样矩阵全都保存到蒙皮常量中
	currSkinnedCB->CopyData(0, skinnedConstants);	//我们仅仅只有一个蒙皮动画. 因此只更新这一个
}

void SkinnedMeshApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();	//材质的常量缓冲区位于帧资源的MaterialBuffer中
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();	//mat中包含了材质采样矩阵、粗糙度、漫反射、R0、纹理图索引、法线图索引
		if (mat->NumFramesDirty > 0)	//我们只更新具有脏标记的材质
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData); //将当前材质的常量复制到指定位置

			--mat->NumFramesDirty;	//递减脏标记
		}
	}
}

void SkinnedMeshApp::UpdateShadowTransform(const GameTimer& gt)
{
	//我们的实现中, 仅仅第一个平行光需要实现阴影
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);	//光源方向
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;	//我们给光源一个位置. 其在天空盒上(因为目标点为包围球的中心)
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);	//目标点为我们的包围球的中心
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);	//上方向为y轴
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);	//根据如此, 我们可以更新观察矩阵: 左手坐标系下, 光源方向为pos, 目标点为center, 上方向为y轴

	XMStoreFloat3(&mLightPosW, lightPos);	//将我们给的光源位置赋值给mLightPos. 其用于绘制阴影图

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));	//我们准备将包围球变换到光照空间中. 首先我们变换包围球的圆心

	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;	//将近裁切和远裁切距离提升为变量
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);	//构建我们的光照投影矩阵. 该矩阵为一个正交矩阵, 其左右下上近远距离为我们上面计算得出的值

	XMMATRIX T(	//构建从NDC空间到纹理空间的变换矩阵. 这个矩阵为列矩阵
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;	//构建从世界空间直接变换到光照空间下采样坐标的变换矩阵
	XMStoreFloat4x4(&mLightView, lightView);	//将光照观察、光照投影、最终的变换矩阵都提升为变量
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void SkinnedMeshApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();	//计算观察、投影、观察投影矩阵及其逆矩阵
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = view * proj;
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);	//我们在计算阴影时, 需要根据shadowTransform来采样点到光源的深度

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();	//观察点就是相机位置
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1.0f;
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };	//更新一下漫反射光
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];	//更新三个光源. 我们只需要更新光照方向(旋转后的)和强度
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.7f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();	//准备更新当前帧常量
	currPassCB->CopyData(0, mMainPassCB);	//MainPass的帧常量序号为0
}

void SkinnedMeshApp::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);	//光源的观察矩阵与投影矩阵我们都已经计算过了, 直接拿来用即可
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);	//同样的, 我们计算其复合矩阵与所有的逆矩阵
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));		//所有的矩阵, 我们在准备提交时, 都要转置
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);

	auto curPassCB = mCurrFrameResource->PassCB.get();
	curPassCB->CopyData(1, mShadowPassCB);	//阴影在PassCB中的常量缓冲区序号为1
}

void SkinnedMeshApp::UpdateSsaoCB(const GameTimer& gt)
{
	SsaoConstants ssaoCB;	//准备构建ssao需要的常量缓冲区

	XMMATRIX P = mCamera.GetProj();	//投影矩阵为从光源位置观察获得的
	XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));	//存储投影采样矩阵. 因为在Ssao的遮蔽率运算中, 我们只需要将坐标变换到观察空间中即可

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);	//更新偏移向量们

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);	//计算边缘模糊, 半径为2.5f
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);	//看清楚了，我们直接用vector从索引开始的4个元素构建了个float4!!!
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);	//每次都直接增加4!

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	ssaoCB.OcclusionRadius = 0.5f;	//遮蔽半径. 只有距离小于遮蔽半径的, 我们才认为可能发生遮蔽. 半径距离越小, 遮蔽率越大
	ssaoCB.OcclusionFadeStart = 0.2f;	//遮蔽衰减开始的距离
	ssaoCB.OcclusionFadeEnd = 2.0f;	//遮蔽衰减到0的距离
	ssaoCB.SurfaceEpsilon = 0.05f;	//z值距离小于此值, 我们认为两个点在一个平面上, 也不会发生遮蔽

	auto curSsaoCB = mCurrFrameResource->SsaoCB.get();	//准备将ssao常量复制到SsaoCB. 其序号同样为0
	curSsaoCB->CopyData(0, ssaoCB);
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
