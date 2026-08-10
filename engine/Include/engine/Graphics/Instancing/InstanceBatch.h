#pragma once

#include "../Material.h"
#include "InstanceData.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <vector>

class Mesh;

namespace jnpf::Graphics
{
	struct InstanceBatch
	{
		std::shared_ptr<Mesh> MeshPtr;
		DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
		MaterialMode Material = MaterialMode::LitTint;
		std::shared_ptr<MaterialResource> MaterialResourcePtr;
		bool ReceivesShadows = true;
		std::vector<InstanceData> Instances;

		bool IsDrawable() const
		{
			return MeshPtr != nullptr && !Instances.empty();
		}
	};
}
