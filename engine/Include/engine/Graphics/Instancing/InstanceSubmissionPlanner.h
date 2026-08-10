#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace jnpf::Graphics
{
	struct InstanceSubmissionPlanner
	{
		// Keep individual transient allocations modest so one submission does not
		// monopolize the frame-wide pool before later color and shadow passes run.
		static constexpr std::uint32_t MaxInstancesPerSubmission = 32u * 1024u;

		static std::uint32_t NextCount(std::uint32_t remaining, std::uint32_t available)
		{
			return std::min(remaining, available);
		}

		// CPU-test helper. Runtime submission uses NextCount with the DX11 ring's live capacity.
		static std::vector<std::uint32_t> Build(std::uint32_t total, std::uint32_t capacity)
		{
			std::vector<std::uint32_t> result;
			if (capacity == 0) return result;
			while (total > 0)
			{
				const std::uint32_t count = NextCount(total, capacity);
				result.push_back(count);
				total -= count;
			}
			return result;
		}
	};
}
