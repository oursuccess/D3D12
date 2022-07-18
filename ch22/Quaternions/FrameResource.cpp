#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));	//先创建一个直接执行的命令分配器. 然后才有可能创建后面的每个缓冲区

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);	//我们默认了上传之后其不会变化(不需要再同步回CPU)
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);	//材质对应的常量缓冲区是有可能需要同步会CPU的
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{
}
