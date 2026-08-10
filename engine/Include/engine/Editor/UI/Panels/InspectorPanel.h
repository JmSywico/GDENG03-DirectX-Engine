#pragma once

#include <DirectXMath.h>

#include <cstdint>

namespace jnpf::Scene
{
	class Scene;
}

namespace jnpf::Editor
{
	struct EditorContext;

	class InspectorPanel
	{
	public:
		void Draw(EditorContext& context);

	private:
		void BeginTransformEdit(
			EditorContext& context,
			std::uint64_t entityID,
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& rotation,
			const DirectX::XMFLOAT4& rotationQuaternion,
			const DirectX::XMFLOAT3& scale);
		void CommitTransformEdit(EditorContext& context);

		Scene::Scene* m_transformEditScene = nullptr;
		std::uint64_t m_transformEditEntityID = 0;
		DirectX::XMFLOAT3 m_transformEditPosition{};
		DirectX::XMFLOAT3 m_transformEditRotation{};
		DirectX::XMFLOAT4 m_transformEditRotationQuaternion{0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 m_transformEditScale{1.0f, 1.0f, 1.0f};
		bool m_transformEditActive = false;
		bool m_linkScaleAxes = false;
	};
}
