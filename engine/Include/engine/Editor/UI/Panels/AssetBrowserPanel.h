#pragma once

#include "Graphics/AssetRegistry.h"

#include <array>

namespace jnpf::Editor
{
	struct EditorContext;

	inline constexpr const char* AssetDragPayloadID = "JNPF_ASSET_REFERENCE";
	struct AssetDragPayload
	{
		Graphics::AssetHandle Handle = Graphics::InvalidAssetHandle;
		Graphics::AssetType Type = Graphics::AssetType::Unknown;
		std::array<char, 512> Path{};
	};

	class AssetBrowserPanel
	{
	public:
		void Draw(EditorContext& context);
	private:
		std::array<char, 128> m_filter{};
		Graphics::AssetReference m_pendingDelete;
		bool m_openDeleteConfirmation = false;
	};
}
