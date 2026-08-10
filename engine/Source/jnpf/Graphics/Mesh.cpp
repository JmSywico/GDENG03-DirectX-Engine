#include "Graphics/Mesh.h"

#include "Graphics/DX11/DX11Context.h"
#include "Logging/Logging.h"

#include <algorithm>
#include <limits>

Mesh::~Mesh()
{
	Release();
}

bool Mesh::Initialize(const void* vertexData, uint32_t vertexBufferSize, uint32_t stride)
{
	if (!vertexData || vertexBufferSize == 0 || stride != sizeof(SimpleVertex))
	{
		LOG_ERRORF(
			"Mesh::Initialize - invalid vertex data (data={}, size={}, stride={}, expected stride={})",
			vertexData != nullptr,
			vertexBufferSize,
			stride,
			sizeof(SimpleVertex));
		return false;
	}
	if (vertexBufferSize % stride != 0)
	{
		LOG_ERRORF(
			"Mesh::Initialize - vertex buffer size {} is not divisible by stride {}",
			vertexBufferSize,
			stride);
		return false;
	}

	Release();
	auto* graphics = jnpf::Graphics::DX11::Context::GetActive();
	if (!graphics)
	{
		LOG_ERROR("Mesh::Initialize - DX11 context is not initialized");
		return false;
	}
	D3D11_BUFFER_DESC description{};
	description.ByteWidth = vertexBufferSize;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{ vertexData };
	if (FAILED(graphics->GetDevice().CreateBuffer(&description, &initial, &m_vertexBuffer)))
	{
		LOG_ERRORF(
			"Mesh::Initialize - DX11 failed to create vertex buffer (size={}, stride={}, vertices={})",
			vertexBufferSize,
			stride,
			vertexBufferSize / stride);
		return false;
	}

	m_vertexCount = vertexBufferSize / stride;
	m_vertexStride = stride;
	UpdateBoundsFromVertices(vertexData, m_vertexCount, stride);
	return true;
}

bool Mesh::InitializeIndexed(
	const void* vertexData,
	uint32_t vertexBufferSize,
	uint32_t stride,
	const void* indexData,
	uint32_t indexBufferSize,
	bool indices32Bit)
{
	if (!Initialize(vertexData, vertexBufferSize, stride))
		return false;
	if (!indexData || indexBufferSize == 0)
	{
		LOG_ERRORF(
			"Mesh::InitializeIndexed - invalid index data (data={}, size={})",
			indexData != nullptr,
			indexBufferSize);
		Release();
		return false;
	}

	const uint32_t indexStride = indices32Bit ? sizeof(uint32_t) : sizeof(uint16_t);
	if (indexBufferSize % indexStride != 0)
	{
		LOG_ERRORF(
			"Mesh::InitializeIndexed - index buffer size {} is not divisible by index stride {}",
			indexBufferSize,
			indexStride);
		Release();
		return false;
	}

	auto* graphics = jnpf::Graphics::DX11::Context::GetActive();
	D3D11_BUFFER_DESC description{};
	description.ByteWidth = indexBufferSize;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{ indexData };
	if (!graphics || FAILED(graphics->GetDevice().CreateBuffer(&description, &initial, &m_indexBuffer)))
	{
		LOG_ERRORF(
			"Mesh::InitializeIndexed - DX11 failed to create index buffer (size={}, stride={}, indices={})",
			indexBufferSize,
			indexStride,
			indexBufferSize / indexStride);
		Release();
		return false;
	}

	m_indexCount = indexBufferSize / indexStride;
	m_indexFormat = indices32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	return true;
}

void Mesh::UpdateBoundsFromVertices(const void* vertexData, uint32_t vertexCount, uint32_t stride)
{
	if (!vertexData || vertexCount == 0 || stride < sizeof(float) * 3)
		return;

	m_boundsMin = {
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max()
	};
	m_boundsMax = {
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest()
	};

	const auto* bytes = static_cast<const uint8_t*>(vertexData);
	for (uint32_t i = 0; i < vertexCount; ++i)
	{
		const auto* position = reinterpret_cast<const float*>(bytes + static_cast<size_t>(i) * stride);
		m_boundsMin.x = std::min(m_boundsMin.x, position[0]);
		m_boundsMin.y = std::min(m_boundsMin.y, position[1]);
		m_boundsMin.z = std::min(m_boundsMin.z, position[2]);
		m_boundsMax.x = std::max(m_boundsMax.x, position[0]);
		m_boundsMax.y = std::max(m_boundsMax.y, position[1]);
		m_boundsMax.z = std::max(m_boundsMax.z, position[2]);
	}
}

void Mesh::Release()
{
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();
	m_vertexCount = 0;
	m_indexCount = 0;
	m_vertexStride = 0;
	m_indexFormat = DXGI_FORMAT_R32_UINT;
}
