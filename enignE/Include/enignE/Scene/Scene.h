#pragma once

#include "../pch.h"
#include "Components/Components.h"

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace enignE::Scene
{
	/**
	 * @brief Monotonic invalidation counters consumed by scene and render systems.
	 * @ingroup scene
	 */
	struct SceneVersions
	{
		std::uint64_t StructureVersion = 1;
		std::uint64_t TransformVersion = 1;
		std::uint64_t RenderVersion = 1;
	};

	/**
	 * @brief Stable-ID references to scene-wide runtime choices.
	 * @ingroup scene
	 */
	struct SceneSettings
	{
		std::uint64_t ActiveCameraEntityID = 0;
		std::uint64_t ActiveLightEntityID = 0;
	};

	struct DirtyTransformSet
	{
		bool All = false;
		std::vector<entt::entity> Roots;
	};

	/**
	 * @brief Owns the EnTT registry and provides version-aware scene mutations.
	 * @ingroup scene
	 *
	 * Persistent references use IDComponent values rather than entt::entity.
	 * Editor and serialization code should prefer the mutation functions on
	 * this class so transform and render caches are invalidated correctly.
	 */
	class Scene
	{
	public:
		explicit Scene(const std::string& name = "DefaultScene");
		~Scene() = default;

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&& other) noexcept;
		Scene& operator=(Scene&& other) noexcept;
		void CopyFrom(const Scene& source);

		entt::entity CreateEntity();
		entt::entity CreateEntity(const std::string& name);
		/**
		 * @brief Creates an entity with a caller-supplied persistent ID.
		 * @note Intended for deserialization and snapshot restoration.
		 */
		entt::entity CreateEntityWithID(std::uint64_t id, const std::string& name = "Entity");
		entt::entity FindEntityByID(std::uint64_t id) const;
		std::uint64_t GetEntityID(entt::entity entity) const;
		bool RenameEntity(entt::entity entity, const std::string& name);

		/**
		 * @brief Queues an entity and its descendants for deferred destruction.
		 * @return false if the entity is invalid or already pending destruction.
		 */
		bool DestroyEntity(entt::entity entity) { return DestroyEntityRecursive(entity, true); }
		bool DestroyEntityRecursive(entt::entity entity, bool recursive = true);
		bool IsEntityPendingDestroy(entt::entity entity) const;
		bool CancelDestroyEntityRecursive(entt::entity entity);
		/** @brief Applies queued destruction. Called by the engine after scene systems update. */
		void FlushDestroyQueue();
		bool IsEntityValid(entt::entity entity) const { return m_registry.valid(entity); }
		std::vector<entt::entity> GetAllEntities() const;
		size_t GetEntityCount() const;

		template <typename T>
		/**
		 * @brief Adds or replaces a component and marks scene structure dirty.
		 * @warning Component-specific mutations should use dedicated setters when
		 * transform or renderer invalidation is required.
		 */
		T& AddComponent(entt::entity entity, const T& component = T{})
		{
			T& result = m_registry.emplace_or_replace<T>(entity, component);
			MarkStructureDirty();
			if constexpr (std::is_same_v<T, TransformComponent>)
				MarkTransformDirty(entity);
			return result;
		}

		template <typename T>
		bool RemoveComponent(entt::entity entity)
		{
			if (!m_registry.valid(entity) || m_registry.remove<T>(entity) == 0)
				return false;
			MarkStructureDirty();
			return true;
		}

		template <typename T>
		bool HasComponent(entt::entity entity) const
		{
			return m_registry.valid(entity) && m_registry.all_of<T>(entity);
		}

		template <typename T>
		const T* GetComponent(entt::entity entity) const
		{
			return m_registry.valid(entity) ? m_registry.try_get<T>(entity) : nullptr;
		}

		template <typename T>
		/**
		 * @brief Returns mutable component storage without automatic invalidation.
		 * @warning Call the appropriate Mark*Dirty function after direct mutation.
		 */
		T* GetComponentMut(entt::entity entity)
		{
			return m_registry.valid(entity) ? m_registry.try_get<T>(entity) : nullptr;
		}

		template <typename... Components>
		auto View() { return m_registry.view<Components...>(); }

		bool SetLocalPosition(entt::entity entity, const DirectX::XMFLOAT3& value);
		bool SetLocalRotation(entt::entity entity, const DirectX::XMFLOAT3& value);
		bool SetLocalRotationQuaternion(
			entt::entity entity,
			const DirectX::XMFLOAT4& value,
			const DirectX::XMFLOAT3& eulerHint);
		bool SetLocalScale(entt::entity entity, const DirectX::XMFLOAT3& value);
		bool SetTransform(
			entt::entity entity,
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& rotation,
			const DirectX::XMFLOAT3& scale);
		bool SetTransformQuaternion(
			entt::entity entity,
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT4& rotation,
			const DirectX::XMFLOAT3& eulerHint,
			const DirectX::XMFLOAT3& scale);
		bool SetRendererVisibility(entt::entity entity, bool visible);
		bool SetRendererShadowFlags(entt::entity entity, bool castShadows, bool receiveShadows);
		bool SetRendererMaterial(entt::entity entity, MaterialMode material);
		bool SetRendererAlbedo(entt::entity entity, const DirectX::XMFLOAT4& albedo);
		bool SetRendererModel(entt::entity entity, std::shared_ptr<Model> model);
		bool SetRendererMaterialResource(
			entt::entity entity,
			std::shared_ptr<MaterialResource> material);
		bool SetCamera(entt::entity entity, const CameraComponent& camera);
		bool SetLight(entt::entity entity, const LightComponent& light);
		bool SetRotator(entt::entity entity, const RotatorComponent& rotator);
		bool SetFlyController(entt::entity entity, const FlyControllerComponent& controller);
		bool SetRigidBody(entt::entity entity, const RigidBodyComponent& body);
		bool SetCollider(entt::entity entity, const ColliderComponent& collider);

		bool CanSetParent(entt::entity child, entt::entity newParent) const;
		/**
		 * @brief Reparents an entity while preserving its local transform.
		 * @return false for invalid entities, self-parenting, or hierarchy cycles.
		 */
		bool SetParent(entt::entity child, entt::entity newParent);
		/**
		 * @brief Reparents an entity while preserving its current world transform.
		 * @return false for invalid entities, missing transforms, self-parenting, or hierarchy cycles.
		 */
		bool SetParentKeepWorld(entt::entity child, entt::entity newParent);
		bool ClearParent(entt::entity child) { return SetParent(child, entt::null); }
		entt::entity GetParent(entt::entity entity) const;
		std::vector<entt::entity> GetChildren(entt::entity entity) const;
		std::vector<entt::entity> GetRootEntities() const;

		const std::string& GetName() const { return m_name; }
		void SetName(const std::string& name) { m_name = name; }
		const SceneSettings& GetSettings() const { return m_settings; }
		bool SetActiveCameraEntityID(std::uint64_t id);
		bool SetActiveLightEntityID(std::uint64_t id);

		std::uint64_t GetStructureVersion() const { return m_versions.StructureVersion; }
		std::uint64_t GetTransformVersion() const { return m_versions.TransformVersion; }
		std::uint64_t GetRenderVersion() const { return m_versions.RenderVersion; }
		/** @brief Increments structure and render versions. */
		void MarkStructureDirty();
		/**
		 * @brief Increments the transform version.
		 * @note RenderVersion advances after propagation only when a renderable
		 * transform actually changed, avoiding geometry-cache rebuilds for cameras.
		 */
		void MarkTransformDirty();
		DirtyTransformSet TakeDirtyTransforms();
		/** @brief Increments only the render version. */
		void MarkRenderDirty();
		void MarkRenderDataChanged() { MarkRenderDirty(); }

		void Clear();
		entt::registry& GetRegistry() { return m_registry; }
		const entt::registry& GetRegistry() const { return m_registry; }

	private:
		static void OnMeshRendererUpdated(entt::registry& registry, entt::entity);
		void BindRegistryCallbacks();
		std::uint64_t GenerateEntityID();
		void MarkTransformSubtreeDirty(entt::entity entity);
		void MarkTransformDirty(entt::entity entity);

		std::string m_name;
		entt::registry m_registry;
		std::unordered_map<std::uint64_t, entt::entity> m_entitiesByID;
		std::vector<entt::entity> m_destroyQueue;
		std::vector<entt::entity> m_dirtyTransformRoots;
		std::unordered_set<entt::entity> m_dirtyTransformRootSet;
		bool m_allTransformsDirty = true;
		SceneVersions m_versions;
		SceneSettings m_settings;
		std::uint64_t m_nextEntityID = 1;
	};
}
