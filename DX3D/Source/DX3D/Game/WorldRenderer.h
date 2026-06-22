#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>
#include <DX3D/Math/Mat4x4.h>
#include <DX3D/Graphics/MeshData.h>

#include <unordered_map>

namespace dx3d
{
	class CombinedMeshComponent;

	class WorldRenderer final : public Base
	{
	public:
		explicit WorldRenderer(
			const WorldRendererDesc& desc
		);

		virtual ~WorldRenderer() override;

		void render(
			const World& world,
			SwapChain& swapChain,
			f32 deltaTime
		);

	private:
		struct alignas(16) ConstantData
		{
			Mat4x4 world{};
			Mat4x4 view{};
			Mat4x4 proj{};
		};

		struct CombinedMeshRenderResources
		{
			RefPtr<VertexBuffer> vertexBuffer{};
			RefPtr<IndexBuffer> indexBuffer{};
			ui32 meshRevision{};
		};

	private:
		GraphicsDevice& m_graphicsDevice;
		RefPtr<DeviceContext> m_deviceContext{};
		RefPtr<GraphicsPipelineState> m_pipeline{};

		RefPtr<VertexBuffer> m_cubeVertexBuffer{};
		RefPtr<IndexBuffer> m_cubeIndexBuffer{};

		RefPtr<VertexBuffer> m_planeVertexBuffer{};
		RefPtr<IndexBuffer> m_planeIndexBuffer{};

		RefPtr<VertexBuffer> m_circleVertexBuffer{};
		RefPtr<IndexBuffer> m_circleIndexBuffer{};

		std::unordered_map<
			const CombinedMeshComponent*,
			CombinedMeshRenderResources
		> m_combinedMeshResources{};

		RefPtr<ConstantBuffer> m_cb{};
	};
}