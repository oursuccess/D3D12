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

//FIXME: 22章重头戏
void QuatApp::DefineSkullAnimation()
{
	XMVECTOR q0 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(30.0f));	//绕着y轴顺时针旋转30度
	XMVECTOR q1 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 1.0f, 2.0f, 0.0f), XMConvertToRadians(45.0f));	//绕着一个斜向轴旋转45度
	XMVECTOR q2 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(-30.0f));	//绕着y轴逆时针旋转30度
	XMVECTOR q3 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(70.0f));	//绕着x轴顺时针旋转70度

	mSkullAnimation.Keyframes.resize(5);
	mSkullAnimation.Keyframes[0].TimePos = 0.0f;	//第0秒
	mSkullAnimation.Keyframes[0].Translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);	//其位移为x轴-7
	mSkullAnimation.Keyframes[0].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);	//其尺寸为0.25
	XMStoreFloat4(&mSkullAnimation.Keyframes[0].RotationQuat, q0);

	mSkullAnimation.Keyframes[1].TimePos = 2.0f;	//第2秒
	mSkullAnimation.Keyframes[1].Translation = XMFLOAT3(0.0f, 2.0f, 10.0f);	//其位移为y的2.0f, z的10.0f
	mSkullAnimation.Keyframes[1].Scale = XMFLOAT3(0.5f, 0.5f, 0.5f);	//其尺寸变为0.5
	XMStoreFloat4(&mSkullAnimation.Keyframes[1].RotationQuat, q1);	//其绕着一个斜向轴旋转45度

	mSkullAnimation.Keyframes[2].TimePos = 4.0f;	//第4秒
	mSkullAnimation.Keyframes[2].Translation = XMFLOAT3(7.0f, 0.0f, 0.0f);	//现在x轴为7了
	mSkullAnimation.Keyframes[2].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);	//其尺寸变为0.25
	XMStoreFloat4(&mSkullAnimation.Keyframes[2].RotationQuat, q2);

	mSkullAnimation.Keyframes[3].TimePos = 6.0f;
	mSkullAnimation.Keyframes[3].Translation = XMFLOAT3(0.0f, 1.0f, -10.0f);	//其现在和第2秒的差不多相对, 但是y少了1
	mSkullAnimation.Keyframes[3].Scale = XMFLOAT3(0.5f, 0.25f, 0.25f);
	XMStoreFloat4(&mSkullAnimation.Keyframes[3].RotationQuat, q3);

	mSkullAnimation.Keyframes[4].TimePos = 8.0f;	//第8秒。 我们通过这种方式实现动画的完全循环播放
	mSkullAnimation.Keyframes[4].Translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);	//其位移为x轴-7
	mSkullAnimation.Keyframes[4].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);	//其尺寸为0.25
	XMStoreFloat4(&mSkullAnimation.Keyframes[4].RotationQuat, q0);
}


void QuatApp::LoadTextures()	//这里原例程逐个创建了资源. 为了通用性，我们改用循环
{
	std::unordered_map<std::string, std::wstring> textureResources{
		{"bricksTex", L"Textures/bricks2.dds"},
		{"stoneTex", L"Textures/stone.dds"},
		{"tileTex", L"Textures/tile.dds"},
		{"crateTex", L"Textures/WoodCrate01.dds"},
		{"defaultTex", L"Textures/white1x1.dds"},
	};

	for (const auto& [texName, fileName] : textureResources) 
	{
		auto tex = std::make_unique<Texture>();
		tex->Name = texName;
		tex->Filename = fileName;
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), fileName.c_str(), tex->Resource, tex->UploadHeap));
		mTextures[texName] = std::move(tex);
	}
}

void QuatApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0);	//初始化该描述符表. 其类型为SRV, 其中有5个元素，从描述符堆的偏移0处开始, 绑定的空间为0. 其用于存储我们的贴图. 对应t0

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];	//创建一个有4个参数的根参数

	slotRootParameter[0].InitAsConstantBufferView(0);	//我们将首元素初始化为根描述符. 其中为常量缓冲区，绑定在空间0的偏移为0的位置，对应b0. 在实际应用中，我们将物体的常量缓冲区绑定于此
	slotRootParameter[1].InitAsConstantBufferView(1);	//我们将下标1初始化为根描述符，其中为常量缓冲区，绑定在空间0的偏移为1的位置，对应b1. 在实际应用中，我们将Pass的常量缓冲区绑定于此
	slotRootParameter[2].InitAsShaderResourceView(0, 1);	//我们将下标2初始化为根描述符, 其中为着色器资源视图, 绑定在空间1的偏移为0的位置, 对应(t0, space1). 在实际应用中，我们将材质数据绑定于此. 为什么命名是数据，我们却要将其绑定为SRV?	--FIXME:
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);	//我们将下标3初始化为描述符表, 其仅有一个描述符,中为我们在上面声明的包含了5个SRV的描述符, 绑定在t0. 在实际应用中, 我们将贴图数据绑定于此. 之所以这里用描述符表, 是因为SRV不能直接作为根描述符绑定

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(),
		staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);	//根据指定的根描述符、静态采样器创建根签名描述. 我们将该根签名描述设置为允许输入阶段的组合，以及输入布局描述

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());	//尝试根据根签名描述创建一个序列化的根签名. 若成功，填充serializedRootSig, 否则填充errorBlob

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));	//根据序列化的根签名将根签名实际绑定到设备上, 并以mRootSignature作为其句柄.
}

void QuatApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};	//创建描述符堆描述
	srvHeapDesc.NumDescriptors = 5;	//该堆中的描述符数量为5
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	//该堆中的类型为CBV/SRV/UAV
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;	//该堆的Flag为允许Shader的可见性
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));	//尝试根据描述符堆描述在mSrvDescriptorHeap处创建描述符堆

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());	//先获取该描述符堆的起始位置，准备一个个一次创建描述符

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};	//创建一个默认的着色器资源视图描述
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//其采样顺序不变, 依然为argb
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//其格式均为Texture2D. 这在有天空盒的情况下是无法生效的
	srvDesc.Texture2D.MostDetailedMip = 0;	//其最细节的Mip层级为0
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;	//我们允许的最低采样Mip层级为0.0f

	//这里的材质需要严格有序, 因此我们必须使用数组
	auto textureNames = std::vector<std::string>{
		"bricksTex", "stoneTex", "tileTex", "crateTex", "defaultTex",
	};
	for (const auto& name : textureNames)
	{
		auto tex = mTextures[name]->Resource;	//获取对应名称的材质
		srvDesc.Format = tex->GetDesc().Format;	//我们根据材质来设置纹理的格式、Mip层级数量
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);	//根据纹理和着色器描述，在当前的描述符句柄上构建ShaderResourceView
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
}

void QuatApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"ALPHA_TEST", "1", NULL, NULL
	};	//定义一下宏, 我们的宏里定义了ALPHA_TEST这一个宏

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");	//读取Default.hlsl文件中的VS方法，使用vs_5_1标准, 不使用宏，将其编译为shader，并存入mShaders, 键值为standardVS
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {	//输入布局描述. 我们在输入-合并阶段传入顶点的变量有POSITION, NORMAL, TEXCOORD. 这些变量在standardVS中使用. 这些变量被传入了顶点缓冲区中!!!
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void QuatApp::BuildShapeGeometry()	//直接复制粘贴
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

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

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

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

	mGeometries[geo->Name] = std::move(geo);
}

void QuatApp::BuildSkullGeometry()	//复制粘贴
{
	std::ifstream fin("Models/skull.txt");

    if(!fin)
    {
        MessageBox(0, L"Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    std::vector<Vertex> vertices(vcount);
    for(UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

        // Project point onto unit sphere and generate spherical texture coordinates.
        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        float theta = atan2f(spherePos.z, spherePos.x);

        // Put in [0, 2pi].
        if(theta < 0.0f)
            theta += XM_2PI;

        float phi = acosf(spherePos.y);

        float u = theta / (2.0f*XM_PI);
        float v = phi / XM_PI;

        vertices[i].TexC = { u, v };

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }

    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(3 * tcount);
    for(UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    //
    // Pack the indices of all the meshes into one index buffer.
    //

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";

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
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.Bounds = bounds;

    geo->DrawArgs["skull"] = submesh;

    mGeometries[geo->Name] = std::move(geo);
}

void QuatApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;	//创建一个流水线状态描述

	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));	//清空流水线状态描述
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };	//其输入布局描述为我们在上面构建的输入布局描述
	opaquePsoDesc.pRootSignature = mRootSignature.Get();	//其根签名同样在前面定义. 其中有4个根参数, 6个静态采样器
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE*> (mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*> (mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);	//其光栅化状态为默认的即可
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);	//其混合状态同样为默认的即可
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);	//其深度/模板状态同样为默认的即可
	opaquePsoDesc.SampleMask = UINT_MAX;	//采样遮罩为最大. 表示均为1. 我们不遮罩任何一个通道
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	//其基础拓扑结构为三角形
	opaquePsoDesc.NumRenderTargets = 1;	//其渲染对象为1
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;	//其渲染对象的格式为我们定义的后台缓冲区格式
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;	//其采样数量, 在打开了Msaa时为4, 否则为1
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;	//若打开了Msaa, 则质量为Msaa的质量-1; 否则为0(最大)
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));	//在设备上创建用于不透明物体的流水线状态对象
}

void QuatApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)	//每有一个帧资源，我们就推入一个
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1,
			(UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
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
