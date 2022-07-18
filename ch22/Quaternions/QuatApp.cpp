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
	void AniamateMaterials(const GameTimer& gt);
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
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
};
