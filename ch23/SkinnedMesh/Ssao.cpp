#include "Ssao.h"

Ssao::Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height)
{
}

UINT Ssao::SsaoMapWidth() const
{
	return 0;
}

UINT Ssao::SsaoMapHeight() const
{
	return 0;
}

void Ssao::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
}

std::vector<float> Ssao::CalcGaussWeights(float sigma)
{
	return std::vector<float>();
}

ID3D12Resource* Ssao::NormalMap()
{
	return nullptr;
}

ID3D12Resource* Ssao::AmbientMap()
{
	return nullptr;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::NormalMapRtv() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::NormalMapSrv() const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::AmbientMapSrv() const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE();
}

void Ssao::BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT srvDescriptorSize)
{
}

void Ssao::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
{
}

void Ssao::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
}

void Ssao::OnResize(UINT newWidth, UINT newHeight)
{
}

void Ssao::ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
{
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
{
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur)
{
}

void Ssao::BuildResources()
{
}

void Ssao::BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList)
{
}

void Ssao::BuildOffsetVectors()
{
}
