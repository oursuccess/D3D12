/* 
	Quiz02: 将彩色立方体演示程序修改为使用两个顶点缓冲区(以及两个输入槽)来向渲染流水线传送顶点数据。
*/

#include "../../QuizCommonHeader.h"
#include "Quiz02Geometry.h"

/*
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};
*/

struct VPosData {
	XMFLOAT3 Pos;
};

struct VColorData {
	XMFLOAT4 Color;
};

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class JeBoxApp : public D3DApp
{
public:
	JeBoxApp(HINSTANCE hInstance);
	JeBoxApp(const JeBoxApp& rhs) = delete;
	JeBoxApp& operator=(const JeBoxApp& rhs) = delete;
	~JeBoxApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

private:
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	std::unique_ptr<ch06::MeshGeometry> mBoxGeo = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4(); 
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	//enable runtime memory check for debug builds
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try 
	{
		JeBoxApp theApp(hInstance);
		if (!theApp.Initialize()) return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

JeBoxApp::JeBoxApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

JeBoxApp::~JeBoxApp()
{
}

bool JeBoxApp::Initialize()
{
	if (!D3DApp::Initialize()) return false;

	//reset the command list to prep for initialization commands
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();

	//execute the initialization commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

	//wait until initialization is complete
	FlushCommandQueue();

	return true;
}

void JeBoxApp::OnResize()
{
	D3DApp::OnResize();

	//the window resized, so update the aspect ratio and recompute the projection matrix
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void JeBoxApp::Update(const GameTimer& gt)
{
	//convert sperical to cartesian coordinates
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	//build the view matrix
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	//update the constant buffer with the latest worldViewProj matrix
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mObjectCB->CopyData(0, objConstants);
}

void JeBoxApp::Draw(const GameTimer& gt)
{
	//reuse the memory associated with command recording
	//we can only reset when the associated command list have finished execution on the GPU
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	//a command list can be reset after it has beend added to the command queue via ExecuteCommandList
	//reusing the command list reuses memory
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//clear the back buffer and depth buffer
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	//specify the buffers we are going to render to
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	//specify the buffers we are going to render to
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->PositionBufferView());
	mCommandList->IASetVertexBuffers(1, 1, &mBoxGeo->ColorBufferView());

	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

	//indicate a state transition on the resource usage

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//done recording commands
	ThrowIfFailed(mCommandList->Close());

	//Add the command list to the queue for execution
	ID3D12CommandList* cmdsList[] = { mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

	//swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	//wait until frame commands are complete. this waiting is ineffient and is done for simplicity.
	FlushCommandQueue();
}

void JeBoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void JeBoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void JeBoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;

}

void JeBoxApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void JeBoxApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
	//offset to the ith object constant buffer in the buffer
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void JeBoxApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	//create a single descriptor table of CBVs
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	//a root signature is an array of root parameters
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	//create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void JeBoxApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		/*
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		*/
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void JeBoxApp::BuildBoxGeometry()
{
	/*
	std::array<Vertex, 8> vertices =
	{
		Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
		Vertex({XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
		Vertex({XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan)}),
		Vertex({XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta)})
	};
	*/
	std::array<VPosData, 8> poss = {
		VPosData({XMFLOAT3(-1.0f, -1.0f, -1.0f)}),
		VPosData({XMFLOAT3(-1.0f, +1.0f, -1.0f)}),
		VPosData({XMFLOAT3(+1.0f, +1.0f, -1.0f)}),
		VPosData({XMFLOAT3(+1.0f, -1.0f, -1.0f)}),
		VPosData({XMFLOAT3(-1.0f, -1.0f, +1.0f)}),
		VPosData({XMFLOAT3(-1.0f, +1.0f, +1.0f)}),
		VPosData({XMFLOAT3(+1.0f, +1.0f, +1.0f)}),
		VPosData({XMFLOAT3(+1.0f, -1.0f, +1.0f)}),
	};

	std::array<VColorData, 8> colors = {
		VColorData{XMFLOAT4(Colors::White)},
		VColorData{XMFLOAT4(Colors::Black)},
		VColorData{XMFLOAT4(Colors::Red)},
		VColorData{XMFLOAT4(Colors::Green)},
		VColorData{XMFLOAT4(Colors::Blue)},
		VColorData{XMFLOAT4(Colors::Yellow)},
		VColorData{XMFLOAT4(Colors::Cyan)},
		VColorData{XMFLOAT4(Colors::Magenta)}
	};

	std::array<std::uint16_t, 36> indices =
	{
		//front face
		0, 1, 2,
		0, 2, 3,
		//back face
		4, 6, 5,
		4, 7, 6,
		//left face
		4, 5, 1,
		4, 1, 0,
		//right face
		3, 2, 6,
		3, 6, 7,
		//top face
		1, 5, 6,
		1, 6, 2,
		//bottom face
		4, 0, 3,
		4, 3, 7,
	};

	//const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT pbByteSize = (UINT)poss.size() * sizeof(VPosData);
	const UINT cbByteSize = (UINT)colors.size() * sizeof(VColorData);
	const UINT vbByteSize = pbByteSize + cbByteSize;
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<ch06::MeshGeometry>();
	mBoxGeo->Name = "boxGeo";

	//ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	//CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	//CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), poss.data(), pbByteSize);
	ThrowIfFailed(D3DCreateBlob(pbByteSize, &mBoxGeo->PositionBufferCPU));
	CopyMemory(mBoxGeo->PositionBufferCPU->GetBufferPointer(), poss.data(), pbByteSize);
	ThrowIfFailed(D3DCreateBlob(cbByteSize, &mBoxGeo->ColorBufferCPU));
	CopyMemory(mBoxGeo->ColorBufferCPU->GetBufferPointer(), colors.data(), cbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);
	mBoxGeo->PositionBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), poss.data(), pbByteSize, mBoxGeo->PositionBufferUploader);
	mBoxGeo->ColorBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), colors.data(), cbByteSize, mBoxGeo->ColorBufferUploader);

	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

	//mBoxGeo->VertexByteStride = sizeof(Vertex);
	//mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->PositionByteStride = sizeof(VPosData);
	mBoxGeo->PositionBufferByteSize = pbByteSize;
	mBoxGeo->ColorByteStride = sizeof(VColorData);
	mBoxGeo->ColorBufferByteSize = cbByteSize;

	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mBoxGeo->DrawArgs["box"] = submesh;
}

void JeBoxApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
