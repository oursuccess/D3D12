#ifndef SSAO_H
#define SSAO_H

#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "FrameResource.h"

class Ssao
{
public:
	Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);

private:
	ID3D12Device* md3dDevice;	//记录设备

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSig;	//记录Ssao根签名

	ID3D12PipelineState* mSsaoPSO = nullptr;	//PSO for Ssao
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

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
};

#endif
