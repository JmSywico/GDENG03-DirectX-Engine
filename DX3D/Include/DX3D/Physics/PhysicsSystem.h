#pragma once

#include <DX3D/Core/Core.h>

namespace dx3d
{
	class World;

	class PhysicsSystem final
	{
	public:
		void update(
			World& world,
			f32 deltaTime
		);

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
	};
}