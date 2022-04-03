#pragma once
//RenderItem(��Ⱦ��)������������һ����������Ҫ�����ݼ���
//ÿ��App����Ⱦ����ܶ��ǲ�ͬ��

#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/d3dUtil.h"

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;

	//�������������ռ�����
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	//��8������ӡ���������ƫ��
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	//���������Ƿ�Ϊ������(��Ҫ����)�������м���֡��Դ������Ҫ������Ϊ������Ϊ������Ҫ��ÿ��֡��Դ��������
	int NumFramesDirty = gNumFrameResources;

	//����ָ�������Ⱦ����ص�GPU�����������е�λ�õ�ƫ��
	UINT ObjCBIndex = -1;

	//ָ�����Ⱦ������Ⱦ�Ķ��������
	MeshGeometry* Geo = nullptr;

	//ͬ��Ϊ��8������ӡ�ָ�����Ⱦ���Ӧ�Ĳ���
	Material* Mat = nullptr;

	//�ö�������˽ṹ
	D3D10_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//����DrawIndexInstanced�����Ĳ���: 
	//�û��ƶ������������
	UINT IndexCount = 0;
	//�û��ƶ����������������е���ʼ����
	UINT StartIndexLocation = 0;
	//�ڱ��λ��Ƶ��ö�ȡ����֮ǰ��Ϊÿ���������ϸ���ֵ
	int BaseVertexLocation = 0;
};
