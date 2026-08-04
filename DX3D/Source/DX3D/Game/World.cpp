#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <algorithm>

dx3d::World::World(const WorldDesc& desc) : Base(desc.base), m_gameContext(desc.gameContext)
{
}

void dx3d::World::update(f32 deltaTime)
{
	if (m_events.size())
	{
		std::swap(m_events, m_eventsSwapBuffer);
		std::swap(m_pendingObjects, m_pendingObjectsSwapBuffer);
	
		for (auto& e : m_eventsSwapBuffer)
		{
			auto objTypeId = e.object->getTypeId();
			auto pendingObjIndex = e.pendingObjectIndex;

			if (e.eventType == EventType::Create)
			{
				auto& obj = m_pendingObjectsSwapBuffer[pendingObjIndex];
				auto ptr = obj.get();

				m_objects[objTypeId].push_back(std::move(obj));
				ptr->onCreate();
			}
			else if (e.eventType == EventType::Destroy)
			{
				destroyGameObjectInternal(e.object);
			}
		}

		m_pendingObjectsSwapBuffer.clear();
		m_eventsSwapBuffer.clear();
	}

	for (auto&& [typeId, objects] : m_objects)
	{
		for (auto& object : objects)
		{
			object->onUpdate(deltaTime);
		}
	}

	if (m_physicsEnabled)
	{
		m_physicsSystem.update(
			*this,
			deltaTime
		);
	}

	for (auto& comp : m_dirtyTransforms)
	{
		comp->updateWorldMatrix();
	}
	m_dirtyTransforms.clear();
}

void dx3d::World::setPhysicsEnabled(
	bool enabled
) noexcept
{
	if (enabled &&
		!m_physicsHasStarted)
	{
		ui32 rigidBodyCount = 0;

		RigidBodyComponent* const* rigidBodies =
			getComponents<RigidBodyComponent>(
				rigidBodyCount
			);

		for (ui32 index = 0;
			index < rigidBodyCount;
			++index)
		{
			if (!rigidBodies[index])
				continue;

			rigidBodies[index]->
				captureInitialState();
		}

		m_physicsHasStarted = true;
	}

	m_physicsEnabled = enabled;

	if (enabled)
	{
		m_physicsSystem.start(
			*this
		);
	}

	if (!enabled)
	{
		m_physicsSystem.resetAccumulator();
	}
}

bool dx3d::World::isPhysicsEnabled() const noexcept
{
	return m_physicsEnabled;
}

void dx3d::World::stopPhysics() noexcept
{
	m_physicsEnabled = false;

	if (m_physicsHasStarted)
	{
		ui32 rigidBodyCount = 0;

		RigidBodyComponent* const* rigidBodies =
			getComponents<RigidBodyComponent>(
				rigidBodyCount
			);

		for (ui32 index = 0;
			index < rigidBodyCount;
			++index)
		{
			if (!rigidBodies[index])
				continue;

			rigidBodies[index]->
				restoreInitialState();
		}
	}

	m_physicsSystem.stop(
		*this
	);

	m_physicsSystem.resetAccumulator();

	m_physicsHasStarted = false;
}

bool dx3d::World::hasPhysicsStarted() const noexcept
{
	return m_physicsHasStarted;
}

dx3d::GameObject* dx3d::World::createGameObjectInternal(UniquePtr<GameObject>& object)
{
	if (!object) return {};

	auto ptr = object.get();

	auto index = m_pendingObjects.size();
	m_pendingObjects.push_back(std::move(object));
	m_events.push_back({ ptr, index, EventType::Create });

	return ptr;
}

void dx3d::World::destroyGameObject(GameObject* object)
{
	if (!object)
		return;

	m_events.push_back(
		{
			object,
			0,
			EventType::Destroy
		}
	);
}

std::vector<dx3d::GameObject*>
dx3d::World::getGameObjects() const
{
	std::vector<GameObject*> result{};

	for (const auto& [typeId, objects] : m_objects)
	{
		for (const auto& object : objects)
		{
			if (object)
			{
				result.push_back(object.get());
			}
		}
	}

	return result;
}

void dx3d::World::destroyGameObjectInternal(GameObject* object)
{
	if (!object)
		return;

	m_physicsSystem.removeGameObject(
		*object
	);

	// Remove every component belonging to this object
	// from the World's component lists.
	for (auto& [componentTypeId, component] : object->m_components)
	{
		auto componentListIt = m_components.find(componentTypeId);

		if (componentListIt == m_components.end())
			continue;

		auto& componentList = componentListIt->second;

		componentList.erase(
			std::remove(
				componentList.begin(),
				componentList.end(),
				component.get()
			),
			componentList.end()
		);

		if (componentList.empty())
		{
			m_components.erase(componentListIt);
		}
	}

	// Make sure the transform is not waiting in the dirty list.
	m_dirtyTransforms.erase(
		std::remove(
			m_dirtyTransforms.begin(),
			m_dirtyTransforms.end(),
			object->m_transform
		),
		m_dirtyTransforms.end()
	);

	// Remove and delete the actual GameObject.
	auto objectListIt = m_objects.find(object->getTypeId());

	if (objectListIt == m_objects.end())
		return;

	auto& objectList = objectListIt->second;

	objectList.erase(
		std::remove_if(
			objectList.begin(),
			objectList.end(),
			[object](const UniquePtr<GameObject>& storedObject)
			{
				return storedObject.get() == object;
			}
		),
		objectList.end()
	);

	if (objectList.empty())
	{
		m_objects.erase(objectListIt);
	}
}

void dx3d::World::addComponentInternal(Component& component)
{
	auto typeId = component.getTypeId();
	m_components[typeId].push_back(&component);
}

void dx3d::World::addDirtyTransformInternal(TransformComponent& component)
{
	m_dirtyTransforms.push_back(&component);
}

dx3d::Component* const* dx3d::World::getComponentsInternal(size_t typeId, ui32* numComponents) const noexcept
{
	auto it = m_components.find(typeId);
	if (it != m_components.end())
	{
		*numComponents = static_cast<ui32>(it->second.size());
		return it->second.data();
	}

	*numComponents = 0u;
	return {};
}
