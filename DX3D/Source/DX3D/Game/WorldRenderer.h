#pragma once
#include <DX3D/Core/Core.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>
#include <DX3D/Math/Mat4x4.h>

namespace dx3d
{
	class WorldRenderer final: public Base
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
		};

	private:
		GraphicsDevice& m_graphicsDevice;
		RefPtr<DeviceContext> m_deviceContext{};
		RefPtr<GraphicsPipelineState> m_pipeline{};

		RefPtr<VertexBuffer> m_cubeVertexBuffer{};
		RefPtr<IndexBuffer> m_cubeIndexBuffer{};

		RefPtr<VertexBuffer> m_circleVertexBuffer{};
		RefPtr<IndexBuffer> m_circleIndexBuffer{};

		RefPtr<ConstantBuffer> m_cb{};
	};
}

