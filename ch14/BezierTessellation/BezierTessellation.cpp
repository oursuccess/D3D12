//copy of BasicTessellation by Frank Luna, ch14

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

	//世界矩阵
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	//材质贴图矩阵
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	//要更新的数量
	int NumFramesDirty = gNumFrameResources;
	//该渲染项对应的物体
	UINT objCBIndex = -1;
	//该渲染项对应的材质
	Material* Mat = nullptr;
	//该渲染项对应的几何
	MeshGeometry* Geo = nullptr;
	//默认的拓扑
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//DrawIndexedInstanced方法所需要的参数: 顶点数量、顶点偏移、顶点基准值
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

//Tessllation类
class BezierTessellation : public D3DApp
{
public:
	BezierTessellation(HINSTANCE hInstance);	//构造函数
	BezierTessellation(const BezierTessellation& rhs) = delete;	//我们不要拷贝构造和拷贝复制函数
	BezierTessellation& operator=(const BezierTessellation& rhs) = delete;	//我们不要拷贝复制函数
	~BezierTessellation();

	virtual bool Initialize() override;	//初始化

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;	
	virtual void Draw(const GameTimer& gt) override;	//当绘制时调用

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShaderAndInputLayout();
	void BuildQuadPatchGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*> ritems);

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

BezierTessellation::BezierTessellation(HINSTANCE hInstance)
{
}

BezierTessellation::~BezierTessellation()
{
}

bool BezierTessellation::Initialize()
{
	return false;
}

void BezierTessellation::OnResize()
{
}

void BezierTessellation::Update(const GameTimer& gt)
{
}

void BezierTessellation::Draw(const GameTimer& gt)
{
}

void BezierTessellation::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void BezierTessellation::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void BezierTessellation::OnMouseMove(WPARAM btnState, int x, int y)
{
}
