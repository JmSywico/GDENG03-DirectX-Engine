#include <DX3D/Game/WorldRenderer.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/DeviceContext.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/VertexBuffer.h>
#include <DX3D/Graphics/IndexBuffer.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/CircleComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>

#include <DX3D/Math/Vec3.h>
#include <fstream>
#include <ranges>
#include <vector>
#include <cmath>
#include <unordered_set>

dx3d::WorldRenderer::WorldRenderer(const WorldRendererDesc& desc)
	: Base(desc.base),
	m_graphicsDevice(desc.engine)
{
	auto& device = m_graphicsDevice;
	m_deviceContext = device.createDeviceContext();

	constexpr char shaderFilePath[] = "DX3D/Assets/Shaders/Basic.hlsl";

	std::ifstream shaderStream(shaderFilePath);

	if (!shaderStream)
		DX3DLogThrowError("Failed to open shader file.");

	std::string shaderFileData{
		std::istreambuf_iterator<char>(shaderStream),
		std::istreambuf_iterator<char>()
	};

	auto shaderSourceCode = shaderFileData.c_str();
	auto shaderSourceCodeSize = shaderFileData.length();

	auto vs = device.compileShader(
		{
			shaderFilePath,
			shaderSourceCode,
			shaderSourceCodeSize,
			"VSMain",
			ShaderType::VertexShader
		}
	);

	auto ps = device.compileShader(
		{
			shaderFilePath,
			shaderSourceCode,
			shaderSourceCodeSize,
			"PSMain",
			ShaderType::PixelShader
		}
	);

	auto vsSig = device.createVertexShaderSignature({ vs });

	m_pipeline = device.createGraphicsPipelineState({ *vsSig, *ps });

	const auto& cubeMesh =
		getCubeMeshData();

	m_cubeVertexBuffer =
		device.createVertexBuffer(
			{
				cubeMesh.vertices.data(),
				static_cast<ui32>(
					cubeMesh.vertices.size()
				),
				sizeof(MeshVertex)
			}
		);

	m_cubeIndexBuffer =
		device.createIndexBuffer(
			{
				cubeMesh.indices.data(),
				static_cast<ui32>(
					cubeMesh.indices.size()
				)
			}
		);

	m_cb = device.createConstantBuffer(
		{ {}, sizeof(ConstantData) }
	);

	const auto& planeMesh =
		getPlaneMeshData();

	m_planeVertexBuffer =
		device.createVertexBuffer(
			{
				planeMesh.vertices.data(),
				static_cast<ui32>(
					planeMesh.vertices.size()
				),
				sizeof(MeshVertex)
			}
		);

	m_planeIndexBuffer =
		device.createIndexBuffer(
			{
				planeMesh.indices.data(),
				static_cast<ui32>(
					planeMesh.indices.size()
				)
			}
		);

	constexpr dx3d::ui32 circleSegments = 64;

	std::vector<MeshVertex> circleVertices;
	std::vector<dx3d::ui32> circleIndices;


	circleVertices.push_back(
		{
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f, 1.0f }
		}
	);


	for (dx3d::ui32 i = 0; i <= circleSegments; ++i)
	{
		const dx3d::f32 angle =
			(static_cast<dx3d::f32>(i) /
				static_cast<dx3d::f32>(circleSegments))
			* 2.0f
			* dx3d::MathUtils::PI;

		const dx3d::f32 x = std::cos(angle) * 0.5f;
		const dx3d::f32 y = std::sin(angle) * 0.5f;

		circleVertices.push_back(
			{
				{ x, y, 0.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f }
			}
		);
	}


	for (dx3d::ui32 i = 1; i <= circleSegments; ++i)
	{
		circleIndices.push_back(0);
		circleIndices.push_back(i + 1);
		circleIndices.push_back(i);
	}

	m_circleVertexBuffer = device.createVertexBuffer(
		{
			circleVertices.data(),
			static_cast<dx3d::ui32>(circleVertices.size()),
			sizeof(MeshVertex)
		}
	);

	m_circleIndexBuffer = device.createIndexBuffer(
		{
			circleIndices.data(),
			static_cast<dx3d::ui32>(circleIndices.size())
		}
	);
}

dx3d::WorldRenderer::~WorldRenderer()
{}

void dx3d::WorldRenderer::render(
	const World& world,
	SwapChain& swapChain,
	f32 deltaTime
)
{
	auto size = swapChain.getSize();

	auto& context = *m_deviceContext;

	context.clearAndSetBackBuffer(
		swapChain,
		{ 0.08f, 0.10f, 0.14f, 1.0f }
	);

	context.setGraphicsPipelineState(*m_pipeline);
	context.setViewportSize(size);

	auto numComponents = 0u;
	ConstantData data{};

	// Simple directional light.
//
// XYZ describes the direction from the surface
// toward the light. W is unused.
	data.lightDirection =
	{
		-0.5f,
		1.0f,
		-0.3f,
		0.0f
	};

	// RGB is the white light color.
	// W is the ambient-light strength.
	data.lightColorAndAmbient =
	{
		1.0f,
		1.0f,
		1.0f,
		0.20f
	};

	{
		auto components =
			world.getComponents<CameraComponent>(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto component = components[i];

			data.view = component->getViewMatrix();

			component->setViewportSize(size);

			data.proj = component->getProjectionMatrix();

			break;
		}
	}

	{
		auto components =
			world.getComponents<CubeComponent>(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto component = components[i];

			auto& transform =
				component->getGameObject().getTransform();

			data.world = transform.getAffineWorldMatrix();

			auto& cb = *m_cb;

			context.updateConstantBuffer(cb, &data);

			auto& vb = *m_cubeVertexBuffer;
			auto& ib = *m_cubeIndexBuffer;

			context.setVertexBuffer(vb);
			context.setConstantBuffer(cb);
			context.setIndexBuffer(ib);

			context.drawIndexedTriangleList(
				ib.getIndexListSize(),
				0u,
				0u
			);
		}
	}

	{
		auto components =
			world.getComponents<PlaneComponent>(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto component = components[i];

			auto& transform =
				component->getGameObject().getTransform();

			data.world = transform.getAffineWorldMatrix();

			auto& cb = *m_cb;

			context.updateConstantBuffer(cb, &data);

			auto& vb = *m_planeVertexBuffer;
			auto& ib = *m_planeIndexBuffer;

			context.setVertexBuffer(vb);
			context.setConstantBuffer(cb);
			context.setIndexBuffer(ib);

			context.drawIndexedTriangleList(
				ib.getIndexListSize(),
				0u,
				0u
			);
		}
	}

	{
		auto components =
			world.getComponents<CircleComponent>(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto component = components[i];

			auto& transform =
				component->getGameObject().getTransform();

			data.world = transform.getAffineWorldMatrix();

			auto& cb = *m_cb;

			context.updateConstantBuffer(cb, &data);

			auto& vb = *m_circleVertexBuffer;
			auto& ib = *m_circleIndexBuffer;

			context.setVertexBuffer(vb);
			context.setConstantBuffer(cb);
			context.setIndexBuffer(ib);

			context.drawIndexedTriangleList(
				ib.getIndexListSize(),
				0u,
				0u
			);
		}
	}
	{
		auto components =
			world.getComponents<CombinedMeshComponent>(
				numComponents
			);

		std::unordered_set<
			const CombinedMeshComponent*
		> activeCombinedMeshes{};

		activeCombinedMeshes.reserve(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto* component = components[i];

			if (!component)
				continue;

			activeCombinedMeshes.insert(component);

			if (!component->hasMeshData())
			{
				m_combinedMeshResources.erase(component);
				continue;
			}

			const auto& meshData =
				component->getMeshData();

			auto& renderResources =
				m_combinedMeshResources[component];

			const bool needsBufferRebuild =
				!renderResources.vertexBuffer ||
				!renderResources.indexBuffer ||
				renderResources.meshRevision !=
				component->getMeshRevision();

			if (needsBufferRebuild)
			{
				renderResources.vertexBuffer =
					m_graphicsDevice.createVertexBuffer(
						{
							meshData.vertices.data(),
							static_cast<ui32>(
								meshData.vertices.size()
							),
							sizeof(MeshVertex)
						}
					);

				renderResources.indexBuffer =
					m_graphicsDevice.createIndexBuffer(
						{
							meshData.indices.data(),
							static_cast<ui32>(
								meshData.indices.size()
							)
						}
					);

				renderResources.meshRevision =
					component->getMeshRevision();
			}

			auto& transform =
				component->getGameObject().getTransform();

			data.world =
				transform.getAffineWorldMatrix();

			auto& cb = *m_cb;

			context.updateConstantBuffer(
				cb,
				&data
			);

			auto& vertexBuffer =
				*renderResources.vertexBuffer;

			auto& indexBuffer =
				*renderResources.indexBuffer;

			context.setVertexBuffer(vertexBuffer);
			context.setConstantBuffer(cb);
			context.setIndexBuffer(indexBuffer);

			context.drawIndexedTriangleList(
				indexBuffer.getIndexListSize(),
				0u,
				0u
			);
		}

		// Delete cached GPU resources belonging to components
		// that no longer exist in the World.
		for (
			auto it = m_combinedMeshResources.begin();
			it != m_combinedMeshResources.end();
			)
		{
			if (
				activeCombinedMeshes.find(it->first) ==
				activeCombinedMeshes.end()
				)
			{
				it = m_combinedMeshResources.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	m_graphicsDevice.executeCommandList(context);
	//swapChain.present();
}