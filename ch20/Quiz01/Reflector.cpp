#include "Reflector.h"

Reflector::Reflector(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height, std::wstring texturePath)
	: md3dDevice(device), mCommandList(cmdList), mWidth(width), mHeight(height)
{
	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResources(texturePath);
}

void Reflector::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv)
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;

	BuildDescriptors();
}

void Reflector::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;

	srvDesc.Texture2D.MipLevels = mReflectorMap->GetDesc().MipLevels;
	srvDesc.Format = mReflectorMap->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mReflectorMap.Get(), &srvDesc, mhCpuSrv);
}

void Reflector::BuildResources(std::wstring texturePath)
{
	auto texMap = std::make_unique<Texture>();
	texMap->Name = "ReflectorMap";
	texMap->Filename = texturePath;
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice, mCommandList, 
		texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));

	mReflectorMap = texMap->Resource;
}
