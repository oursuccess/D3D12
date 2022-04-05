#pragma once
//RenderItem(渲染项)用于描述绘制一个对象所需要的数据集。
//每个App的渲染项可能都是不同的

#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/d3dUtil.h"

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;

	//描述对象的世界空间坐标
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	//第8章中添加。描述材质偏移
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	//描述物体是否为脏数据(需要更新)。我们有几个帧资源，就需要脏数据为几。因为我们需要把每个帧资源都更新了
	int NumFramesDirty = gNumFrameResources;

	//描述指向与该渲染项相关的GPU常量缓冲区中的位置的偏移
	UINT ObjCBIndex = -1;

	//指向该渲染项中渲染的对象的数据
	MeshGeometry* Geo = nullptr;

	//同样为第8章中添加。指向该渲染项对应的材质
	Material* Mat = nullptr;

	//该对象的拓扑结构
	D3D10_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//用于DrawIndexInstanced方法的参数: 
	//该绘制对象的索引数量
	UINT IndexCount = 0;
	//该绘制对象在索引缓冲区中的起始索引
	UINT StartIndexLocation = 0;
	//在本次绘制调用读取顶点之前，为每个索引加上该数值
	int BaseVertexLocation = 0;
};
