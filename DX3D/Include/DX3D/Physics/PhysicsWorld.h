#pragma once

#include <DX3D/Core/Core.h>

namespace dx3d
{
	class World;

	class PhysicsWorld final
	{
	public:
		struct Stats
		{
			ui32 bodyCount{};
			ui32 activeBodyCount{};
			ui32 contactCount{};
			f32 stepMilliseconds{};
		};

		void step(World& world, f32 fixedDeltaTime);
		void reset(World& world);
		const Stats& getStats() const noexcept;

	private:
		Stats m_stats{};
	};
}
