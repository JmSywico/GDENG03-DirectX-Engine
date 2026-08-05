#pragma once

#include <DX3D/Core/Common.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Identifiable.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <entt/entt.hpp>

namespace dx3d
{
	class World final : public Base
	{
	public:
		explicit World(const WorldDesc& desc);

		template <typename T>
		T* createGameObject() requires IsRegistered<GameObject, T>
		{
			return createGameObjectWithId<T>(0);
		}

		template <typename T>
		T* createGameObjectWithId(ui64 stableId)
			requires IsRegistered<GameObject, T>
		{
			UniquePtr<GameObject> e = std::make_unique<T>(
				GameObjectDesc
				{
					{ m_logger },
					m_gameContext,
					*this
				}
			);

			return static_cast<T*>(createGameObjectInternal(e, stableId));
		}

		template <typename T>
			requires IsRegistered<Component, T>
		T* const* getComponents(ui32& numComponents) const noexcept
		{
			m_componentQueryCache.clear();
			auto view = m_registry.view<T>();
			m_componentQueryCache.reserve(view.size_hint());
			for (auto entity : view)
				m_componentQueryCache.push_back(&view.get<T>(entity));
			numComponents = static_cast<ui32>(m_componentQueryCache.size());
			return reinterpret_cast<T* const*>(m_componentQueryCache.data());
		}

		void update(f32 deltaTime);
		void fixedUpdate(f32 fixedDeltaTime);

		void destroyGameObject(GameObject* object);

		std::vector<GameObject*> getGameObjects() const;
		GameObject* findGameObject(ui64 entityId) const noexcept;

		template <typename T>
			requires IsRegistered<Component, T>
		T* createOrGetComponent(entt::entity entity, GameObject& object)
		{
			if (!m_registry.valid(entity)) return nullptr;
			if (auto* existing = m_registry.try_get<T>(entity)) return existing;
			return &m_registry.emplace<T>(
				entity,
				ComponentDesc{ {m_logger}, object, *this }
			);
		}

		template <typename T>
			requires IsRegistered<Component, T>
		T* getComponent(entt::entity entity) noexcept
		{
			return m_registry.valid(entity)
				? m_registry.try_get<T>(entity)
				: nullptr;
		}

		template <typename T>
			requires IsRegistered<Component, T>
		bool removeComponent(entt::entity entity)
		{
			if (!m_registry.valid(entity)) return false;
			return m_registry.remove<T>(entity) != 0;
		}

		entt::registry& getRegistry() noexcept { return m_registry; }
		const entt::registry& getRegistry() const noexcept { return m_registry; }
		bool setParent(GameObject* child, GameObject* parent);
		bool isDescendantOf(
			const GameObject* object,
			const GameObject* potentialAncestor
		) const noexcept;

	private:
		GameObject* createGameObjectInternal(
			UniquePtr<GameObject>& object,
			ui64 requestedId
		);

		void destroyGameObjectInternal(
			GameObject* object
		);

		void addDirtyTransformInternal(
			TransformComponent& component
		);

	private:
		enum class EventType
		{
			Create = 0,
			Destroy
		};

		struct GameObjectEvent
		{
			GameObject* object{};
			size_t pendingObjectIndex{};
			EventType eventType{};
		};

	private:
		GameContext m_gameContext;

		std::unordered_map<
			size_t,
			std::vector<UniquePtr<GameObject>>
		> m_objects{};

		std::unordered_map<ui64, GameObject*> m_entityIndex{};
		ui64 m_nextEntityId{ 1 };
		mutable entt::registry m_registry{};
		mutable std::vector<Component*> m_componentQueryCache{};

		std::vector<TransformComponent*> m_dirtyTransforms{};

		std::vector<UniquePtr<GameObject>> m_pendingObjects;
		std::vector<UniquePtr<GameObject>> m_pendingObjectsSwapBuffer;

		std::vector<GameObjectEvent> m_events{};
		std::vector<GameObjectEvent> m_eventsSwapBuffer{};
		std::unordered_set<GameObject*> m_pendingDestruction{};

		friend class GameObject;
		friend class TransformComponent;
	};
}
