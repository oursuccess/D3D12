#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
	: md3dDevice(device), mWidth(width), mHeight(height),
	mViewport {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f},	//我们记录一下视口
	mScissorRect {0, 0, (int)width, (int)height}	//以整个视口作为裁剪矩阵, 说明我们完全不裁剪
{
	BuildResource();
}

UINT ShadowMap::Width() const
{
	return mWidth;
}

UINT ShadowMap::Height() const
{
	return mHeight;
}

ID3D12Resource* ShadowMap::Resource()
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
	return mhCpuDsv;
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
	return mScissorRect;
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;

	BuildDescriptors();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();

		BuildDescriptors();
	}
}

void ShadowMap::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};	//我们首先决定将mShadowMap对应的资源描述为Srv. 需要注意的是, 我们决定是总是设置CPU侧的. GPU侧并没有这么复杂的管理机制(只能靠程序员自觉)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//其argb采样顺序无需任何变化, 正常采样即可
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;	//其类型为R24X8, R24为UNORM, X8表示不使用
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//我们决定将其描述为2D纹理图
	srvDesc.Texture2D.MostDetailedMip = 0;	//其最细节的Mip为0
	srvDesc.Texture2D.MipLevels = 1;	//其Mip层级总数为1, 即没有Mip
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;	//而我们接受的最细节Mip为0.0f, 低于此将直接被裁剪至0.0f
	srvDesc.Texture2D.PlaneSlice = 0;	//FIXME: 之后再看
	md3dDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mhCpuSrv);	//我们根据资源和描述来创建将资源视为CpuSrv的描述符

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};	//同理, 我们再将该资源视为Dsv重复进行一次
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//其作为DSV， 24位和8位已经可以理解了, 分别用于D和S,格式为UNORM和UINT
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	//其为2D深度图
	dsvDesc.Texture2D.MipSlice = 0;	//其Mip为0, 不进行任何分片
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;	//其作为DSV, 没有任何Flag
	md3dDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mhCpuDsv);	//我们根据资源和描述来创建将资源视为CpuDsv的描述符
}

void ShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc;	//我们创建一个资源, 该资源用于Dsv, 并在debug/Ssao中作为着色器资源被使用. 因此其格式为D24S8,维度为2D, 而MipLevels为1
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;	//该资源中只有一个元素, 其从0开始偏移(上面的alignment)
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//该资源的布局为位置的. 意味着我们不去考虑该资源的布局, 由硬件决定即可
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//该资源是允许作为DSV的,既然如此, 其格式自然只能为D24S8

	D3D12_CLEAR_VALUE optClear;	//我们创建一个重置时的值设定
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//我们设置重置时的格式
	optClear.DepthStencil.Depth = 1.0f;	//我们设置重置时的深度, 其为无限远
	optClear.DepthStencil.Stencil = 0;	//我们设置重置时的模板值, 其为0

	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mShadowMap)));	//创建可提交资源, 其默认的状态为只读
}
