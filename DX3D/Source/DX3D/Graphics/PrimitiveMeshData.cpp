#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Math/MathUtils.h>

#include <cmath>

namespace dx3d
{
	namespace
	{
		const Vec4 sphereColor
		{
			0.88f,
			0.82f,
			0.68f,
			1.0f
		};

		const Vec4 capsuleColor
		{
			0.62f,
			0.78f,
			0.92f,
			1.0f
		};

		void addLatLongIndices(
			MeshData& mesh,
			ui32 ringCount,
			ui32 segments
		)
		{
			const ui32 stride =
				segments + 1;

			for (ui32 ring = 0;
				ring + 1 < ringCount;
				++ring)
			{
				for (ui32 segment = 0;
					segment < segments;
					++segment)
				{
					const ui32 first =
						ring * stride +
						segment;

					const ui32 second =
						first + stride;

					mesh.indices.push_back(
						first
					);
					mesh.indices.push_back(
						second
					);
					mesh.indices.push_back(
						first + 1
					);

					mesh.indices.push_back(
						first + 1
					);
					mesh.indices.push_back(
						second
					);
					mesh.indices.push_back(
						second + 1
					);
				}
			}
		}

		MeshData buildSphereMeshData()
		{
			constexpr ui32 rings = 16;
			constexpr ui32 segments = 32;
			constexpr f32 radius = 0.5f;

			MeshData mesh{};

			mesh.vertices.reserve(
				(rings + 1) *
				(segments + 1)
			);

			for (ui32 ring = 0;
				ring <= rings;
				++ring)
			{
				const f32 v =
					static_cast<f32>(ring) /
					static_cast<f32>(rings);

				const f32 theta =
					v * MathUtils::PI;

				const f32 y =
					std::cos(theta) *
					radius;

				const f32 ringRadius =
					std::sin(theta) *
					radius;

				for (ui32 segment = 0;
					segment <= segments;
					++segment)
				{
					const f32 u =
						static_cast<f32>(segment) /
						static_cast<f32>(segments);

					const f32 phi =
						u * MathUtils::PI *
						2.0f;

					const f32 x =
						std::cos(phi) *
						ringRadius;

					const f32 z =
						std::sin(phi) *
						ringRadius;

					const Vec3 normal =
						Vec3::normalize(
							{
								x,
								y,
								z
							}
						);

					mesh.vertices.push_back(
						{
							{ x, y, z },
							sphereColor,
							normal,
							{ u, v }
						}
					);
				}
			}

			addLatLongIndices(
				mesh,
				rings + 1,
				segments
			);

			return mesh;
		}

		MeshData buildCapsuleMeshData()
		{
			constexpr ui32 hemisphereRings = 8;
			constexpr ui32 segments = 32;
			constexpr f32 radius = 0.5f;
			constexpr f32 cylinderHalfHeight = 0.5f;

			MeshData mesh{};

			constexpr ui32 ringCount =
				hemisphereRings + 1 +
				hemisphereRings;

			mesh.vertices.reserve(
				ringCount *
				(segments + 1)
			);

			auto addRing =
				[&](
					f32 y,
					f32 ringRadius,
					f32 normalY,
					f32 v
					)
				{
					for (ui32 segment = 0;
						segment <= segments;
						++segment)
					{
						const f32 u =
							static_cast<f32>(segment) /
							static_cast<f32>(
								segments
							);

						const f32 phi =
							u * MathUtils::PI *
							2.0f;

						const f32 x =
							std::cos(phi) *
							ringRadius;

						const f32 z =
							std::sin(phi) *
							ringRadius;

						const Vec3 normal =
							Vec3::normalize(
								{
									x,
									normalY *
									radius,
									z
								}
							);

						mesh.vertices.push_back(
							{
								{ x, y, z },
								capsuleColor,
								normal,
								{ u, v }
							}
						);
					}
				};

			for (ui32 ring = 0;
				ring <= hemisphereRings;
				++ring)
			{
				const f32 t =
					static_cast<f32>(ring) /
					static_cast<f32>(
						hemisphereRings
					);

				const f32 angle =
					t * MathUtils::PI *
					0.5f;

				addRing(
					cylinderHalfHeight +
					std::cos(angle) * radius,
					std::sin(angle) * radius,
					std::cos(angle),
					t * 0.5f
				);
			}

			for (ui32 ring = 1;
				ring <= hemisphereRings;
				++ring)
			{
				const f32 t =
					static_cast<f32>(ring) /
					static_cast<f32>(
						hemisphereRings
					);

				const f32 angle =
					MathUtils::PI * 0.5f +
					t * MathUtils::PI * 0.5f;

				addRing(
					-cylinderHalfHeight +
					std::cos(angle) * radius,
					std::sin(angle) * radius,
					std::cos(angle),
					0.5f + t * 0.5f
				);
			}

			addLatLongIndices(
				mesh,
				ringCount,
				segments
			);

			return mesh;
		}
	}

	const MeshData& getCubeMeshData() noexcept
	{
		static const MeshData mesh
		{
			{
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, -1.0f },
					{ 1.0f, 1.0f }
				},

				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, 1.0f },
					{ 1.0f, 1.0f }
				},

				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 1.0f, 1.0f }
				},

				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 0.0f, -1.0f, 0.0f },
					{ 1.0f, 1.0f }
				},

				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f },
					{ 1.0f, 0.0f, 0.0f },
					{ 1.0f, 1.0f }
				},

				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f },
					{ 1.0f, 0.0f }
				},
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f },
					{ -1.0f, 0.0f, 0.0f },
					{ 1.0f, 1.0f }
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
					{ 0.0f, 1.0f, 0.0f },
					{ 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 0.0f, 0.0f }
				},
				{
					{ 0.5f, 0.0f, -0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 1.0f, 1.0f }
				},
				{
					{ 0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f },
					{ 0.0f, 1.0f, 0.0f },
					{ 1.0f, 0.0f }
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
		static const MeshData mesh =
			buildSphereMeshData();

		return mesh;
	}

	const MeshData& getCapsuleMeshData() noexcept
	{
		static const MeshData mesh =
			buildCapsuleMeshData();

		return mesh;
	}
}
