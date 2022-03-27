#include "Waves.h"
#include <ppl.h>
#include <algorithm>
#include <vector>
#include <cassert>

using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping) : mNumRows(m), mNumCols(n), mVertexCount(m * n),
    mTriangleCount((m-1) * (n-1) * 2), mTimeStep(dt), mSpatialStep(dx)
{
    const float d = damping * dt + 2.0f;
    const float e = (speed * speed) * (dt * dt) / (dx * dx);
    mK1 = (damping * dt - 2.0f) / d;
    mK2 = (4.0f - 8.0f * e) / d;
    mK3 = (2.0f * e) / d;

    const int sz = m * n;
    mPrevSolution.resize(sz);
    mCurrSolution.resize(sz);
    mNormals.resize(sz);
    mTangentX.resize(sz);

    //generate grid vertices in system memory
    const float halfWidth = (n - 1) * dx * 0.5f;
    const float halfDepth = (m - 1) * dx * 0.5f;
    for (int i = 0; i < m; ++i) 
    {
        float z = halfDepth - i * dx;
        for(int j = 0; j < n; ++j) 
        {
            float x = -halfWidth + j * dx;
            
            const auto idx = i * n + j;
            mPrevSolution[idx] = XMFLOAT3(x, 0.0f, z);
            mCurrSolution[idx] = XMFLOAT3(x, 0.0f, z);
            mNormals[idx] = XMFLOAT3(0.0f, 1.0f, 0.0f);
            mTangentX[idx] = XMFLOAT3(1.0f, 0.0f, 0.0f);
        }
    }
}

Waves::~Waves()
{
}

int Waves::RowCount() const
{
    return mNumRows;
}

int Waves::ColumnCount() const
{
    return mNumCols;
}

int Waves::VertexCount() const
{
    return mVertexCount;
}

int Waves::TriangleCount() const
{
    return mTriangleCount;
}

float Waves::Width() const
{
    return mNumCols * mSpatialStep;
}

float Waves::Depth() const
{
    return mNumRows * mSpatialStep;
}

void Waves::Update(float dt)
{
    static float t = 0;
    t += dt;

    const auto jMax = mNumCols - 1;
    const auto iMax = mNumRows - 1;
    if (t >= mTimeStep) 
    {
        //only update interior points, we use zero boundary conditions
        concurrency::parallel_for(1, iMax, [this, jMax](int i)
            {
                for (int j = 1; j < jMax; ++j) {
                    const auto idx = i * mNumCols + j;
                    mPrevSolution[idx].y = mK1 * mPrevSolution[idx].y + mK2 * mCurrSolution[idx].y + mK3 * (
                        mCurrSolution[idx + mNumCols].y + mCurrSolution[idx - mNumCols].y + mCurrSolution[idx + 1].y + mCurrSolution[idx - 1].y);
                }
            });

        std::swap(mPrevSolution, mCurrSolution);

        t = 0.0f;

        //compute normals using finite difference scheme
        concurrency::parallel_for(1, iMax, [this, jMax](int i)
            {
                for (int j = 1; j < jMax; ++j)
                {
                    const auto idx = i * mNumCols + j;
                    const auto l = mCurrSolution[idx - 1].y, r = mCurrSolution[idx + 1].y, t = mCurrSolution[idx - mNumCols].y, b = mCurrSolution[idx + mNumCols].y;
                    mNormals[idx] = { l - r, 2.0f * mSpatialStep, b - t };

                    XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mNormals[idx]));
                    XMStoreFloat3(&mNormals[idx], n);

                    mTangentX[idx] = XMFLOAT3(2.0f * mSpatialStep, r - l, 0.0f);
                    XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&mTangentX[idx]));
                    XMStoreFloat3(&mTangentX[idx], T);
                }
            });
    }
}

void Waves::Disturb(int i, int j, float magnitude)
{
    //boundary no disturb
    assert(i > 1 && i < mNumRows - 2);
    assert(j > 1 && j < mNumCols - 2);

    const float halfMag = 0.5f * magnitude;
    const int idx = i * mNumCols + j;
    mCurrSolution[idx].y += magnitude;
    mCurrSolution[idx + 1].y += halfMag;
    mCurrSolution[idx - 1].y += halfMag;
    mCurrSolution[idx + mNumCols].y += halfMag;
    mCurrSolution[idx - mNumCols].y += halfMag;
}
