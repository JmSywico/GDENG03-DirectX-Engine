#pragma once

#include "../../Scene/Scene.h"
#include "../../Scene/SceneFactory.h"
#include "EntitySnapshot.h"
#include "Editor/PrefabAsset.h"
#include "Graphics/AssetRegistry.h"
#include "Scene/ComponentCatalog.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace enignE::Editor
{
	/** @brief Reversible editor operation stored by CommandStack. @ingroup editor */
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() = default;
		virtual void Execute() = 0;
		virtual void Undo() = 0;
		virtual bool MergeWith(const EditorCommand&) { return false; }
	};

	/** @brief Copyable local-transform value used by transform commands. @ingroup editor */
	struct TransformValue
	{
		DirectX::XMFLOAT3 Position{};
		DirectX::XMFLOAT3 Rotation{};
		DirectX::XMFLOAT3 Scale{1.0f, 1.0f, 1.0f};
		DirectX::XMFLOAT4 RotationQuaternion{0.0f, 0.0f, 0.0f, 1.0f};
		bool HasRotationQuaternion = false;
	};

	/** @brief Renames an entity addressed by stable ID. @ingroup editor */
	class RenameEntityCommand final : public EditorCommand
	{
	public:
		RenameEntityCommand(Scene::Scene& scene, std::uint64_t id, std::string before, std::string after);
		void Execute() override;
		void Undo() override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		std::string m_before;
		std::string m_after;
	};

	/** @brief Replaces one entity's complete local transform. @ingroup editor */
	class SetTransformCommand final : public EditorCommand
	{
	public:
		SetTransformCommand(Scene::Scene& scene, std::uint64_t id, TransformValue before, TransformValue after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		void Apply(const TransformValue& value);

		Scene::Scene& m_scene;
		std::uint64_t m_id;
		TransformValue m_before;
		TransformValue m_after;
	};

	/** @brief Changes an entity's parent while preserving world placement. @ingroup editor */
	class SetParentCommand final : public EditorCommand
	{
	public:
		SetParentCommand(Scene::Scene& scene, std::uint64_t child, std::uint64_t beforeParent, std::uint64_t afterParent);
		void Execute() override;
		void Undo() override;

	private:
		static TransformValue ReadTransform(const Scene::TransformComponent& transform);
		void Apply(std::uint64_t parent, const TransformValue& transform);

		Scene::Scene& m_scene;
		std::uint64_t m_child;
		std::uint64_t m_before;
		std::uint64_t m_after;
		TransformValue m_beforeTransform;
		TransformValue m_afterTransform;
		bool m_hasAfterTransform = false;
	};

	class SetRendererVisibilityCommand final : public EditorCommand
	{
	public:
		SetRendererVisibilityCommand(Scene::Scene& scene, std::uint64_t id, bool before, bool after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		bool m_before;
		bool m_after;
	};

	class SetRendererAlbedoCommand final : public EditorCommand
	{
	public:
		SetRendererAlbedoCommand(Scene::Scene& scene, std::uint64_t id, DirectX::XMFLOAT4 before, DirectX::XMFLOAT4 after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		DirectX::XMFLOAT4 m_before;
		DirectX::XMFLOAT4 m_after;
	};

	/** @brief Treats several editor operations as one undo/redo step. */
	class CompositeCommand final : public EditorCommand
	{
	public:
		void Add(std::unique_ptr<EditorCommand> command);
		bool Empty() const { return m_commands.empty(); }
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		std::vector<std::unique_ptr<EditorCommand>> m_commands;
	};

	class SetRendererShadowFlagsCommand final : public EditorCommand
	{
	public:
		SetRendererShadowFlagsCommand(
			Scene::Scene& scene,
			std::uint64_t id,
			bool beforeCast,
			bool beforeReceive,
			bool afterCast,
			bool afterReceive);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		bool m_beforeCast;
		bool m_beforeReceive;
		bool m_afterCast;
		bool m_afterReceive;
	};

	class SetRendererMaterialCommand final : public EditorCommand
	{
	public:
		SetRendererMaterialCommand(Scene::Scene& scene, std::uint64_t id, MaterialMode before, MaterialMode after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		MaterialMode m_before;
		MaterialMode m_after;
	};

	class SetCameraCommand final : public EditorCommand
	{
	public:
		SetCameraCommand(
			Scene::Scene& scene,
			std::uint64_t id,
			Scene::CameraComponent before,
			Scene::CameraComponent after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		Scene::CameraComponent m_before;
		Scene::CameraComponent m_after;
	};

	class SetLightCommand final : public EditorCommand
	{
	public:
		SetLightCommand(
			Scene::Scene& scene,
			std::uint64_t id,
			Scene::LightComponent before,
			Scene::LightComponent after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		Scene::LightComponent m_before;
		Scene::LightComponent m_after;
		std::uint64_t m_beforeActive = 0;
		std::uint64_t m_afterActive = 0;
	};

	class SetActiveCameraCommand final : public EditorCommand
	{
	public:
		SetActiveCameraCommand(Scene::Scene& scene, std::uint64_t before, std::uint64_t after);
		void Execute() override;
		void Undo() override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_before;
		std::uint64_t m_after;
	};

	class SetActiveLightCommand final : public EditorCommand
	{
	public:
		SetActiveLightCommand(Scene::Scene& scene, std::uint64_t before, std::uint64_t after);
		void Execute() override;
		void Undo() override;

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_before;
		std::uint64_t m_after;
	};

	/**
	 * @brief Creates an entity and retains a snapshot for stable redo.
	 * @ingroup editor
	 */
	class CreateEntityCommand final : public EditorCommand
	{
	public:
		CreateEntityCommand(Scene::Scene& scene, std::string name = "Entity", std::uint64_t parent = 0);
		CreateEntityCommand(
			Scene::Scene& scene,
			std::string name,
			const Scene::CameraComponent& camera,
			std::uint64_t parent = 0);
		CreateEntityCommand(
			Scene::Scene& scene,
			std::string name,
			const Scene::LightComponent& light,
			std::uint64_t parent = 0);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_snapshot.GetRootID(); }

	private:
		Scene::Scene& m_scene;
		std::string m_name;
		std::uint64_t m_parent = 0;
		std::optional<Scene::CameraComponent> m_camera;
		std::optional<Scene::LightComponent> m_light;
		EntitySnapshot m_snapshot;
	};

	/**
	 * @brief Creates a procedural primitive entity with renderer state.
	 * @ingroup editor
	 */
	class CreatePrimitiveCommand final : public EditorCommand
	{
	public:
		CreatePrimitiveCommand(
			Scene::Scene& scene,
			Scene::PrimitiveDesc desc,
			Scene::PrimitiveModelFactory modelFactory,
			std::uint64_t parent = 0);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_snapshot.GetRootID(); }

	private:
		Scene::Scene& m_scene;
		Scene::PrimitiveDesc m_desc;
		Scene::PrimitiveModelFactory m_modelFactory;
		std::uint64_t m_parent = 0;
		EntitySnapshot m_snapshot;
	};

	/**
	 * @brief Deletes an entity subtree and restores it from a captured snapshot.
	 * @ingroup editor
	 */
	class DeleteEntityCommand final : public EditorCommand
	{
	public:
		DeleteEntityCommand(Scene::Scene& scene, entt::entity entity);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_snapshot.GetRootID(); }

	private:
		Scene::Scene& m_scene;
		EntitySnapshot m_snapshot;
	};

	class SetRotatorCommand final : public EditorCommand
	{
	public:
		SetRotatorCommand(Scene::Scene& scene, std::uint64_t id,
			Scene::RotatorComponent before, Scene::RotatorComponent after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;
	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		Scene::RotatorComponent m_before;
		Scene::RotatorComponent m_after;
	};

	class AddComponentCommand final : public EditorCommand
	{
	public:
		AddComponentCommand(Scene::Scene& scene, std::uint64_t id, Scene::ComponentKind kind);
		void Execute() override;
		void Undo() override;
	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		Scene::ComponentKind m_kind;
	};

	class SetFlyControllerCommand final : public EditorCommand
	{
	public:
		SetFlyControllerCommand(Scene::Scene& scene, std::uint64_t id,
			Scene::FlyControllerComponent before, Scene::FlyControllerComponent after);
		void Execute() override; void Undo() override; bool MergeWith(const EditorCommand&) override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id;
		Scene::FlyControllerComponent m_before, m_after;
	};

	class SetRigidBodyCommand final : public EditorCommand
	{
	public:
		SetRigidBodyCommand(Scene::Scene& scene, std::uint64_t id,
			Scene::RigidBodyComponent before, Scene::RigidBodyComponent after);
		void Execute() override; void Undo() override; bool MergeWith(const EditorCommand&) override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id;
		Scene::RigidBodyComponent m_before, m_after;
	};

	class SetColliderCommand final : public EditorCommand
	{
	public:
		SetColliderCommand(Scene::Scene& scene, std::uint64_t id,
			Scene::ColliderComponent before, Scene::ColliderComponent after);
		void Execute() override; void Undo() override; bool MergeWith(const EditorCommand&) override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id;
		Scene::ColliderComponent m_before, m_after;
	};

	class RemoveRigidBodyCommand final : public EditorCommand
	{
	public:
		RemoveRigidBodyCommand(Scene::Scene& scene, std::uint64_t id, Scene::RigidBodyComponent value);
		void Execute() override; void Undo() override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id; Scene::RigidBodyComponent m_value;
	};

	class RemoveColliderCommand final : public EditorCommand
	{
	public:
		RemoveColliderCommand(Scene::Scene& scene, std::uint64_t id, Scene::ColliderComponent value);
		void Execute() override; void Undo() override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id; Scene::ColliderComponent m_value;
	};

	class RemoveFlyControllerCommand final : public EditorCommand
	{
	public:
		RemoveFlyControllerCommand(Scene::Scene& scene, std::uint64_t id, Scene::FlyControllerComponent value);
		void Execute() override; void Undo() override;
	private:
		Scene::Scene& m_scene; std::uint64_t m_id; Scene::FlyControllerComponent m_value;
	};

	class RemoveRotatorCommand final : public EditorCommand
	{
	public:
		RemoveRotatorCommand(Scene::Scene& scene, std::uint64_t id, Scene::RotatorComponent value);
		void Execute() override;
		void Undo() override;
	private:
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		Scene::RotatorComponent m_value;
	};

	class SetRendererMaterialResourceCommand final : public EditorCommand
	{
	public:
		SetRendererMaterialResourceCommand(
			Scene::Scene& scene,
			std::uint64_t id,
			std::optional<MaterialResource> before,
			std::optional<MaterialResource> after);
		void Execute() override;
		void Undo() override;
		bool MergeWith(const EditorCommand& other) override;

	private:
		void Apply(const std::optional<MaterialResource>& value);
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		std::optional<MaterialResource> m_before;
		std::optional<MaterialResource> m_after;
	};

	class SetRendererModelCommand final : public EditorCommand
	{
	public:
		SetRendererModelCommand(
			Scene::Scene& scene, std::uint64_t id,
			std::shared_ptr<Model> beforeModel,
			std::optional<Scene::RenderSourceComponent> beforeSource,
			std::shared_ptr<Model> afterModel,
			std::optional<Scene::RenderSourceComponent> afterSource);
		void Execute() override;
		void Undo() override;
	private:
		void Apply(const std::shared_ptr<Model>& model,
			const std::optional<Scene::RenderSourceComponent>& source);
		Scene::Scene& m_scene;
		std::uint64_t m_id;
		std::shared_ptr<Model> m_beforeModel;
		std::shared_ptr<Model> m_afterModel;
		std::optional<Scene::RenderSourceComponent> m_beforeSource;
		std::optional<Scene::RenderSourceComponent> m_afterSource;
	};

	/** @brief Duplicates an entity subtree with fresh stable IDs. */
	class DuplicateEntityCommand final : public EditorCommand
	{
	public:
		DuplicateEntityCommand(Scene::Scene& scene, entt::entity source);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_snapshot.GetRootID(); }

	private:
		Scene::Scene& m_scene;
		std::uint64_t m_sourceID = 0;
		EntitySnapshot m_snapshot;
	};

	class PasteEntityCommand final : public EditorCommand
	{
	public:
		PasteEntityCommand(Scene::Scene& scene, EntitySnapshot source);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_pasted.GetRootID(); }
	private:
		Scene::Scene& m_scene;
		EntitySnapshot m_source;
		EntitySnapshot m_pasted;
	};

	class InstantiatePrefabCommand final : public EditorCommand
	{
	public:
		InstantiatePrefabCommand(
			Scene::Scene& scene,
			PrefabAsset prefab,
			std::uint64_t assetHandle,
			std::string assetPath,
			std::uint64_t parentID = 0,
			std::optional<DirectX::XMFLOAT3> rootPosition = std::nullopt);
		void Execute() override;
		void Undo() override;
		std::uint64_t GetEntityID() const { return m_instance.GetRootID(); }
	private:
		Scene::Scene& m_scene;
		PrefabAsset m_prefab;
		std::uint64_t m_assetHandle;
		std::string m_assetPath;
		std::uint64_t m_parentID;
		std::optional<DirectX::XMFLOAT3> m_rootPosition;
		EntitySnapshot m_instance;
	};

	class DeleteAssetCommand final : public EditorCommand
	{
	public:
		DeleteAssetCommand(Graphics::AssetRegistry& assets, Graphics::AssetReference asset);
		void Execute() override;
		void Undo() override;
	private:
		Graphics::AssetRegistry& m_assets;
		Graphics::AssetReference m_asset;
		std::filesystem::path m_source;
		std::filesystem::path m_sourceMetadata;
		std::filesystem::path m_trash;
		std::filesystem::path m_trashMetadata;
		bool m_deleted = false;
	};

	class UnpackPrefabCommand final : public EditorCommand
	{
	public:
		UnpackPrefabCommand(Scene::Scene& scene, entt::entity instanceEntity);
		void Execute() override;
		void Undo() override;
	private:
		struct Entry { std::uint64_t ID; Scene::PrefabInstanceComponent Component; };
		Scene::Scene& m_scene;
		std::vector<Entry> m_entries;
	};

	class AdoptEntityCommand final : public EditorCommand
	{
	public:
		AdoptEntityCommand(Scene::Scene& scene, EntitySnapshot snapshot);
		void Execute() override;
		void Undo() override;
	private:
		Scene::Scene& m_scene;
		EntitySnapshot m_snapshot;
	};
}
