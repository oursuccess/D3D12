#include "SkinnedData.h"

Keyframe::Keyframe()
{
}

Keyframe::~Keyframe()
{
}

float BoneAnimation::GetStartTime() const
{
	return 0.0f;
}

float BoneAnimation::GetEndTime() const
{
	return 0.0f;
}

void BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& M) const
{
}

float AnimationClip::GetClipStartTime() const
{
	return 0.0f;
}

float AnimationClip::GetClipEndTime() const
{
	return 0.0f;
}

void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const
{
}

UINT SkinnedData::BoneCount() const
{
	return 0;
}

float SkinnedData::GetClipStartTime(const std::string& clipName) const
{
	return 0.0f;
}

float SkinnedData::GetClipEndTime(const std::string& clipName) const
{
	return 0.0f;
}

void SkinnedData::Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets, std::unordered_map<std::string, AnimationClip>& animations)
{
}

void SkinnedData::GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const
{
}
