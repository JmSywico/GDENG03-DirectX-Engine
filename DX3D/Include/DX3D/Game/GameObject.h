#pragma once

#include <DX3D/Core/Common.h>
#include <DX3D/Core/Identifiable.h>
#include <DX3D/Game/Component.h>

#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace dx3d
{
	class GameObject : public Identifiable
	{
		dx3d_typeid(GameObject)

	public:
		explicit GameObject(const GameObjectDesc& desc);

		template <typename T>
		T* createOrGetComponent()
			requires IsRegistered<Component, T>;

		template <typename T>
		T* getComponent()
			requires IsRegistered<Component, T>;

		template <typename T>
		bool removeComponent()
			requires IsRegistered<Component, T>;

		void setName(const std::string& name);
		const std::string& getName() const noexcept;
		void setActive(bool active) noexcept;
		bool isActiveSelf() const noexcept;
		bool isActiveInHierarchy() const noexcept;
		ui64 getEntityId() const noexcept;
		GameObject* getParent() const noexcept;
		const std::vector<GameObject*>& getChildren() const noexcept;

		TransformComponent& getTransform() noexcept;
		World& getWorld() noexcept;
		InputSystem& getInputSystem() noexcept;

	protected:
		virtual void onCreate() {}
		virtual void onUpdate(f32 deltaTime) {}

	private:
		std::string m_name{ "GameObject" };
		bool m_activeSelf{ true };
		ui64 m_entityId{};
		entt::entity m_registryEntity{ entt::null };
		GameObject* m_parent{};
		std::vector<GameObject*> m_children{};

		TransformComponent* m_transform{};
		GameContext m_gameContext;
		World& m_world;

		friend class World;
	};
}

#include <DX3D/Game/World.h>

template <typename T>
T* dx3d::GameObject::createOrGetComponent()
	requires IsRegistered<Component, T>
{
	return m_world.createOrGetComponent<T>(m_registryEntity, *this);
}

template <typename T>
T* dx3d::GameObject::getComponent()
	requires IsRegistered<Component, T>
{
	return m_world.getComponent<T>(m_registryEntity);
}

template <typename T>
bool dx3d::GameObject::removeComponent()
	requires IsRegistered<Component, T>
{
	return m_world.removeComponent<T>(m_registryEntity);
}
