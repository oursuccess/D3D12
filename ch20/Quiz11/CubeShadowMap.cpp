#include "CubeShadowMap.h"

CubeShadowMap::CubeShadowMap(ID3D12Device* device, UINT width, UINT height) 
	: md3dDevice(device), mWidth(width), mHeight(height), mViewport({ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f }), mScissorRect({ 0, 0, (int)width, (int)height })
{
	BuildResource();
}

UINT CubeShadowMap::Width() const
{
	return mWidth;
}

UINT CubeShadowMap::Height() const
{
	return mHeight;
}

ID3D12Resource* CubeShadowMap::Resource()
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeShadowMap::Srv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeShadowMap::Dsv(int index) const
{
	return mhCpuDsv[index];
}

D3D12_VIEWPORT CubeShadowMap::Viewport() const
{
	return mViewport;
}

D3D12_RECT CubeShadowMap::ScissorRect() const
{
	return mScissorRect;
}

void CubeShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv[6])
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;

	for (int i = 0; i < 6; ++i)
		mhCpuDsv[i] = hCpuDsv[i];

	BuildDescriptors();
}

void CubeShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if (newWidth != mWidth || newHeight != mHeight)
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();

		BuildDescriptors();
	}
}

void CubeShadowMap::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mhCpuSrv);

	for (int i = 0; i < 6; ++i)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;

		md3dDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mhCpuDsv[i]);
	}
}

void CubeShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mShadowMap)));
}
