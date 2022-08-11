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

class SkinnedMeshApp : public D3DApp
{
public:
	SkinnedMeshApp(HINSTANCE hInstance);
	SkinnedMeshApp(const SkinnedMeshApp& rhs) = delete;
	SkinnedMeshApp& operator=(const SkinnedMeshApp& rhs) = delete;
	~SkinnedMeshApp();

	virtual bool Initialize() override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildSsaoRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void LoadSkinnedModel();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawSceneToShadowMap();
	void DrawNormalsAndDepth();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
};
