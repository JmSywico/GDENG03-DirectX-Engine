#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Game/World.h>

dx3d::GameObject::GameObject(const GameObjectDesc& desc)
	: Identifiable(desc.base),
	m_world(desc.world),
	m_gameContext(desc.gameContext)
{
}

void dx3d::GameObject::setName(
	const std::string& name
)
{
	if (name.empty())
	{
		m_name = "GameObject";
		return;
	}

	m_name = name;
}

const std::string&
dx3d::GameObject::getName() const noexcept
{
	return m_name;
}

void dx3d::GameObject::setActive(bool active) noexcept
{
	m_activeSelf = active;
}

bool dx3d::GameObject::isActiveSelf() const noexcept
{
	return m_activeSelf;
}

bool dx3d::GameObject::isActiveInHierarchy() const noexcept
{
	if (!m_activeSelf)
		return false;

	for (const GameObject* parent = m_parent; parent; parent = parent->m_parent)
		if (!parent->m_activeSelf)
			return false;

	return true;
}

dx3d::TransformComponent&
dx3d::GameObject::getTransform() noexcept
{
	return *m_transform;
}

dx3d::World&
dx3d::GameObject::getWorld() noexcept
{
	return m_world;
}

dx3d::InputSystem&
dx3d::GameObject::getInputSystem() noexcept
{
	return m_gameContext.input;
}

dx3d::ui64 dx3d::GameObject::getEntityId() const noexcept
{
	return m_entityId;
}

dx3d::GameObject* dx3d::GameObject::getParent() const noexcept
{
	return m_parent;
}

const std::vector<dx3d::GameObject*>&
dx3d::GameObject::getChildren() const noexcept
{
	return m_children;
}
