#include "Waves.h"
#include <ppl.h>
#include <algorithm>
#include <vector>
#include <cassert>

using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping) : mNumRows(m), mNumCols(n), mVertexCount(m*n), mTriangleCount((m-1)*(n-1)*2), mTimeStep(dt),
	mSpatialStep(dx), mPrevSolution(std::vector<XMFLOAT3>(m*n)), mCurrSolution(std::vector<XMFLOAT3>(m*n)), mNormals(std::vector<XMFLOAT3>(m*n)), mTangentX(std::vector<XMFLOAT3>(m*n))
{
	float d = damping * dt + 2.0f;
	float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK1 = (damping * dt - 2.0f) / d;
	mK2 = (4.0f - 8.0f * e) / d;
	mK3 = (2.0f * e) / d;

	float halfWidth = (n - 1) * dx * 0.5f, halfDepth = (m - 1) * dx * 0.5f;
	for (int i = 0; i < m; ++i)
	{
		float z = halfDepth - i * dx;
		for (int j = 0; i < n; ++j)
		{
			float x = -halfWidth + j * dx;
			mPrevSolution[i * n + j] = XMFLOAT3(x, 0.0f, z);
			mCurrSolution[i * n + j] = XMFLOAT3(x, 0.0f, z);
			mNormals[i * n + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			mTangentX[i * n + j] = XMFLOAT3(1.0f, 0.0f, 0.0f);
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

	if (t >= mTimeStep)
	{
		concurrency::parallel_for(1, mNumRows - 1, [this](int i)
			{
				for (int j = 1; j < mNumRows - 1; ++j)
				{
					auto index = i * mNumCols + j;
					mPrevSolution[index].y = mK1 * mPrevSolution[index].y + mK2 * mCurrSolution[index].y + mK3 * (mCurrSolution[index + mNumCols].y +
						mCurrSolution[index - mNumCols].y + mCurrSolution[index + 1].y + mCurrSolution[index - 1].y);
				}
			});
		std::swap(mPrevSolution, mCurrSolution);

		t = 0.0f;
		concurrency::parallel_for(1, mNumRows - 1, [this](int i)
			{
				for (int j = 1; j < mNumCols - 1; ++j)
				{
					auto index = i * mNumCols + j;
					float l = mCurrSolution[index - 1].y, r = mCurrSolution[index + 1].y, t = mCurrSolution[index - mNumCols].y, b = mCurrSolution[index + mNumCols].y;
					mNormals[index] = { l - r, 2.0f * mSpatialStep, b - t };

					XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mNormals[index]));
					XMStoreFloat3(&mNormals[index], n);

					mTangentX[index] = XMFLOAT3(2.0f * mSpatialStep, r - l, 0.0f);
					XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&mTangentX[index]));
					XMStoreFloat3(&mTangentX[index], T);
				}
			});
	}
}

void Waves::Disturb(int i, int j, float magnitude)
{
	assert(i > 1 && i < mNumRows - 2 && j > 1 && j < mNumCols - 2);

	float halfMag = 0.5f * magnitude;

	const auto index = i * mNumCols + j;
	mCurrSolution[index].y += magnitude;
	mCurrSolution[index + 1].y += halfMag;
	mCurrSolution[index - 1].y += halfMag;
	mCurrSolution[index + mNumCols].y += halfMag;
	mCurrSolution[index - mNumCols].y += halfMag;
}
