#pragma once

#include "Scene.h"
#include "../Graphics/Material.h"

#include <functional>
#include <memory>

class Model;
struct MeshData;

namespace jnpf::Scene
{
	struct PrimitiveDesc
	{
		PrimitiveType Type = PrimitiveType::Cube;
		const char* Name = "Primitive";
		DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Rotation = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Scale = {1.0f, 1.0f, 1.0f};
		float Size = 1.0f;
		float Width = 1.0f;
		float Height = 1.0f;
		float Depth = 1.0f;
		float Radius = 0.5f;
		int Slices = 16;
		int Stacks = 16;
		DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
		MaterialMode Material = MaterialMode::LitTint;
		std::shared_ptr<MaterialResource> MaterialResourcePtr;
		bool Visible = true;
	};

	struct CameraDesc
	{
		const char* Name = "Camera";
		DirectX::XMFLOAT3 Position = {0.0f, 2.0f, -5.0f};
		DirectX::XMFLOAT3 Rotation = {0.0f, 0.0f, 0.0f};
		float FOVDegrees = 45.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;
		bool SetActive = true;
	};

	entt::entity CreateCamera(Scene& scene, const CameraDesc& desc = {});

	using PrimitiveModelFactory = std::function<std::shared_ptr<Model>(const MeshData&)>;

	entt::entity CreatePrimitive(
		Scene& scene,
		const PrimitiveDesc& desc,
		const PrimitiveModelFactory& modelFactory);
}
