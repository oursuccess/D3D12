#pragma once

#include "../../../d3d12book-master/Common/d3dUtil.h"

class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device, UINT width, UINT height);
	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width() const;
	UINT Height() const;
	ID3D12Resource* Resource(int i);
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv(int i) const;	//获取指定csm层级的srv
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv(int i) const;	//获取指定csm层级的dsv
	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

	int CSMlayers() const;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv, 
		UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize);
	void OnResize(UINT newWidth, UINT newHeight);
private:
	void BuildDescriptors();
	void BuildResource();
private:
	ID3D12Device* md3dDevice = nullptr;
	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

#pragma region CSM
	int csmLayers = 1;	//csm层数. 最多为4, 最少为1

	UINT mCbvSrvUavDescriptorSize;
	UINT mDsvDescriptorSize;
#pragma endregion

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mShadowMaps;
};

