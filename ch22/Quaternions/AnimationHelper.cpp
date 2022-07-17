#include "AnimationHelper.h"

using namespace DirectX;

Keyframe::Keyframe() :
    TimePos(0.0f),
    Translation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f),
    RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)
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
    if (t <= Keyframes.front().TimePos)     //若还没到第一帧,则直接用第一帧的
    {
        XMVECTOR S = XMLoadFloat3(&Keyframes.front().Scale);
        XMVECTOR P = XMLoadFloat3(&Keyframes.front().Translation);
        XMVECTOR Q = XMLoadFloat4(&Keyframes.front().RotationQuat);

        XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);    //从原点开始变换
        XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));   //变换的顺序是先缩放后旋转最后平移! 和Unity中一样! 我们需要注意的是, 旋转顺序为zxy!
    }
    else if (t >= Keyframes.back().TimePos) //若超过了最后一帧,则直接用最后一帧的
    {
        XMVECTOR S = XMLoadFloat3(&Keyframes.back().Scale);
        XMVECTOR P = XMLoadFloat3(&Keyframes.back().Translation);
        XMVECTOR Q = XMLoadFloat4(&Keyframes.back().RotationQuat);

        XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);    //从原点开始变换
        XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));   //变换的顺序是先缩放后旋转最后平移! 和Unity中一样! 我们需要注意的是, 旋转顺序为zxy!
    }
    else // 否则,我们需要在两个帧之间插值
    {
        for (UINT i = 0; i < Keyframes.size() - 1; ++i) 
        {
            if (t >= Keyframes[i].TimePos && t <= Keyframes[i + 1].TimePos)
            {
                float lerpPercent = (t - Keyframes[i].TimePos) / (Keyframes[i + 1].TimePos - Keyframes[i].TimePos); //插值根据时间得出

                XMVECTOR s0 = XMLoadFloat3(&Keyframes[i].Scale);
                XMVECTOR s1 = XMLoadFloat3(&Keyframes[i+1].Scale);

                XMVECTOR p0 = XMLoadFloat3(&Keyframes[i].Translation);
                XMVECTOR p1 = XMLoadFloat3(&Keyframes[i + 1].Translation);

                XMVECTOR q0 = XMLoadFloat4(&Keyframes[i].RotationQuat);
                XMVECTOR q1 = XMLoadFloat4(&Keyframes[i + 1].RotationQuat);

                XMVECTOR S = XMVectorLerp(s0, s1, lerpPercent);
                XMVECTOR P = XMVectorLerp(p0, p1, lerpPercent);
                XMVECTOR Q = XMQuaternionSlerp(q0, q1, lerpPercent);    //旋转矩阵不能简单lerp,而是需要重新计算

                XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));

                break;
            }
        }
    }
}
