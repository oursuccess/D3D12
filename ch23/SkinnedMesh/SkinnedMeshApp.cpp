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
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));	//我们将后台缓冲区的资源状态从只读变更为渲染目标

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
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//然后, 我们可以关闭命令列表, 并执行命令队列
	ThrowIfFailed(mCommandList->Close());
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
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);	//我们在计算阴影时, 需要根据shadowTransform来采样点到光源的深度

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();	//观察点就是相机位置
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
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

	auto curSsaoCB = mCurrFrameResource->SsaoCB.get();	//准备将ssao常量复制到SsaoCB. 其序号同样为0, 注意其序号为0!!!!
	curSsaoCB->CopyData(0, ssaoCB);
}

void SkinnedMeshApp::LoadTextures()
{
	std::vector<std::string> texNames =
	{
		"bricksDiffuseMap",
		"bricksNormalMap",
		"tileDiffuseMap",
		"tileNormalMap",
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap",
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/bricks2.dds",
		L"Textures/bricks2_nmap.dds",
		L"Textures/tile.dds",
		L"Textures/tile_nmap.dds",
		L"Textures/white1x1.dds",
		L"Textures/default_nmap.dds",
		L"Textures/desertcube1024.dds"
	};

	for (UINT i = 0; i < mSkinnedMats.size(); ++i)	//这里非常dirty. 我们将蒙皮的纹理图也加载进来.
	{
		std::string diffuseName = mSkinnedMats[i].DiffuseMapName;	//这里直接存的就是文件名(但是没有路径)
		std::string normalName = mSkinnedMats[i].NormalMapName;

		std::wstring diffuseFileName = L"Textures/" + AnsiToWString(diffuseName);	//根据文件名获取对应的纹理
		std::wstring normalFileName = L"Textures/" + AnsiToWString(normalName);

		diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));	//取消后缀, 将其转为key
		normalName = normalName.substr(0, normalName.find_last_of("."));

		mSkinnedTextureNames.push_back(diffuseName);	//将蒙皮的纹理图/法线图也推进来. 蒙皮的纹理图中也需要记录
		texNames.push_back(diffuseName);
		texFilenames.push_back(diffuseFileName);

		mSkinnedTextureNames.push_back(normalName);
		texNames.push_back(normalName);
		texFilenames.push_back(normalFileName);
	}

	for (int i = 0; i < texNames.size(); ++i)	//逐个加载每个纹理图
	{
		if (mTextures.find(texNames[i]) == std::end(mTextures))	//防止重复加载
		{
			auto texMap = std::make_unique<Texture>();
			texMap->Name = texNames[i];
			texMap->Filename = texFilenames[i];
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));	//我们根据文件名在对应的资源区, 使用对应的上传堆加载纹理图

			mTextures[texMap->Name] = std::move(texMap);
		}
	}
}

void SkinnedMeshApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);	//首个根描述符表为SRV, 其中有3个元素, 从t0开始

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 48, 3, 0);	//第二个根描述符表同样为SRV, 其中有48个元素, 其紧跟着上面的texTable开始(t3)

	CD3DX12_ROOT_PARAMETER slotRootParameter[6];	//创建一个有6个根参数的参数表

	slotRootParameter[0].InitAsConstantBufferView(0);	//其第0个参数初始化为根描述符, 绑定在b0
	slotRootParameter[1].InitAsConstantBufferView(1);	//其第1个参数初始化为根描述符, 绑定在b1
	slotRootParameter[2].InitAsConstantBufferView(2);	//其第2个参数初始化为根描述符, 绑定在b2
	slotRootParameter[3].InitAsShaderResourceView(0, 1);	//其第3个参数初始化为根描述符, 绑定在space1的t0. 这里是ShaderResourceView!!! t0!
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);	//其第4和第5个元素均为一个根描述符表, 均为像素着色器可见
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();	//获取静态采样器们

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);	//创建一个根签名的描述. 其中记录了根参数和静态采样器们, 且允许输入合并阶段, 允许输入布局

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());	//准备序列化根签名. 其版本为VERSION_1, 若成功, 存储到SerializedRooSig; 否则, 将报错信息输入到errorBlob

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));	//构建根签名. 其根据serializedRootSig的描述在mRootSignature的位置创建

}

void SkinnedMeshApp::BuildSsaoRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);	//texTable0的srv有2个, 从t0开始

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);	//texTable的srv有1个, 紧跟上面, 从t2开始

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];	//Ssao的根参数只需要4个
	slotRootParameter[0].InitAsConstantBufferView(0);	//其第一个为根描述符, 绑定在b0
	slotRootParameter[1].InitAsConstants(1, 1);	//第二个为根常量. 常量数量为0, 绑定在b1
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(0,	//绑定在s0
		D3D12_FILTER_MIN_MAG_MIP_POINT,	//点采样
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//uvw均为clamp. 即截断(强行变为最接近的[0, 1]的值)
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(1,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,	//border为不合法的边界值, 使用我们指定的值
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,	//只要深度值比现在的更小/相等, 我们即可采样
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);	//我们指定边缘值为1(即深度无限)

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//wrap为仅仅采样小数部分
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers{
		pointClamp, linearClamp, depthMapSam, linearWrap };	//从这里开始向下均可以参考BuildRootSignature

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));	//构建Ssao所需的根签名
}

void SkinnedMeshApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};	//先创建一个默认清空的描述符堆说明
	srvHeapDesc.NumDescriptors = 64;	//其中的描述符数量为64
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	//其中的描述符类型为CBV/SRV/UAV
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;	//这些说明符在shader中可见
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());	//准备在CPU侧开始创建每个说明符

	std::vector<ComPtr<ID3D12Resource>> tex2DList =	//记录我们需要创建SRV说明符的资源. 理论上, 每个贴图我们都要创建对应的SRV. 这里记录的是未在人物上使用的部分
	{
		mTextures["bricksDiffuseMap"]->Resource,
		mTextures["bricksNormalMap"]->Resource,
		mTextures["tileDiffuseMap"]->Resource,
		mTextures["tileNormalMap"]->Resource,
		mTextures["defaultDiffuseMap"]->Resource,
		mTextures["defaultNormalMap"]->Resource
	};

	mSkinnedSrvHeapStart = (UINT)tex2DList.size();	//人物使用的则在上面的未使用部分之后开始

	for (UINT i = 0; i < (UINT)mSkinnedTextureNames.size(); ++i)
	{
		auto texResource = mTextures[mSkinnedTextureNames[i]]->Resource;
		assert(texResource != nullptr);
		tex2DList.push_back(texResource);	//将人物使用的纹理们也一个个推入tex2DList中
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};	//开始创建每个描述符的描述
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//其RGBA4通道的采样顺序不变
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//其为2D纹理
	srvDesc.Texture2D.MostDetailedMip = 0;	//其最高的Mip层级为0
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;	//不允许采样比0.0更低的层级, 我们将之截断到0.0

	for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)	//遍历所有纹理, 并逐个创建描述符
	{
		srvDesc.Format = tex2DList[i]->GetDesc().Format;	//我们记录其格式与Mip层级数量
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);	//根据tex2DList[i], 以desc作为说明, 在hDescriptor的位置创建SRV

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);	//hDescriptor后移一个
	}

	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;	//找到天空纹理
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;	//准备创建天空纹理. 其视图为一个立方体图
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;	//记录其Mip层级
	srvDesc.Format = skyCubeMap->GetDesc().Format;	//记录其格式
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;	//同样的, 其最高Mip层级为0, 我们要将低于0.0的采样层级截断到0.0
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);	//创建天空盒对应的SRV. 后面不再需要hDescriptor后移了. 因为我们后面换了其它描述符句柄

	mSkyTexHeapIndex = (UINT)tex2DList.size();	//从上面我们也可以看出, 天空盒的纹理资源、天空盒的SRV在普通纹理、动画角色纹理的后面
	mShadowMapHeapIndex = mSkyTexHeapIndex + 1;	//其后面为阴影图堆
	mSsaoHeapIndexStart = mShadowMapHeapIndex + 1;	//阴影后面为Ssao
	mSsaoAmbientMapIndex = mSsaoHeapIndexStart + 3;	//Ssao中有Normal, Depth, randomVector和两张AmbientMap. 这里原作者写错了!!!! AmbientMap在Ssao用的描述符的前面!!!, 应该是+0!
	mNullCubeSrvIndex = mSsaoAmbientMapIndex + 5;	//因为Ssao中有5个描述符
	mNullTexSrvIndex1 = mNullCubeSrvIndex + 1;	//其后后面又跟了两个不使用的Tex的Srv
	mNullTexSrvIndex2 = mNullTexSrvIndex1 + 1;

	auto nullSrv = GetCpuSrv(mNullCubeSrvIndex);	//获取对应nullCube的下标的CPU侧SRV, 并准备创建
	mNullSrv = GetGpuSrv(mNullCubeSrvIndex);	//我们保存的Srv当然是GPU侧的, 因为shader中才需要这个

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);	//因为其为空的, 因此我们不需要传入resource, 硬件自动处理即可. 因为上面最后创建的是天空纹理, 因此我们不需要修改ViewDimension什么的
	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);	//后移句柄. 其后面刚好为两个nullSrv

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//将srvDesc的视图修改回2D
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//随便指定一个格式, 尽量小
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);	//开始创建空的2DSrv
	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);	//继续后移句柄

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);	//继续创建新的空2DSrv

	mShadowMap->BuildDescriptors(GetCpuSrv(mShadowMapHeapIndex), GetGpuSrv(mShadowMapHeapIndex), GetDsv(1));	//通知阴影图管理类创建描述符表

	mSsao->BuildDescriptors(mDepthStencilBuffer.Get(), GetCpuSrv(mSsaoHeapIndexStart), GetGpuSrv(mSsaoHeapIndexStart), GetRtv(SwapChainBufferCount), mCbvSrvUavDescriptorSize, mRtvDescriptorSize);	//通知Ssao管理类创建描述符表
}

void SkinnedMeshApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =	//定义两个宏, 一个是是否开启透明度测试, 一个是是否为蒙皮角色
	{
		"ALPHA_TEST", "1", NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1", NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");	//蒙皮的顶点和标准的顶点着色器中间仅仅是一个宏的区别. 开启该宏后, 我们在顶点着色器阶段需要根据骨骼位移变换
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", skinnedDefines, "VS", "vs_5_1");	//同样的, 蒙皮骨骼的阴影也要根据骨骼的位移而变换阴影的计算. 同样只影响了VS
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");	//如果开启了透明度测试, 则在PS阶段, 我们需要直接将其剔除

	mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["drawNormalsVS"] = d3dUtil::CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedDrawNormalsVS"] = d3dUtil::CompileShader(L"Shaders\\DrawNormals.hlsl", skinnedDefines, "VS", "vs_5_1");	//运动了的蒙皮, 其法线也要跟着变换
	mShaders["drawNormalsPS"] = d3dUtil::CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoVS"] = d3dUtil::CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoPS"] = d3dUtil::CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoBlurVS"] = d3dUtil::CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoBlurPS"] = d3dUtil::CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =	//一个标准的输入布局描述(对应VS)有顶点、法线、纹理、切线
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},	//位置大小为4 * 3
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//法线大小为4 * 3
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//uv大小为4 * 2
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//切线大小为4 * 3
	};

	mSkinnedInputLayout =	//对于一个蒙皮骨骼, 其输入布局描述应当在上面的顶点、法线、纹理、切线外，额外加上影响顶点的骨骼和每个骨骼的权重(由于骨骼不超过4个, 因此权重可以只记录3个, 最后一个为1 - 前三者)
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},	//位置大小为4 * 3
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//法线大小为4 * 3
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//uv大小为4 * 2
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//切线大小为4 * 3
		{"WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//权重大小为4 * 3, 我们可以只记录3个, 第4个由1 - (sum[0..3])得出
		{"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},	//顶点受到哪些骨骼的影响. 由于骨骼数量不会太多, 因此我们只用R8G8B8A8记录即可(256), 大小为1 * 4
	};
}

void SkinnedMeshApp::BuildShapeGeometry()
{
	 GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
    GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    
	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    SubmeshGeometry quadSubmesh;
    quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
    quadSubmesh.StartIndexLocation = quadIndexOffset;
    quadSubmesh.BaseVertexLocation = quadVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() + 
        quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;
	}

    for(int i = 0; i < quad.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = quad.Vertices[i].Position;
        vertices[k].Normal = quad.Vertices[i].Normal;
        vertices[k].TexC = quad.Vertices[i].TexC;
        vertices[k].TangentU = quad.Vertices[i].TangentU;
    }

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
    geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void SkinnedMeshApp::LoadSkinnedModel()
{
	std::vector<M3DLoader::SkinnedVertex> vertices;
	std::vector<std::uint16_t> indices;

	M3DLoader m3dLoader;
	m3dLoader.LoadM3d(mSkinnedModelFilename, vertices, indices, mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);	//让M3D加载类加载对应的蒙皮, 并将submesh, mat, skinnedInfo保存在指定的位置

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();	//创建我们的实际的蒙皮模型的实例
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;	//其蒙皮信息为mSkinnedInfo
	mSkinnedModelInst->TimePos = 0.0f;	//其默认时间戳为0
	mSkinnedModelInst->ClipName = "Take1";	//其动画为Take1. (其实这个模型中也只有这么1个动画)
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());	//我们让最终变换的大小变为bone数量. 因为每个骨骼都有自己的变换

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);	//计算我们的顶点和索引的总大小
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();	//创建对应我们的动画角色的几何
	geo->Name = mSkinnedModelFilename;	//其名称我们用model的文件名来索引

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));	//创建一个blob, 其大小为顶点缓冲区大小, 并将空间绑定到geo的VertexBufferCPU
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);	//将vertices里的数据直接复制到我们刚才创建的Blob中

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);	//对索引缓冲区, 我们同样如此做

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);	//我们根据上面的Blob, 通过Uploader, 在设备上利用命令列表来将资源也上传到GPU中. 首先对顶点缓冲区GPU如此

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);	//对索引缓冲区同样如此, 将CPU侧的资源递交至GPU

	geo->VertexByteStride = sizeof(SkinnedVertex);	//其每个元素的大小为SkinnedVertex的大小
	geo->VertexBufferByteSize = vbByteSize;	//记录其顶点缓冲区总大小
	geo->IndexBufferByteSize = ibByteSize;	//记录其索引缓冲区的总大小
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;	//其索引的格式为R16_UINT, 表示我们不会有超过65535个顶点

	for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
	{
		SubmeshGeometry submesh;
		std::string name = "sm_" + std::to_string(i);	//每个submesh的名字我们用索引标记

		submesh.IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;	//一个面由3个面组成.
		submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
		submesh.BaseVertexLocation = 0;	//这个直接是0. 这是因为我们并没有把角色的顶点和其它顶点合起来!!! LoadM3d里的那个VertexStart本来就是不能加的!

		geo->DrawArgs[name] = submesh;	//将submesh存入hash
	}

	mGeometries[geo->Name] = std::move(geo);
}

void SkinnedMeshApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;	//新建对应不透明物体的PSO说明

	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));	//我们先将该PSO说明清空
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };	//然后我们设置其InputLayout
	opaquePsoDesc.pRootSignature = mRootSignature.Get();	//我们设置其根签名
	opaquePsoDesc.VS =		//我们设置其VS和PS
	{
		reinterpret_cast<BYTE*> (mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*> (mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);	//其栅格化方式为默认的
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);	//其混合方式为默认的
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);	//其深度/模板方式同样为默认的
	opaquePsoDesc.SampleMask = UINT_MAX;	//其采样遮罩为全1. 表示我们全部都是采样的
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	//其拓扑为三角形
	opaquePsoDesc.NumRenderTargets = 1;	//其渲染对象的数量为1
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;	//其第一个渲染对象的格式为我们的后台缓冲区的格式
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;	//若开启了msaa, 则我们的采样数量为4; 否则我们的采样数量为1
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;	//若开启了msaa, 则其质量为我们的质量; 否则为0
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;	//其DSV格式为我们的深度/模板缓冲区格式
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));	//创建对应opaque的PSO

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;	//在不透明物体的基础上, 添加蒙皮不透明物体的PSO
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };	//相比于opaquePsoDesc, 需要调整PSO和VS, PS
	skinnedOpaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	skinnedOpaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));	//新建对应蒙皮动画的PSO

	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;	//新建阴影绘制的pso
	smapPsoDesc.RasterizerState.DepthBias = 100000;	//其光栅化时, 需要添加深度偏移, 从而防止毛刺, 但也不能出现彼得潘现象
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;	//阴影绘制时, 不需要渲染对象, 因此其格式可以直接为UNKNOW
	smapPsoDesc.NumRenderTargets = 0;	//渲染目标的数量也直接为0
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;	//新建蒙皮角色的阴影绘制的PSO
	skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };	//其输入布局描述又变为阴影绘制的了
	skinnedSmapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
		mShaders["skinnedShadowVS"]->GetBufferSize()
	};
	skinnedSmapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow_opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;	//接下来是debug的PSO. debug中, 我们绘制了遮蔽率图
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = opaquePsoDesc;	//绘制法线的pso. 我们在绘制法线时, 渲染对象格式当然是NormalMapFormat
	drawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;	//绘制法线的流水线状态, 在遇到存在蒙皮动画的mesh时, 会需要根据动画变更法线
	skinnedDrawNormalsPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };	//其输入描述布局为蒙皮动画顶点的布局
	skinnedDrawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["skinnedDrawNormalsVS"]->GetBufferSize()
	};
	skinnedDrawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedDrawNormals"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = opaquePsoDesc;	//Ssao的流水线状态说明
	ssaoPsoDesc.InputLayout = { nullptr, 0 };	//ssao不需要输入描述布局
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();	//ssao的根签名是独立的
	ssaoPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;	//在Ssao计算时, 我们不检测深度. 因为我们就是根据之前的深度图尝试在光源的观察空间中还原每个点到光源的距离的
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;	//我们在Ssao计算时, 渲染对象是AmbientMap, 因此格式也要是AmientMap的Format
	ssaoPsoDesc.SampleDesc.Count = 1;	//采样的数量为1, 没有msaa
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;	//深度/模板不需要, 因此其格式自然也不重要
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;	//创建SsaoBlur的PSO
	ssaoBlurPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurVS"]->GetBufferPointer()),
		mShaders["ssaoBlurVS"]->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurPS"]->GetBufferPointer()),
		mShaders["ssaoBlurPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	//天空在任何方向都应该是可以看到的. 我们不能剔除掉所谓的“逆时针”的背面
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;	//不能只有在less的时候才绘制天空. 在equal(即为1)的时候也同样要能绘制
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

void SkinnedMeshApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2, (UINT)mAllRitems.size(), 1, (UINT)mMaterials.size()));	//每个帧资源, 我们都要推入一个新的unique_ptr, 其中的帧资源数量为: pass2, object为allRitems.size(), 蒙皮模型数量为1, mat为materials.size()
	}
}

void SkinnedMeshApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();	//材质的属性包含粗糙度, R0, 材质在缓冲区中的索引, 纹理图, 法线图, 漫反射, 材质名
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->NormalSrvHeapIndex = 1;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->NormalSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->Name = "mirror0";
	mirror0->MatCBIndex = 2;
	mirror0->DiffuseSrvHeapIndex = 4;
	mirror0->NormalSrvHeapIndex = 5;
	mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = 3;
	sky->DiffuseSrvHeapIndex = 6;
	sky->NormalSrvHeapIndex = 7;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks0"] = std::move(bricks0);	//将上面的材质都存入materials
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["mirror0"] = std::move(mirror0);
	mMaterials["sky"] = std::move(sky);

	UINT matCBIndesx = 4;
	UINT srvHeapIndex = mSkinnedSrvHeapStart;	//将蒙皮角色用的材质推入Materials中. 这里之所以是mSkinnedSrvHeapStart, 是因为从这里开始(到蒙皮角色的材质全部读入之前)全都是skinnedMats了!
	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		auto mat = std::make_unique<Material>();
		mat->Name = mSkinnedMats[i].Name;
		mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
		mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
		mat->DiffuseSrvHeapIndex = srvHeapIndex++;	//这里和我们构建srvHeap时候的顺序是严格对应的!!!
		mat->NormalSrvHeapIndex = srvHeapIndex++;
		mat->Roughness = mSkinnedMats[i].Roughness;
		mat->MatCBIndex = matCBIndesx++;	//因为非蒙皮角色只用了4个材质

		mMaterials[mat->Name] = std::move(mat);
	}
}

void SkinnedMeshApp::BuildRenderItems()
{
	auto skyRitem = std::make_unique<RenderItem>();	//我们开始构建每个渲染项
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));	//天空盒尽量大即可
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = 0;	//其对应的objCB为0
	skyRitem->Mat = mMaterials["sky"].get();	//每个渲染项都有自己的材质、网格、拓扑、顶点、索引
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));
    
    auto quadRitem = std::make_unique<RenderItem>();
    quadRitem->World = MathHelper::Identity4x4();
    quadRitem->TexTransform = MathHelper::Identity4x4();
    quadRitem->ObjCBIndex = 1;
    quadRitem->Mat = mMaterials["bricks0"].get();
    quadRitem->Geo = mGeometries["shapeGeo"].get();
    quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
    quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
    mAllRitems.push_back(std::move(quadRitem));
    
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 1.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["bricks0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 3;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
	UINT objCBIndex = 4;
	for(int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["mirror0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["mirror0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

    for(UINT i = 0; i < mSkinnedMats.size(); ++i)
    {
        std::string submeshName = "sm_" + std::to_string(i);	//对于蒙皮角色, 其有数个submesh. 每个我们都需要一个对应的渲染项

        auto ritem = std::make_unique<RenderItem>();

        // Reflect to change coordinate system from the RHS the data was exported out as.
        XMMATRIX modelScale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
        XMMATRIX modelRot = XMMatrixRotationY(MathHelper::Pi);
        XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 0.0f, -5.0f);
        XMStoreFloat4x4(&ritem->World, modelScale*modelRot*modelOffset);

        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = objCBIndex++;
        ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
        ritem->Geo = mGeometries[mSkinnedModelFilename].get();
        ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

        // All render items for this solider.m3d instance share
        // the same skinned model instance.
        ritem->SkinnedCBIndex = 0;		//蒙皮动画角色还有蒙皮常量缓冲区,与蒙皮模型实例
        ritem->SkinnedModelInst = mSkinnedModelInst.get();

        mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
        mAllRitems.push_back(std::move(ritem));
    }
}

void SkinnedMeshApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));	//获取每个物体常量和蒙皮常量缓冲区的大小
	UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();	//获取当前的物体常量缓冲区, 与蒙皮常量缓冲区
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());	//设置当前渲染的物体的顶点缓冲区
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());	//设置当前渲染的物体的索引缓冲区
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;	//获取当前渲染物体对应的物体缓冲区的位置
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);	//设置根参数的0, 其绑定了物体的常量缓冲区

		if (ri->SkinnedModelInst != nullptr)
		{
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;	//计算当前蒙皮模型对应的物体缓冲区的位置!!!
			cmdList->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);	//如果有蒙皮模型, 则将蒙皮模型绑定到根参数1
		}
		else 
		{
			cmdList->SetGraphicsRootConstantBufferView(1, 0);	//否则, 我们将空绑定到根参数1
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);	//绘制调用, 索引数为indexCount, 起始索引为StartIndexLocation, 顶点偏移为BaseVertexLocation, 绘制1次
	}
}

void SkinnedMeshApp::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());	//设置栅格化阶段的视口和裁剪矩形
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));	//将shadowMap的resource从只读改为深度写如

	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);	//我们重置模板/深度视图的值, 深度重置为1, 模板重置为0

	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());	//设置绘制时的dsv为我们的阴影图. 同时, 由于我们不需要最终的图像, 因此直接将RenderTargets的数量设为0, 并传入nullptr

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;	//shadowPass在mainpass的下一个, 因此我们需要偏移过去(偏移一个passConstants大小)
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);	//设置根描述符，对应根参数2, 其为我们的shadowpassCB

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());	//先绘制所有opaque物体	 我们设定了仅仅只有不透明物体才会产生阴影！！！！！
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());	//然后绘制蒙皮的opaque物体
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),	//只有一个barrier
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));	//将shadowMap的resource从深度写入改回只读
}

void SkinnedMeshApp::DrawNormalsAndDepth()
{
	mCommandList->RSSetViewports(1, &mScreenViewport);	//设置视口和裁剪矩形
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto normalMap = mSsao->NormalMap();	//获取normalMap资源 和对应的渲染视口
	auto normalMapRtv = mSsao->NormalMapRtv();

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));	//将normalMap对应的资源的状态从只读变为渲染对象

	float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	mCommandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);	//清空normalMapRtv指向的资源. 其值清空为z为1.  没有不清空的地方
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);	//清空深度/模板缓冲区. 其值清空为深度1.0, 模板0

	mCommandList->OMSetRenderTargets(1, &normalMapRtv, true, &DepthStencilView());	//准备绘制,渲染目标为法线图, 深度为DepthStencilView

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());	//设置根描述符, 对应根参数2, 其直接绑定到passCB上. 因为我们直接在Ssao这里共用了MainPass的CB!

	mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());	//我们需要将不运动的物体和有动画的物体的法线分开绘制
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedDrawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));	//将normalMap对应的资源的状态从渲染对象变为只读
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetCpuSrv(int index) const
{
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetGpuSrv(int index) const
{
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetDsv(int index) const
{
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(index, mDsvDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetRtv(int index) const
{
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(index, mRtvDescriptorSize);
	return rtv;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SkinnedMeshApp::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}
