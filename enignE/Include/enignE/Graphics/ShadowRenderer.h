#pragma once

#include <DirectXMath.h>
#include "Graphics/DX11/DX11Context.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace enignE::Graphics
{
	struct ShadowViewData
	{
		DirectX::XMFLOAT4X4 View{};
		DirectX::XMFLOAT4X4 Projection{};
		DirectX::XMFLOAT4X4 SampleMatrix{};
		float SplitFar = 0.0f;
	};

	struct ShadowFrameData
	{
		static constexpr std::uint8_t MaxCascades = 4;
		std::array<ShadowViewData, MaxCascades> Views{};
		std::uint8_t ViewCount = 0;
		bool Directional = false;

		bool IsValid() const { return ViewCount > 0; }
	};

	class ShadowRenderer
	{
	public:
		static constexpr std::uint16_t MapSize = 2048;
		static constexpr std::uint8_t DefaultDirectionalCascadeCount = 3;

		bool Initialize();
		void Shutdown();
		bool IsAvailable() const { return m_available; }
		static std::array<float, ShadowFrameData::MaxCascades> CalculateCascadeSplits(
			float cameraNear,
			float cameraFar,
			float splitLambda = 0.65f,
			std::uint8_t cascadeCount = ShadowFrameData::MaxCascades)
		{
			cameraNear = std::max(0.001f, cameraNear);
			cameraFar = std::max(cameraNear + 0.001f, cameraFar);
			splitLambda = std::clamp(splitLambda, 0.0f, 1.0f);
			cascadeCount = std::clamp<std::uint8_t>(cascadeCount, 1, ShadowFrameData::MaxCascades);
			std::array<float, ShadowFrameData::MaxCascades> splits{};
			for (std::uint8_t index = 0; index < cascadeCount; ++index)
			{
				const float ratio = static_cast<float>(index + 1) / cascadeCount;
				const float logarithmic = cameraNear * std::pow(cameraFar / cameraNear, ratio);
				const float uniform = cameraNear + (cameraFar - cameraNear) * ratio;
				splits[index] = uniform + (logarithmic - uniform) * splitLambda;
			}
			for (std::uint8_t index = cascadeCount; index < ShadowFrameData::MaxCascades; ++index)
				splits[index] = cameraFar;
			return splits;
		}
		/** @brief Configures directional cascades before Initialize(). */
		bool SetDirectionalCascadeCount(std::uint8_t cascadeCount)
		{
			if (m_available)
				return false;
			m_directionalCascadeCount = std::clamp<std::uint8_t>(
				cascadeCount, 1, ShadowFrameData::MaxCascades);
			return true;
		}
		std::uint8_t GetDirectionalCascadeCount() const { return m_directionalCascadeCount; }

		ShadowFrameData BuildDirectional(
			const DirectX::XMFLOAT4X4& cameraView,
			const DirectX::XMFLOAT4X4& cameraProjection,
			float cameraNear,
			float cameraFar,
			const DirectX::XMFLOAT3& lightDirection,
			float shadowDistance) const;
		ShadowFrameData BuildSpot(
			const DirectX::XMFLOAT3& lightPosition,
			const DirectX::XMFLOAT3& lightDirection,
			float spotAngleDegrees,
			float range) const;

		DX11::DepthTarget* GetTarget(std::uint8_t index);
		const DX11::DepthTarget* GetTarget(std::uint8_t index) const;

	private:
		DirectX::XMMATRIX BuildCropMatrix() const;

		std::array<DX11::DepthTarget, ShadowFrameData::MaxCascades> m_targets{};
		std::uint8_t m_directionalCascadeCount = DefaultDirectionalCascadeCount;
		bool m_available = false;
	};
}
