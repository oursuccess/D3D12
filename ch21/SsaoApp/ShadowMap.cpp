#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height) :
    md3dDevice(device), mWidth(width), mHeight(height), mViewPort({0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f}),
    mScissorRect({0, 0, (int)width, (int)height})
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
    return mViewPort;
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
    if (mWidth != newWidth || mHeight != newHeight) 
    {
        mWidth = newWidth;
        mHeight = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void ShadowMap::BuildDescriptors()
{
    //构建着色器资源视图，从而允许我们可以用其对阴影图进行采样
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; //描述我们应当对该着色器资源的RGBA的哪些分部进行采样. 我们对RGBA均进行采样
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; //描述该着色器资源的格式. 阴影图只需要单通道。 因此我们使用24位存储R值，剩下8位不使用
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;  //描述该着色器资源的纬度。 阴影图为2D图
    srvDesc.Texture2D.MostDetailedMip = 0;  //描述该2D着色器资源LOD最高的层级
    srvDesc.Texture2D.MipLevels = 1;    //默认该2D着色器资源的Mip层级数量
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;   //默认该2D着色器资源允许的最小的LOD值。 我们采样的值不能比该值小
    srvDesc.Texture2D.PlaneSlice = 0;   //描述该2D着色器资源使用的平面的索引值
    md3dDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mhCpuSrv); //创建着色器资源视图

    //构建保存深度信息的深度/模板视图
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;  //描述该深度视图的纬度。 2D
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //描述该深度视图的格式。 我们使用24位存储深度，使用8位存储模板
    dsvDesc.Texture2D.MipSlice = 0; //描述该深度图的最小索引值
    md3dDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mhCpuDsv);
}

void ShadowMap::BuildResource()
{
    //我们无法压缩资源，因为压缩格式无法用于随机采样
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mShadowMap)));

}
