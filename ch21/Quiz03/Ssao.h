#ifndef SSAO_H
#define SSAO_H

#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "FrameResource.h"

//用于实现SSAO(环境光遮蔽)效果的类
//为了实现环境光遮蔽，我们在后处理时尝试还原出对应像素当前可视物体所在世界空间中的遮蔽率。 为此，我们需要深度图(这个可以和ShadowMap共用)，同时，我们还需要法线图用来计算朝向，然后将遮蔽率存储到另一张贴图(AmbientMap)中
//计算遮蔽率时，我们在当前位置周围的随机位置上进行采样，并计算随机位置对应的实际世界空间中是否可能对当前点产生了遮蔽, 因此还需要一个额外的随机采样位置数组
//为了避免边缘的粉刺，我们还需要使用一下模糊效果
class Ssao
{
public:
	Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
	Ssao(const Ssao& rhs) = delete;
	Ssao& operator=(const Ssao& rhs) = delete;
	~Ssao() = default;

	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;	//用来存储遮蔽率的贴图的格式。 我们只需要使用单通道即可
	static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;	//法线贴图的格式。 法线贴图存在RGB

	static const int MaxBlurRadius = 5;	//为了避免边缘处的粉刺，我们需要使用一下模糊, 这里我们定义了模糊的半径

	UINT SsaoMapWidth() const;	//分别返回环境光遮蔽返回的遮蔽率的贴图的宽高
	UINT SsaoMapHeight() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);	//获取我们对像素点计算遮蔽率时的偏移数组
	std::vector<float> CalcGaussWeights(float sigma);	//计算高斯模糊的权重

	ID3D12Resource* NormalMap();	//获取法线贴图
	ID3D12Resource* AmbientMap();	//获取遮蔽率贴图

	CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;	//获取作为RenderTarget的法线贴图。 从而允许其它app将法线数据渲染至此
	CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;	//获取作为着色器资源试图的法线贴图。 从而允许我们使用之来判断正向
	CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMapSrv() const;	//获取作为着色器资源视图的遮蔽率贴图，从而允许我们使用之来判断遮蔽率

	//构建环境光遮蔽需要的描述符， 为了构建描述符，我们需要: 深度/模板缓冲区，用来分别存放CPU和GPU端着色器资源视图的句柄，以及存放作为渲染目标的CPU端句柄，srv描述符大小、rtv描述符的大小
	void BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize);	

	void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);	//当深度模板缓冲区发生变化时，我们需要重新构建描述符

	void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);	//使用ssao和blur对应的PipelineState来设置流水线状态对象

	void OnResize(UINT newWidth, UINT newHeight);	//当分辨率发生变化时，我们需要重新构建描述符和贴图大小

	void ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);	//使用当前的命令列表、帧资源和模糊级数来计算ssao

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);	//模糊遮蔽率图
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);	//我们将x和y方向分别blur，从而将n*n的计算变为n+n

	void BuildResources();	//实际的资源构建
	void BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);	//构建采样贴图

	void BuildOffsetVectors();	//构建计算遮蔽率时对当前点周围进行偏移的采样方向

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSig;	//存储了根签名

	ID3D12PipelineState* mSsaoPso = nullptr;	//ssao对应的pso
	ID3D12PipelineState* mBlurPso = nullptr;	//blur对应的pso

	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;	//分别存储了随机采样点的长度!，法线图，两个遮蔽率图
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;	//之所以要两个遮蔽率图，是因为我们需要进行blur

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;	//分别为法线图、深度图、随机采样图、两个遮蔽率图对应的Srv与Rtv. 不需要在本对象中动态构建的对象不需要rtv
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;	//Srv有两个，一个是CPU一个是GPU.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;	//Rtv则只有一个CPU

	
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
#pragma region Quiz2103
	//我们需要将AmibentMap的Rtv替换为Uav.
    //CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuUav;
#pragma endregion

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
#pragma region Quiz2103
	//我们需要将AmibentMap的Rtv替换为Uav.
    //CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuUav;
#pragma endregion

	UINT mRenderTargetWidth;	//作为渲染对象的贴图的宽和高
	UINT mRenderTargetHeight;

	DirectX::XMFLOAT4 mOffsets[14];	//存储了采样点周围计算的随机点相对采样点的方向向量

	D3D12_VIEWPORT mViewport;	//分别存储了视口和裁剪矩形
	D3D12_RECT mScissorRect;
};

#endif
