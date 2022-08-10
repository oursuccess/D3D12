#ifndef LOADM3D_H
#define LOADM3D_H

#include "SkinnedData.h"

class M3DLoader
{
public:
	struct Vertex	//我们在这里重新定义一下我们自己需要的结构体. 顶点需要包含位置、法线、纹理坐标和切线
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexC;
		DirectX::XMFLOAT4 TangentU;	//这里并不一定请求是Float4, 只是为了对其要求, 我们直接设置成了float4
	};

	struct SkinnedVertex	//蒙皮的顶点还要有影响其的骨骼, 和每个骨骼的权重(骨骼数量最多为4)
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexC;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT3 BoneWeights;
		BYTE BoneIndices[4];
	};

	struct Subset	//一个mesh其实对应了很多个submesh, 每个submesh有自己的顶点和面片
	{
		UINT Id = -1;
		UINT VertexStart = 0;
		UINT VertexCount = 0;
		UINT FaceStart = 0;
		UINT FaceCount = 0;
	};

	struct M3dMaterial	//材质. 一个材质有自己的名称, 漫反射率, R0值, 粗糙度, 是否开启透明度测试, 法线图名, 纹理图名称, 材质类型名
	{
		std::string Name;

		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01 };
		float Roughness = 0.8f;
		bool AlphaClip = false;

		std::string MaterialTypeName;
		std::string DiffuseMapName;
		std::string NormalMapName;
	};

	bool LoadM3d(const std::string& filename, std::vector<Vertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats);	//一个加载m3d模型的方法. 其接收文件名, 然后将顶点、索引、子mesh、材质们分别存储到对应的参数中
	bool LoadM3d(const std::string& filename, std::vector<SkinnedVertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats, SkinnedData& skinInfo);	//加载m3d模型, 在上面的顶点、索引、子mesh、材质之外, 我们还加入了蒙皮信息

private:
	void ReadMaterials(std::ifstream& fin, UINT numMaterials, std::vector<M3dMaterial>& mats);	//从文件流中读取指定数量的材质, 将结果存储到mats中. 下面依次与上面相同, 分别读了不同的内容
	void ReadSubsetTable(std::ifstream& fin, UINT numSubsets, std::vector<Subset>& subsets);
	void ReadVertices(std::ifstream& fin, UINT numVertices, std::vector<Vertex>& vertices);
	void ReadSkinnedVertices(std::ifstream& fin, UINT numVertices, std::vector<SkinnedVertex>& vertices);	//读取蒙皮顶点
	void ReadTriangles(std::ifstream& fin, UINT numTriangles, std::vector<USHORT>& indices);	//读取顶点索引
	void ReadBoneOffsets(std::ifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets);	//读取模型空间到骨骼局部空间的变换矩阵
	void ReadBoneHierarchy(std::ifstream& fin, UINT numBonese, std::vector<int>& boneIndexToParentIndex);	//读取每个骨骼的父节点(的索引)
	void ReadAnimationClips(std::ifstream& fin, UINT numBones, UINT numAnimationClips, std::unordered_map<std::string, AnimationClip>& animations);	//读取所有动画片段
	void ReadBoneKeyframes(std::ifstream& fin, UINT numBones, BoneAnimation& boneAnimation);	//读取完整的骨骼动画
};

#endif
