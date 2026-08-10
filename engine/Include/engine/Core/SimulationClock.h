#pragma once

#include <algorithm>
#include <cstdint>

namespace jnpf::Core
{
	class SimulationClock
	{
	public:
		explicit SimulationClock(double fixedDeltaSeconds = 1.0 / 60.0)
			: m_fixedDeltaSeconds(std::max(0.000001, fixedDeltaSeconds)) {}
		void SetFixedDeltaSeconds(double value)
		{
			m_fixedDeltaSeconds = std::max(0.000001, value);
			m_accumulator = std::min(m_accumulator, m_fixedDeltaSeconds);
		}
		double GetFixedDeltaSeconds() const { return m_fixedDeltaSeconds; }
		double GetInterpolationAlpha() const
		{
			return std::clamp(m_accumulator / m_fixedDeltaSeconds, 0.0, 1.0);
		}
		std::uint64_t GetTickCount() const { return m_tickCount; }
		void Reset() { m_accumulator = 0.0; m_tickCount = 0; m_stepRequested = false; }
		void RequestStep() { m_stepRequested = true; }
		std::uint32_t Advance(double frameDeltaSeconds, bool running, std::uint32_t maxSteps = 8)
		{
			if (m_stepRequested)
			{
				m_stepRequested = false;
				++m_tickCount;
				return 1;
			}
			if (!running) return 0;
			m_accumulator += std::clamp(frameDeltaSeconds, 0.0, 0.25);
			std::uint32_t steps = 0;
			while (m_accumulator >= m_fixedDeltaSeconds && steps < maxSteps)
			{
				m_accumulator -= m_fixedDeltaSeconds;
				++steps;
				++m_tickCount;
			}
			if (steps == maxSteps && m_accumulator >= m_fixedDeltaSeconds) m_accumulator = 0.0;
			return steps;
		}
	private:
		double m_fixedDeltaSeconds;
		double m_accumulator = 0.0;
		std::uint64_t m_tickCount = 0;
		bool m_stepRequested = false;
	};
}
