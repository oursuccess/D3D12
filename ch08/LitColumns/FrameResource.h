#pragma once
//FrameResource��������ÿ֡�������Դ����

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

//ÿ�������ڸ���ʱ��Ҫ�ĳ������ݡ�������������仯ʱ����
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	//��8������ӵ����ݡ�����ƫ��
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

//ÿ֡��Ҫ���µĳ������ݣ����򵥸�������ı�
struct PassConstants
{
	//�۲���󡣽����������ռ�ת�����۲�ռ�
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	//�۲���������󡣽�����ӹ۲�ռ�ת��������ռ�
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	//ͶӰ���󡣽�����ӹ۲�ռ�ת�����ü��ռ�(ͶӰ)
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	//ͶӰ���������󡣽�����Ӳü��ռ�ת���ع۲�ռ�
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	//�۲�ͶӰ���󡣽�����ֱ�Ӵ�����ռ�ת�����ü��ռ�
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	//�۲�ͶӰ���������󡣽�����ֱ�ӴӲü��ռ�ת��������ռ�
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	//�۲��ߵ�λ��(���������ʾ������λ�á������������Ӱӳ���������ɣ������ǿ����õ��ǹ�Դλ��)
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	//����pad��������С���ֽڶ��룬�Ӷ�����c++���Զ�������ƿ��ܵ��µ�CPU--GPU֮����Դλ�ò���Ӧ(ƫ��)����
	float cbPerObjectPad = 0.0f;
	//��ȾĿ�껺������С
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	//��Ⱦ��������С�ĵ���
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	//���ü�ƽ�浽����ľ���
	float NearZ = 0.0f;
	//Զ����ƽ�浽����ľ���
	float FarZ = 0.0f;
	//���е����ڵ���ʱ��
	float TotalTime = 0.0f;
	//��һ֡����һ֡��ʱ��
	float DeltaTime = 0.0f;

	//��8������ӵ�����
	//���������
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	//���Ĺ��������� MaxLights��d3dUtil�ж���
	Light Lights[MaxLights];
};

//��������һ���������������
struct Vertex
{
	//����λ��(ģ�Ϳռ��ڵ�λ��)
	DirectX::XMFLOAT3 Pos;
	//���ߡ�����Ϊ���ܹ���������ͷ
	DirectX::XMFLOAT4 Normal;
};

//�洢��һ֡��CPU���������б�(CommandList)�������Դ
struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//ÿ��֡��Դ����Ҫ�Լ������������������֡�������ݺ����峣������
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	//��8������ӵ����ݡ�ÿ��֡��Դ������һ���Լ��Ĳ��ʳ���
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	//Χ�������ڱ�֤������ʹ�ø�֡��Դʱ������Դһ���Ѿ���GPUʹ������ˡ�
	UINT64 Fence = 0;
};
