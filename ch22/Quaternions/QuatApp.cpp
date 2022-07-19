//QuatApp

#include "../../QuizCommonHeader.h"
#include "FrameResource.h"
#include "AnimationHelper.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;	//帧资源的数量为3. 表示CPU最多可超前GPU2个

struct RenderItem	//渲染项. 渲染项中存放了要绘制一个形状所需要的资源
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMFLOAT4X4 World = MathHelper::Identity4x4();	//将形状从局部空间变换到世界空间的变换矩阵.	这个和下面的Tex在ObjectConstants中也有定义
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();	//对顶点进行统一纹理采样变换的纹理采样变换矩阵

	int NumFramesDirty = gNumFrameResources;	//脏标记. 每个脏标记代表需要更新一个从当前开始的帧资源

	UINT ObjCBIndex = -1;	//形状对应的GPU常量缓冲区中的偏移

	Material* Mat = nullptr;	//渲染项材质
	MeshGeometry* Geo = nullptr;	//渲染项的顶点/索引

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	//其拓扑. 默认为三角形列表

	UINT IndexCount = 0;	//该渲染项对应的顶点数量
	UINT StartIndexLocation = 0;	//该渲染项的起始顶点位置
	int BaseVertexLocation = 0;	//该渲染项对应的顶点起始偏移
};

class QuatApp : public D3DApp
{
public:
	QuatApp(HINSTANCE hInstance);
	QuatApp(const QuatApp& rhs) = delete;
	QuatApp& operator= (const QuatApp &rhs) = delete;
	~QuatApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void DefineSkullAnimation();	//定义骷髅的动画
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;	//存储帧资源们
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;	//当前帧资源在数组中的下标

	UINT mCbvSrvDescriptorSize = 0;	//在Initialize中定义

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;	//根签名

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;	//着色器资源描述符堆

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;	//流水线状态对象们. 为什么有的是ComPtr, 有的是unique_ptr?

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;	//输入布局描述们

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mOpaqueRitems;

	RenderItem* mSkullRitem = nullptr;	//骷髅的渲染项. 之所以将其拿出来，是因为我们的动画就是调整骷髅的世界坐标与旋转的
	XMFLOAT4X4 mSkullWorld = MathHelper::Identity4x4();	//通过世界变换矩阵，我们可以实现骷髅的位移、旋转

	PassConstants mMainPassCB;

	Camera mCamera;

	float mAnimTimePos = 0.0f;
	BoneAnimation mSkullAnimation;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) 
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try 
	{
		QuatApp theApp(hInstance);
		if (!theApp.Initialize()) return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

QuatApp::QuatApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
	DefineSkullAnimation();	//在初始化时构建骷髅动画
}

QuatApp::~QuatApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();	//如果设备不为空，则等待命令队列清空后再退出，防止GPU需要的资源随着APP退出而被提前释放，导致报错
}

bool QuatApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));	//先将命令列表清空. 因为D3DApp中可能也用到了命令列表

	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	//获取描述符堆中类型为Cbv/Srv/Uav的描述符的大小. 然后将之赋值给CbvSrv!, 注意后面没有Uav

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);	//设置相机位置. 我们将其设置到高度为2，屏幕近处15的地方

	LoadTextures();	//加载纹理. 加载纹理只对本地资源有依赖
	BuildRootSignature();	//构建根签名. 根签名中定义了描述符表、使用的采样器并创建了根签名. 对其它项无依赖
	BuildDescriptorHeaps();	//构建描述符堆. 描述符堆中使用了纹理，并借助这些纹理创建对应的SRV. 其依赖于LoadTextures
	BuildShadersAndInputLayout();	//构建Shader和输入布局描述. Shader对本地资源有依赖
	BuildShapeGeometry();	//构建形状并创建对应的索引、顶点缓冲区. 其对其它方法无依赖
	BuildSkullGeometry();	//构建骷髅形状并创建对应的索引、顶点换乘功能区. 其对其它方法无依赖
	BuildMaterials();	//构建材质. 材质中指明了每个材质对应的纹理、法线贴图的偏移，以及各自的漫反射、粗糙度、材质变换矩阵. 该方法依赖于BuildDescriptorHeaps, 因为偏移是在BuildDescriptorHeaps之后才去顶的
	BuildRenderItems();	//构建渲染项. 根据材质和Shape、Skull创建渲染项. 每个渲染项中包含了材质、几何、拓扑、索引数量、起始索引、顶点偏移量、世界变换矩阵、纹理采样矩阵. 该方法依赖于BuildMaterials和BuildShapeGeometry,BuildSkullGeometry
	BuildFrameResources();	//构建帧资源们. 该方法仅仅是创建一下帧资源. 对其它方法无依赖
	BuildPSOs();	//创建流水线状态对象. PSO存在的意义是加快GPU的运算, 在创建PSO之前我们需要将Shader、根签名、输入布局描述都准备好. 其对BuildShadersAndInputLayout,BuildRootSignature有依赖

	ThrowIfFailed(mCommandList->Close());	//在将命令列表中的命令推入命令队列前，我们需要先关闭命令列表
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };	//然后获取命令列表们
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);	//让命令队列执行命令列表. 需要先传入要执行的命令数量

	FlushCommandQueue();	//等待命令队列执行完毕后才是初始化完成

	return true;
}

void QuatApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);	//我们写死了相机的FOV为90, nearZ和farZ分别为1和1000
}

void QuatApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	mAnimTimePos += gt.DeltaTime();
	if (mAnimTimePos >= mSkullAnimation.GetEndTime()) mAnimTimePos = 0.0f;	//我们让动画Loop播放

	mSkullAnimation.Interpolate(mAnimTimePos, mSkullWorld);	//根据时间插值出骷髅当前的世界变换矩阵
	mSkullRitem->World = mSkullWorld;	//更新骷髅的世界矩阵
	mSkullRitem->NumFramesDirty = gNumFrameResources;	//脏标记, 表示我们应当更新骷髅了

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)	//若当前帧资源还没有被GPU处理完(Fence值还没有更新), 则我们要等待GPU释放该帧资源
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));	//当mFence的值变为mCurrFrameResource->Fence时, 触发eventHandle事件
		WaitForSingleObject(eventHandle, INFINITE);	//无限等待eventHandle事件
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);	//然后开始更新材质、常量缓冲区等
	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
}

void QuatApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;	//每个帧资源都有自己的命令分配器，来分配自己当前帧的命令

	ThrowIfFailed(cmdListAlloc->Reset());	//首先我们将命令分配器重置

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));	//然后我们将命令列表重置为使用当前命令分配器，且默认的PSO为opaque

	mCommandList->RSSetViewports(1, &mScreenViewport);	//我们设置视口和裁剪矩形
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));	//我们将当前后台缓冲区的状态从展示改为渲染对象

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);	//将后台缓冲区视图重置为浅蓝色，且清理整个视图
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);	//我们将深度/模板试图清理为深度1(无限远), 模板0, 同样的我们清除整个视图

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());	//我们设置输出合并阶段的渲染对象为当前的后台缓冲区，而将深度/模板渲染到深度/模板视图中

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };	//我们根据Srv描述符的大小来设置命令列表中的描述符堆
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());	//设置根签名

	auto passCB = mCurrFrameResource->PassCB->Resource();	//获取当前帧的Pass常量缓冲区
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());	//将Pass常量缓冲区绑定到根签名中的1的位置, 其为根描述符

	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());	//将当前帧的材质缓冲区绑定到根签名中2的位置，其为根描述符

	mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());	//将Srv描述符堆绑定到根签名中3的位置，其为描述符表

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);	//绘制所有的不透明对象. 在本章例程中，只有不透明对象

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));	//在绘制命令完成后，我们将后台缓冲区重新设置为展示状态

	ThrowIfFailed(mCommandList->Close());	//然后关闭命令列表并执行命令队列

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));	//交换前台和后台缓冲区
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;	//我们将当前帧的围栏值更新为mCurrentFence(这里增加了mCurrentFence)

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);	//通知命令队列，当命令全部执行完成后将Fence的值设置为mCurrentFence
}

void QuatApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);	//在按下鼠标左键移动时，固定屏幕
}

void QuatApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void QuatApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);	//pitch: 绕x旋转. yaw: 绕y旋转, roll: 绕z旋转
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void QuatApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000) 
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

void QuatApp::AnimateMaterials(const GameTimer& gt)
{
}

void QuatApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));	//之所以要转置，是因为dx中是行优先，但是hlsl中是列优先
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);	//更新currObjectCB中索引为objCBIndex的数据，更新为objConstants

			e->NumFramesDirty--;
		}
	}
}

void QuatApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			mat->NumFramesDirty--;
		}
	}
}

void QuatApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void QuatApp::DefineSkullAnimation()
{
}

void QuatApp::LoadTextures()
{
}

void QuatApp::BuildRootSignature()
{
}

void QuatApp::BuildDescriptorHeaps()
{
}

void QuatApp::BuildShadersAndInputLayout()
{
}

void QuatApp::BuildShapeGeometry()
{
}

void QuatApp::BuildSkullGeometry()
{
}

void QuatApp::BuildPSOs()
{
}

void QuatApp::BuildFrameResources()
{
}

void QuatApp::BuildMaterials()
{
}

void QuatApp::BuildRenderItems()
{
}

void QuatApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> QuatApp::GetStaticSamplers()
{
	return std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>();
}
