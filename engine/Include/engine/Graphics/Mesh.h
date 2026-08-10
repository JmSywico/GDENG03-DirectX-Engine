#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <cstdint>

#include "VertexTypes.h"

class Mesh
{
public:
	Mesh() = default;
	~Mesh();

	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	bool Initialize(const void* vertexData, uint32_t vertexBufferSize, uint32_t stride);
	bool InitializeIndexed(
		const void* vertexData,
		uint32_t vertexBufferSize,
		uint32_t stride,
		const void* indexData,
		uint32_t indexBufferSize,
		bool indices32Bit = true);

	ID3D11Buffer* GetVertexBuffer() const { return m_vertexBuffer.Get(); }
	ID3D11Buffer* GetIndexBuffer() const { return m_indexBuffer.Get(); }
	bool HasIndices() const { return m_indexBuffer != nullptr; }
	uint32_t GetIndexCount() const { return m_indexCount; }
	uint32_t GetVertexCount() const { return m_vertexCount; }
	uint32_t GetVertexStride() const { return m_vertexStride; }
	DXGI_FORMAT GetIndexFormat() const { return m_indexFormat; }
	const DirectX::XMFLOAT3& GetBoundsMin() const { return m_boundsMin; }
	const DirectX::XMFLOAT3& GetBoundsMax() const { return m_boundsMax; }

private:
	void UpdateBoundsFromVertices(const void* vertexData, uint32_t vertexCount, uint32_t stride);
	void Release();

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
	uint32_t m_vertexCount = 0;
	uint32_t m_indexCount = 0;
	uint32_t m_vertexStride = 0;
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;
	DirectX::XMFLOAT3 m_boundsMin = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 m_boundsMax = {0.0f, 0.0f, 0.0f};
};
