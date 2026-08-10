#pragma once

#include "../Input/Input.h"
#include "../Scene/Camera.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

namespace jnpf::Editor
{
	class EditorCamera
	{
	public:
		EditorCamera()
		{
			DirectX::XMStoreFloat4x4(&m_view, DirectX::XMMatrixIdentity());
			DirectX::XMStoreFloat4x4(&m_projection, DirectX::XMMatrixIdentity());
		}

		void SetView(
			const DirectX::XMFLOAT3& eye,
			const DirectX::XMFLOAT3& at,
			const DirectX::XMFLOAT3& up)
		{
			m_position = eye;
			const DirectX::XMVECTOR eyeVector = DirectX::XMLoadFloat3(&eye);
			const DirectX::XMVECTOR atVector = DirectX::XMLoadFloat3(&at);
			const DirectX::XMVECTOR forwardVector =
				DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(atVector, eyeVector));
			DirectX::XMFLOAT3 forward{};
			DirectX::XMStoreFloat3(&forward, forwardVector);
			m_yaw = std::atan2(forward.x, forward.z);
			m_pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
			DirectX::XMStoreFloat4x4(
				&m_view,
				DirectX::XMMatrixLookAtLH(eyeVector, atVector, DirectX::XMLoadFloat3(&up)));
		}

		void SetProjection(float fovYRadians, float aspect, float nearPlane, float farPlane)
		{
			m_camera.FOVDegrees = DirectX::XMConvertToDegrees(fovYRadians);
			m_camera.NearPlane = nearPlane;
			m_camera.FarPlane = farPlane;
			m_aspect = aspect;
			RebuildProjection();
		}

		void SetMoveSpeed(float unitsPerSecond) { m_moveSpeed = unitsPerSecond; }
		void SetLookSensitivity(float radiansPerPixel) { m_lookSensitivity = radiansPerPixel; }
		void AddMouseDelta(float x, float y)
		{
			m_mouseDeltaX += x;
			m_mouseDeltaY += y;
		}
		void AddWheelDelta(float value) { m_wheelDelta += value; }
		void BeginLook() { m_looking = true; }
		void EndLook() { m_looking = false; }
		void BeginPan() { m_panning = true; }
		void EndPan() { m_panning = false; }
		void ResetInteraction()
		{
			m_looking = false;
			m_panning = false;
			m_mouseDeltaX = 0.0f;
			m_mouseDeltaY = 0.0f;
			m_wheelDelta = 0.0f;
		}

		void Update(float deltaTime, InputManager& input)
		{
			constexpr float maxPitch = DirectX::XM_PIDIV2 - 0.01f;
			bool changed = false;
			DirectX::XMVECTOR forward{};
			DirectX::XMVECTOR right{};
			DirectX::XMVECTOR up{};
			RebuildAxes(forward, right, up);

			if (m_looking)
			{
				m_yaw += m_mouseDeltaX * m_lookSensitivity;
				m_pitch = std::clamp(
					m_pitch - m_mouseDeltaY * m_lookSensitivity,
					-maxPitch,
					maxPitch);
				RebuildAxes(forward, right, up);
				changed = true;
			}

			DirectX::XMVECTOR movement = DirectX::XMVectorZero();
			const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			if (m_looking)
			{
				if (input.IsKeyDown('W')) movement = DirectX::XMVectorAdd(movement, forward);
				if (input.IsKeyDown('S')) movement = DirectX::XMVectorSubtract(movement, forward);
				if (input.IsKeyDown('D')) movement = DirectX::XMVectorAdd(movement, right);
				if (input.IsKeyDown('A')) movement = DirectX::XMVectorSubtract(movement, right);
				if (input.IsKeyDown('E')) movement = DirectX::XMVectorAdd(movement, worldUp);
				if (input.IsKeyDown('Q')) movement = DirectX::XMVectorSubtract(movement, worldUp);
			}
			if (m_panning)
			{
				movement = DirectX::XMVectorSubtract(
					movement,
					DirectX::XMVectorScale(right, m_mouseDeltaX * m_panSensitivity));
				movement = DirectX::XMVectorAdd(
					movement,
					DirectX::XMVectorScale(up, m_mouseDeltaY * m_panSensitivity));
			}
			if (m_wheelDelta != 0.0f)
			{
				movement = DirectX::XMVectorAdd(
					movement,
					DirectX::XMVectorScale(forward, m_wheelDelta * m_zoomSpeed));
			}

			if (!DirectX::XMVector3Equal(movement, DirectX::XMVectorZero()))
			{
				const float speed =
					m_moveSpeed * (input.IsKeyDown(VK_SHIFT) ? m_fastMultiplier : 1.0f);
				if (m_looking)
					movement = DirectX::XMVectorScale(
						DirectX::XMVector3Normalize(movement),
						speed * deltaTime);
				else
					movement = DirectX::XMVectorScale(movement, deltaTime * 60.0f);
				const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_position);
				DirectX::XMStoreFloat3(&m_position, DirectX::XMVectorAdd(position, movement));
				changed = true;
			}

			if (changed)
				RebuildView();
			m_mouseDeltaX = 0.0f;
			m_mouseDeltaY = 0.0f;
			m_wheelDelta = 0.0f;
		}

		void SetAspect(float aspect)
		{
			m_aspect = aspect;
			RebuildProjection();
		}

		const DirectX::XMFLOAT4X4& GetViewMatrix() const { return m_view; }
		const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return m_projection; }
		const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
		float GetNearPlane() const { return m_camera.NearPlane; }
		float GetFarPlane() const { return m_camera.FarPlane; }

	private:
		void RebuildProjection()
		{
			DirectX::XMStoreFloat4x4(&m_projection, m_camera.BuildPerspective(m_aspect));
		}

		void RebuildAxes(
			DirectX::XMVECTOR& forward,
			DirectX::XMVECTOR& right,
			DirectX::XMVECTOR& up) const
		{
			const float cosPitch = std::cos(m_pitch);
			const DirectX::XMFLOAT3 direction{
				cosPitch * std::sin(m_yaw),
				std::sin(m_pitch),
				cosPitch * std::cos(m_yaw)
			};
			const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			forward = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction));
			right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward));
			up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(forward, right));
		}

		void RebuildView()
		{
			const float cosPitch = std::cos(m_pitch);
			const DirectX::XMFLOAT3 forward{
				cosPitch * std::sin(m_yaw),
				std::sin(m_pitch),
				cosPitch * std::cos(m_yaw)
			};
			const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&m_position);
			const DirectX::XMVECTOR at = DirectX::XMVectorAdd(
				eye,
				DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&forward)));
			DirectX::XMStoreFloat4x4(
				&m_view,
				DirectX::XMMatrixLookAtLH(
					eye,
					at,
					DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
		}

		Scene::Camera m_camera;
		DirectX::XMFLOAT4X4 m_view{};
		DirectX::XMFLOAT4X4 m_projection{};
		DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, -3.0f};
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
		float m_aspect = 16.0f / 9.0f;
		bool m_looking = false;
		bool m_panning = false;
		float m_moveSpeed = 8.0f;
		float m_fastMultiplier = 4.0f;
		float m_lookSensitivity = 0.003f;
		float m_panSensitivity = 0.01f;
		float m_zoomSpeed = 6.0f;
		float m_mouseDeltaX = 0.0f;
		float m_mouseDeltaY = 0.0f;
		float m_wheelDelta = 0.0f;
	};
}
