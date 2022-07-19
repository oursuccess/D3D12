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

	void DefineSkullAniamtion();	//定义骷髅的动画
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
	DefineSkullAniamtion();	//在初始化时构建骷髅动画
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

	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	//获取描述符堆中类型为Cbv/Srv/Uav的描述符的大小

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

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
}

void QuatApp::Draw(const GameTimer& gt)
{
}

void QuatApp::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void QuatApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void QuatApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void QuatApp::OnKeyboardInput(const GameTimer& gt)
{
}

void QuatApp::AnimateMaterials(const GameTimer& gt)
{
}

void QuatApp::UpdateObjectCBs(const GameTimer& gt)
{
}

void QuatApp::UpdateMaterialBuffer(const GameTimer& gt)
{
}

void QuatApp::UpdateMainPassCB(const GameTimer& gt)
{
}

void QuatApp::DefineSkullAniamtion()
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
