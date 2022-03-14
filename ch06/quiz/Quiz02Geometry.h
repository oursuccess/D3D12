#pragma once

#include "../../QuizCommonHeader.h"

namespace ch06 {
	struct MeshGeometry
	{
		// Give it a name so we can look it up by name.
		std::string Name;

		// System memory copies.  Use Blobs because the vertex/index format can be generic.
		// It is up to the client to cast appropriately.  
		//Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> PositionBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> ColorBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

		//Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> PositionBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

		//Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> PositionBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

		// Data about the buffers.
		//UINT VertexByteStride = 0;
		//UINT VertexBufferByteSize = 0;
		UINT PositionByteStride = 0;
		UINT PositionBufferByteSize = 0;
		UINT ColorByteStride = 0;
		UINT ColorBufferByteSize = 0;

		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
		UINT IndexBufferByteSize = 0;

		// A MeshGeometry may store multiple geometries in one vertex/index buffer.
		// Use this container to define the Submesh geometries so we can draw
		// the Submeshes individually.
		std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

		/*
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.StrideInBytes = VertexByteStride;
			vbv.SizeInBytes = VertexBufferByteSize;

			return vbv;
		}
		*/
		D3D12_VERTEX_BUFFER_VIEW PositionBufferView() const
		{
			D3D12_VERTEX_BUFFER_VIEW res;
			res.BufferLocation = PositionBufferGPU->GetGPUVirtualAddress();
			res.StrideInBytes = PositionByteStride;
			res.SizeInBytes = PositionBufferByteSize;
			return res;
		}

		D3D12_VERTEX_BUFFER_VIEW ColorBufferView() const
		{
			D3D12_VERTEX_BUFFER_VIEW res;
			res.BufferLocation = ColorBufferGPU->GetGPUVirtualAddress();
			res.StrideInBytes = ColorByteStride;
			res.SizeInBytes = ColorBufferByteSize;
			return res;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		/*
		// We can free this memory after we finish upload to the GPU.
		void DisposeUploaders()
		{
			VertexBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
		*/
		void DisposeUploaders()
		{
			PositionBufferUploader = nullptr;
			ColorBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
	};
}

