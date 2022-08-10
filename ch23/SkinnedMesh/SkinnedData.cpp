#include "SkinnedData.h"

using namespace DirectX;

Keyframe::Keyframe()
	: TimePos(0.0f), Translation(0.0f, 0.0f, 0.0f), Scale(1.0f, 1.0f, 1.0f), RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)	//我们将关键帧的几个属性都进行初始化, 时间为0,位移为0, 尺寸为1, 旋转为0
{
}

Keyframe::~Keyframe()
{
}

float BoneAnimation::GetStartTime() const
{
	return Keyframes.front().TimePos;
}

float BoneAnimation::GetEndTime() const
{
	return Keyframes.back().TimePos;
}

void BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& M) const
{
	//将单个骨骼的变换进行插值
	XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);	//初始的状态, 初始的时候, 理应在原点的位置, 且旋转轴没有任何变化.

	if (t <= GetStartTime())	//如果时间在起始之前, 我们直接返回起始时间的动画状态
	{	
		XMVECTOR Scale = XMLoadFloat3(&Keyframes.front().Scale);
		XMVECTOR Translation = XMLoadFloat3(&Keyframes.front().Translation);
		XMVECTOR Rotation = XMLoadFloat4(&Keyframes.front().RotationQuat);	//时刻牢记我们在第22章学到的, 我们用3个数字表示u(三个旋转轴, 加上旋转角度的正弦), 用1个数字表示v(旋转角度的余弦)

		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(Scale, zero, Rotation, Translation));	//复合出变换矩阵的时候, 我们需要记得, 先缩放(轴不变, 坐标不变), 后旋转(轴变换, 坐标不变), 最后位移(坐标变换)
	}
	else if (t >= GetEndTime())		//如果时间在结束之后, 我们直接返回结束时候的动画状态
	{
		XMVECTOR Scale = XMLoadFloat3(&Keyframes.back().Scale);
		XMVECTOR Translation = XMLoadFloat3(&Keyframes.back().Translation);
		XMVECTOR Rotation = XMLoadFloat4(&Keyframes.back().RotationQuat);

		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(Scale, zero, Rotation, Translation));
	}
	//否则, 我们需要进行插值(我们需要先计算出来时间t在哪两个关键帧之间)
	else 
	{
		for (UINT i = 0; i < Keyframes.size() - 1; ++i)
		{
			if (t >= Keyframes[i].TimePos && t <= Keyframes[i].TimePos)
			{
				Keyframe begin = Keyframes[i], end = Keyframes[i + 1];	//我们找到了我们需要插值的两帧

				float lerpPercent = (t - begin.TimePos) / (end.TimePos - begin.TimePos);	//我们计算出当前帧需要的插值(线性计算即可)

				XMVECTOR Scale0 = XMLoadFloat3(&begin.Scale), Scale1 = XMLoadFloat3(&end.Scale);
				XMVECTOR Translation0 = XMLoadFloat3(&begin.Translation), Translation1 = XMLoadFloat3(&end.Translation);
				XMVECTOR Rotation0 = XMLoadFloat4(&begin.RotationQuat), Rotation1 = XMLoadFloat4(&end.RotationQuat);

				XMVECTOR Scale = XMVectorLerp(Scale0, Scale1, lerpPercent);
				XMVECTOR Translation = XMVectorLerp(Translation0, Translation1, lerpPercent);
				XMVECTOR Rotation = XMVectorLerp(Rotation0, Rotation1, lerpPercent);

				XMStoreFloat4x4(&M, XMMatrixAffineTransformation(Scale, zero, Rotation, Translation));

				break;	//找到之后, 直接退出循环
			}
		}
	}
}

float AnimationClip::GetClipStartTime() const
{
	//找到一个片段中控制的所有骨骼动画中的最早开始时间
	float t = MathHelper::Infinity;
	for (const auto& boneAnimation : BoneAnimations)
	{
		t = MathHelper::Min(boneAnimation.GetStartTime(), t);
	}
	return t;
}

float AnimationClip::GetClipEndTime() const
{
	//找到一个片段中控制的所有骨骼动画的最晚结束时间
	float t = 0.0f;
	for (const auto& boneAnimation : BoneAnimations)
	{
		t = MathHelper::Max(boneAnimation.GetEndTime(), t);
	}
	return t;
}

void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const
{
	//我们将该片段中控制的所有骨骼都依次进行时刻t的插值即可. 这里, 我们假定了骨骼变换矩阵的骨骼是恰好满足我们需要的(恰好和片段控制的骨骼数量相同)!!!
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		BoneAnimations[i].Interpolate(t, boneTransforms[i]);
	}
}

UINT SkinnedData::BoneCount() const
{
	return mBoneHierarchy.size();	//获取骨骼数量即可
}

float SkinnedData::GetClipStartTime(const std::string& clipName) const
{
	return mAnimations.find(clipName)->second.GetClipStartTime();	//直接找到该动画信息中对应片段的起始时间即可
}

float SkinnedData::GetClipEndTime(const std::string& clipName) const
{
	return mAnimations.find(clipName)->second.GetClipEndTime();
}

void SkinnedData::Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets, std::unordered_map<std::string, AnimationClip>& animations)
{
	mBoneHierarchy = boneHierarchy;
	mBoneOffsets = boneOffsets;
	mAnimations = animations;
}

void SkinnedData::GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const
{
	UINT numBones = mBoneOffsets.size();	//我们先获取骨骼数量(为什么这里是BoneOffsets?)

	std::vector<XMFLOAT4X4> toParentTransforms(numBones);

	auto clip = mAnimations.find(clipName);	//找到对应的动画片段
	clip->second.Interpolate(timePos, toParentTransforms);	//我们找到t时刻的动画片段中所有骨骼的相对于自己局部空间的变换

	std::vector<XMFLOAT4X4> toRootTransforms(numBones);	//准备将每个骨骼当前的变换变换到模型空间
	toRootTransforms[0] = toParentTransforms[0];	//根节点无需再次变换, 因为其就是模型空间

	for (UINT i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);	//我们加载骨骼的变换

		int parentIndex = mBoneHierarchy[i];	//然后, 我们找到骨骼的父节点, 并求出骨骼父节点的变换
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);	//由于我们保证了骨骼父节点的下标一定比骨骼下标小, 因此, 在处理骨骼时, 其父节点一定也被处理过了

		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);	//注意这里的叉乘顺序, 骨骼的变换 X 骨骼父节点到模型空间的变换! 这里之所以是这样, 是因为我们之后会将顶点作为行向量, 放在左边与最终的变化矩阵叉乘

		XMStoreFloat4x4(&toRootTransforms[i], toRoot);	//我们将最终的变换结果存到toRootTransforms中. 这里变换的只是骨骼, 而非顶点(因为顶点是定义在模型空间中的!)
	}

	for (UINT i = 0; i < numBones; ++i)	
	{
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);	//这是从模型空间变换到骨骼空间的矩阵
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
		XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);	//通过这次变换, 我们得到了将模型空间中的顶点根据骨骼进行变化的矩阵
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}
