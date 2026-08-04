#pragma once

#include <DX3D/Core/Core.h>

namespace dx3d
{
	class GameObject;
	class World;

	class PhysicsSystem final
	{
	public:
		PhysicsSystem();
		~PhysicsSystem();

		dx3d_disable_copy_and_move(PhysicsSystem)

	public:
		void start(
			World& world
		);

		void update(
			World& world,
			f32 deltaTime
		);

		void stop(
			World& world
		) noexcept;

		void removeGameObject(
			GameObject& object
		) noexcept;

		void resetAccumulator() noexcept;

	private:
		void fixedUpdate(
			World& world,
			f32 fixedDeltaTime
		);

	private:
		static constexpr f32 FixedDeltaTime =
			1.0f / 60.0f;

		f32 m_accumulator{};

		struct Impl;
		UniquePtr<Impl> m_impl{};
	};
}
