//AnimationHelper. 定义了关键帧, 以及一个骨骼动画的关键帧们

#ifndef ANIMATION_HELPER_H
#define ANIMATION_HELPER_H

#include "../../d3d12book-master/Common/d3dUtil.h"

//旋转四元数: Rq(v) = qvq*, 当q为单位四元数且可以表示为q = (sin(θ)n + cos(θ))时，表示让v沿着轴n旋转2θ
struct Keyframe	//关键帧记录了该关键帧的时间戳、其对应的位移、缩放、旋转四元数
{

};

#endif
