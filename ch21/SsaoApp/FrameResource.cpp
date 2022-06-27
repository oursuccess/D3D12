#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, 
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	SsaoCB = std::make_unique<UploadBuffer<SsaoConstants>>(device, 1, true);	//环境光遮蔽需要的常量只需要1份
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, passCount, true);
}

FrameResource::~FrameResource()
{
}
