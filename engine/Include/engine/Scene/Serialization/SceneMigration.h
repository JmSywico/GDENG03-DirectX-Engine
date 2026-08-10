#pragma once

#include <nlohmann/json_fwd.hpp>

namespace jnpf::Scene
{
	class SceneMigration
	{
	public:
		static constexpr int CurrentVersion = 8;
		static constexpr int MinimumVersion = 1;

		static void Migrate(nlohmann::json& document);
	};
}
