#pragma once

#include "../../pch.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include "../../Graphics/Material.h"
#include "../../Graphics/Model.h"
#include <entt/entity/entity.hpp>

namespace jnpf::Scene
{
	struct IDComponent
	{
		std::uint64_t ID = 0;
	};

	enum class RenderSourceType : std::uint8_t
	{
		Unsupported,
		Primitive,
		ImportedModel
	};

	enum class PrimitiveType : std::uint8_t
	{
		Cube,
		Rectangle,
		Pyramid,
		Plane,
		Sphere
	};

	struct RenderSourceComponent
	{
		RenderSourceType Type = RenderSourceType::Unsupported;
		PrimitiveType Primitive = PrimitiveType::Cube;
		float Size = 1.0f;
		float Width = 1.0f;
		float Height = 1.0f;
		float Depth = 1.0f;
		float Radius = 0.5f;
		int Slices = 16;
		int Stacks = 16;
		std::uint64_t AssetHandle = 0;
		std::string AssetPath;
	};

	enum class PrefabOverride : std::uint32_t
	{
		None = 0,
		Tag = 1u << 0,
		Transform = 1u << 1,
		Renderer = 1u << 2,
		Hierarchy = 1u << 3,
		Camera = 1u << 4,
		Light = 1u << 5
	};

	struct PrefabInstanceComponent
	{
		std::uint64_t PrefabAssetHandle = 0;
		std::string PrefabAssetPath;
		std::uint64_t PrefabLocalID = 0;
		std::uint32_t OverrideMask = 0;
		bool IsRoot = false;
	};

	// Editor-only transient content rendered through the normal scene path but never persisted.
	struct TransientEditorComponent { std::uint8_t Marker = 1; };

	// Intrusive sibling list used by hierarchy systems to avoid child-vector allocations.
	struct HierarchyComponent
	{
		entt::entity Parent = entt::null;
		entt::entity FirstChild = entt::null;
		entt::entity NextSibling = entt::null;
		entt::entity PrevSibling = entt::null;
		uint32_t ChildCount = 0;

		bool IsRoot() const
		{
			return Parent == entt::null;
		}

		bool HasChildren() const
		{
			return ChildCount > 0;
		}
	};

	// Local transforms are authoritative; world matrices are cached by TransformPropagationSystem.
	struct TransformComponent
	{
	private:
		DirectX::XMFLOAT3 m_localPosition = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_localRotation = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 m_localRotationQuaternion = {0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 m_localScale = {1.0f, 1.0f, 1.0f};

		DirectX::XMMATRIX m_worldMatrix = DirectX::XMMatrixIdentity();
		bool m_bWorldTransformDirty = true;

	public:
		void SetLocalPosition(const DirectX::XMFLOAT3& position)
		{
			m_localPosition = position;
			m_bWorldTransformDirty = true;
		}

		void SetLocalRotation(const DirectX::XMFLOAT3& rotation)
		{
			m_localRotation = rotation;
			DirectX::XMStoreFloat4(
				&m_localRotationQuaternion,
				DirectX::XMQuaternionRotationRollPitchYawFromVector(
					DirectX::XMLoadFloat3(&rotation)));
			m_bWorldTransformDirty = true;
		}

		void SetLocalRotationQuaternion(
			const DirectX::XMFLOAT4& rotation,
			const DirectX::XMFLOAT3& eulerHint)
		{
			DirectX::XMStoreFloat4(
				&m_localRotationQuaternion,
				DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&rotation)));
			m_localRotation = eulerHint;
			m_bWorldTransformDirty = true;
		}

		void SetLocalScale(const DirectX::XMFLOAT3& scale)
		{
			m_localScale = scale;
			m_bWorldTransformDirty = true;
		}

		void SetLocalTransform(
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& rotation,
			const DirectX::XMFLOAT3& scale)
		{
			m_localPosition = position;
			SetLocalRotation(rotation);
			m_localScale = scale;
			m_bWorldTransformDirty = true;
		}

		void SetLocalTransformQuaternion(
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT4& rotation,
			const DirectX::XMFLOAT3& eulerHint,
			const DirectX::XMFLOAT3& scale)
		{
			m_localPosition = position;
			SetLocalRotationQuaternion(rotation, eulerHint);
			m_localScale = scale;
			m_bWorldTransformDirty = true;
		}

		const DirectX::XMFLOAT3& GetLocalPosition() const { return m_localPosition; }

		const DirectX::XMFLOAT3& GetLocalRotation() const { return m_localRotation; }
		const DirectX::XMFLOAT4& GetLocalRotationQuaternion() const { return m_localRotationQuaternion; }

		const DirectX::XMFLOAT3& GetLocalScale() const { return m_localScale; }

		bool IsWorldTransformDirty() const { return m_bWorldTransformDirty; }

		void MarkWorldTransformDirty() { m_bWorldTransformDirty = true; }

		void ClearWorldTransformDirtyFlag() { m_bWorldTransformDirty = false; }

		DirectX::XMMATRIX GetLocalMatrix() const
		{
			DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(&m_localScale);
			DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&m_localRotationQuaternion);
			DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_localPosition);

			DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(scale);
			DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(rotation);
			DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(position);

			return S * R * T;
		}

		DirectX::XMMATRIX GetWorldMatrix() const
		{
			return m_worldMatrix;
		}

		void SetWorldMatrix(const DirectX::XMMATRIX& worldMatrix)
		{
			m_worldMatrix = worldMatrix;
		}
	};

	// Runtime renderer state; model pointers are intentionally not serialized.
	struct MeshRendererComponent
	{
		std::shared_ptr<class Model> ModelPtr;
		DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
		MaterialMode Material = MaterialMode::LitTint;
		std::shared_ptr<MaterialResource> MaterialResourcePtr;
		bool bVisible = true;
		bool bCastShadows = true;
		bool bReceiveShadows = true;
	};

	struct LightComponent
	{
		enum class Type : uint8_t
		{
			Directional = 0,
			Point = 1,
			Spot = 2
		};

		Type LightType = Type::Directional;
		bool bEnabled = true;
		DirectX::XMFLOAT3 Color = {1.0f, 1.0f, 1.0f};
		float Intensity = 1.0f;

		float Range = 100.0f;

		float SpotAngle = 45.0f;

		bool bCastShadows = true;
		float ShadowStrength = 1.0f;
		float ShadowBias = 0.0012f;
		float ShadowNormalBias = 0.02f;
		float ShadowDistance = 100.0f;
	};

	// Minimal engine-owned gameplay behavior, evaluated only by the fixed-step simulation.
	struct RotatorComponent
	{
		// Euler angular velocity in radians per second (pitch, yaw, roll).
		DirectX::XMFLOAT3 AngularVelocity = {0.0f, 1.0f, 0.0f};
		bool Enabled = true;
	};

	struct FlyControllerComponent
	{
		float MoveSpeed = 5.0f;
		float LookSensitivity = 0.0025f;
		float BoostMultiplier = 4.0f;
		float PitchLimitDegrees = 89.0f;
		bool Enabled = true;
	};

	enum class RigidBodyMotion : std::uint8_t
	{
		Static,
		Dynamic,
		Kinematic
	};

	struct RigidBodyComponent
	{
		RigidBodyMotion Motion = RigidBodyMotion::Static;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float LinearDamping = 0.05f;
		float AngularDamping = 0.05f;
		float GravityFactor = 1.0f;
		bool Enabled = true;
	};

	enum class ColliderShape : std::uint8_t
	{
		Box,
		Sphere
	};

	struct ColliderComponent
	{
		ColliderShape Shape = ColliderShape::Box;
		DirectX::XMFLOAT3 HalfExtents = {0.5f, 0.5f, 0.5f};
		float Radius = 0.5f;
	};

	struct TagComponent
	{
		static constexpr size_t MaxTagLength = 64;
		char Tag[MaxTagLength] = {0};

		TagComponent() = default;

		explicit TagComponent(const char* tag)
		{
			strncpy_s(Tag, MaxTagLength, tag, MaxTagLength - 1);
		}

		bool operator==(const char* other) const
		{
			return strcmp(Tag, other) == 0;
		}
	};
}
