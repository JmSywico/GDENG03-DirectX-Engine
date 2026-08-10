#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>
#include <DX3D/Math/Mat4x4.h>
#include <DX3D/Graphics/MeshData.h>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <d3d11.h>
#include <wrl.h>

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
		static constexpr ui32 MaxLights = 16;

		struct alignas(16) ConstantData
		{
			Mat4x4 world{};
			Mat4x4 view{};
			Mat4x4 proj{};

			Vec4 lightDirections[MaxLights]{};
			Vec4 lightColors[MaxLights]{};
			Vec4 lightPositions[MaxLights]{};
			Vec4 lightParameters[MaxLights]{};
			Vec4 lightMeta{};
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

		struct TextureRenderResource
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{};
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view{};
			Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler{};
		};

		TextureRenderResource* getTextureResource(const std::string& path);

	private:
		GraphicsDevice& m_graphicsDevice;

		RefPtr<DeviceContext> m_deviceContext{};
		RefPtr<GraphicsPipelineState> m_pipeline{};
		RefPtr<ShadowMap> m_shadowMap{};
		RefPtr<ShadowMap> m_pointShadowMap{};

		RefPtr<VertexBuffer> m_cubeVertexBuffer{};
		RefPtr<IndexBuffer> m_cubeIndexBuffer{};

		RefPtr<VertexBuffer> m_planeVertexBuffer{};
		RefPtr<IndexBuffer> m_planeIndexBuffer{};

		RefPtr<VertexBuffer> m_circleVertexBuffer{};
		RefPtr<IndexBuffer> m_circleIndexBuffer{};
		RefPtr<VertexBuffer> m_sphereVertexBuffer{};
		RefPtr<IndexBuffer> m_sphereIndexBuffer{};
		RefPtr<VertexBuffer> m_cylinderVertexBuffer{};
		RefPtr<IndexBuffer> m_cylinderIndexBuffer{};
		RefPtr<VertexBuffer> m_capsuleVertexBuffer{};
		RefPtr<IndexBuffer> m_capsuleIndexBuffer{};

		std::unordered_map<
			const CombinedMeshComponent*,
			CombinedMeshRenderResources
		> m_combinedMeshResources{};
		std::unordered_map<std::string, TextureRenderResource> m_textureResources{};
		std::unordered_set<std::string> m_failedTexturePaths{};

		RefPtr<ConstantBuffer> m_cb{};
	};
}
