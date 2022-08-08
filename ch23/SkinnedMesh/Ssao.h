#ifndef SSAO_H
#define SSAO_H

#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "FrameResource.h"

class Ssao
{
public:
	Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
	Ssao(const Ssao& rhs) = delete;
	Ssao& operator=(const Ssao& rhs) = delete;
	~Ssao() = default;

	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;	//对于Ssao中遮蔽率的部分, 我们只需要使用16位的整数即可表示
	static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;	//对于法线, 我们使用16位的RGBAFloat来表示

	static const int MaxBlurRadius = 5;	//对于模糊, 我们计算5次, 来防止过度的锐化

	UINT SsaoMapWidth() const;	//计算Ssao图的宽与高
	UINT SsaoMapHeight() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);	//获取14个偏移向量. 我们手动写出这些偏移向量, 从而防止随机采样时方向不同的问题
	std::vector<float> CalcGaussWeights(float sigma);	//计算在半径为sigma时的高斯模糊在每个距离上的权重

	ID3D12Resource* NormalMap();	//分别用于获取法线图和遮蔽率图的资源
	ID3D12Resource* AmbientMap();

	CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;	//当需要获取法线图作为渲染对象时, 我们一定将其指明为了CPU侧的资源
	CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;	//当需要获取法线图作为着色器资源时, 我们一定将其指明为了GPU侧的资源
	CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMapSrv() const;	//当我们获取遮蔽率图作为着色器资源时, 同样的, 我们一定将其指明为了GPU侧的资源

	void BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT srvDescriptorSize);	//创建描述符们. 我们需要CPU侧的Srv和Rtv, GPU侧的Srv, 以及资源的起始位置, 每个资源的大小

	void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);	//重建描述符

	void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);	//设置流水线状态对象们. SSAO中有两种流水线状态对象, 分别为SSAO与Blur

	void OnResize(UINT newWidth, UINT newHeight);	//当窗口变大变更时, 我们重新设置

	void ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);	//计算Ssao. 我们在这里顺手也把模糊给做了

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);	//尝试模糊. 用于ComputeSsao中的调用
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);	//模糊, 根据当前为横向还是纵向, 我们需要设置不同的Srv和Rtv

	void BuildResources();	//构建资源. 我们在公开方法中已经设置了资源位置和Srv,Rtv的位置, 在这里进行实际的创建
	void BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);	//构建采样时每个位置的随机采样向量大小

	void BuildOffsetVectors();	//构建偏移向量们. 这里构建了14个向量

private:
	ID3D12Device* md3dDevice;	//记录设备

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSig;	//记录Ssao根签名

	ID3D12PipelineState* mSsaoPso = nullptr;	//PSO for Ssao
	ID3D12PipelineState* mBlurPso = nullptr;	//PSO for Blur

	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;	//随机采样的贴图. 用于记录随机采样向量在每个方向长度的
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;	//用于负责将贴图进行上传的缓冲区
	Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;	//法线贴图对应的资源
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;	//和1一样, 两个漫反射贴图, 用于记录每个像素点的遮蔽率(不一定是每个像素都和屏幕一一对应, 也可能一个像素对应屏幕的多个像素点)
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;	//用于法线贴图的Cpu侧的着色器资源视图
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;	//用于法线贴图的Gpu侧的着色器资源视图. 当GPU需要法线贴图时， 通过该句柄进行访问
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;	//用于法线贴图的Cpu侧的渲染对象视图. 我们肯定是将资源渲染回CPU了, 才需要在Cpu侧记录

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;	//用于深度贴图的Cpu侧的着色器资源视图
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;	//用于深度贴图的Gpu侧的着色器资源视图

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;	//用于Cpu侧的随机采样向量的贴图
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;	//用于两个轮换的遮蔽率计算的贴图中的首个的Cpu侧的着色器资源视图
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;	//和上面对应, GPU侧
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;	//和遮蔽图0对应的渲染对象

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;	//用于上面两个轮换的遮蔽率计算的贴图的第二个的Cpu侧的着色器资源视图
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	UINT mRenderTargetWidth;	//记录渲染对象的大小
	UINT mRenderTargetHeight;

	DirectX::XMFLOAT4 mOffsets[14];	//预先定义好的14个随机采样向量. 但是仅仅记录了方向

	D3D12_VIEWPORT mViewport;	//视口
	D3D12_RECT mScissorRect;	//裁剪矩阵
};

#endif
