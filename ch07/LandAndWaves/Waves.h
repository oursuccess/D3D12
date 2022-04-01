//≤®¿À…˙≥…À„∑®
#pragma once

#include <vector>
#include <DirectXMath.h>

class Waves
{
public:
	Waves(int m, int n, float dx, float dt, float speed, float damping);
	Waves(const Waves& rhs) = delete;
	Waves& operator=(const Waves& rhs) = delete;
	~Waves();

	int RowCount()const { return mNumRows; }
	int ColumnCount()const { return mNumCols; }
	int VertexCount()const { return mVertexCount; }
	int TriangleCount()const { return mTriangleCount; }
	float Width()const { return mNumCols * mSpatialStep; }
	float Depth()const { return mNumRows * mSpatialStep; }

	//solution at the ith grid point
	const DirectX::XMFLOAT3& Position(int i)const { return mCurrSolution[i]; }
	//solution normal at the ith grid point
	const DirectX::XMFLOAT3& Normal(int i)const { return mNormals[i]; }
	//the unit tangent vector at the ith grid point in the local x-axis direction
	const DirectX::XMFLOAT3& TangentX(int i)const { return mTangentX[i]; }

	void Update(float dt);
	void Disturb(int i, int j, float magnitude);

private:
	int mNumRows = 0;
	int mNumCols = 0;

	int mVertexCount = 0;
	int mTriangleCount = 0;
	
	//simulation constants we can precompute
	float mK1 = 0.0f, mK2 = 0.0f, mK3 = 0.0f;

	float mTimeStep = 0.0f;
	float mSpatialStep = 0.0f;
	
	std::vector<DirectX::XMFLOAT3> mPrevSolution;
	std::vector<DirectX::XMFLOAT3> mCurrSolution;
	std::vector<DirectX::XMFLOAT3> mNormals;
	std::vector<DirectX::XMFLOAT3> mTangentX;
};

