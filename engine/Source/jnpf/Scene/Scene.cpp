#include "Scene/Scene.h"

#include "ECS/Systems/HierarchySystem.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
	bool Equal(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs)
	{
		return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
	}

	bool Equal(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs)
	{
		return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
	}
}

namespace jnpf::Scene
{
	Scene::Scene(const std::string& name)
		: m_name(name)
	{
		BindRegistryCallbacks();
	}

	Scene::Scene(Scene&& other) noexcept
	{
		*this = std::move(other);
	}

	Scene& Scene::operator=(Scene&& other) noexcept
	{
		if (this == &other)
			return *this;
		m_name = std::move(other.m_name);
		m_registry = std::move(other.m_registry);
		m_entitiesByID = std::move(other.m_entitiesByID);
		m_destroyQueue = std::move(other.m_destroyQueue);
		m_dirtyTransformRoots = std::move(other.m_dirtyTransformRoots);
		m_dirtyTransformRootSet = std::move(other.m_dirtyTransformRootSet);
		m_allTransformsDirty = other.m_allTransformsDirty;
		m_versions = other.m_versions;
		m_settings = other.m_settings;
		m_nextEntityID = other.m_nextEntityID;
		BindRegistryCallbacks();
		return *this;
	}

	void Scene::BindRegistryCallbacks()
	{
		if (m_registry.ctx().contains<Scene*>())
			m_registry.ctx().erase<Scene*>();
		m_registry.ctx().emplace<Scene*>(this);
		m_registry.on_update<MeshRendererComponent>().disconnect<&Scene::OnMeshRendererUpdated>();
		m_registry.on_update<MeshRendererComponent>().connect<&Scene::OnMeshRendererUpdated>();
	}

	void Scene::OnMeshRendererUpdated(entt::registry& registry, entt::entity)
	{
		if (Scene* scene = registry.ctx().get<Scene*>())
			scene->MarkRenderDirty();
	}

	std::uint64_t Scene::GenerateEntityID()
	{
		while (m_nextEntityID == 0 || FindEntityByID(m_nextEntityID) != entt::null)
			++m_nextEntityID;
		return m_nextEntityID++;
	}

	entt::entity Scene::CreateEntity()
	{
		return CreateEntityWithID(GenerateEntityID(), "Entity");
	}

	entt::entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithID(GenerateEntityID(), name);
	}

	entt::entity Scene::CreateEntityWithID(std::uint64_t id, const std::string& name)
	{
		if (id == 0 || m_entitiesByID.contains(id))
			return entt::null;
		const entt::entity entity = m_registry.create();
		m_registry.emplace<IDComponent>(entity, id);
		m_registry.emplace<TagComponent>(entity, name.c_str());
		m_entitiesByID.emplace(id, entity);
		m_nextEntityID = std::max(m_nextEntityID, id + 1);
		MarkStructureDirty();
		return entity;
	}

	entt::entity Scene::FindEntityByID(std::uint64_t id) const
	{
		if (id == 0)
			return entt::null;
		const auto found = m_entitiesByID.find(id);
		return found != m_entitiesByID.end() && m_registry.valid(found->second)
			? found->second : entt::null;
	}

	std::uint64_t Scene::GetEntityID(entt::entity entity) const
	{
		const auto* id = GetComponent<IDComponent>(entity);
		return id ? id->ID : 0;
	}

	bool Scene::RenameEntity(entt::entity entity, const std::string& name)
	{
		auto* tag = GetComponentMut<TagComponent>(entity);
		if (!tag || name == tag->Tag)
			return false;
		strncpy_s(tag->Tag, TagComponent::MaxTagLength, name.c_str(), TagComponent::MaxTagLength - 1);
		return true;
	}

	std::vector<entt::entity> Scene::GetAllEntities() const
	{
		std::vector<entt::entity> result;
		const auto* storage = m_registry.storage<entt::entity>();
		if (!storage)
			return result;
		result.reserve(storage->size());
		for (auto [entity] : storage->each())
			result.push_back(entity);
		return result;
	}

	size_t Scene::GetEntityCount() const
	{
		const auto* storage = m_registry.storage<entt::entity>();
		if (!storage)
			return 0;
		size_t count = 0;
		for ([[maybe_unused]] auto [entity] : storage->each())
			++count;
		return count;
	}

	void Scene::MarkStructureDirty()
	{
		++m_versions.StructureVersion;
		++m_versions.RenderVersion;
	}

	void Scene::MarkTransformDirty()
	{
		m_allTransformsDirty = true;
		m_dirtyTransformRoots.clear();
		m_dirtyTransformRootSet.clear();
		++m_versions.TransformVersion;
	}

	DirtyTransformSet Scene::TakeDirtyTransforms()
	{
		DirtyTransformSet result;
		result.All = m_allTransformsDirty;
		if (!result.All)
			result.Roots = std::move(m_dirtyTransformRoots);
		m_dirtyTransformRoots.clear();
		m_dirtyTransformRootSet.clear();
		m_allTransformsDirty = false;
		return result;
	}

	void Scene::MarkRenderDirty()
	{
		++m_versions.RenderVersion;
	}

	void Scene::MarkTransformSubtreeDirty(entt::entity entity)
	{
		if (!IsEntityValid(entity))
			return;
		if (auto* transform = GetComponentMut<TransformComponent>(entity))
		{
			transform->MarkWorldTransformDirty();
			if (!m_allTransformsDirty && m_dirtyTransformRootSet.insert(entity).second)
				m_dirtyTransformRoots.push_back(entity);
		}
	}

	void Scene::MarkTransformDirty(entt::entity entity)
	{
		MarkTransformSubtreeDirty(entity);
		++m_versions.TransformVersion;
	}

	bool Scene::SetLocalPosition(entt::entity entity, const DirectX::XMFLOAT3& value)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform || Equal(transform->GetLocalPosition(), value))
			return false;
		transform->SetLocalPosition(value);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetLocalRotation(entt::entity entity, const DirectX::XMFLOAT3& value)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform || Equal(transform->GetLocalRotation(), value))
			return false;
		transform->SetLocalRotation(value);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetLocalRotationQuaternion(
		entt::entity entity,
		const DirectX::XMFLOAT4& value,
		const DirectX::XMFLOAT3& eulerHint)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform)
			return false;
		transform->SetLocalRotationQuaternion(value, eulerHint);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetLocalScale(entt::entity entity, const DirectX::XMFLOAT3& value)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform || Equal(transform->GetLocalScale(), value))
			return false;
		transform->SetLocalScale(value);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetTransformQuaternion(
		entt::entity entity,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT4& rotation,
		const DirectX::XMFLOAT3& eulerHint,
		const DirectX::XMFLOAT3& scale)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform)
			return false;
		transform->SetLocalTransformQuaternion(position, rotation, eulerHint, scale);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetTransform(
		entt::entity entity,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& rotation,
		const DirectX::XMFLOAT3& scale)
	{
		auto* transform = GetComponentMut<TransformComponent>(entity);
		if (!transform
			|| (Equal(transform->GetLocalPosition(), position)
				&& Equal(transform->GetLocalRotation(), rotation)
				&& Equal(transform->GetLocalScale(), scale)))
			return false;
		transform->SetLocalTransform(position, rotation, scale);
		MarkTransformDirty(entity);
		return true;
	}

	bool Scene::SetRendererVisibility(entt::entity entity, bool visible)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer || renderer->bVisible == visible)
			return false;
		renderer->bVisible = visible;
		MarkRenderDirty();
		return true;
	}

	void Scene::CopyFrom(const Scene& source)
	{
		if (this == &source) return;
		Clear();
		m_name = source.m_name;
		const std::vector<entt::entity> sourceEntities = source.GetAllEntities();
		std::unordered_map<const MaterialResource*, std::shared_ptr<MaterialResource>> materialCopies;
		for (const entt::entity sourceEntity : sourceEntities)
		{
			if (source.HasComponent<TransientEditorComponent>(sourceEntity)) continue;
			const std::uint64_t id = source.GetEntityID(sourceEntity);
			const auto* tag = source.GetComponent<TagComponent>(sourceEntity);
			const entt::entity entity = CreateEntityWithID(id, tag ? tag->Tag : "Entity");
			if (const auto* value = source.GetComponent<TransformComponent>(sourceEntity))
			{
				auto copy = *value;
				copy.MarkWorldTransformDirty();
				AddComponent<TransformComponent>(entity, copy);
			}
			if (source.HasComponent<HierarchyComponent>(sourceEntity))
				AddComponent<HierarchyComponent>(entity);
			if (const auto* value = source.GetComponent<MeshRendererComponent>(sourceEntity))
			{
				auto copy = *value;
				if (value->MaterialResourcePtr)
				{
					const MaterialResource* sourceMaterial = value->MaterialResourcePtr.get();
					if (const auto cached = materialCopies.find(sourceMaterial);
						cached != materialCopies.end())
						copy.MaterialResourcePtr = cached->second;
					else
					{
						copy.MaterialResourcePtr = std::make_shared<MaterialResource>(*sourceMaterial);
						materialCopies.emplace(sourceMaterial, copy.MaterialResourcePtr);
					}
				}
				AddComponent<MeshRendererComponent>(entity, copy);
			}
			if (const auto* value = source.GetComponent<RenderSourceComponent>(sourceEntity))
				AddComponent<RenderSourceComponent>(entity, *value);
			if (const auto* value = source.GetComponent<CameraComponent>(sourceEntity))
				AddComponent<CameraComponent>(entity, *value);
			if (const auto* value = source.GetComponent<LightComponent>(sourceEntity))
				AddComponent<LightComponent>(entity, *value);
			if (const auto* value = source.GetComponent<RotatorComponent>(sourceEntity))
				AddComponent<RotatorComponent>(entity, *value);
			if (const auto* value = source.GetComponent<FlyControllerComponent>(sourceEntity))
				AddComponent<FlyControllerComponent>(entity, *value);
			if (const auto* value = source.GetComponent<RigidBodyComponent>(sourceEntity))
				AddComponent<RigidBodyComponent>(entity, *value);
			if (const auto* value = source.GetComponent<ColliderComponent>(sourceEntity))
				AddComponent<ColliderComponent>(entity, *value);
			if (const auto* value = source.GetComponent<PrefabInstanceComponent>(sourceEntity))
				AddComponent<PrefabInstanceComponent>(entity, *value);
		}
		for (const entt::entity sourceEntity : sourceEntities)
		{
			if (source.HasComponent<TransientEditorComponent>(sourceEntity)) continue;
			const entt::entity sourceParent = source.GetParent(sourceEntity);
			if (sourceParent == entt::null) continue;
			SetParent(
				FindEntityByID(source.GetEntityID(sourceEntity)),
				FindEntityByID(source.GetEntityID(sourceParent)));
		}
		m_settings = source.m_settings;
		m_destroyQueue.clear();
		MarkStructureDirty();
		MarkTransformDirty();
		MarkRenderDirty();
	}

	bool Scene::SetRendererShadowFlags(
		entt::entity entity,
		bool castShadows,
		bool receiveShadows)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer
			|| (renderer->bCastShadows == castShadows
				&& renderer->bReceiveShadows == receiveShadows))
			return false;
		renderer->bCastShadows = castShadows;
		renderer->bReceiveShadows = receiveShadows;
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetRendererMaterial(entt::entity entity, MaterialMode material)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer || renderer->Material == material)
			return false;
		renderer->Material = material;
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetRendererAlbedo(entt::entity entity, const DirectX::XMFLOAT4& albedo)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer || Equal(renderer->Albedo, albedo))
			return false;
		renderer->Albedo = albedo;
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetRendererModel(entt::entity entity, std::shared_ptr<Model> model)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer || renderer->ModelPtr == model)
			return false;
		renderer->ModelPtr = std::move(model);
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetRendererMaterialResource(
		entt::entity entity,
		std::shared_ptr<MaterialResource> material)
	{
		auto* renderer = GetComponentMut<MeshRendererComponent>(entity);
		if (!renderer)
			return false;
		renderer->MaterialResourcePtr = std::move(material);
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetCamera(entt::entity entity, const CameraComponent& camera)
	{
		auto* current = GetComponentMut<CameraComponent>(entity);
		if (!current
			|| camera.FOVDegrees < 1.0f
			|| camera.FOVDegrees > 179.0f
			|| camera.NearPlane <= 0.0f
			|| camera.FarPlane <= camera.NearPlane
			|| camera.AspectRatio <= 0.0f)
			return false;
		*current = camera;
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetLight(entt::entity entity, const LightComponent& light)
	{
		auto* current = GetComponentMut<LightComponent>(entity);
		const int type = static_cast<int>(light.LightType);
		if (!current
			|| type < static_cast<int>(LightComponent::Type::Directional)
			|| type > static_cast<int>(LightComponent::Type::Spot)
			|| !std::isfinite(light.Color.x)
			|| !std::isfinite(light.Color.y)
			|| !std::isfinite(light.Color.z)
			|| !std::isfinite(light.Intensity)
			|| !std::isfinite(light.Range)
			|| !std::isfinite(light.SpotAngle)
			|| !std::isfinite(light.ShadowStrength)
			|| !std::isfinite(light.ShadowBias)
			|| !std::isfinite(light.ShadowNormalBias)
			|| !std::isfinite(light.ShadowDistance)
			|| light.Color.x < 0.0f
			|| light.Color.y < 0.0f
			|| light.Color.z < 0.0f
			|| light.Intensity < 0.0f
			|| light.Range < 0.0f
			|| light.SpotAngle <= 0.0f
			|| light.SpotAngle >= 180.0f
			|| light.ShadowStrength < 0.0f
			|| light.ShadowStrength > 1.0f
			|| light.ShadowBias < 0.0f
			|| light.ShadowNormalBias < 0.0f
			|| light.ShadowDistance <= 0.0f)
			return false;
		*current = light;
		MarkRenderDirty();
		return true;
	}

	bool Scene::SetRotator(entt::entity entity, const RotatorComponent& rotator)
	{
		auto* current = GetComponentMut<RotatorComponent>(entity);
		if (!current || !std::isfinite(rotator.AngularVelocity.x)
			|| !std::isfinite(rotator.AngularVelocity.y)
			|| !std::isfinite(rotator.AngularVelocity.z)) return false;
		*current = rotator;
		MarkStructureDirty();
		return true;
	}

	bool Scene::SetFlyController(entt::entity entity, const FlyControllerComponent& controller)
	{
		auto* current = GetComponentMut<FlyControllerComponent>(entity);
		if (!current || !std::isfinite(controller.MoveSpeed)
			|| !std::isfinite(controller.LookSensitivity)
			|| !std::isfinite(controller.BoostMultiplier)
			|| !std::isfinite(controller.PitchLimitDegrees)
			|| controller.MoveSpeed < 0 || controller.LookSensitivity < 0
			|| controller.BoostMultiplier < 1 || controller.PitchLimitDegrees <= 0
			|| controller.PitchLimitDegrees >= 90) return false;
		*current = controller;
		MarkStructureDirty();
		return true;
	}

	bool Scene::SetRigidBody(entt::entity entity, const RigidBodyComponent& body)
	{
		auto* current = GetComponentMut<RigidBodyComponent>(entity);
		if (!current || body.Motion < RigidBodyMotion::Static || body.Motion > RigidBodyMotion::Kinematic
			|| !std::isfinite(body.Friction) || !std::isfinite(body.Restitution)
			|| !std::isfinite(body.LinearDamping) || !std::isfinite(body.AngularDamping)
			|| !std::isfinite(body.GravityFactor) || body.Friction < 0.0f
			|| body.Restitution < 0.0f || body.Restitution > 1.0f
			|| body.LinearDamping < 0.0f || body.AngularDamping < 0.0f)
			return false;
		*current = body;
		MarkStructureDirty();
		return true;
	}

	bool Scene::SetCollider(entt::entity entity, const ColliderComponent& collider)
	{
		auto* current = GetComponentMut<ColliderComponent>(entity);
		if (!current || collider.Shape < ColliderShape::Box || collider.Shape > ColliderShape::Sphere
			|| !std::isfinite(collider.HalfExtents.x) || !std::isfinite(collider.HalfExtents.y)
			|| !std::isfinite(collider.HalfExtents.z) || !std::isfinite(collider.Radius)
			|| collider.HalfExtents.x <= 0.0f || collider.HalfExtents.y <= 0.0f
			|| collider.HalfExtents.z <= 0.0f || collider.Radius <= 0.0f)
			return false;
		*current = collider;
		MarkStructureDirty();
		return true;
	}

	bool Scene::SetActiveCameraEntityID(std::uint64_t id)
	{
		if (id != 0)
		{
			const entt::entity entity = FindEntityByID(id);
			if (entity == entt::null || !HasComponent<CameraComponent>(entity))
				return false;
		}
		if (m_settings.ActiveCameraEntityID == id)
			return false;
		m_settings.ActiveCameraEntityID = id;
		return true;
	}

	bool Scene::SetActiveLightEntityID(std::uint64_t id)
	{
		if (id != 0)
		{
			const entt::entity entity = FindEntityByID(id);
			const auto* light = GetComponent<LightComponent>(entity);
			if (!light)
				return false;
		}
		if (m_settings.ActiveLightEntityID == id)
			return false;
		m_settings.ActiveLightEntityID = id;
		MarkRenderDirty();
		return true;
	}

	bool Scene::CanSetParent(entt::entity child, entt::entity newParent) const
	{
		return IsEntityValid(child)
			&& (newParent == entt::null || IsEntityValid(newParent))
			&& child != newParent
			&& (newParent == entt::null
				|| !ECS::HierarchySystem::IsAncestor(m_registry, child, newParent));
	}

	bool Scene::SetParent(entt::entity child, entt::entity newParent)
	{
		if (!CanSetParent(child, newParent) || GetParent(child) == newParent)
			return false;
		ECS::HierarchySystem::SetParent(m_registry, child, newParent);
		MarkStructureDirty();
		MarkTransformDirty(child);
		return true;
	}

	bool Scene::SetParentKeepWorld(entt::entity child, entt::entity newParent)
	{
		if (!CanSetParent(child, newParent)
			|| GetParent(child) == newParent
			|| !HasComponent<TransformComponent>(child))
			return false;
		ECS::HierarchySystem::ReparentKeepWorldTransform(m_registry, child, newParent);
		MarkStructureDirty();
		MarkTransformDirty(child);
		return true;
	}

	entt::entity Scene::GetParent(entt::entity entity) const
	{
		return ECS::HierarchySystem::GetParent(m_registry, entity);
	}

	std::vector<entt::entity> Scene::GetChildren(entt::entity entity) const
	{
		return ECS::HierarchySystem::GetChildren(m_registry, entity);
	}

	std::vector<entt::entity> Scene::GetRootEntities() const
	{
		std::vector<entt::entity> roots;
		for (const entt::entity entity : GetAllEntities())
		{
			if (GetParent(entity) == entt::null)
				roots.push_back(entity);
		}
		return roots;
	}

	bool Scene::DestroyEntityRecursive(entt::entity entity, bool recursive)
	{
		if (!IsEntityValid(entity))
			return false;
		if (std::find(m_destroyQueue.begin(), m_destroyQueue.end(), entity) == m_destroyQueue.end())
			m_destroyQueue.push_back(entity);
		const auto children = GetChildren(entity);
		if (recursive)
		{
			for (const entt::entity child : children)
				DestroyEntityRecursive(child, true);
		}
		else
		{
			for (const entt::entity child : children)
				ClearParent(child);
		}
		return true;
	}

	bool Scene::IsEntityPendingDestroy(entt::entity entity) const
	{
		return std::find(m_destroyQueue.begin(), m_destroyQueue.end(), entity) != m_destroyQueue.end();
	}

	bool Scene::CancelDestroyEntityRecursive(entt::entity entity)
	{
		if (!IsEntityValid(entity))
			return false;
		bool cancelled = false;
		const auto removePending = [this, &cancelled](entt::entity candidate)
		{
			const auto it = std::remove(m_destroyQueue.begin(), m_destroyQueue.end(), candidate);
			if (it != m_destroyQueue.end())
			{
				m_destroyQueue.erase(it, m_destroyQueue.end());
				cancelled = true;
			}
		};
		removePending(entity);
		for (const entt::entity child : GetChildren(entity))
			cancelled = CancelDestroyEntityRecursive(child) || cancelled;
		return cancelled;
	}

	void Scene::FlushDestroyQueue()
	{
		bool destroyed = false;
		for (const entt::entity entity : m_destroyQueue)
		{
			if (!m_registry.valid(entity))
				continue;
			const std::uint64_t id = GetEntityID(entity);
			if (const auto* hierarchy = m_registry.try_get<HierarchyComponent>(entity);
				hierarchy && hierarchy->Parent != entt::null)
			{
				ECS::HierarchySystem::RemoveChild(m_registry, hierarchy->Parent, entity);
			}
			m_registry.destroy(entity);
			m_entitiesByID.erase(id);
			if (m_settings.ActiveCameraEntityID == id)
				m_settings.ActiveCameraEntityID = 0;
			if (m_settings.ActiveLightEntityID == id)
				m_settings.ActiveLightEntityID = 0;
			destroyed = true;
		}
		m_destroyQueue.clear();
		if (destroyed)
			MarkStructureDirty();
	}

	void Scene::Clear()
	{
		m_destroyQueue.clear();
		m_dirtyTransformRoots.clear();
		m_dirtyTransformRootSet.clear();
		m_allTransformsDirty = true;
		m_registry.clear();
		m_entitiesByID.clear();
		m_settings = {};
		m_nextEntityID = 1;
		MarkStructureDirty();
		MarkTransformDirty();
	}
}
