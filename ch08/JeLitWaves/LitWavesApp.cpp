//LitWavesApp 

#include "../../QuizCommonHeader.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFrameDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

class LitWavesApp : public D3DApp
{
public:
	LitWavesApp(HINSTANCE hInstance);
	LitWavesApp(const LitWavesApp& rhs) = delete;
	LitWavesApp& operator=(const LitWavesApp& rhs) = delete;
	~LitWavesApp();

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
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometryBuffers();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z) const;
	XMFLOAT2 GetHillsNormal(float x, float z) const;
private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	RenderItem* mWavesRitem = nullptr;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.1f;
	float mRadius = 50.0f;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	POINT mLastMousePos;
};

LitWavesApp::LitWavesApp(HINSTANCE hInstance)
{
}

LitWavesApp::~LitWavesApp()
{
}

bool LitWavesApp::Initialize()
{
	return false;
}

void LitWavesApp::OnResize()
{
}

void LitWavesApp::Update(const GameTimer& gt)
{
}

void LitWavesApp::Draw(const GameTimer& gt)
{
}

void LitWavesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void LitWavesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void LitWavesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void LitWavesApp::OnKeyboardInput(const GameTimer& gt)
{
}

void LitWavesApp::UpdateCamera(const GameTimer& gt)
{
}

void LitWavesApp::UpdateObjectCBs(const GameTimer& gt)
{
}

void LitWavesApp::UpdateMaterialCBs(const GameTimer& gt)
{
}

void LitWavesApp::UpdateMainPassCB(const GameTimer& gt)
{
}

void LitWavesApp::UpdateWaves(const GameTimer& gt)
{
}

void LitWavesApp::BuildRootSignature()
{
}

void LitWavesApp::BuildShadersAndInputLayout()
{
}

void LitWavesApp::BuildLandGeometry()
{
}

void LitWavesApp::BuildWavesGeometryBuffers()
{
}

void LitWavesApp::BuildPSOs()
{
}

void LitWavesApp::BuildFrameResources()
{
}

void LitWavesApp::BuildMaterials()
{
}

void LitWavesApp::BuildRenderItems()
{
}

void LitWavesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
}

float LitWavesApp::GetHillsHeight(float x, float z) const
{
	return 0.0f;
}

XMFLOAT2 LitWavesApp::GetHillsNormal(float x, float z) const
{
	return XMFLOAT2();
}
