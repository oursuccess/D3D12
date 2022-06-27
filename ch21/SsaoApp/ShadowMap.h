#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"

//用于记录当前所在的立方体图的面, 分别表示与+x, -x, +y, -y等相交的面。 坐标系为左手坐标系(参见18.1)
enum class CubeMapFace : int
{
	PositiveX = 0,	
	NegativeX = 1,
	PositiveY = 2,
	NegativeY = 3,
	PositiveZ = 4,
	NegativeZ = 5,
};

class ShadowMap
{
public:
	//创建指定长宽的阴影纹理图
	ShadowMap(ID3D12Device* device, UINT width, UINT height);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width() const;
	UINT Height() const;  //分别获取阴影纹理图的宽度和高度
	ID3D12Resource* Resource(); //获取阴影纹理图对应的GPU资源(ID3D12Resource)
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;	//获取阴影纹理对应的着色器资源视图
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;	//获取阴影纹理对应的深度/模板视图

	D3D12_VIEWPORT Viewport() const;	//获取当前渲染视窗
	D3D12_RECT ScissorRect() const;	//获取当前的裁剪矩形

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);	//根据指定的Cpu着色器资源视图、Gpu着色器资源视图、Cpu深度/模板视图构建描述符堆

	void OnResize(UINT newWidth, UINT newHeight);	//当阴影纹理的宽高变化时调用

private:
	void BuildDescriptors();	//构建描述符堆
	void BuildResource();	//构建该阴影纹理所需的资源

private:
	ID3D12Device* md3dDevice = nullptr;
	D3D12_VIEWPORT mViewPort;
	D3D12_RECT mScissorRect;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;

};

