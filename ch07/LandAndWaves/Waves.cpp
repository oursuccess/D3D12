#include "Waves.h"
#include <ppl.h>
#include <algorithm>
#include <cassert>
#include <vector>

using std::vector;
using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping) : mNumRows(m), mNumCols(n), mVertexCount(m*n), mTriangleCount((m-1)*(n-1)*2), mTimeStep(dt), mSpatialStep(dx),
	mPrevSolution(vector<XMFLOAT3>(m*n)), mCurrSolution(vector<XMFLOAT3>(m*n)), mNormals(vector<XMFLOAT3>(m*n)), mTangentX(vector<XMFLOAT3>(m*n))
{
	const float d = damping * dt + 2.0f;
	const float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK1 = (damping * dt - 2.0f) / d;
	mK2 = (4.0f - 8.0f * e) / d;
	mK3 = (2.0f * e) / d;

	//generate grid vertices in system memory
	float halfWidth = (n - 1) * dx * 0.5f;
	float halfDepth = (m - 1) * dx * 0.5f;
	for (int i = 0; i < m; ++i)
	{
		float z = halfDepth - i * dx;
		for (int j = 0; j < n; ++j)
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

void Waves::Update(float dt)
{
	static float t = 0;
	t += dt;

	if (t >= mTimeStep)
	{
		concurrency::parallel_for(1, mNumRows - 1, [this](int i) 
			{
				for (int j = 1; j < mNumCols - 1; ++j)
				{
					const auto idx = i * mNumCols + j;
					mPrevSolution[idx].y = mK1 * mPrevSolution[idx].y + mK2 * mCurrSolution[idx].y +
						mK3 * (mCurrSolution[idx + mNumCols].y + mCurrSolution[idx - mNumCols].y + mCurrSolution[idx + 1].y + mCurrSolution[idx - 1].y);
				}
			});

		std::swap(mPrevSolution, mCurrSolution);
		t = 0.0f;
		concurrency::parallel_for(1, mNumRows - 1, [this](int i)
			{
				for (int j = 1; j < mNumCols - 1; ++j)
				{
					const auto idx = i * mNumCols + j;
					const float l = mCurrSolution[idx - 1].y, r = mCurrSolution[idx + 1].y, t = mCurrSolution[idx + mNumCols].y, b = mCurrSolution[idx - mNumCols].y;

					XMStoreFloat3(&mNormals[idx], XMVector3Normalize(XMVECTOR{l - r, 2.0f * mSpatialStep, b - t}));
					XMStoreFloat3(&mTangentX[idx], XMVector3Normalize(XMVECTOR{2.0f * mSpatialStep, l - r, 0.0f}));
				}
			});
	}
}

void Waves::Disturb(int i, int j, float magnitude)
{
	assert(i > 1 && j < mNumRows - 2 && j > 1 && j < mNumCols - 2);

	const float halfMag = 0.5f * magnitude;
	const auto idx = i * mNumCols + j;
	
	mCurrSolution[idx].y += magnitude;
	mCurrSolution[idx + 1].y += halfMag;
	mCurrSolution[idx - 1].y += halfMag;
	mCurrSolution[idx + mNumCols].y += halfMag;
	mCurrSolution[idx - mNumCols].y += halfMag;
}
