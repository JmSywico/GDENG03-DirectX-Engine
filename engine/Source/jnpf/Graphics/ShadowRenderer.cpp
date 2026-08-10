#include "Graphics/ShadowRenderer.h"

#include "Logging/Logging.h"

#include <algorithm>
#include <cmath>

namespace jnpf::Graphics
{
	namespace
	{
		DirectX::XMVECTOR SafeUp(DirectX::FXMVECTOR direction)
		{
			using namespace DirectX;
			const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			return std::abs(XMVectorGetX(XMVector3Dot(direction, worldUp))) > 0.99f
				? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
				: worldUp;
		}
	}

	bool ShadowRenderer::Initialize()
	{
		Shutdown();
		auto* graphics = DX11::Context::GetActive();
		if (!graphics) return false;

		for (std::uint8_t index = 0; index < m_directionalCascadeCount; ++index)
		{
			if (!m_targets[index].Initialize(graphics->GetDevice(), MapSize, MapSize))
			{
				LOG_ERRORF("Failed to create shadow depth target {}", index);
				Shutdown();
				return false;
			}
		}
		m_available = true;
		return true;
	}

	void ShadowRenderer::Shutdown()
	{
		for (auto& target : m_targets) target.Reset();
		m_available = false;
	}

	DirectX::XMMATRIX ShadowRenderer::BuildCropMatrix() const
	{
		return DirectX::XMMATRIX(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
	}

	ShadowFrameData ShadowRenderer::BuildDirectional(
		const DirectX::XMFLOAT4X4& cameraView,
		const DirectX::XMFLOAT4X4& cameraProjection,
		float cameraNear,
		float cameraFar,
		const DirectX::XMFLOAT3& lightDirection,
		float shadowDistance) const
	{
		using namespace DirectX;
		ShadowFrameData result;
		if (!m_available)
			return result;

		cameraNear = std::max(0.001f, cameraNear);
		cameraFar = std::max(cameraNear + 0.001f, std::min(cameraFar, shadowDistance));
		const auto splits = CalculateCascadeSplits(
			cameraNear, cameraFar, 0.65f, m_directionalCascadeCount);

		const XMMATRIX inverseView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&cameraView));
		const XMFLOAT4X4 projection = cameraProjection;
		XMVECTOR direction = XMLoadFloat3(&lightDirection);
		if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.0001f)
			direction = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		else
			direction = XMVector3Normalize(direction);
		const XMVECTOR up = SafeUp(direction);
		const XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, direction));
		const XMVECTOR lightUp = XMVector3Normalize(XMVector3Cross(direction, right));
		const XMMATRIX crop = BuildCropMatrix();

		float splitNear = cameraNear;
		for (std::uint8_t cascade = 0; cascade < m_directionalCascadeCount; ++cascade)
		{
			const float splitFar = splits[cascade];
			std::array<XMVECTOR, 8> corners{};
			std::uint8_t cornerIndex = 0;
			for (const float z : {splitNear, splitFar})
			{
				const float x = z / std::max(std::abs(projection._11), 0.0001f);
				const float y = z / std::max(std::abs(projection._22), 0.0001f);
				for (const float sy : {-1.0f, 1.0f})
					for (const float sx : {-1.0f, 1.0f})
						corners[cornerIndex++] = XMVector3TransformCoord(
							XMVectorSet(sx * x, sy * y, z, 1.0f), inverseView);
			}

			XMVECTOR center = XMVectorZero();
			for (const XMVECTOR corner : corners) center = XMVectorAdd(center, corner);
			center = XMVectorScale(center, 1.0f / corners.size());
			float radius = 0.0f;
			for (const XMVECTOR corner : corners)
				radius = std::max(radius, XMVectorGetX(XMVector3Length(XMVectorSubtract(corner, center))));
			radius = std::max(0.5f, std::ceil(radius * 16.0f) / 16.0f);

			const float texelSize = (2.0f * radius) / static_cast<float>(MapSize);
			const float centerRight = XMVectorGetX(XMVector3Dot(center, right));
			const float centerUp = XMVectorGetX(XMVector3Dot(center, lightUp));
			const float snappedRight = std::floor(centerRight / texelSize + 0.5f) * texelSize;
			const float snappedUp = std::floor(centerUp / texelSize + 0.5f) * texelSize;
			center = XMVectorAdd(
				center,
				XMVectorAdd(
					XMVectorScale(right, snappedRight - centerRight),
					XMVectorScale(lightUp, snappedUp - centerUp)));

			const float depthPadding = std::max(10.0f, radius * 0.5f);
			const float eyeDistance = radius + depthPadding;
			const XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(direction, eyeDistance));
			const XMMATRIX lightView = XMMatrixLookAtLH(eye, center, up);
			const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(
				-radius, radius, -radius, radius, 0.1f, eyeDistance * 2.0f);
			const XMMATRIX sampleMatrix = lightView * lightProjection * crop;

			XMStoreFloat4x4(&result.Views[cascade].View, lightView);
			XMStoreFloat4x4(&result.Views[cascade].Projection, lightProjection);
			XMStoreFloat4x4(&result.Views[cascade].SampleMatrix, sampleMatrix);
			result.Views[cascade].SplitFar = splitFar;
			splitNear = splitFar;
		}
		result.ViewCount = m_directionalCascadeCount;
		result.Directional = true;
		return result;
	}

	ShadowFrameData ShadowRenderer::BuildSpot(
		const DirectX::XMFLOAT3& lightPosition,
		const DirectX::XMFLOAT3& lightDirection,
		float spotAngleDegrees,
		float range) const
	{
		using namespace DirectX;
		ShadowFrameData result;
		if (!m_available || range <= 0.001f)
			return result;
		XMVECTOR direction = XMLoadFloat3(&lightDirection);
		if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.0001f)
			direction = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		else
			direction = XMVector3Normalize(direction);
		const XMVECTOR eye = XMLoadFloat3(&lightPosition);
		const XMMATRIX lightView = XMMatrixLookAtLH(eye, XMVectorAdd(eye, direction), SafeUp(direction));
		const float nearPlane = std::max(0.05f, range * 0.001f);
		const XMMATRIX lightProjection = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(std::clamp(spotAngleDegrees, 1.0f, 179.0f)),
			1.0f,
			nearPlane,
			std::max(range, nearPlane + 0.001f));
		XMStoreFloat4x4(&result.Views[0].View, lightView);
		XMStoreFloat4x4(&result.Views[0].Projection, lightProjection);
		XMStoreFloat4x4(
			&result.Views[0].SampleMatrix,
			lightView * lightProjection * BuildCropMatrix());
		result.Views[0].SplitFar = range;
		result.ViewCount = 1;
		result.Directional = false;
		return result;
	}

	DX11::DepthTarget* ShadowRenderer::GetTarget(std::uint8_t index)
	{
		return m_available && index < ShadowFrameData::MaxCascades ? &m_targets[index] : nullptr;
	}

	const DX11::DepthTarget* ShadowRenderer::GetTarget(std::uint8_t index) const
	{
		return m_available && index < ShadowFrameData::MaxCascades ? &m_targets[index] : nullptr;
	}
}
