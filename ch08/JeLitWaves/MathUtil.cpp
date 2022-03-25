#include "MathUtil.h"
#include <float.h>
#include <cmath>

using namespace DirectX;

const float MathUtil::Infinity = FLT_MAX;
const float MathUtil::Pi = 3.1415926535f;

float MathUtil::AngleFromXY(float x, float y)
{
    float theta = 0.0f;

    //Quadrant I or IV
    if (x >= 0.0f)
    {
        //If x = 0, then atanf(y/x) = +pi/2 if y > 0
        //               atanf(y/x) = -pi
        theta = atanf(y / x);   //in [-pi/2, +pi/2]

        if (theta < 0.0f) theta += 2.0f * Pi;
    }
    else 
    {
        theta = atanf(y / x) + Pi;
    }

    return theta;
}

DirectX::XMVECTOR MathUtil::RandUnitVec3()
{
    XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    XMVECTOR Zero = XMVectorZero();

    //keep trying until we get a point on/in the hemisphere
    while (true) {
        XMVECTOR v = XMVectorSet(MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), 0.0f);
        if (XMVector3Greater(XMVector3LengthSq(v), One)) continue;
        return XMVector3Normalize(v);
    }
}

DirectX::XMVECTOR MathUtil::RandHemisphereUnitVec3(DirectX::XMVECTOR n)
{
    XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    XMVECTOR Zero = XMVectorZero();

    // Keep trying until we get a point on/in the hemisphere.
    while (true)
    {
        XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

        if (XMVector3Greater(XMVector3LengthSq(v), One))
            continue;

        if (XMVector3Less(XMVector3Dot(n, v), Zero))
            continue;

        return XMVector3Normalize(v);

}
