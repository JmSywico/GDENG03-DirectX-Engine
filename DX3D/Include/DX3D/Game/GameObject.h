#pragma once

#include <DX3D/Core/Common.h>
#include <DX3D/Core/Identifiable.h>
#include <DX3D/Game/Component.h>

#include <unordered_map>
#include <string>
#include <vector>

namespace dx3d
{
	class GameObject : public Identifiable
	{
		dx3d_typeid(GameObject)

	public:
		explicit GameObject(const GameObjectDesc& desc);

		template <typename T>
		T* createOrGetComponent()
			requires IsRegistered<Component, T>
		{
			auto c = getComponent<T>();

			if (c)
				return c;

			UniquePtr<Component> cp =
				std::make_unique<T>(
					ComponentDesc
					{
						{ m_logger },
						*this,
						m_world
					}
				);

			return static_cast<T*>(
				createComponentInternal(cp)
				);
		}

		template <typename T>
		T* getComponent()
			requires IsRegistered<Component, T>
		{
			return static_cast<T*>(
				getComponentInternal(T::GetTypeId())
				);
		}

		void setName(const std::string& name);
		const std::string& getName() const noexcept;
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
		Component* createComponentInternal(
			UniquePtr<Component>& component
		);

		Component* getComponentInternal(size_t id);

	private:
		std::string m_name{ "GameObject" };
		ui64 m_entityId{};
		GameObject* m_parent{};
		std::vector<GameObject*> m_children{};

		std::unordered_map<
			size_t,
			UniquePtr<Component>
		> m_components{};

		TransformComponent* m_transform{};
		GameContext m_gameContext;
		World& m_world;

		friend class World;
	};
}
