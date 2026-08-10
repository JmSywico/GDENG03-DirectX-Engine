#pragma once

#include <DirectXMath.h>

#include <algorithm>

namespace jnpf::Scene
{
	class Camera
	{
	public:
		float FOVDegrees = 45.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;

		DirectX::XMMATRIX BuildPerspective(float aspectRatio) const
		{
			const float nearPlane = std::max(0.001f, NearPlane);
			const float farPlane = std::max(nearPlane + 0.001f, FarPlane);
			return DirectX::XMMatrixPerspectiveFovLH(
				DirectX::XMConvertToRadians(std::clamp(FOVDegrees, 1.0f, 179.0f)),
				std::max(0.001f, aspectRatio),
				nearPlane,
				farPlane);
		}
	};
}
