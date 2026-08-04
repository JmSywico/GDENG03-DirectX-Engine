#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Component/TransformComponent.h>
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

	for (auto& comp : m_dirtyTransforms)
	{
		comp->updateWorldMatrix();
	}
	m_dirtyTransforms.clear();
}

dx3d::GameObject* dx3d::World::createGameObjectInternal(
	UniquePtr<GameObject>& object,
	ui64 requestedId
)
{
	if (!object) return {};

	auto ptr = object.get();
	ui64 entityId = requestedId;

	if (entityId == 0 || m_entityIndex.contains(entityId))
	{
		while (m_entityIndex.contains(m_nextEntityId) || m_nextEntityId == 0)
			++m_nextEntityId;
		entityId = m_nextEntityId++;
	}
	else if (entityId >= m_nextEntityId)
	{
		m_nextEntityId = entityId + 1;
	}

	ptr->m_entityId = entityId;
	m_entityIndex[entityId] = ptr;

	auto index = m_pendingObjects.size();
	m_pendingObjects.push_back(std::move(object));
	m_events.push_back({ ptr, index, EventType::Create });

	return ptr;
}

dx3d::GameObject* dx3d::World::findGameObject(
	ui64 entityId
) const noexcept
{
	const auto found = m_entityIndex.find(entityId);
	return found == m_entityIndex.end() ? nullptr : found->second;
}

bool dx3d::World::isDescendantOf(
	const GameObject* object,
	const GameObject* potentialAncestor
) const noexcept
{
	if (!object || !potentialAncestor)
		return false;

	for (const GameObject* parent = object->m_parent;
		parent;
		parent = parent->m_parent)
	{
		if (parent == potentialAncestor)
			return true;
	}

	return false;
}

bool dx3d::World::setParent(
	GameObject* child,
	GameObject* parent
)
{
	if (!child || child == parent || child->m_parent == parent)
		return child && child->m_parent == parent;

	if (parent && isDescendantOf(parent, child))
		return false;

	if (child->m_parent)
	{
		auto& siblings = child->m_parent->m_children;
		siblings.erase(
			std::remove(siblings.begin(), siblings.end(), child),
			siblings.end()
		);
	}

	child->m_parent = parent;
	if (parent)
		parent->m_children.push_back(child);

	addDirtyTransformInternal(child->getTransform());
	return true;
}

void dx3d::World::destroyGameObject(GameObject* object)
{
	if (!object || m_pendingDestruction.contains(object))
		return;

	// Hierarchy ownership is explicit: destroying a parent destroys its
	// subtree. Queue children first so no live child observes a dead parent.
	const auto children = object->m_children;
	for (auto* child : children)
		destroyGameObject(child);

	m_pendingDestruction.insert(object);

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

	m_pendingDestruction.erase(object);

	if (object->m_parent)
	{
		auto& siblings = object->m_parent->m_children;
		siblings.erase(
			std::remove(siblings.begin(), siblings.end(), object),
			siblings.end()
		);
		object->m_parent = nullptr;
	}

	const auto children = object->m_children;
	for (auto* child : children)
	{
		if (child)
		{
			child->m_parent = nullptr;
			addDirtyTransformInternal(child->getTransform());
		}
	}
	object->m_children.clear();
	m_entityIndex.erase(object->m_entityId);

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
	auto markSubtree = [this](auto&& self, GameObject& object) -> void
	{
		auto* transform = object.m_transform;
		if (transform)
		{
			transform->m_dirty = true;
			if (std::find(
				m_dirtyTransforms.begin(),
				m_dirtyTransforms.end(),
				transform) == m_dirtyTransforms.end())
			{
				m_dirtyTransforms.push_back(transform);
			}
		}

		for (auto* child : object.m_children)
		{
			if (child) self(self, *child);
		}
	};

	markSubtree(markSubtree, component.getGameObject());
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
