//AnimationHelper. 定义了关键帧, 以及一个骨骼动画的关键帧们

#ifndef ANIMATION_HELPER_H
#define ANIMATION_HELPER_H

#include "../../d3d12book-master/Common/d3dUtil.h"

//旋转四元数: Rq(v) = qvq*, 当q为单位四元数且可以表示为q = (sin(θ)n + cos(θ))时，表示让v沿着轴n旋转2θ
struct Keyframe	//关键帧记录了该关键帧的时间戳、其对应的位移、缩放、旋转四元数
{
	Keyframe();
	~Keyframe();

	float TimePos;
	DirectX::XMFLOAT3 Translation;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4 RotationQuat;
};

struct BoneAnimation	//一个骨骼动画包含了起始、结束时间，以及所有关键帧们. 以及根据一个时间节点进行插值获得该时间节点对应的动画的方法. 在我们当前的动画中，仅有缩放旋转和位移,因此可以将动画直接通过一个变换矩阵导出
{
	float GetStartTime() const;
	float GetEndTime() const;

	void Interpolate(float t, DirectX::XMFLOAT4X4& M) const;

	std::vector<Keyframe> Keyframes;
};

#endif
