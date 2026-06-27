#pragma once
#include <DX3D/Core/Core.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>
#include <DX3D/Math/Mat4x4.h>

namespace dx3d
{
<<<<<<< Updated upstream
	class WorldRenderer final: public Base
=======
	class ShadowMap;
	class CombinedMeshComponent;

	class WorldRenderer final : public Base
>>>>>>> Stashed changes
	{
	public:
		explicit WorldRenderer(const WorldRendererDesc& desc);
		virtual ~WorldRenderer() override;

		void render(const World& world, SwapChain& swapChain, f32 deltaTime);
	private:
		struct Vertex
		{
			Vec3 position;
			Vec4 color;
		};
		struct alignas(16) ConstantData
		{
			Mat4x4 world{};
			Mat4x4 view{};
			Mat4x4 proj{};
<<<<<<< Updated upstream
=======

			Vec4 lightDirection{};
			Vec4 lightColorAndAmbient{};

			Mat4x4 inverseWorld{};

			Mat4x4 lightView{};
			Mat4x4 lightProj{};
		};

		struct CombinedMeshRenderResources
		{
			RefPtr<VertexBuffer> vertexBuffer{};
			RefPtr<IndexBuffer> indexBuffer{};
			ui32 meshRevision{};
>>>>>>> Stashed changes
		};

	private:
		GraphicsDevice& m_graphicsDevice;
		RefPtr<DeviceContext> m_deviceContext{};
		RefPtr<GraphicsPipelineState> m_pipeline{};
		RefPtr<ShadowMap> m_shadowMap{};

		RefPtr<VertexBuffer> m_cubeVertexBuffer{};
		RefPtr<IndexBuffer> m_cubeIndexBuffer{};

		RefPtr<VertexBuffer> m_circleVertexBuffer{};
		RefPtr<IndexBuffer> m_circleIndexBuffer{};

		RefPtr<ConstantBuffer> m_cb{};
	};
}

