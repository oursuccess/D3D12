#include "Ssao.h"

Ssao::Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) :
	md3dDevice(device)
{
	OnResize(width, height);

	BuildOffsetVectors();
	BuildRandomVectorTexture();
}

UINT Ssao::SsaoMapWidth() const
{
	return mRenderTargetWidth / 2;	//我们以实际分辨率的一半来创建环境光遮蔽图，从而减少性能开销
}

UINT Ssao::SsaoMapHeight() const
{
	return mRenderTargetHeight / 2; //我们以实际分辨率的一半来创建环境光遮蔽图，从而减少性能开销
}

void Ssao::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
	std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);	//将mOffsets拷贝到offsets中
}

std::vector<float> Ssao::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	int blurRadius = (int)ceil(2.0f * sigma);	//以sigma来模拟blur的半径

	assert(blurRadius <= MaxBlurRadius);	//不允许blur半径超过最大允许的半径值

	std::vector<float> weights(2 * blurRadius + 1);	//开始计算权重。 横向和纵向的权重是相同的, 因此只需要算一个方向的即可
	float weightSum = 0.0f;	//计算总权重

	for (int i = -blurRadius; i <= blurRadius; ++i)		//这儿其实也是没必要的。 因为左右也是对称的，因此我们大可一次计算直接赋值左右两边
	{
		float x = (float)i;
		weights[i + blurRadius] = expf(-x * x / twoSigma2);	//高斯函数的计算方法, e^(i*i / (2n*n)) / weightSum
		weightSum += weights[i + blurRadius];
	}

	//除以总权重，从而让所有的权重和为1
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
	return mAmbientMap0.Get();	//我们返回的是AmbientMap0, 因为1只是用来模糊的时候内部使用的
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::NormalMapRtv() const	//下面三个都是相同，返回对应的句柄即可
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

void Ssao::BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,	
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize)	//将调用者给出的Srv,Rtv句柄与Ssao需要的进行关联
{
	//在Ssao中，需要5个Srv和2个Rtv的堆空间。 而这需要调用该方法的程序预留
	mhAmbientMap0CpuSrv = hCpuSrv;	//注意Srv的顺序, 遮蔽图0-->遮蔽图1-->法线图-->深度图-->随机采样图. 而在Rtv上，这个顺序变为了法线图-->遮蔽图0-->遮蔽图1
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

	RebuildDescriptors(depthStencilBuffer);	//实际进行重建描述符
}

void Ssao::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)	//进行描述符指向区域的Srv、Rtv的创建
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//设置采样方式。 我们允许对4个通道进行采样
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//我们指明我们需要的为2D纹理图
	srvDesc.Format = NormalMapFormat;	//我们为法线贴图指定的格式为R16G16B16A16
	srvDesc.Texture2D.MostDetailedMip = 0;	//我们指定LOD的初始层级为0，LOD层数为1
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mNormalMap.Get(), &srvDesc, mhNormalMapCpuSrv);	//根据srvDesc来创建法线贴图的描述符，其资源在mNormalMap中持有，而其对应的句柄则为mhNormalMapCpuSrv. 为什么我们要指定? 是因为我们就是要用特定的资源来创建到GPU中

	//此后，我们不断更新格式, 并分别创建相应的srv
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;	//深度图只需要R24通道	
	md3dDevice->CreateShaderResourceView(depthStencilBuffer, &srvDesc, mhRandomVectorMapCpuSrv);	//由于深度图并不是我们创建，且我们也不需要在SsaoCPU侧使用，因此我们不需要持有其资源，只需要持有其句柄即可

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//偏移向量纹理图的格式为R8B8G8A8
	md3dDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &srvDesc, mhRandomVectorMapCpuSrv);

	srvDesc.Format = AmbientMapFormat;	//遮蔽率贴图的格式只需要R16通道
	md3dDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);	//遮蔽率贴图有两张
	md3dDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};	//然后我们创建rtv
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	//我们指定需要的是2D的渲染目标图
	rtvDesc.Format = NormalMapFormat;	//其格式和NormalMap相同
	rtvDesc.Texture2D.MipSlice = 0;	//指定我们在渲染到该渲染目标的时候，渲染到的Mip层级
	rtvDesc.Texture2D.PlaneSlice = 0;	//指定我们在渲染到该渲染目标的时候，渲染到的下标
	md3dDevice->CreateRenderTargetView(mNormalMap.Get(), &rtvDesc, mhNormalMapCpuRtv);	//根据rtvDesc创建实际的法线纹理描述符, 其资源我们指定在mNormalMap中

	rtvDesc.Format = AmbientMapFormat;
	md3dDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);	//然后我们创建遮蔽图渲染对象视图. 其同样有两个
	md3dDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
}

void Ssao::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
	mSsaoPso = ssaoPso;
	mBlurPso = ssaoBlurPso;
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
