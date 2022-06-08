//用于保存需要投影的内容的类。 存储了投影纹理的大小与资源(SRV)

#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"

class Reflector
{
public:
	Reflector(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height, std::wstring texturePath);
	Reflector(const Reflector& rhs) = delete;
	Reflector& operator=(const Reflector& rhs) = delete;
	~Reflector() = default;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

private:
	void BuildDescriptors();
	void BuildResources(std::wstring texturePath);

private:
	ID3D12Device* md3dDevice = nullptr;
	ID3D12GraphicsCommandList* mCommandList;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
	UINT mWidth = 0;
	UINT mHeight = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mReflectorMap = nullptr;
};

