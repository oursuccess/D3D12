//copy of StecilApp by Frack Luna, ch11

#include "../../QuizCommonHeader.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;

	//世界矩阵和纹理矩阵
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	//脏标记
	int NumFramesDirty = gNumFrameResources;
	//缓冲区内偏移
	UINT ObjCBIndex = -1;
	//材质和几何
	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	//拓扑方式
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//用于DrawIndexedInstanced方法
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

class Stencil : public D3DApp
{
public:
	Stencil(HINSTANCE hInstance);
	Stencil(const Stencil& rhs) = delete;
	Stencil& operator=(const Stencil& rhs) = delete;
	~Stencil();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	//ch11,这里原来的方法里都是有gt的，我直接删掉了。因为类里本来就有一个GameTiemr成员
	void OnKeyboardInput();
	void UpdateCamera();
	void AnimateMaterials();
	void UpdateObjectCBs();
	void UpdateMaterialCBs();
	void UpdateMainPassCB();
	//ch11, 添加一个阴影的更新方法
	void UpdateReflectedPassCB();

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	//ch11, 构建房间
	void BuildRoomGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	RenderItem* mSkullRitem = nullptr;
	RenderItem* mReflectedSkullRitem = nullptr;
	RenderItem* mShadowedSkullRitem = nullptr;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	PassConstants mMainPassCB;
	PassConstants mReflectedPassCB;

	XMFLOAT3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.24f * XM_PI;
	float mPhi = 0.42f * XM_PI;
	float mRadius = 12.0f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		Stencil stencil(hInstance);
		if (!stencil.Initialize()) return 0;
		return stencil.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR FAILED", MB_OK);
		return 0;
	}
}

Stencil::Stencil(HINSTANCE hInstance)
{
}

Stencil::~Stencil()
{
}

bool Stencil::Initialize()
{
	return false;
}

void Stencil::OnResize()
{
}

void Stencil::Update(const GameTimer& gt)
{
}

void Stencil::Draw(const GameTimer& gt)
{
}

void Stencil::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void Stencil::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void Stencil::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void Stencil::OnKeyboardInput()
{
}

void Stencil::UpdateCamera()
{
}

void Stencil::AnimateMaterials()
{
}

void Stencil::UpdateObjectCBs()
{
}

void Stencil::UpdateMaterialCBs()
{
}

void Stencil::UpdateMainPassCB()
{
}

void Stencil::UpdateReflectedPassCB()
{
}

void Stencil::LoadTextures()
{
}

void Stencil::BuildRootSignature()
{
}

void Stencil::BuildDescriptorHeaps()
{
}

void Stencil::BuildShadersAndInputLayout()
{
}

void Stencil::BuildRoomGeometry()
{
}

void Stencil::BuildSkullGeometry()
{
}

void Stencil::BuildPSOs()
{
}

void Stencil::BuildFrameResources()
{
}

void Stencil::BuildMaterials()
{
}

void Stencil::BuildRenderItems()
{
}

void Stencil::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}
