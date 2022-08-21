#ifndef SKINNED_DATA_H
#define SKINNED_DATA_H

#include "../../../d3d12book-master/Common/d3dUtil.h"
#include "../../../d3d12book-master/Common/MathHelper.h"

struct Keyframe	//关键帧用于记录动画的一个关键节点, 在节点之间, 我们通过简单的插值动画实现. 我们的动画仅仅记录了时间、位移、缩放和旋转
{
	Keyframe();
	~Keyframe();

	float TimePos;
	DirectX::XMFLOAT3 Translation;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4 RotationQuat;
};

struct BoneAnimation	//骨骼动画结构体. 骨骼动画中存储了一个个关键帧, 并可以返回一个在时刻t对应的动画状态的变换矩阵(包含了缩放、位移、旋转), 请记住合理的矩阵变换组合为先缩放, 然后旋转, 最后位移!!!
{
	float GetStartTime() const;	//返回动画的起始时间
	float GetEndTime() const;	//返回动画的结束时间

	void Interpolate(float t, DirectX::XMFLOAT4X4& M) const;	//获取时刻t的插值后动画变换矩阵

	std::vector<Keyframe> Keyframes;	//记录所有关键帧
};

struct AnimationClip	//动画片段其实是一个个骨骼的动画。因此在一个动画片段中, 我们存储的是该动画片段影响的所有骨骼的所有动画! 而我们返回的则是这些骨骼各自在时刻t的变换矩阵!
{
	float GetClipStartTime() const;	//获取动画片段的开始时间
	float GetClipEndTime() const;	//获取动画片段的结束时间

	void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const;	//获取时刻t时所有被动画控制的骨骼的变换矩阵

	std::vector<BoneAnimation> BoneAnimations;	//保存该动画片段控制的所有骨骼
};

class SkinnedData
{
public:
	UINT BoneCount() const;	//获取该皮肤下的所有骨骼

	float GetClipStartTime(const std::string& clipName) const;	//获取指定动画片段的开始和结束时间
	float GetClipEndTime(const std::string& clipName) const;

	void Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
		std::unordered_map<std::string, AnimationClip>& animations);	//根据骨骼层级, 每个骨骼的相对偏移, 以及所有动画片段们来构建皮肤

	void GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const;	//获取一个动画片段在时刻t的最终变换矩阵们. 我们将这些矩阵通过参数finalTransforms传递出去

private:
	std::vector<int> mBoneHierarchy;	//分别保存骨骼层级、骨骼相对偏移、动画片段们
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
	std::unordered_map<std::string, AnimationClip> mAnimations;
};

#endif
