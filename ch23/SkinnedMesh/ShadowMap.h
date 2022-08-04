#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"

enum class CubeMapFace : int	//立方体的6个面, 可以通过enum进行索引。 其顺序依次为+x, -x, +y, -y, +z, -z
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
	ShadowMap(ID3D12Device* device, UINT width, UINT height);	//构造函数, 我们需要设备和视口长宽
	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width() const;	//分别返回视口的宽、长
	UINT Height() const;
	ID3D12Resource* Resource();	//返回我们持有的阴影图对应的资源. 其在GPU侧作为Srv被使用, 在Cpu侧作为Dsv被转换为Srv/被绘制. 这个资源是在ShadowMap中创建的! 其存在的意义在于从CPU侧作为从Dsv到CPUSrv的中转站. 我们之所以要向外部开放索引该资源的方法, 是因为现在绘制阴影图的调用在外部, 而在绘制之前, 我们需要先更改该资源的状态(从GENERIC_READ变为DEPTH_WRITE)
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;	//返回可以直接作为着色器资源视图的阴影图. 由于其可以作为着色器资源视图, 因此自然是GPU侧的. 我们还有一个Cpu侧的Srv, 其和Gpu侧的Srv对应, 用于从CPU侧提交到GPU侧. 而CPU侧的Srv位置则和Dsv相同, 因为我们返回的着色器资源实际上就是Dsv. 该方法并未使用. 之所以不使用, 是因为App.cpp中根本就知道我们Srv对应的偏移量!
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;	//返回可以直接作为深度/模板视图的阴影图. 由于其需要在设置后被传回, 然后更新成SRV, 因此是CPU侧的. 

	D3D12_VIEWPORT Viewport() const;	//返回视口
	D3D12_RECT ScissorRect() const;	//返回裁剪矩阵

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);	//构建描述符们.

	void OnResize(UINT newWidth, UINT newHeight);	//当尺寸变换时调用, 我们需要更新宽、长，并重建视口和描述符

private:
	void BuildDescriptors();	//使用给定的资源, 我们指定怎么描述这个资源. 在ShadowMap中，对于我们的资源, 有两种描述方式: 作为Srv，以及作为Dsv. 这两个描述符位置不同, 但是其描述的资源其实是同一份!
	void BuildResource();	//创建可以被描述的资源. 需要注意的是, 我们总是要先有资源才能决定如何描述!

private:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;	//该类型是用于Srv的类型说明. 在Dsv中, 其同样为R24G8,但是此时我们知道其24位用于Depth，为UNorm；8位用于Sample,为UINT

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;	//我们在Srv方法中描述了CpuSrv, GpuSrv和CpuDsv之间的关系
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};

