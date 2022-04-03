#pragma once

#include "../../d3d12book-master/Common/d3dUtil.h"
#include "../../d3d12book-master/Common/MathHelper.h"
#include "../../d3d12book-master/Common/UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
};

class FrameResource
{
};

