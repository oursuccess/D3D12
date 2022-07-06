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
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;		//获取当前帧的命令分配器

	ThrowIfFailed(cmdListAlloc->Reset());	//重置当前帧的命令分配器

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));	//将命令列表重置为以当前帧的命令分配器分配，且初始的流水线状态为Opaque

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };	//获取当前的着色器资源描述符堆
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);	//以当前的着色器资源描述符堆设置命令列表中的描述符堆

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());	//将当前根签名设置为默认根签名

	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();	//获取当前帧持有的材质缓冲区的资源们
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());	//以材质资源来设置根签名中的第2个位置(该位置为一个着色器资源的根描述符,对应了一个结构化的纹理资源的数组). 需要注意的是我们传入的是一个缓冲区中的位置，而非实际的着色器资源

	mCommandList->SetGraphicsRootDescriptorTable(3, mNullSrv);	//以nullSrv来设置shadowMap帧的根签名中的第3个位置(该位置为描述符表). 需要注意的是，我们不需要传入一个table/array, 也无需传入数组长度， 根签名知道该table中需要多少描述符

	mCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//以实际的着色器资源视图来设置根签名的第4个位置(该位置为描述符表)

	DrawSceneToShadowMap();	//将场景绘制到引用图中

	DrawNormalsAndDepth();	//绘制法线和深度图

	mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());	//将根签名更改为Ssao
	mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 3);	//在当前帧中以特定的blur值计算Ssao

	//从现在开始进入main pass
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());	//再次使用当前材质缓冲区的位置重设根签名的第2个元素(着色器资源的根描述符)

	mCommandList->RSSetViewports(1, &mScreenViewport);	//设置视图窗口. 只有1个视图. RS代表Rasterizer Stage, 光栅化阶段. 因为只有这个阶段才正式渲染到视口上
	mCommandList->RSSetScissorRects(1, &mScissorRect);	//设置裁剪矩形

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), 
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));	//将当前的后台缓冲区设置为渲染对象

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);	//以浅绿色、没有裁剪矩形的状态重置后台缓冲区视图

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());	//我们设置输出-装配阶段的渲染目标和深度/模板视图资源.深度/模板视图由GPU使用, 而在这之前我们即可计算得出

	mCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//同样的，以实际的着色器资源视图来设置根签名的第4个位置(描述符表)

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());	//以当前帧的帧常量缓冲区作为根描述符

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//开始准备绑定天空盒. 首先需要获取天空盒的描述符
	skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvUavDescriptorSize);	//该描述符的位置应当在GPU的描述符起始位置的基础上向后移动mSkyHeapIndex * mCbvSrvUavDescriptorSize
	mCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);	//以天空盒纹理来设置根签名的第3个位置(该位置为一个描述符表，里面有6个Srv)

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());	//依次绘制Opaque，Debug，Sky。 首先将流水线状态设置为opaque
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);	//绘制. 后面个的两个类似

	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Debug]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));	//将后台缓冲区从渲染对象修改为准备好显示的状态

	ThrowIfFailed(mCommandList->Close());	//将命令列表关闭，并准备根据此创建命令队列

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);	//根据命令列表创建命令队列

	ThrowIfFailed(mSwapChain->Present(0, 0));	//交换当前的前台和后台缓冲区
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;	//设置新的Fence值. 用来保证CPU不会覆盖GPU尚未处理的帧资源
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);	//通知队列，当完成后将Fence值设为mCurrentFence
}

void SsaoApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);	//将画面定格为鼠标按下前
}

void SsaoApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();	//重新释放画面
}

void SsaoApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)	//若按下的是鼠标左键
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));	//根据当前移动的x值，将其转为弧度. 我们假定从当前屏幕移动的范围为-90 ~ 90
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));	//根据当前移动的y值，将其转为弧度. 我们假定从当前屏幕移动的范围同样为-90 ~ 90

		mCamera.Pitch(dy);	//让相机绕着x轴旋转dy
		mCamera.RotateY(dx);	//让相机绕着y周旋转dx(yaw)
	}

	mLastMousePos.x = x;	//更新上次按下鼠标的位置
	mLastMousePos.y = y;
}

void SsaoApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)	//相机的上、下、左、右移动
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();	//在相机位置变化时，更新其观察矩阵
}

void SsaoApp::AnimateMaterials(const GameTimer& gt)
{
}

void SsaoApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();	//获取当前所有的物体
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)	//我们仅仅更新那些被打上了脏标记(即需要更新的物体)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);	//获取其世界矩阵
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);	//获取其纹理采样矩阵

			ObjectConstants objConstants;
			//dx中，XMMATRIX为row-major, hlsl默认为column-major:
			//https://docs.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmloadfloat4x4
			//https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-per-component-math
			//相比之下, opengl和cg中默认均为column-major, 因为opengl期望顶点/矢量在矩阵乘法的右边, 被转为列矩阵
			//总结即为, 只有dx中为row-major, 其它均为column-major
			//我们需要在从dx传入hlsl时进行一次转置
			//https://stackoverflow.com/questions/31504378/why-do-we-need-to-use-the-transpose-of-a-transformed-matrix-direct3d11
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));	//我们将世界矩阵的转置矩阵存入objConstants. 在dx的矩阵变换中，矩阵为按行摆放的，然后顶点/向量在左边，这里之所以转置，是因为hlsl默认是column-major, 而XMMATRIX则默认是row-major!!!
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));	//同样的，存入objConstants的纹理采样矩阵的也是其纹理采样矩阵的转置矩阵
			objConstants.MaterialIndex = e->Mat->MatCBIndex;	//其材质下标和原本的下标相同

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);	//将新的objConstants存入原本对应e的ObjCBIndex的常量缓冲区的位置中

			e->NumFramesDirty--;	//减少其脏标记值。 这里脏标记初始值为gNumFrameResources
		}
	}
}

void SsaoApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)	//更新材质们
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)	//材质也同样只有在需要更新时才计算
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);	//MatTransfomr之前难道是列优先的么?

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;	//我们记录材质的漫反射颜色、R0、粗糙度、其法线贴图位置、漫反射贴图位置，以及其材质偏移矩阵
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);	//同样，将新的材质数据存储doa原本对应e的MatCBIndex的材质缓冲区中的位置

			mat->NumFramesDirty--;
		}
	}
}

void SsaoApp::UpdateShadowTransform(const GameTimer& gt)
{
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);	//我们只为主光源(第一个光源)产生阴影
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;	//我们让光源生成在天空盒外，且其为平行光. 因此让其在光的方向的基础上，乘以世界包围球的直径
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);	//而光源的目标位置则为包围球的中心点
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);	//创建一个上向量
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);	//以从lightPos看向targetPos，上方向为lightUp，创建左手坐标系下的光线的观察矩阵

	XMStoreFloat3(&mLightPosW, lightPos);	//存储光线的世界位置

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));	//将包围球的中心点变换到光源的观察空间中. XMVector3TransformCoord为将顶点按照指定矩阵进行变换

	float l = sphereCenterLS.x - mSceneBounds.Radius;	//在光源观察空间下，计算新的包围球. 我们假设了观察空间不会缩放.
	float b = sphereCenterLS.y - mSceneBounds.Radius;	//l:left, r:right, n:near, f:far, b:bottom, t:top
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;	//更新光源的nearZ和farZ
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);	//根据l,r,b,t,n,f来创建投影的平截头体

	XMMATRIX T{	//创建将NDC空间坐标[-1, 1]变换到纹理采样坐标[0, 1]的矩阵. 这里的矩阵仅仅影响了x,y. 同时，其为行优先, 且由于dx的设计，ndc纵坐标的0点为上方, 而纹理纵坐标的0点为下方
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f,
	};

	XMMATRIX S = lightView * lightProj * T;	//联立获得从世界空间经过光源的观察、光源投影、NDC到采样后的光源下世界坐标的纹理采样矩阵. 此即为阴影采样矩阵!
	XMStoreFloat4x4(&mLightView, lightView);	//将光源观察矩阵、光源投影矩阵、阴影变换矩阵分别存入对应位置
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void SsaoApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();	//从相机中获取当前的观察矩阵和投影矩阵
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);	//view和proj叉乘即为观察投影矩阵
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);	//获取view的逆矩阵. determinant用来辅助逆矩阵的运算
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);	//获取投影矩阵的逆矩阵
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);	//获取观察投影矩阵的逆矩阵

	XMMATRIX T{	//同样的，将NDC空间坐标[-1, 1]变换到纹理采样矩阵[0, 1]的矩阵. 仅影响x,y，且为行优先. 由于dx的设计，ndc纵坐标的原点在上方, 而纹理纵坐标的原点为下方
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	};

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);	//获取观察投影采样矩阵，用于直接获取世界坐标对应的纹理采样坐标
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);	//获取光源空间下从世界坐标直接变换到世界坐标对应的阴影图纹理采样坐标的变换矩阵

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));	//依次将观察、投影、观察投影、逆观察、逆投影、逆观察投影、观察投影采样、阴影采样矩阵存入主帧的常量缓冲区中. 由于DX和hlsl分别为行优先和列优先，因此需要转置
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();	//获取观察点(即相机位置)
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);	//设置渲染目标的大小. 其应当刚好为视口的大小
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);	//设置渲染目标大小的倒数
	mMainPassCB.NearZ = 1.0f;	//设置 主帧的NearZ和FarZ，分别为定值1和1000
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();	//计算总时间和dt
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.4f, 0.4f, 0.6f, 1.0f };	//设置环境光
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];	//设置三点布光系统中三个光源的方向和光强
	mMainPassCB.Lights[0].Strength = { 0.4f, 0.4f, 0.5f };	//这里，其对蓝色的贡献比其他的大
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.1f, 0.1f, 0.1f };	//辅光的贡献已经很小了
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.0f, 0.0f, 0.0f };	//补光的贡献为0

	auto currPassCB = mCurrFrameResource->PassCB.get();	//将我们的帧常量缓冲区中内容拷贝到当前帧的帧常量缓冲区中
	currPassCB->CopyData(0, mMainPassCB);
}

void SsaoApp::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);	//获取光源的观察矩阵
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);	//获取光源的投影矩阵

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);	//同样的，计算观察投影矩阵、逆观察矩阵、逆投影矩阵、逆观察投影矩阵
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();	//获取阴影图的宽高
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));	//同样的，将用于阴影计算的观察、逆观察、投影、逆投影、观察投影、逆观察投影矩阵存入ShadowPass的常量缓冲区中
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;	//阴影渲染时，观察点为光源点
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);	//该阴影渲染对象的大小为阴影图的大小
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);	//其渲染对象的大小的逆倒数同样根据阴影图计算
	mShadowPassCB.NearZ = mLightNearZ;	//其NearZ和FarZ为光源的NearZ和FarZ
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();	//同样获取当前帧的常量缓冲区
	currPassCB->CopyData(1, mShadowPassCB);	//注意这里复制到的下标位置为1，而不是MainPass对应的0!	而currFrameResourceCB的长度为2. 可参见BuildFrameResources方法
}

void SsaoApp::UpdateSsaoCB(const GameTimer& gt)
{
	SsaoConstants ssaoCB;	//创建ssaoCB
	
	XMMATRIX P = mCamera.GetProj();	//获取相机的投影矩阵

	XMMATRIX T{	//将坐标从[-1, 1]的NDC空间变换到[0, 1]的纹理采样坐标的变换矩阵
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f,
	};

	ssaoCB.Proj = mMainPassCB.Proj;	//从mainPassCB中复制投影和逆投影矩阵
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));	//投影采样矩阵，则可以由相机的投影矩阵和采样矩阵联立获得. 因为我们只需要将坐标变换到相机的观察空间中. 无需真的变换回世界空间. 我们判断一个点是否被其它点挡住，肯定是要在观察空间里！

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);	//从Ssao中获取一个点计算遮蔽率时的偏移向量们

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);	//根据权重计算模糊的权重们. 传入2.5f时，我们计算的是长度为11的高斯模糊表
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);	//将权重们取出. 0为最左边的一个权重. 为什么是这三个?
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);	//4对应中间偏左的权重
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);	//8对应的是中间偏右的权重

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());	//渲染目标的倒数同样需要用1除

	ssaoCB.OcclusionRadius = 0.5f;	//我们判断是否可能被遮挡时随机采样的最大距离
	ssaoCB.OcclusionFadeStart = 0.2f;	//低于该距离，我们认为完全遮蔽
	ssaoCB.OcclusionFadeEnd = 1.0f;	//高于此距离，我们认为无法遮蔽. 在Start到End中，遮蔽程度随着距离变化
	ssaoCB.SurfaceEpsilon = 0.05f;	//低于此距离，我们认为在同一平面，因此不可能被遮蔽

	auto currSsaoCB = mCurrFrameResource->SsaoCB.get();	//这里获取的是SsaoCB, 而非PassCB
	currSsaoCB->CopyData(0, ssaoCB);
}

void SsaoApp::LoadTextures()
{
	std::vector<std::string> texNames{	//设置贴图的名字. 我们需要砖、地板、默认的白色，以及天空盒. 其中天空不需要法线贴图
		"bricksDiffuseMap",	//diffuse, 自发光
		"bricksNormalMap",	//normal, 法线
		"tileDiffuseMap",
		"tileNormalMap",
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap",
	};

	std::vector<std::wstring> texFilenames{	//按照上面设置的贴图名字，我们依次设置对应的纹理的路径. 使用的是相对路径
		L"Textures/bricks2.dds",
		L"Textures/bricks2_nmap.dds",
		L"Textures/tile.dds",
		L"Textures/tile_nmap.dds",
		L"Textures/white1x1.dds",
		L"Textures/default_nmap.dds",
		L"Textures/sunsetcube1024.dds",
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<Texture>();	//为每个我们声明的贴图申请一个Texture空间
		texMap->Name = texNames[i];	//指定该tex的名字和文件名
		texMap->Filename = texFilenames[i];
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),	//然后，根据该文件名加载DDS贴图
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		mTextures[texMap->Name] = std::move(texMap);	//然后，将该Texture存入mTextures中
	}
}

void SsaoApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;	//声明第一个纹理描述符. 其存储着色器资源视图，描述符数量为3，其基准的shader寄存器为0，绑定空间为0. 即对应t0 - t2.
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;	//声明第二个纹理描述符. 其同样存储着色器资源，描述符数量为10. 其基准的shader寄存器刚好在上面的后面，即从3开始. 绑定的空间同样为1. 
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 3, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[5];	//创建五个根参数

	slotRootParameter[0].InitAsConstantBufferView(0);	//第一个为根描述符, 其为常量缓冲区视图. 对应的寄存器为0, 即b0
	slotRootParameter[1].InitAsConstantBufferView(1);	//第二个同样为根描述符, 同样为常量缓冲区视图,对应的寄存器为1, 即b1
	slotRootParameter[2].InitAsShaderResourceView(0, 1);	//第三个同样为根描述符, 但是为着色器资源视图. 起对应的寄存器为空间1的0, 即t0, 但是在space1
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);	//第四个为描述符表. 其中的描述符为1个. 其可见性为像素着色器
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);	//第五个同样为描述符表, 其中的描述符也是1个

	auto staticSamplers = GetStaticSamplers();	//获取静态采样器们

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);	//根据现在的根参数、静态采样器们创建一个根签名描述. 该根签名还允许输入装配阶段的输入布局描述

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());	//我们根据上面的根签名描述来序列化根签名.并将其序列化到serializedRootSig处 若出错，则将结果保存到errorBlob中

	if (errorBlob != nullptr)	//如果出错, 则errorBlob将不为空, 此时我们将errorBlob中的保存信息打印出来
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),	//第一个参数指定了GPU设备. 单GPU系统中，我们将参数尚未0即可
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));	//否则，我们根据序列化好的根签名来在mRootSignature处创建根签名
}

void SsaoApp::BuildSsaoRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;	//为ssao创建描述符表. 其为着色器资源视图, 其中的描述符数量为2. 绑定在t0 - t1
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;	//创建第二个描述符表, 其同样为着色器资源视图, 其中的描述符数量为1, 绑定在t2
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];	//创建根参数. ssao需要的根参数为4

	slotRootParameter[0].InitAsConstantBufferView(0);	//第一个参数被初始化为根描述符. 其绑定在b0. 在Ssao中，其用于传入ssao所需的常量
	slotRootParameter[1].InitAsConstants(1, 1);	//第二个参数直接被初始化为根常量们. 其绑定在b1, 而根常量数量为1. 在Ssao中，其用于传入bool值，记录blur时是按行还是按列
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);	//将第三和第四个都初始化为描述符表，其中的描述符范围数量均为1个. 第三个中记录了法线和深度贴图
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);	//第四个描述符表中记录了随机访问向量贴图

	//从现在开始到下面，创建4个采样器
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp{ 0,	//第一个，绑定在s0上, 其过滤模式为点过滤，同时在u、v、w上都采取截断方式
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//Clamp模式将会在超出[0, 1]范围时，将对应点绘制为最接近的[0, 1]范围对应的颜色
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp{ 1,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	};

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam{ 2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,	//Border模式将会在超出[0, 1]范围时，将颜色绘制为指定的颜色
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f, 0, D3D12_COMPARISON_FUNC_LESS_EQUAL,	//mipmap层级不变，最大各向异性值为0(在LINEAR时该选项无效), 我们需要的是后面的比较方法，因为我们只绘制深度值最小(最接近相机)的顶点
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,	//超出边界的部分被绘制为白色
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap{ 3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//当超出[0, 1]范围时，我们只取其小数部分
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	};

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers{
		pointClamp, linearClamp, depthMapSam, linearWrap,
	};

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,	//我们准备创建ssao所需的根签名。 步骤和创建正常根签名类似. 同样要指定根参数数量与根参数， 静态采样器数量与静态采样器，以及最后的Flag
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
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
