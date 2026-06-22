#include <DX3D/Graphics/PrimitiveMeshData.h>

namespace dx3d
{
	const MeshData& getCubeMeshData() noexcept
	{
		static const MeshData mesh
		{
			// Vertices
			{
				{
					{ -0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, -0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f }
				},
				{
					{ 0.5f, 0.5f, -0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f }
				},
				{
					{ 0.5f, -0.5f, -0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f }
				},

				{
					{ 0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 1.0f, 1.0f }
				},
				{
					{ 0.5f, 0.5f, 0.5f },
					{ 0.0f, 0.0f, 1.0f, 1.0f }
				},
				{
					{ -0.5f, 0.5f, 0.5f },
					{ 0.0f, 1.0f, 0.0f, 1.0f }
				},
				{
					{ -0.5f, -0.5f, 0.5f },
					{ 1.0f, 0.0f, 0.0f, 1.0f }
				}
			},

			// Indices
			{
				0, 1, 2,
				2, 3, 0,

				4, 5, 6,
				6, 7, 4,

				1, 6, 5,
				5, 2, 1,

				7, 0, 3,
				3, 4, 7,

				3, 2, 5,
				5, 4, 3,

				7, 6, 1,
				1, 0, 7
			}
		};

		return mesh;
	}

	const MeshData& getPlaneMeshData() noexcept
	{
		static const MeshData mesh
		{
			// Vertices
			{
				{
					{ -0.5f, 0.0f, -0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f }
				},
				{
					{ -0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f }
				},
				{
					{ 0.5f, 0.0f, -0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f }
				},
				{
					{ 0.5f, 0.0f, 0.5f },
					{ 0.20f, 0.65f, 0.70f, 1.0f }
				}
			},

			// Indices
			{
				0, 1, 2,
				2, 1, 3
			}
		};

		return mesh;
	}
}