#pragma once

#include <DX3D/Core/Common.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Identifiable.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

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
			return reinterpret_cast<T* const*>(
				getComponentsInternal(
					T::GetTypeId(),
					&numComponents
				)
				);
		}

		void update(f32 deltaTime);

		void destroyGameObject(GameObject* object);

		std::vector<GameObject*> getGameObjects() const;
		GameObject* findGameObject(ui64 entityId) const noexcept;
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

		void addComponentInternal(Component& component);

		void addDirtyTransformInternal(
			TransformComponent& component
		);

		Component* const* getComponentsInternal(
			size_t typeId,
			ui32* numComponents
		) const noexcept;

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

		std::unordered_map<
			size_t,
			std::vector<Component*>
		> m_components{};

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
