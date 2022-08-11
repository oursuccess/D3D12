#include "LoadM3d.h"

using namespace DirectX;

bool M3DLoader::LoadM3d(const std::string& filename, std::vector<Vertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats)
{
	std::ifstream fin(filename);	//根据文件名创建输入流

	if (!fin) return false;	//若无法正常创建流, 则直接返回false

	UINT numMaterials = 0;	//先记录一下材质数、顶点数、三角面数量、骨骼数量与动画片段数量
	UINT numVertices = 0;
	UINT numTriangles = 0;
	UINT numBones = 0;
	UINT numANimationClips = 0;

	std::string ignore;	//我们知道m3d格式文件是如何组织的, 因此自然知道其中不应读取的部分(operator>>遇空格即停, 然后每次调用从空格之后的下一个有效字符重新读取)

	fin >> ignore;	//文件头
	fin >> ignore >> numMaterials;
	fin >> ignore >> numVertices;
	fin >> ignore >> numTriangles;
	fin >> ignore >> numBones;
	fin >> ignore >> numANimationClips;

	ReadMaterials(fin, numMaterials, mats);	//需要注意, 我们需要严格按照顺序读取!!!
	ReadSubsetTable(fin, numMaterials, subsets);	//每个submesh都有自己独立的材质
	ReadVertices(fin, numVertices, vertices);	
	ReadTriangles(fin, numTriangles, indices);	//当我们知道拓扑是三角形列表还是三角形带的时候, 我们也就知道了定义这么多三角形需要多少个索引!!!

	return true;
}

bool M3DLoader::LoadM3d(const std::string& filename, std::vector<SkinnedVertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats, SkinnedData& skinInfo)
{
	std::ifstream fin(filename);

	if (!fin) return false;

	UINT numMaterials = 0;
	UINT numVertices = 0;
	UINT numTriangles = 0;
	UINT numBones = 0;
	UINT numAnimationClips = 0;

	std::string ignore;

	/*
	 * 我们的m3d头部格式(line01-06):
	 * **********m3d-File-Header*****	//this line is intended to ignore
	 * #Materials 5	//#Materials should be ingored, numMaterials should be stored to numMaterials
	 * #Vertices 13748 //#Vertices should be ignored, store number to numVertices
	 * #Triangles 22507 
	 * #Bones 58 
	 * #AnimationClips //so to #Triangles, #Bones, #AnimationsClips
	*/

	fin >> ignore;	//这些都和上面的重载函数对应
	fin >> ignore >> numMaterials;
	fin >> ignore >> numVertices;
	fin >> ignore >> numTriangles;
	fin >> ignore >> numBones;
	fin >> ignore >> numAnimationClips;	//我们在读取蒙皮时, 默认其有骨骼动画, 因此我们读取动画片段数量

	/*
	* 我们的m3d后续格式(示意):
	* *******Mateirals*****	//材质区域
	* //...
	* *****SubsetTable****	//子mesh区域
	* //...
	* *****Vertices****		//顶点区域
	* //...
	* *****Triangles***		//三角面区域(索引区域, 3个指向顶点的索引定义了一个三角面)
	* //...
	* *****BoneOffsets***	//模型空间到骨骼局部空间的变换矩阵区域
	* //...
	* *****BoneHierarchy***	//骨骼的层级关系区域
	* //...
	* *****AnimationClips***	//动画区域
	*/
	//因此, 我们读取时, 也要严格保持该顺序读取!!!
	ReadMaterials(fin, numMaterials, mats);
	ReadSubsetTable(fin, numMaterials, subsets);
	ReadSkinnedVertices(fin, numVertices, vertices);
	ReadTriangles(fin, numTriangles, indices);
	
	std::vector<XMFLOAT4X4> boneOffsets;	//创建几个辅助数组, 分别存储模型空间到骨骼的局部空间的变换矩阵, 骨骼的父节点的索引, 以及所有动画(以动画名为key, 动画片段为value)
	std::vector<int> boneIndexToParentIndex;
	std::unordered_map<std::string, AnimationClip> animations;

	ReadBoneOffsets(fin, numBones, boneOffsets);	//读取模型空间到每个骨骼的变换矩阵
	ReadBoneHierarchy(fin, numBones, boneIndexToParentIndex);	//读取每个骨骼的父节点的索引
	ReadAnimationClips(fin, numBones, numAnimationClips, animations);	//读取所有动画, 我们需要知道有多少个骨骼, 以及有多少个动画片段

	skinInfo.Set(boneIndexToParentIndex, boneOffsets, animations);	//将读取的骨骼与动画设置到蒙皮信息上

	return true;
}

void M3DLoader::ReadMaterials(std::ifstream& fin, UINT numMaterials, std::vector<M3dMaterial>& mats)
{
	std::string ignore;
	mats.resize(numMaterials);	//将材质数组变为与材质数量对应

	std::string diffuseMapName;	//时刻牢记材质包含的信息: 纹理图, 法线图, 粗糙度, R0, 漫反射, 材质名
	std::string normalMapName;	//我们还在本章中加入了材质是否需要透明度测试, 材质类型

	/*
	* Material部分每个Material的格式:
	* Name: soldier_head	//材质名
	* Diffse: 1 1 1			//漫反射
	* FresnelR0: 0.05 0.05 0.05	//R0
	* Roughness: 0.5		//粗糙度
	* AlphaClip: 0			//是否进行混合度测试(0为否)
	* MaterialTypeName: Skinned	//材质类型
	* DiffuseMap: head_diff.dds	//纹理图名
	* NormalMap: head_norm.dds	//法线图名
	*/
	//我们按照上面的这个顺序读取即可

	fin >> ignore;
	for (auto& mat : mats)
	{
		fin >> ignore >> mat.Name >> ignore >> mat.DiffuseAlbedo.x >> mat.DiffuseAlbedo.y >> mat.DiffuseAlbedo.z >>
			ignore >> mat.FresnelR0.x >> mat.FresnelR0.y >> mat.FresnelR0.z >> ignore >> mat.Roughness >> ignore >> mat.AlphaClip >>
			ignore >> mat.MaterialTypeName >> ignore >> mat.DiffuseMapName >> ignore >> mat.NormalMapName;
	}
}

void M3DLoader::ReadSubsetTable(std::ifstream& fin, UINT numSubsets, std::vector<Subset>& subsets)
{
}

void M3DLoader::ReadVertices(std::ifstream& fin, UINT numVertices, std::vector<Vertex>& vertices)
{
}

void M3DLoader::ReadSkinnedVertices(std::ifstream& fin, UINT numVertices, std::vector<SkinnedVertex>& vertices)
{
}

void M3DLoader::ReadTriangles(std::ifstream& fin, UINT numTriangles, std::vector<USHORT>& indices)
{
}

void M3DLoader::ReadBoneOffsets(std::ifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets)
{
}

void M3DLoader::ReadBoneHierarchy(std::ifstream& fin, UINT numBonese, std::vector<int>& boneIndexToParentIndex)
{
}

void M3DLoader::ReadAnimationClips(std::ifstream& fin, UINT numBones, UINT numAnimationClips, std::unordered_map<std::string, AnimationClip>& animations)
{
}

void M3DLoader::ReadBoneKeyframes(std::ifstream& fin, UINT numBones, BoneAnimation& boneAnimation)
{
}
