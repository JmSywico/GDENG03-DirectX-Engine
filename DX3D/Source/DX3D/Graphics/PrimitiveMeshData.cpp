#include <DX3D/Graphics/PrimitiveMeshData.h>

#include <cmath>

namespace dx3d
{
	const MeshData& getCubeMeshData() noexcept
	{
		static const MeshData mesh
		{
			{
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f }
				},
				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f }
				},
				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f }
				},

				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f }
				},
				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f }
				},

				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},

				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f }
				},
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f }
				},

				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f }
				},

				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f }
				},
				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f }
				},
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f }
				}
			},
			{
				0, 1, 2,
				2, 3, 0,

				4, 5, 6,
				6, 7, 4,

				8, 9, 10,
				10, 11, 8,

				12, 13, 14,
				14, 15, 12,

				16, 17, 18,
				18, 19, 16,

				20, 21, 22,
				22, 23, 20
			}
		};

		return mesh;
	}

	const MeshData& getPlaneMeshData() noexcept
	{
		static const MeshData mesh
		{
			{
				{
					{ -0.5f, 0.0f, -0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ -0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ 0.5f, 0.0f, -0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				},
				{
					{ 0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f }
				}
			},
			{
				0, 1, 2,
				2, 1, 3
			}
		};

		return mesh;
	}

	const MeshData& getSphereMeshData() noexcept
	{
		static const MeshData mesh = []
		{
			MeshData result{};
			constexpr ui32 slices = 24;
			constexpr ui32 stacks = 16;
			constexpr f32 pi = 3.14159265358979323846f;
			for (ui32 stack = 0; stack <= stacks; ++stack)
			{
				const f32 v = static_cast<f32>(stack) / stacks;
				const f32 phi = v * pi;
				const f32 y = std::cos(phi);
				const f32 ring = std::sin(phi);
				for (ui32 slice = 0; slice <= slices; ++slice)
				{
					const f32 u = static_cast<f32>(slice) / slices;
					const f32 theta = u * 2.0f * pi;
					const Vec3 normal{ ring * std::cos(theta), y, ring * std::sin(theta) };
					result.vertices.push_back({
						{ normal.x * 0.5f, normal.y * 0.5f, normal.z * 0.5f },
						{ 1.0f, 1.0f, 1.0f, 1.0f }, normal });
				}
			}
			for (ui32 stack = 0; stack < stacks; ++stack)
			{
				for (ui32 slice = 0; slice < slices; ++slice)
				{
					const ui32 a = stack * (slices + 1) + slice;
					const ui32 b = a + slices + 1;
					result.indices.insert(result.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
				}
			}
			return result;
		}();
		return mesh;
	}

	const MeshData& getCylinderMeshData() noexcept
	{
		static const MeshData mesh = []
		{
			MeshData result{};
			constexpr ui32 slices = 32;
			constexpr f32 pi = 3.14159265358979323846f;
			for (ui32 slice = 0; slice <= slices; ++slice)
			{
				const f32 angle = static_cast<f32>(slice) / slices * 2.0f * pi;
				const f32 x = std::cos(angle);
				const f32 z = std::sin(angle);
				const Vec3 normal{ x, 0.0f, z };
				result.vertices.push_back({ { x * 0.5f, -0.5f, z * 0.5f }, { 1, 1, 1, 1 }, normal });
				result.vertices.push_back({ { x * 0.5f,  0.5f, z * 0.5f }, { 1, 1, 1, 1 }, normal });
			}
			for (ui32 slice = 0; slice < slices; ++slice)
			{
				const ui32 a = slice * 2;
				result.indices.insert(result.indices.end(), { a, a + 1, a + 2, a + 2, a + 1, a + 3 });
			}

			const ui32 bottomCenter = static_cast<ui32>(result.vertices.size());
			result.vertices.push_back({ { 0, -0.5f, 0 }, { 1, 1, 1, 1 }, { 0, -1, 0 } });
			const ui32 topCenter = static_cast<ui32>(result.vertices.size());
			result.vertices.push_back({ { 0, 0.5f, 0 }, { 1, 1, 1, 1 }, { 0, 1, 0 } });
			for (ui32 slice = 0; slice <= slices; ++slice)
			{
				const f32 angle = static_cast<f32>(slice) / slices * 2.0f * pi;
				const f32 x = std::cos(angle) * 0.5f;
				const f32 z = std::sin(angle) * 0.5f;
				result.vertices.push_back({ { x, -0.5f, z }, { 1, 1, 1, 1 }, { 0, -1, 0 } });
				result.vertices.push_back({ { x,  0.5f, z }, { 1, 1, 1, 1 }, { 0, 1, 0 } });
			}
			const ui32 capStart = bottomCenter + 2;
			for (ui32 slice = 0; slice < slices; ++slice)
			{
				const ui32 ring = capStart + slice * 2;
				result.indices.insert(result.indices.end(), { bottomCenter, ring + 2, ring });
				result.indices.insert(result.indices.end(), { topCenter, ring + 1, ring + 3 });
			}
			return result;
		}();
		return mesh;
	}

	const MeshData& getCapsuleMeshData() noexcept
	{
		static const MeshData mesh = []
		{
			MeshData result{};
			constexpr ui32 slices = 32;
			constexpr ui32 hemisphereRings = 8;
			constexpr f32 pi = 3.14159265358979323846f;
			for (ui32 ringIndex = 0; ringIndex <= hemisphereRings * 2; ++ringIndex)
			{
				const f32 t = static_cast<f32>(ringIndex) / (hemisphereRings * 2);
				const f32 phi = t * pi;
				const f32 normalY = std::cos(phi);
				const f32 radial = std::sin(phi);
				const f32 centerY = normalY >= 0.0f ? 0.5f : -0.5f;
				const f32 y = centerY + normalY * 0.5f;
				for (ui32 slice = 0; slice <= slices; ++slice)
				{
					const f32 theta = static_cast<f32>(slice) / slices * 2.0f * pi;
					const Vec3 normal{ radial * std::cos(theta), normalY, radial * std::sin(theta) };
					result.vertices.push_back({
						{ normal.x * 0.5f, y, normal.z * 0.5f },
						{ 1, 1, 1, 1 }, normal });
				}
			}
			for (ui32 ringIndex = 0; ringIndex < hemisphereRings * 2; ++ringIndex)
				for (ui32 slice = 0; slice < slices; ++slice)
				{
					const ui32 a = ringIndex * (slices + 1) + slice;
					const ui32 b = a + slices + 1;
					result.indices.insert(result.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
				}
			return result;
		}();
		return mesh;
	}
}
