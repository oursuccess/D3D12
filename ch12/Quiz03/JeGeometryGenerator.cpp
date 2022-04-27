#include "JeGeometryGenerator.h"

using namespace DirectX;

GeometryGenerator::MeshData JeGeometryGenerator::CreateGeosphere20Face(float radius)
{
	GeometryGenerator::MeshData meshData;

	const float X = 0.525731f;
	const float Z = 0.850651f;
	//赋值二十面体的12个顶点坐标
	XMFLOAT3 pos[12] =
	{
		XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),
		XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),
		XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X),
		XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),
		XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f),
		XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
	};
	//二十面体的图元索引
	GeometryGenerator::uint32 k[60] =
	{
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
	};
	//填充顶点和索引缓冲区
	meshData.Vertices.resize(12);
	meshData.Indices32.assign(&k[0], &k[60]);//填充索引缓冲区

	for (GeometryGenerator::uint32 i = 0; i < 12; ++i)
	{
		meshData.Vertices[i].Position = pos[i];//赋值顶点缓冲区中的顶点坐标
		XMVECTOR vertNormal = XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Position));//归一化顶点法线
		XMVECTOR vertPos = vertNormal * radius;//球面映射后的顶点坐标

		XMStoreFloat3(&meshData.Vertices[i].Normal, vertNormal);//赋值顶点缓冲区中的法线
		XMStoreFloat3(&meshData.Vertices[i].Position, vertPos);//赋值顶点缓冲区中的顶点坐标
		meshData.Vertices[i].TexC = XMFLOAT2(0.0f, 0.0f);	
	}
	
	return meshData;
}