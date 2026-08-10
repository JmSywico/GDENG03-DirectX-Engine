#include "Scene/Serialization/SceneMigration.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace
{
	using json = nlohmann::json;

	void MigrateV1ToV2(json& document)
	{
		for (json& entity : document["entities"])
		{
			json& components = entity["components"];
			if (!components.contains("MeshRendererComponent"))
				continue;
			json& renderer = components["MeshRendererComponent"];
			if (!renderer.contains("material"))
			{
				renderer["material"] = {
					{"mode", renderer.value("materialMode", 0)},
					{"albedo", renderer.value("albedo", json::array({1.0, 1.0, 1.0, 1.0}))},
					{"albedoTexture", ""}
				};
			}
		}
		document["scene"]["version"] = 2;
	}

	json AssetRecord(const json& path)
	{
		return {
			{"handle", 0},
			{"path", path.is_string() ? path : json("")}
		};
	}

	void MigrateV2ToV3(json& document)
	{
		for (json& entity : document["entities"])
		{
			json& components = entity["components"];
			if (!components.contains("MeshRendererComponent"))
				continue;
			json& renderer = components["MeshRendererComponent"];
			if (renderer.contains("assetPath") && !renderer.contains("modelAsset"))
				renderer["modelAsset"] = AssetRecord(renderer["assetPath"]);
			renderer.erase("assetPath");

			if (!renderer.contains("material"))
				continue;
			json& material = renderer["material"];
			if (!material.contains("albedoTextureAsset"))
				material["albedoTextureAsset"] =
					AssetRecord(material.value("albedoTexture", json("")));
			material.erase("albedoTexture");
			if (!material.contains("normalTextureAsset"))
				material["normalTextureAsset"] = AssetRecord("");
			if (!material.contains("metallicRoughnessTextureAsset"))
				material["metallicRoughnessTextureAsset"] = AssetRecord("");
			if (!material.contains("emissive"))
				material["emissive"] = json::array({0.0, 0.0, 0.0});
			if (!material.contains("metallic"))
				material["metallic"] = 0.0;
			if (!material.contains("roughness"))
				material["roughness"] = 1.0;
		}
		document["scene"]["version"] = 3;
	}

	void MigrateV3ToV4(json& document)
	{
		json& scene = document["scene"];
		if (!scene.contains("activeLight"))
			scene["activeLight"] = scene.value("activeDirectionalLight", std::uint64_t{0});
		scene.erase("activeDirectionalLight");
		scene["version"] = 4;
	}

	void MigrateV4ToV5(json& document)
	{
		document["scene"]["version"] = 5;
	}

	void MigrateV5ToV6(json& document)
	{
		for (json& entity : document["entities"])
		{
			json& components = entity["components"];
			if (components.contains("MeshRendererComponent"))
			{
				json& renderer = components["MeshRendererComponent"];
				renderer["castShadows"] = renderer.value("castShadows", true);
				renderer["receiveShadows"] = renderer.value("receiveShadows", true);
			}
			if (components.contains("LightComponent"))
			{
				json& light = components["LightComponent"];
				light["castShadows"] = light.value("castShadows", true);
				light["shadowStrength"] = light.value("shadowStrength", 1.0f);
				light["shadowBias"] = light.value("shadowBias", 0.0012f);
				light["shadowNormalBias"] = light.value("shadowNormalBias", 0.02f);
				light["shadowDistance"] = light.value("shadowDistance", 100.0f);
			}
		}
		document["scene"]["version"] = 6;
	}

	void MigrateV6ToV7(json& document)
	{
		document["scene"]["version"] = 7;
	}

	void MigrateV7ToV8(json& document)
	{
		document["scene"]["version"] = 8;
	}
}

namespace enignE::Scene
{
	void SceneMigration::Migrate(nlohmann::json& document)
	{
		if (!document.is_object() || !document.contains("scene")
			|| !document["scene"].is_object() || !document.contains("entities")
			|| !document["entities"].is_array())
			throw std::runtime_error("scene document must contain scene object and entities array");

		int version = document["scene"].at("version").get<int>();
		if (version < MinimumVersion || version > CurrentVersion)
			throw std::runtime_error("unsupported scene format version " + std::to_string(version));
		while (version < CurrentVersion)
		{
			switch (version)
			{
			case 1: MigrateV1ToV2(document); break;
			case 2: MigrateV2ToV3(document); break;
			case 3: MigrateV3ToV4(document); break;
			case 4: MigrateV4ToV5(document); break;
			case 5: MigrateV5ToV6(document); break;
			case 6: MigrateV6ToV7(document); break;
			case 7: MigrateV7ToV8(document); break;
			default: throw std::runtime_error("missing scene migration step");
			}
			version = document["scene"]["version"].get<int>();
		}
	}
}
