#include "FrameResource.h"

//添加了materialCount参数
FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);

	//ch08新增加的资源我们也要正常初始化
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
}

FrameResource::~FrameResource()
{
}
