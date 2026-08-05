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
	class ShadowMap;
	class CombinedMeshComponent;

	class WorldRenderer final :
		public Base
	{
	public:
		explicit WorldRenderer(
			const WorldRendererDesc& desc
		);

		virtual ~WorldRenderer() override;

		void render(
			const World& world,
			SwapChain& swapChain,
			f32 deltaTime,
			bool useSceneCamera
		);

	private:
		struct alignas(16) ConstantData
		{
			Mat4x4 world{};
			Mat4x4 view{};
			Mat4x4 proj{};

			Vec4 lightDirection{};
			Vec4 lightColorAndAmbient{};
			Vec4 materialAlbedo{};
			Vec4 materialEmissiveAndStrength{};
			Vec4 materialParameters{};

			Mat4x4 inverseWorld{};

			Mat4x4 lightView{};
			Mat4x4 lightProj{};
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
		RefPtr<ShadowMap> m_shadowMap{};

		RefPtr<VertexBuffer> m_cubeVertexBuffer{};
		RefPtr<IndexBuffer> m_cubeIndexBuffer{};

		RefPtr<VertexBuffer> m_planeVertexBuffer{};
		RefPtr<IndexBuffer> m_planeIndexBuffer{};

		RefPtr<VertexBuffer> m_circleVertexBuffer{};
		RefPtr<IndexBuffer> m_circleIndexBuffer{};
		RefPtr<VertexBuffer> m_sphereVertexBuffer{};
		RefPtr<IndexBuffer> m_sphereIndexBuffer{};

		std::unordered_map<
			const CombinedMeshComponent*,
			CombinedMeshRenderResources
		> m_combinedMeshResources{};

		RefPtr<ConstantBuffer> m_cb{};
	};
}
