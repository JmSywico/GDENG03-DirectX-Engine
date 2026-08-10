#include "Editor/Commands/EditorCommand.h"
#include "Graphics/AssetRegistry.h"

namespace jnpf::Editor
{
	SetRotatorCommand::SetRotatorCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::RotatorComponent before, Scene::RotatorComponent after)
		: m_scene(scene), m_id(id), m_before(before), m_after(after) {}
	void SetRotatorCommand::Execute() { m_scene.SetRotator(m_scene.FindEntityByID(m_id), m_after); }
	void SetRotatorCommand::Undo() { m_scene.SetRotator(m_scene.FindEntityByID(m_id), m_before); }
	bool SetRotatorCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRotatorCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id) return false;
		m_after = command->m_after;
		return true;
	}

	AddComponentCommand::AddComponentCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::ComponentKind kind) : m_scene(scene), m_id(id), m_kind(kind) {}
	void AddComponentCommand::Execute()
	{
		Scene::ComponentCatalog::AddDefault(m_scene, m_scene.FindEntityByID(m_id), m_kind);
	}

	SetFlyControllerCommand::SetFlyControllerCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::FlyControllerComponent before, Scene::FlyControllerComponent after)
		: m_scene(scene), m_id(id), m_before(before), m_after(after) {}
	void SetFlyControllerCommand::Execute() { m_scene.SetFlyController(m_scene.FindEntityByID(m_id), m_after); }
	void SetFlyControllerCommand::Undo() { m_scene.SetFlyController(m_scene.FindEntityByID(m_id), m_before); }
	bool SetFlyControllerCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetFlyControllerCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id) return false;
		m_after = command->m_after; return true;
	}
	RemoveFlyControllerCommand::RemoveFlyControllerCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::FlyControllerComponent value) : m_scene(scene), m_id(id), m_value(value) {}
	void RemoveFlyControllerCommand::Execute()
	{
		Scene::ComponentCatalog::Remove(m_scene, m_scene.FindEntityByID(m_id), Scene::ComponentKind::FlyController);
	}
	void RemoveFlyControllerCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		if (Scene::ComponentCatalog::AddDefault(m_scene, entity, Scene::ComponentKind::FlyController))
			m_scene.SetFlyController(entity, m_value);
	}
	void AddComponentCommand::Undo()
	{
		Scene::ComponentCatalog::Remove(m_scene, m_scene.FindEntityByID(m_id), m_kind);
	}

	RemoveRotatorCommand::RemoveRotatorCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::RotatorComponent value) : m_scene(scene), m_id(id), m_value(value) {}
	void RemoveRotatorCommand::Execute()
	{
		Scene::ComponentCatalog::Remove(m_scene, m_scene.FindEntityByID(m_id), Scene::ComponentKind::Rotator);
	}
	void RemoveRotatorCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		if (Scene::ComponentCatalog::AddDefault(m_scene, entity, Scene::ComponentKind::Rotator))
			m_scene.SetRotator(entity, m_value);
	}

	SetRigidBodyCommand::SetRigidBodyCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::RigidBodyComponent before, Scene::RigidBodyComponent after)
		: m_scene(scene), m_id(id), m_before(before), m_after(after) {}
	void SetRigidBodyCommand::Execute() { m_scene.SetRigidBody(m_scene.FindEntityByID(m_id), m_after); }
	void SetRigidBodyCommand::Undo() { m_scene.SetRigidBody(m_scene.FindEntityByID(m_id), m_before); }
	bool SetRigidBodyCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRigidBodyCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id) return false;
		m_after = command->m_after; return true;
	}

	SetColliderCommand::SetColliderCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::ColliderComponent before, Scene::ColliderComponent after)
		: m_scene(scene), m_id(id), m_before(before), m_after(after) {}
	void SetColliderCommand::Execute() { m_scene.SetCollider(m_scene.FindEntityByID(m_id), m_after); }
	void SetColliderCommand::Undo() { m_scene.SetCollider(m_scene.FindEntityByID(m_id), m_before); }
	bool SetColliderCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetColliderCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id) return false;
		m_after = command->m_after; return true;
	}

	RemoveRigidBodyCommand::RemoveRigidBodyCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::RigidBodyComponent value) : m_scene(scene), m_id(id), m_value(value) {}
	void RemoveRigidBodyCommand::Execute()
	{
		Scene::ComponentCatalog::Remove(m_scene, m_scene.FindEntityByID(m_id), Scene::ComponentKind::RigidBody);
	}
	void RemoveRigidBodyCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		if (Scene::ComponentCatalog::AddDefault(m_scene, entity, Scene::ComponentKind::RigidBody))
			m_scene.SetRigidBody(entity, m_value);
	}

	RemoveColliderCommand::RemoveColliderCommand(Scene::Scene& scene, std::uint64_t id,
		Scene::ColliderComponent value) : m_scene(scene), m_id(id), m_value(value) {}
	void RemoveColliderCommand::Execute()
	{
		Scene::ComponentCatalog::Remove(m_scene, m_scene.FindEntityByID(m_id), Scene::ComponentKind::Collider);
	}
	void RemoveColliderCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		if (Scene::ComponentCatalog::AddDefault(m_scene, entity, Scene::ComponentKind::Collider))
			m_scene.SetCollider(entity, m_value);
	}

	void CompositeCommand::Add(std::unique_ptr<EditorCommand> command)
	{
		if (command)
			m_commands.push_back(std::move(command));
	}

	void CompositeCommand::Execute()
	{
		for (const auto& command : m_commands)
			command->Execute();
	}

	void CompositeCommand::Undo()
	{
		for (auto command = m_commands.rbegin(); command != m_commands.rend(); ++command)
			(*command)->Undo();
	}

	bool CompositeCommand::MergeWith(const EditorCommand& other)
	{
		const auto* composite = dynamic_cast<const CompositeCommand*>(&other);
		if (!composite || composite->m_commands.size() != m_commands.size())
			return false;
		for (size_t index = 0; index < m_commands.size(); ++index)
		{
			if (!m_commands[index]->MergeWith(*composite->m_commands[index]))
				return false;
		}
		return true;
	}

	RenameEntityCommand::RenameEntityCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		std::string before,
		std::string after)
	: m_scene(scene), m_id(id), m_before(std::move(before)), m_after(std::move(after)) {}

	void RenameEntityCommand::Execute()
	{
		m_scene.RenameEntity(m_scene.FindEntityByID(m_id), m_after);
	}

	void RenameEntityCommand::Undo()
	{
		m_scene.RenameEntity(m_scene.FindEntityByID(m_id), m_before);
	}

	SetTransformCommand::SetTransformCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		TransformValue before,
		TransformValue after)
	: m_scene(scene), m_id(id), m_before(before), m_after(after) {}

	void SetTransformCommand::Apply(const TransformValue& value)
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		if (value.HasRotationQuaternion)
		{
			m_scene.SetTransformQuaternion(
				entity,
				value.Position,
				value.RotationQuaternion,
				value.Rotation,
				value.Scale);
		}
		else
		{
			m_scene.SetTransform(entity, value.Position, value.Rotation, value.Scale);
		}
	}

	void SetTransformCommand::Execute()
	{
		Apply(m_after);
	}

	void SetTransformCommand::Undo()
	{
		Apply(m_before);
	}

	bool SetTransformCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetTransformCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetParentCommand::SetParentCommand(
		Scene::Scene& scene,
		std::uint64_t child,
		std::uint64_t beforeParent,
		std::uint64_t afterParent)
	: m_scene(scene), m_child(child), m_before(beforeParent), m_after(afterParent)
	{
		const entt::entity childEntity = m_scene.FindEntityByID(m_child);
		if (const auto* transform = m_scene.GetComponent<Scene::TransformComponent>(childEntity))
			m_beforeTransform = ReadTransform(*transform);
	}

	TransformValue SetParentCommand::ReadTransform(const Scene::TransformComponent& transform)
	{
		return {
			transform.GetLocalPosition(),
			transform.GetLocalRotation(),
			transform.GetLocalScale(),
				transform.GetLocalRotationQuaternion(),
			true
		};
	}

	void SetParentCommand::Apply(std::uint64_t parent, const TransformValue& transform)
	{
		const entt::entity child = m_scene.FindEntityByID(m_child);
		const entt::entity target = parent == 0 ? entt::null : m_scene.FindEntityByID(parent);
		m_scene.SetParent(child, target);
		if (transform.HasRotationQuaternion)
		{
			m_scene.SetTransformQuaternion(
				child,
				transform.Position,
				transform.RotationQuaternion,
				transform.Rotation,
				transform.Scale);
		}
		else
		{
			m_scene.SetTransform(child, transform.Position, transform.Rotation, transform.Scale);
		}
	}

	void SetParentCommand::Execute()
	{
		if (m_hasAfterTransform)
		{
			Apply(m_after, m_afterTransform);
			return;
		}

		const entt::entity child = m_scene.FindEntityByID(m_child);
		const entt::entity target = m_after == 0 ? entt::null : m_scene.FindEntityByID(m_after);
		if (!m_scene.SetParentKeepWorld(child, target))
			return;
		if (const auto* transform = m_scene.GetComponent<Scene::TransformComponent>(child))
		{
			m_afterTransform = ReadTransform(*transform);
			m_hasAfterTransform = true;
		}
	}

	void SetParentCommand::Undo()
	{
		Apply(m_before, m_beforeTransform);
	}

	SetRendererVisibilityCommand::SetRendererVisibilityCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		bool before,
		bool after)
	: m_scene(scene), m_id(id), m_before(before), m_after(after) {}

	void SetRendererVisibilityCommand::Execute()
	{
		m_scene.SetRendererVisibility(m_scene.FindEntityByID(m_id), m_after);
	}

	void SetRendererVisibilityCommand::Undo()
	{
		m_scene.SetRendererVisibility(m_scene.FindEntityByID(m_id), m_before);
	}

	bool SetRendererVisibilityCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRendererVisibilityCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetRendererShadowFlagsCommand::SetRendererShadowFlagsCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		bool beforeCast,
		bool beforeReceive,
		bool afterCast,
		bool afterReceive)
		: m_scene(scene)
		, m_id(id)
		, m_beforeCast(beforeCast)
		, m_beforeReceive(beforeReceive)
		, m_afterCast(afterCast)
		, m_afterReceive(afterReceive)
	{
	}

	void SetRendererShadowFlagsCommand::Execute()
	{
		m_scene.SetRendererShadowFlags(
			m_scene.FindEntityByID(m_id), m_afterCast, m_afterReceive);
	}

	void SetRendererShadowFlagsCommand::Undo()
	{
		m_scene.SetRendererShadowFlags(
			m_scene.FindEntityByID(m_id), m_beforeCast, m_beforeReceive);
	}

	bool SetRendererShadowFlagsCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRendererShadowFlagsCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_afterCast = command->m_afterCast;
		m_afterReceive = command->m_afterReceive;
		return true;
	}

	SetRendererAlbedoCommand::SetRendererAlbedoCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		DirectX::XMFLOAT4 before,
		DirectX::XMFLOAT4 after)
	: m_scene(scene), m_id(id), m_before(before), m_after(after) {}

	void SetRendererAlbedoCommand::Execute()
	{
		m_scene.SetRendererAlbedo(m_scene.FindEntityByID(m_id), m_after);
	}

	void SetRendererAlbedoCommand::Undo()
	{
		m_scene.SetRendererAlbedo(m_scene.FindEntityByID(m_id), m_before);
	}

	bool SetRendererAlbedoCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRendererAlbedoCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetRendererMaterialCommand::SetRendererMaterialCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		MaterialMode before,
		MaterialMode after)
	: m_scene(scene), m_id(id), m_before(before), m_after(after) {}

	void SetRendererMaterialCommand::Execute()
	{
		m_scene.SetRendererMaterial(m_scene.FindEntityByID(m_id), m_after);
	}

	void SetRendererMaterialCommand::Undo()
	{
		m_scene.SetRendererMaterial(m_scene.FindEntityByID(m_id), m_before);
	}

	bool SetRendererMaterialCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRendererMaterialCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetCameraCommand::SetCameraCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		Scene::CameraComponent before,
		Scene::CameraComponent after)
	: m_scene(scene), m_id(id), m_before(before), m_after(after) {}

	void SetCameraCommand::Execute()
	{
		m_scene.SetCamera(m_scene.FindEntityByID(m_id), m_after);
	}

	void SetCameraCommand::Undo()
	{
		m_scene.SetCamera(m_scene.FindEntityByID(m_id), m_before);
	}

	bool SetCameraCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetCameraCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetLightCommand::SetLightCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		Scene::LightComponent before,
		Scene::LightComponent after)
	: m_scene(scene)
	, m_id(id)
	, m_before(before)
	, m_after(after)
	, m_beforeActive(scene.GetSettings().ActiveLightEntityID)
	, m_afterActive(m_beforeActive)
	{
	}

	void SetLightCommand::Execute()
	{
		m_scene.SetLight(m_scene.FindEntityByID(m_id), m_after);
		m_scene.SetActiveLightEntityID(m_afterActive);
	}

	void SetLightCommand::Undo()
	{
		m_scene.SetLight(m_scene.FindEntityByID(m_id), m_before);
		m_scene.SetActiveLightEntityID(m_beforeActive);
	}

	bool SetLightCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetLightCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		m_afterActive = command->m_afterActive;
		return true;
	}

	SetActiveCameraCommand::SetActiveCameraCommand(
		Scene::Scene& scene,
		std::uint64_t before,
		std::uint64_t after)
	: m_scene(scene), m_before(before), m_after(after) {}

	void SetActiveCameraCommand::Execute()
	{
		m_scene.SetActiveCameraEntityID(m_after);
	}

	void SetActiveCameraCommand::Undo()
	{
		m_scene.SetActiveCameraEntityID(m_before);
	}

	SetActiveLightCommand::SetActiveLightCommand(
		Scene::Scene& scene,
		std::uint64_t before,
		std::uint64_t after)
	: m_scene(scene), m_before(before), m_after(after) {}

	void SetActiveLightCommand::Execute()
	{
		m_scene.SetActiveLightEntityID(m_after);
	}

	void SetActiveLightCommand::Undo()
	{
		m_scene.SetActiveLightEntityID(m_before);
	}

	CreateEntityCommand::CreateEntityCommand(Scene::Scene& scene, std::string name, std::uint64_t parent)
		: m_scene(scene), m_name(std::move(name)), m_parent(parent)
	{
	}

	CreateEntityCommand::CreateEntityCommand(
		Scene::Scene& scene,
		std::string name,
		const Scene::CameraComponent& camera,
		std::uint64_t parent)
		: m_scene(scene)
		, m_name(std::move(name))
		, m_parent(parent)
		, m_camera(camera)
	{
	}

	CreateEntityCommand::CreateEntityCommand(
		Scene::Scene& scene,
		std::string name,
		const Scene::LightComponent& light,
		std::uint64_t parent)
		: m_scene(scene)
		, m_name(std::move(name))
		, m_parent(parent)
		, m_light(light)
	{
	}

	void CreateEntityCommand::Execute()
	{
		if (!m_snapshot.Empty())
		{
			m_snapshot.Restore(m_scene);
			return;
		}

		const entt::entity entity = m_scene.CreateEntity(m_name);
		m_scene.AddComponent<Scene::TransformComponent>(entity);
		m_scene.AddComponent<Scene::HierarchyComponent>(entity);
		if (m_camera)
			m_scene.AddComponent<Scene::CameraComponent>(entity, *m_camera);
		if (m_light)
			m_scene.AddComponent<Scene::LightComponent>(entity, *m_light);
		if (m_parent != 0)
			m_scene.SetParent(entity, m_scene.FindEntityByID(m_parent));
		m_snapshot = EntitySnapshot::CaptureSubtree(m_scene, entity);
	}

	void CreateEntityCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_snapshot.GetRootID());
		if (entity != entt::null)
			m_scene.DestroyEntity(entity);
	}

	CreatePrimitiveCommand::CreatePrimitiveCommand(
		Scene::Scene& scene,
		Scene::PrimitiveDesc desc,
		Scene::PrimitiveModelFactory modelFactory,
		std::uint64_t parent)
		: m_scene(scene)
		, m_desc(desc)
		, m_modelFactory(std::move(modelFactory))
		, m_parent(parent)
	{
	}

	void CreatePrimitiveCommand::Execute()
	{
		if (!m_snapshot.Empty())
		{
			m_snapshot.Restore(m_scene);
			return;
		}
		if (!m_modelFactory)
			return;

		const entt::entity entity = Scene::CreatePrimitive(m_scene, m_desc, m_modelFactory);
		if (entity == entt::null)
			return;
		if (m_parent != 0)
			m_scene.SetParent(entity, m_scene.FindEntityByID(m_parent));
		m_snapshot = EntitySnapshot::CaptureSubtree(m_scene, entity);
	}

	void CreatePrimitiveCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_snapshot.GetRootID());
		if (entity != entt::null)
			m_scene.DestroyEntity(entity);
	}

	DeleteEntityCommand::DeleteEntityCommand(Scene::Scene& scene, entt::entity entity)
		: m_scene(scene)
		, m_snapshot(EntitySnapshot::CaptureSubtree(scene, entity))
	{
	}

	void DeleteEntityCommand::Execute()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_snapshot.GetRootID());
		if (entity != entt::null)
			m_scene.DestroyEntity(entity);
	}

	void DeleteEntityCommand::Undo()
	{
		m_snapshot.Restore(m_scene);
	}

	SetRendererMaterialResourceCommand::SetRendererMaterialResourceCommand(
		Scene::Scene& scene,
		std::uint64_t id,
		std::optional<MaterialResource> before,
		std::optional<MaterialResource> after)
		: m_scene(scene), m_id(id), m_before(std::move(before)), m_after(std::move(after)) {}

	void SetRendererMaterialResourceCommand::Apply(
		const std::optional<MaterialResource>& value)
	{
		m_scene.SetRendererMaterialResource(
			m_scene.FindEntityByID(m_id),
			value ? std::make_shared<MaterialResource>(*value) : nullptr);
	}

	void SetRendererMaterialResourceCommand::Execute() { Apply(m_after); }
	void SetRendererMaterialResourceCommand::Undo() { Apply(m_before); }

	bool SetRendererMaterialResourceCommand::MergeWith(const EditorCommand& other)
	{
		const auto* command = dynamic_cast<const SetRendererMaterialResourceCommand*>(&other);
		if (!command || &command->m_scene != &m_scene || command->m_id != m_id)
			return false;
		m_after = command->m_after;
		return true;
	}

	SetRendererModelCommand::SetRendererModelCommand(
		Scene::Scene& scene, std::uint64_t id,
		std::shared_ptr<Model> beforeModel,
		std::optional<Scene::RenderSourceComponent> beforeSource,
		std::shared_ptr<Model> afterModel,
		std::optional<Scene::RenderSourceComponent> afterSource)
		: m_scene(scene), m_id(id), m_beforeModel(std::move(beforeModel)),
		m_afterModel(std::move(afterModel)), m_beforeSource(std::move(beforeSource)),
		m_afterSource(std::move(afterSource)) {}

	void SetRendererModelCommand::Apply(
		const std::shared_ptr<Model>& model,
		const std::optional<Scene::RenderSourceComponent>& source)
	{
		const entt::entity entity = m_scene.FindEntityByID(m_id);
		m_scene.SetRendererModel(entity, model);
		if (source)
			m_scene.AddComponent<Scene::RenderSourceComponent>(entity, *source);
		else
			m_scene.RemoveComponent<Scene::RenderSourceComponent>(entity);
		m_scene.MarkRenderDataChanged();
	}

	void SetRendererModelCommand::Execute() { Apply(m_afterModel, m_afterSource); }
	void SetRendererModelCommand::Undo() { Apply(m_beforeModel, m_beforeSource); }

	DuplicateEntityCommand::DuplicateEntityCommand(Scene::Scene& scene, entt::entity source)
		: m_scene(scene)
		, m_sourceID(scene.GetEntityID(source))
	{
	}

	void DuplicateEntityCommand::Execute()
	{
		if (!m_snapshot.Empty())
		{
			m_snapshot.Restore(m_scene);
			return;
		}
		m_snapshot = EntitySnapshot::DuplicateSubtree(
			m_scene,
			m_scene.FindEntityByID(m_sourceID));
	}

	void DuplicateEntityCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_snapshot.GetRootID());
		if (entity != entt::null)
			m_scene.DestroyEntity(entity);
	}

	PasteEntityCommand::PasteEntityCommand(Scene::Scene& scene, EntitySnapshot source)
		: m_scene(scene), m_source(std::move(source)) {}

	void PasteEntityCommand::Execute()
	{
		if (m_pasted.Empty())
			m_pasted = m_source.Duplicate(m_scene);
		else
			m_pasted.Restore(m_scene);
	}

	void PasteEntityCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_pasted.GetRootID());
		if (entity != entt::null)
			m_scene.DestroyEntity(entity);
	}

	InstantiatePrefabCommand::InstantiatePrefabCommand(
		Scene::Scene& scene,
		PrefabAsset prefab,
		std::uint64_t assetHandle,
		std::string assetPath,
		std::uint64_t parentID,
		std::optional<DirectX::XMFLOAT3> rootPosition)
		: m_scene(scene), m_prefab(std::move(prefab)), m_assetHandle(assetHandle)
		, m_assetPath(std::move(assetPath)), m_parentID(parentID)
		, m_rootPosition(rootPosition) {}

	void InstantiatePrefabCommand::Execute()
	{
		if (m_instance.Empty())
		{
			m_instance = m_prefab.Instantiate(m_scene, m_assetHandle, m_assetPath, m_parentID);
			if (m_rootPosition && !m_instance.Empty())
			{
				const entt::entity root = m_scene.FindEntityByID(m_instance.GetRootID());
				m_scene.SetLocalPosition(root, *m_rootPosition);
				m_instance = EntitySnapshot::CaptureSubtree(m_scene, root);
			}
		}
		else
			m_instance.Restore(m_scene);
	}

	void InstantiatePrefabCommand::Undo()
	{
		const entt::entity entity = m_scene.FindEntityByID(m_instance.GetRootID());
		if (entity != entt::null) m_scene.DestroyEntity(entity);
	}

	DeleteAssetCommand::DeleteAssetCommand(
		Graphics::AssetRegistry& assets,
		Graphics::AssetReference asset)
		: m_assets(assets), m_asset(std::move(asset))
	{
		m_source = m_assets.ResolvePath(m_asset.Path);
		m_sourceMetadata = m_source.string() + ".meta";
		const std::filesystem::path directory =
			m_assets.GetProjectRoot() / ".jnpf" / "trash";
		std::string trashName = std::to_string(m_asset.Handle) + "_" + m_source.filename().string();
		m_trash = directory / trashName;
		for (unsigned suffix = 1; std::filesystem::exists(m_trash); ++suffix)
			m_trash = directory / (trashName + "." + std::to_string(suffix));
		m_trashMetadata = m_trash.string() + ".meta";
	}

	void DeleteAssetCommand::Execute()
	{
		std::error_code error;
		std::filesystem::create_directories(m_trash.parent_path(), error);
		error.clear();
		if (!std::filesystem::exists(m_source) || std::filesystem::exists(m_trash)) return;
		std::filesystem::rename(m_source, m_trash, error);
		if (error) return;
		if (std::filesystem::exists(m_sourceMetadata))
		{
			error.clear();
			std::filesystem::rename(m_sourceMetadata, m_trashMetadata, error);
		}
		m_assets.Unregister(m_asset.Handle);
		m_deleted = true;
	}

	void DeleteAssetCommand::Undo()
	{
		if (!m_deleted || !std::filesystem::exists(m_trash)) return;
		std::error_code error;
		std::filesystem::create_directories(m_source.parent_path(), error);
		error.clear();
		std::filesystem::rename(m_trash, m_source, error);
		if (error) return;
		if (std::filesystem::exists(m_trashMetadata))
		{
			error.clear();
			std::filesystem::rename(m_trashMetadata, m_sourceMetadata, error);
		}
		m_assets.RegisterReference(m_asset);
		m_deleted = false;
	}

	UnpackPrefabCommand::UnpackPrefabCommand(Scene::Scene& scene, entt::entity instanceEntity)
		: m_scene(scene)
	{
		const auto* selected = scene.GetComponent<Scene::PrefabInstanceComponent>(instanceEntity);
		if (!selected) return;
		entt::entity root = instanceEntity;
		while (true)
		{
			const entt::entity parent = scene.GetParent(root);
			if (parent == entt::null) break;
			const auto* parentPrefab = scene.GetComponent<Scene::PrefabInstanceComponent>(parent);
			if (!parentPrefab || parentPrefab->PrefabAssetHandle != selected->PrefabAssetHandle) break;
			root = parent;
		}
		const auto collect = [this, &scene, handle = selected->PrefabAssetHandle](
			auto&& self, entt::entity entity) -> void
		{
			if (const auto* prefab = scene.GetComponent<Scene::PrefabInstanceComponent>(entity);
				prefab && prefab->PrefabAssetHandle == handle)
				m_entries.push_back({scene.GetEntityID(entity), *prefab});
			for (const entt::entity child : scene.GetChildren(entity)) self(self, child);
		};
		collect(collect, root);
	}

	void UnpackPrefabCommand::Execute()
	{
		for (const Entry& entry : m_entries)
			m_scene.RemoveComponent<Scene::PrefabInstanceComponent>(
				m_scene.FindEntityByID(entry.ID));
	}

	void UnpackPrefabCommand::Undo()
	{
		for (const Entry& entry : m_entries)
		{
			const entt::entity entity = m_scene.FindEntityByID(entry.ID);
			if (entity != entt::null)
				m_scene.AddComponent<Scene::PrefabInstanceComponent>(entity, entry.Component);
		}
	}

	AdoptEntityCommand::AdoptEntityCommand(Scene::Scene& scene, EntitySnapshot snapshot)
		: m_scene(scene), m_snapshot(std::move(snapshot)) {}

	void AdoptEntityCommand::Execute()
	{
		m_snapshot.Restore(m_scene);
	}

	void AdoptEntityCommand::Undo()
	{
		const entt::entity root = m_scene.FindEntityByID(m_snapshot.GetRootID());
		if (root != entt::null) m_scene.DestroyEntityRecursive(root);
	}
}
