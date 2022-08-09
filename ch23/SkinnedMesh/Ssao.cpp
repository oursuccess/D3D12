#include "Ssao.h"
#include <DirectXPackedVector.h>

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

Ssao::Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) 
	: md3dDevice(device)
{
	OnResize(width, height);	//初始的时候, 先构建一次视口

	BuildOffsetVectors();	//然后, 我们初始化偏移向量(仅仅初始化这么一次)
	BuildRandomVectorTexture(cmdList);	//然后, 初始化随机采样纹理图(同样仅仅初始化这么一次)
}

UINT Ssao::SsaoMapWidth() const
{
	return mRenderTargetWidth / 2;	//我们Ssao内部假设, SsaoMap的分辨率为渲染目标大小的一半
}

UINT Ssao::SsaoMapHeight() const
{
	return mRenderTargetHeight / 2;
}

void Ssao::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
	std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);	//将我们计算好的偏移们拷贝到offsets中
}

std::vector<float> Ssao::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	int blurRadius = (int)ceil(2.0f * sigma);	//我们根据sigma计算出blur的半径(向上取整)

	assert(blurRadius <= MaxBlurRadius);

	std::vector<float> weights;
	weights.resize(2 * blurRadius * 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;
		weights[i + blurRadius] = expf(-x * x / twoSigma2);
		weightSum += weights[i + blurRadius];
	}

	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}

ID3D12Resource* Ssao::NormalMap()
{
	return mNormalMap.Get();
}

ID3D12Resource* Ssao::AmbientMap()
{
	return mAmbientMap0.Get();	//我们这里返回的是AmbientMap0, 因为我们内部才用了1
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::NormalMapRtv() const
{
	return mhNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::NormalMapSrv() const
{
	return mhNormalMapGpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::AmbientMapSrv() const
{
	return mhAmbientMap0GpuSrv;
}

void Ssao::BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize)
{
	//我们假定了App传递来的SRV描述符的顺序(CPU侧和GPU顺序当然是完全对应的): AmbientMap0, AmbientMap1, normalMap, depthMap, randomVectorMap
	//而Rtv描述符的顺序则为: normalMap, AmbientMap0, AmbientMap1 这里normalMap跑到了前面!
	mhAmbientMap0CpuSrv = hCpuSrv;
	mhAmbientMap1CpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhDepthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhRandomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

	mhAmbientMap0GpuSrv = hGpuSrv;
	mhAmbientMap1GpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhDepthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
	mhRandomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

	mhNormalMapCpuRtv = hCpuRtv;
	mhAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);
	mhAmbientMap1CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

	//然后, 我们构建描述符(上面只是指定了描述符的位置, 但是描述符还没有和资源实际绑定!) 要绑定, 我们需要知道资源缓冲区的位置!
	RebuildDescriptors(depthStencilBuffer);
}

void Ssao::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;	//先创建srv用的, 可以看到我们在BuildDescriptors方法中指定的顺序, 依序创建即可.
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//我们创建的srv, 视图纬度统一为Texture2D
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//其采样顺序为默认熟悉怒
	srvDesc.Texture2D.MipLevels = 1;	//其没有mip(miplevel为0)
	srvDesc.Texture2D.MostDetailedMip = 0;	//最精细的mip层级即为0

	srvDesc.Format = AmbientMapFormat;
	md3dDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);	//我们按照BuildDescriptors方法中指定的顺序依次创建, 需要注意中间可能需要更换format. 创建时, 我们将cpu这边的handle与resource进行绑定, 其视图为我们刚刚传递的视图
	md3dDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);

	srvDesc.Format = NormalMapFormat;
	md3dDevice->CreateShaderResourceView(mNormalMap.Get(), &srvDesc, mhNormalMapCpuSrv);

	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;	//开始创建depth的srv描述符, 其格式为R24_UNORM_X8_TYPELESS
	md3dDevice->CreateShaderResourceView(depthStencilBuffer, &srvDesc, mhDepthMapCpuSrv);

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//开始创建RandomVectorMap的srv描述符, 其为R8G8B8A8_UNORM
	md3dDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &srvDesc, mhRandomVectorMapCpuSrv);

	//然后创建rtv用的描述符, rtv有三个, 同样可以按照顺序来
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	//其视图维度同样为Texture2D
	rtvDesc.Texture2D.MipSlice = 0;	//其mip切片为0
	rtvDesc.Texture2D.PlaneSlice = 0;	//其plane切片同样为0

	rtvDesc.Format = NormalMapFormat;
	md3dDevice->CreateRenderTargetView(mNormalMap.Get(), &rtvDesc, mhNormalMapCpuRtv);

	rtvDesc.Format = AmbientMapFormat;
	md3dDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);
	md3dDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
}

void Ssao::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
	mSsaoPso = ssaoPso;
	mBlurPso = ssaoBlurPso;
}

void Ssao::OnResize(UINT newWidth, UINT newHeight)
{
	if (mRenderTargetWidth != newWidth || mRenderTargetHeight != newHeight)	//提前退出
	{
		mRenderTargetWidth = newWidth;	//我们修改渲染对象的宽高
		mRenderTargetHeight = newHeight;

		mViewport.TopLeftX = 0.0f;	//设置视口, 其为全屏大小, 深度从0到1没有任何剔除, 但是其宽高仅为渲染对象的一半, 从而节省性能
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = mRenderTargetWidth / 2;
		mViewport.Height = mRenderTargetHeight / 2;
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, (int)mRenderTargetWidth / 2, (int)mRenderTargetHeight / 2 };	//设置其裁剪矩阵, 同样的, 我们也什么都不裁剪

		BuildResources();	//构建资源
	}
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
	//如果之前已有资源, 则将其 free
	mNormalMap = nullptr;
	mAmbientMap0 = nullptr;
	mAmbientMap1 = nullptr;

	D3D12_RESOURCE_DESC texDesc;	//我们先构建资源描述
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));	//将资源描述清空
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//其视图为2D
	texDesc.Alignment = 0;	//其在数组中的偏移量为0
	texDesc.DepthOrArraySize = 1;	//其所在的数组大小为1, 即没有多个资源并列
	texDesc.MipLevels = 1;	//其Mip层级为1, 表明我们没有Mip
	texDesc.SampleDesc.Count = 1;	//多重采样时, 我们也仅仅采样1个像素
	texDesc.SampleDesc.Quality = 0;	//表明其质量为最低的
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//我们无需定制其布局, 由硬件选择即可
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;	//其允许作为渲染对象. 因为无论是NormalMap还是AmbientMap, 都是由GPU计算得到的

	texDesc.Width = mRenderTargetWidth;	//其宽高为正常的渲染视口大小(由此我们知道其现在是NormalMap, 而非AmbientMap)
	texDesc.Height = mRenderTargetHeight;
	texDesc.Format = Ssao::NormalMapFormat;	//其格式为NormalMap的, 这也很明显
	float normalClearValue[4]{ 0.0f, 0.0f, 1.0f, 0.0f };	//对于normal, 我们将其重置为竖直指向正上方(0, 0, 1)
	CD3DX12_CLEAR_VALUE optClear{ NormalMapFormat, normalClearValue};	//设置重置时的clearValue
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mNormalMap)));	//创建实际的NormalMap对应的Resource, 其为默认的heap, flag为none, 说明为我们上面的texDesc, 默认状态为只读, 重置值为optClear, 句柄创建到mNormalMap

	texDesc.Width = mRenderTargetWidth / 2;	//我们开始创建遮蔽率图. 其宽高各为渲染对象的一半
	texDesc.Height = mRenderTargetHeight / 2;
	texDesc.Format = Ssao::AmbientMapFormat;	//其格式同样为AmbientMap的格式
	float ambientClearValue[4] = { 1.0f, 1.0f, 1.0f, 1.0f };	//对于遮蔽率图, 我们将其重置为(1, 1, 1), 表示其被完全遮挡
	optClear = CD3DX12_CLEAR_VALUE(AmbientMapFormat, ambientClearValue);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mAmbientMap0)));	//创建遮蔽率图0，其同样为默认的heap, flag为none, 说明为我们上面的texDesc, 默认状态为只读, 重置值为optClear, 句柄位于AmbientMap0
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mAmbientMap1)));	//创建遮蔽率图0，其同样为默认的heap, flag为none, 说明为我们上面的texDesc, 默认状态为只读, 重置值为optClear, 句柄位于AmbientMap1
}

void Ssao::BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_DESC texDesc;	//创建RandomVectorMap的描述, 之所以在这里而不是在RebuildDescriptors中, 是因为随机采样贴图只需要创建一次, 其大小不和渲染对象的尺寸绑定
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 256;
	texDesc.Height = 256;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//其格式为R8G8B8A8_UNORM, 和描述符对应
	texDesc.SampleDesc.Count = 1;	//多重采样时采样的像素数量
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//其布局我们无须指定, 由硬件确认
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;	//其没有Flag

	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mRandomVectorMap)));	//我们不为其指定重置默认值, 因为我们并不期望其被重置

	//我们创建一个上传堆, 然后为每个分辨率创建一个随机向量并存入堆，然后再将堆上传到对应RandomVectorMap的位置
	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, num2DSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));	//我们创建一个上传堆用的资源, 其type为upload, 其desc为upload对应的desc

	XMCOLOR initData[256 * 256];	//准备填充和RandomVectorMap分辨率相同大小的像素
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			initData[i * 256 + j] = XMCOLOR(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF(), 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData;	//我们创建要上传的资源
	subResourceData.pData = initData;
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);	//该资源的行大小为256 * colorsize(每行有256个Color)
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;	//该资源的slice大小为行大小*256

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));	//我们将RandomVectorMap设置为拷贝目的
	UpdateSubresources(cmdList, mRandomVectorMap.Get(), mRandomVectorMapUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData); //我们将我们创建的initData上传到RandomVectorMap中
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));	//我们再将RandomVectorMap重新设为只读
}

void Ssao::BuildOffsetVectors()
{
	//创建14个随机采样向量
	//8个点用于记录Cube的8个顶点
	mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);
	mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);
	mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);
	mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	//6个点用于记录Cube的6个面的中心
	mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);
	mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		float s = MathHelper::RandF(0.25f, 1.0f);	//随机出我们要采样的距离
		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));
		XMStoreFloat4(&mOffsets[i], v);	//我们将方向乘以距离后, 重新存入数组
	}
}
