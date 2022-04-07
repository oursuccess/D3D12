//copy of Crate by Frank Luna, ch09

#include "../../QuizCommonHeader.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

//渲染项是渲染一个形状所需的资源
struct RenderItem
{
	RenderItem() = default;

	//世界变换矩阵
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	//贴图变换矩阵
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	//标识是否需要更新
	int NumFramesDirty = gNumFrameResources;
	//标识该渲染项在物体常量缓冲区中对应的偏移
	UINT ObjCBIndex = -1;
	//标识该渲染项指向的材质
	Material* Mat = nullptr;
	//标识该渲染项指向的形状
	MeshGeometry* Geo = nullptr;
	//标识该渲染项的拓扑
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//DrawIndexedInstanced所需的3个参数
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocatio = 0;
};

class Crate : public D3DApp
{
public:
	Crate(HINSTANCE hInstance);
	Crate(const Crate& rhs) = delete;
	Crate& operator=(const Crate& rhs) = delete;
	~Crate();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCBs(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	//指向当前的帧资源
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	//用于在缓冲区中找到指定的缓冲区，我们使用该大小乘上偏移的index，即可得到偏移量
	UINT mCbvDrvDescriptorSize = 0;

	//根签名。由于资源的SRV无法作为根描述符被绑定，因此我们必须使用根签名
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	
	//描述符堆。我们用描述符堆来存储资源的SRV
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	//由于我们同时绘制的资源很多，而我们不想为每个资源创建单独的变量，因此我们使用hash将它们存入同样的位置，然后用名称索引
	//形状
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	//材质
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	//贴图
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	//Shader
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	//输入元素的描述
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	//一个正常的渲染的渲染状态对象(PSO)
	ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;

	//所有渲染项的列表，每个都使用指针描述。
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	//直接指向正常PSO对应的渲染项的列表
	std::vector<RenderItem*> mOpaqueRitems;

	//常量缓冲区。在这里当然是只需要一个的。因为我们在build这个之后，将其送入了FrameResource
	PassConstants mMainPassCB;

	//和上面的一样。这里都只需要一个
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.3f * XM_PI;
	float mPhi = 0.4f * XM_PI;
	float mRadius = 2.5f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	//开启运行时内存检查
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		Crate crate(hInstance);
		if (!crate.Initialize()) return 0;
		return crate.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed!", MB_OK);
		return 0;
	}
}

Crate::Crate(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

Crate::~Crate()
{
	//刷新命令队列，避免在退出时还有GPU需要引用的资源，导致退出时崩溃
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool Crate::Initialize()
{
	//初始化，构建必须的各种数据
	if (!D3DApp::Initialize()) return false;

	//重置命令列表，从而为命令列表的初始化做准备
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//计算CbvSrv描述符的大小
	mCbvDrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//贴图加载
	LoadTextures();
	//根签名创建
	BuildRootSignature();
	//描述符堆创建
	BuildDescriptorHeaps();
	//Shader和输入布局描述创建
	BuildShadersAndInputLayout();
	//形状创建
	BuildShapeGeometry();
	//创建材质
	BuildMaterials();
	//创建渲染项
	BuildRenderItems();
	//创建帧资源
	BuildFrameResources();
	//创建流水线状态对象
	BuildPSOs();

	//将命令列表关闭然后推入命令队列
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	//刷新命令队列。这是一个同步方法。
	FlushCommandQueue();

	return true;
}

void Crate::OnResize()
{
	D3DApp::OnResize();

	//重置投影矩阵。因为现在视窗变了
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void Crate::Update(const GameTimer& gt)
{
	//每帧更新时，我们需要响应玩家输入、尝试更新相机，然后，步进到下一个要处理的帧资源，在该帧资源的基础上更新材质、物体常量缓冲区、材质缓冲区和帧缓冲区
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	//设置围栏值
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		//当GPU完成时，将围栏值(mFence->GetCompleteValue())更新为mCurrFrameResource->Fence
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		//如果GPU还没有处理完这一帧，则直接等到，且等待无限时间！！！
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//更新缓冲区
	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCBs(gt);
}

void Crate::Draw(const GameTimer& gt)
{
}

void Crate::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void Crate::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void Crate::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void Crate::OnKeyboardInput(const GameTimer& gt)
{
}

void Crate::UpdateCamera(const GameTimer& gt)
{
}

void Crate::AnimateMaterials(const GameTimer& gt)
{
}

void Crate::UpdateObjectCBs(const GameTimer& gt)
{
}

void Crate::UpdateMaterialCBs(const GameTimer& gt)
{
}

void Crate::UpdateMainPassCBs(const GameTimer& gt)
{
}

void Crate::LoadTextures()
{
}

void Crate::BuildRootSignature()
{
}

void Crate::BuildDescriptorHeaps()
{
}

void Crate::BuildShadersAndInputLayout()
{
}

void Crate::BuildShapeGeometry()
{
}

void Crate::BuildPSOs()
{
}

void Crate::BuildFrameResources()
{
}

void Crate::BuildMaterials()
{
}

void Crate::BuildRenderItems()
{
}

void Crate::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Crate::GetStaticSamplers()
{
	return std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>();
}
