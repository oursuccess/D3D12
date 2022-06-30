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

void Ssao::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)	//记录流水线状态对象
{
	mSsaoPso = ssaoPso;
	mBlurPso = ssaoBlurPso;
}

void Ssao::OnResize(UINT newWidth, UINT newHeight)	//当分辨率变化时，同步更新视口、裁剪矩形、渲染对象分辨率，然后重建资源
{
	if (mRenderTargetWidth != newWidth || mRenderTargetHeight != newHeight) 
	{
		mRenderTargetWidth = newWidth;
		mRenderTargetHeight = newHeight;

		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = mRenderTargetWidth / 2.0f;	//我们以实际分辨率的一半构建遮蔽率图
		mViewport.Height = mRenderTargetHeight / 2.0f;	
		mViewport.MinDepth = 0.0f;	//在视锥体空间中，低于该depth的将被剔除. 最小值为0
		mViewport.MaxDepth = 1.0f;	//在视锥体空间中，高于该depth的将被剔除. 最大值为1

		mScissorRect = { 0, 0, (int)mRenderTargetWidth / 2, (int)mRenderTargetHeight / 2};	//我们的裁剪矩阵同样是实际分辨率的一半

		BuildResources();
	}
}

void Ssao::ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
{
	cmdList->RSSetViewports(1, &mViewport);	//将命令列表的视口与裁剪矩阵设为ssao所需
	cmdList->RSSetScissorRects(1, &mScissorRect);	//1表示我们只有一个裁剪矩阵

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(), 
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));	//将遮蔽率图指向的资源的状态从只读改为渲染对象

	float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };	//指定我们将遮蔽率图清除为什么. 尽管我们只用到了R16，但是我们在清除的时候，还是需要传入RGBA
	cmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);	//我们清除的是RenderTargetView，因此自然是传入Rtv

	//OM: Output-Merger 输出合并阶段, 在此阶段我们才可能设置渲染对象
	cmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);	//设置渲染对象, 我们将渲染对象数量设置为1，传入遮蔽图1的地址，并声称我们传入的地址们是连续的(尽管由于只传入了1个，因此不需要额外指定)，最后，由于我们不需要实际持有该资源，因此由底层自行创建即可

	auto ssaoCBAddress = currFrame->SsaoCB->Resource()->GetGPUVirtualAddress();	//我们从当前帧资源中获取ssao所需的常量缓冲区的地址，并在命令行列表中将其绑定到寄存器的编号0开始
	cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);
	cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);	//然后，我们传入一个新的根常量，该常量绑定到寄存器的编号1

	cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);	//然后，在寄存器的编号2上，我们将我们的法线贴图绑定到描述符表
	cmdList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv); //在寄存器的编号3上，我们将我们的随机向量采样贴图绑定到描述符表

	cmdList->SetPipelineState(mSsaoPso);	//在所需的资源绑定完毕后，我们即可设置渲染状态

	//IA: Input-Assembler 输入装配阶段, 在此阶段我们传入需要处理的顶点和材质们
	cmdList->IASetVertexBuffers(0, 0, nullptr);	//我们不需要传入顶点和索引， 因为我们只需要使用法线贴图、深度图、随机向量图、世界矩阵、投影矩阵来进行计算
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	//将顶点的拓扑修改为三角形列表
	cmdList->DrawInstanced(6, 1, 0, 0);	//简单画一个全屏的四边形

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));	//在绘制调用后，我们可以将遮蔽图0修改为只读状态了

	BlurAmbientMap(cmdList, currFrame, blurCount);	//开始模糊，从而减少毛刺现象
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
{
	cmdList->SetPipelineState(mBlurPso);	//将流水线状态改为blur

	auto ssaoCBAddress = currFrame->SsaoCB->Resource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);	//设置模糊所需要的常量缓冲区视图

	for (int i = 0; i < blurCount; ++i)
	{
		BlurAmbientMap(cmdList, true);	//我们将Blur分为横向和纵向两部分, 其除了合并的位置，其它方向的计算相同
		BlurAmbientMap(cmdList, false);
	}
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur)
{
	ID3D12Resource* output = nullptr;	//定义我们的输入、输出对象，以及过程中所需要的资源
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	if (horzBlur == true)	//如果我们横向模糊，则我们使用ambientMap1，同时我们以AmbientMap0的GpuSrv作为输入，将其输出到mhAmbientMap1CpuRtv中
	{
		output = mAmbientMap1.Get();
		inputSrv = mhAmbientMap0GpuSrv;
		outputRtv = mhAmbientMap1CpuRtv;
	}
	else //否则，我们使用ambient0进行存储，并且使用AmbientMap1的GpuSrv作为输入，将其输出到AmbientMap0中。 从这里，我们可以看出，我们要先进行横向模糊，再纵向模糊。 因为初始时我们只渲染了AmbientMap0, 最后又把数据写回到了AbmientMap0
	{
		output = mAmbientMap0.Get();
		inputSrv = mhAmbientMap1GpuSrv;
		outputRtv = mhAmbientMap0CpuRtv;
	}

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));	//我们将output的状态从只读变为渲染对象. 因为我们在其它对该对象的操作离开前都将其变回了Generic_Read

	float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);	//我们清除我们渲染目标的当前值. 我们在以0为输入的时候清除了1，在以1为输入的时候清除了0，因此不会错误清除

	cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);	//设置渲染目标

	//由于在ComputeSsao中我们绑定了法线贴图，因此事实上这一行是可以不存在的. 但这里是为了减少前面一定要是ComputeSsao的假设
	cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

	cmdList->SetGraphicsRootDescriptorTable(3, inputSrv); //以我们现在所需要的输入(遮蔽率图0/1来作为着色器资源，来计算环境光遮蔽)

	//下面这3行感觉也是不需要的， 因为我们在ComputeSsao中就是如此. 但是这是为了减少对ComputSsao的依赖
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));	//然后将渲染目标的状态再变回Generic Read
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
