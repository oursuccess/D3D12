#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));	//先创建一个直接执行的命令分配器. 然后才有可能创建后面的每个缓冲区

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);	//我们默认了上传之后其不会变化(不需要再同步回CPU)
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);	//false仅仅用来表示其存储的并非是常量缓冲区, 但这是为什么? 是因为这里有了动画? 发现这儿改成true了之后会丢失一些材质 --FIXME: @Je
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{
}
