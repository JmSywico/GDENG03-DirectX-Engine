#pragma once

#include "../Camera.h"

#include <DirectXMath.h>

namespace enignE::Scene
{
	struct CameraComponent
	{
		float FOVDegrees = 45.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;
		float AspectRatio = 16.0f / 9.0f;
		bool bPrimary = false;

		Camera ToCamera() const
		{
			Camera camera;
			camera.FOVDegrees = FOVDegrees;
			camera.NearPlane = NearPlane;
			camera.FarPlane = FarPlane;
			return camera;
		}

		DirectX::XMMATRIX GetProjectionMatrix() const
		{
			return ToCamera().BuildPerspective(AspectRatio);
		}

		DirectX::XMMATRIX GetViewMatrix(
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& forward,
			const DirectX::XMFLOAT3& up) const
		{
			const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&position);
			const DirectX::XMVECTOR fwd = DirectX::XMLoadFloat3(&forward);
			const DirectX::XMVECTOR upVector = DirectX::XMLoadFloat3(&up);
			return DirectX::XMMatrixLookAtLH(
				eye,
				DirectX::XMVectorAdd(eye, fwd),
				upVector);
		}
	};
}
