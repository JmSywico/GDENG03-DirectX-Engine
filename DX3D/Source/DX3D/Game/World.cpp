#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/RotatorComponent.h>
#include <DX3D/Component/FlyControllerComponent.h>
#include <DX3D/Input/InputSystem.h>
#include <algorithm>
#include <cmath>

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
			if (object->isActiveInHierarchy())
				object->onUpdate(deltaTime);
		}
	}	

	for (auto& comp : m_dirtyTransforms)
	{
		comp->updateWorldMatrix();
	}
	m_dirtyTransforms.clear();
}

void dx3d::World::fixedUpdate(f32 fixedDeltaTime)
{
	auto rotators = m_registry.view<RotatorComponent>();
	for (auto entity : rotators)
	{
		auto& rotator = rotators.get<RotatorComponent>(entity);
		if (!rotator.isEnabled() ||
			!rotator.getGameObject().isActiveInHierarchy()) continue;
		auto& transform = rotator.getGameObject().getTransform();
		transform.setRotation(
			transform.getRotation() + rotator.getAngularVelocity() * fixedDeltaTime);
	}

	auto controllers = m_registry.view<FlyControllerComponent>();
	for (auto entity : controllers)
	{
		auto& controller = controllers.get<FlyControllerComponent>(entity);
		if (!controller.isEnabled() ||
			!controller.getGameObject().isActiveInHierarchy()) continue;
		auto& transform = controller.getGameObject().getTransform();
		auto rotation = transform.getRotation();
		if (m_gameContext.input.isKeyDown(KeyCode::MouseRight))
		{
			const auto mouse = m_gameContext.input.getMouseDelta();
			rotation.x += mouse.y * controller.getLookSensitivity();
			rotation.y += mouse.x * controller.getLookSensitivity();
			const f32 limit = controller.getPitchLimitDegrees() * 0.01745329252f;
			rotation.x = std::clamp(rotation.x, -limit, limit);
			transform.setRotation(rotation);
		}
		Vec3 direction{};
		if (m_gameContext.input.isKeyDown(KeyCode::W)) direction = direction + transform.forward();
		if (m_gameContext.input.isKeyDown(KeyCode::S)) direction = direction + transform.forward() * -1.0f;
		if (m_gameContext.input.isKeyDown(KeyCode::D)) direction = direction + transform.right();
		if (m_gameContext.input.isKeyDown(KeyCode::A)) direction = direction + transform.right() * -1.0f;
		if (m_gameContext.input.isKeyDown(KeyCode::E)) direction = direction + transform.up();
		if (m_gameContext.input.isKeyDown(KeyCode::Q)) direction = direction + transform.up() * -1.0f;
		const f32 lengthSquared = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
		if (lengthSquared > 0.000001f)
		{
			const f32 boost = m_gameContext.input.isKeyDown(KeyCode::Shift)
				? controller.getBoostMultiplier() : 1.0f;
			transform.setPosition(transform.getPosition()
				+ Vec3::normalize(direction) * controller.getMoveSpeed() * boost * fixedDeltaTime);
		}
	}
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
	ptr->m_registryEntity = m_registry.create();
	m_entityIndex[entityId] = ptr;
	ptr->m_transform = ptr->createOrGetComponent<TransformComponent>();
	if (ptr->m_transform)
		ptr->m_transform->markAsDirty();

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

	m_dirtyTransforms.erase(
		std::remove(
			m_dirtyTransforms.begin(),
			m_dirtyTransforms.end(),
			object->m_transform
		),
		m_dirtyTransforms.end()
	);

	if (m_registry.valid(object->m_registryEntity))
		m_registry.destroy(object->m_registryEntity);
	object->m_registryEntity = entt::null;
	object->m_transform = nullptr;

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

