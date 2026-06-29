#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>
#include <DX3D/Math/MathUtils.h>

#include <cmath>
#include <cassert>

namespace dx3d
{
	class Mat4x4
	{
	public:
		Mat4x4() = default;

		static Mat4x4 identity() noexcept
		{
			Mat4x4 result{};

			result.m_data[0][0] = 1.0f;
			result.m_data[1][1] = 1.0f;
			result.m_data[2][2] = 1.0f;
			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 translate(
			const Vec3& translation
		) noexcept
		{
			auto result =
				Mat4x4::identity();

			result.m_data[3][0] =
				translation.x;

			result.m_data[3][1] =
				translation.y;

			result.m_data[3][2] =
				translation.z;

			return result;
		}

		static Mat4x4 scale(
			const Vec3& scale
		) noexcept
		{
			Mat4x4 result{};

			result.m_data[0][0] = scale.x;
			result.m_data[1][1] = scale.y;
			result.m_data[2][2] = scale.z;
			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 rotateX(
			f32 x
		) noexcept
		{
			Mat4x4 result{};

			result.m_data[0][0] = 1.0f;
			result.m_data[1][1] = std::cos(x);
			result.m_data[1][2] = std::sin(x);
			result.m_data[2][1] = -std::sin(x);
			result.m_data[2][2] = std::cos(x);
			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 rotateY(
			f32 y
		) noexcept
		{
			Mat4x4 result{};

			result.m_data[0][0] = std::cos(y);
			result.m_data[1][1] = 1.0f;
			result.m_data[0][2] = -std::sin(y);
			result.m_data[2][0] = std::sin(y);
			result.m_data[2][2] = std::cos(y);
			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 rotateZ(
			f32 z
		) noexcept
		{
			Mat4x4 result{};

			result.m_data[0][0] = std::cos(z);
			result.m_data[0][1] = std::sin(z);
			result.m_data[1][0] = -std::sin(z);
			result.m_data[1][1] = std::cos(z);
			result.m_data[2][2] = 1.0f;
			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 orthoLH(
			f32 width,
			f32 height,
			f32 zNear,
			f32 zFar
		) noexcept
		{
			assert(
				width != 0.0f &&
				"OrthoLH: width must not be zero"
			);

			assert(
				height != 0.0f &&
				"OrthoLH: height must not be zero"
			);

			assert(
				zFar != zNear &&
				"OrthoLH: zNear and zFar cannot be equal"
			);

			Mat4x4 result{};

			result.m_data[0][0] =
				2.0f / width;

			result.m_data[1][1] =
				2.0f / height;

			result.m_data[2][2] =
				1.0f /
				(zFar - zNear);

			result.m_data[3][2] =
				-zNear /
				(zFar - zNear);

			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 lookAtLH(
			const Vec3& eye,
			const Vec3& target,
			const Vec3& up
		) noexcept
		{
			f32 forwardX =
				target.x - eye.x;

			f32 forwardY =
				target.y - eye.y;

			f32 forwardZ =
				target.z - eye.z;

			const f32 forwardLength =
				std::sqrt(
					forwardX * forwardX +
					forwardY * forwardY +
					forwardZ * forwardZ
				);

			if (forwardLength <= 0.000001f)
				return Mat4x4::identity();

			forwardX /= forwardLength;
			forwardY /= forwardLength;
			forwardZ /= forwardLength;

			f32 rightX =
				up.y * forwardZ -
				up.z * forwardY;

			f32 rightY =
				up.z * forwardX -
				up.x * forwardZ;

			f32 rightZ =
				up.x * forwardY -
				up.y * forwardX;

			const f32 rightLength =
				std::sqrt(
					rightX * rightX +
					rightY * rightY +
					rightZ * rightZ
				);

			if (rightLength <= 0.000001f)
				return Mat4x4::identity();

			rightX /= rightLength;
			rightY /= rightLength;
			rightZ /= rightLength;

			const f32 correctedUpX =
				forwardY * rightZ -
				forwardZ * rightY;

			const f32 correctedUpY =
				forwardZ * rightX -
				forwardX * rightZ;

			const f32 correctedUpZ =
				forwardX * rightY -
				forwardY * rightX;

			Mat4x4 result{};

			result.m_data[0][0] = rightX;
			result.m_data[0][1] = correctedUpX;
			result.m_data[0][2] = forwardX;

			result.m_data[1][0] = rightY;
			result.m_data[1][1] = correctedUpY;
			result.m_data[1][2] = forwardY;

			result.m_data[2][0] = rightZ;
			result.m_data[2][1] = correctedUpZ;
			result.m_data[2][2] = forwardZ;

			result.m_data[3][0] =
				-(
					rightX * eye.x +
					rightY * eye.y +
					rightZ * eye.z
					);

			result.m_data[3][1] =
				-(
					correctedUpX * eye.x +
					correctedUpY * eye.y +
					correctedUpZ * eye.z
					);

			result.m_data[3][2] =
				-(
					forwardX * eye.x +
					forwardY * eye.y +
					forwardZ * eye.z
					);

			result.m_data[3][3] = 1.0f;

			return result;
		}

		static Mat4x4 perspectiveFovLH(
			f32 fov,
			f32 aspect,
			f32 zNear,
			f32 zFar
		) noexcept
		{
			assert(
				fov > 0.001f &&
				"perspectiveFovLH: fov must be greater than 0 radians"
			);

			assert(
				fov <
				MathUtils::PI -
				0.001f &&
				"perspectiveFovLH: fov must be less than PI radians"
			);

			assert(
				aspect > 0.0f &&
				"perspectiveFovLH: aspect ratio must be greater than 0"
			);

			assert(
				zFar != zNear &&
				"perspectiveFovLH: zNear and zFar cannot be equal"
			);

			Mat4x4 result{};

			const f32 yScale =
				1.0f /
				std::tan(
					fov / 2.0f
				);

			const f32 xScale =
				yScale / aspect;

			result.m_data[0][0] = xScale;
			result.m_data[1][1] = yScale;

			result.m_data[2][2] =
				zFar /
				(zFar - zNear);

			result.m_data[2][3] = 1.0f;

			result.m_data[3][2] =
				(-zNear * zFar) /
				(zFar - zNear);

			result.m_data[3][3] = 0.0f;

			return result;
		}

		static Mat4x4 inverse(
			const Mat4x4& rhs
		) noexcept
		{
			Mat4x4 output{};
			Vec4 vectors[3]{};

			const auto determinantValue =
				Mat4x4::determinant(rhs);

			if (!determinantValue)
				return {};

			for (auto i = 0; i < 4; ++i)
			{
				for (auto j = 0; j < 4; ++j)
				{
					if (j == i)
						continue;

					auto destinationIndex = j;

					if (j > i)
					{
						destinationIndex -= 1;
					}

					vectors[destinationIndex].x =
						rhs.m_data[j][0];

					vectors[destinationIndex].y =
						rhs.m_data[j][1];

					vectors[destinationIndex].z =
						rhs.m_data[j][2];

					vectors[destinationIndex].w =
						rhs.m_data[j][3];
				}

				const auto crossProduct =
					Vec4::cross(
						vectors[0],
						vectors[1],
						vectors[2]
					);

				const f32 sign =
					static_cast<f32>(
						std::pow(
							-1.0f,
							i
						)
						);

				output.m_data[0][i] =
					sign *
					crossProduct.x /
					determinantValue;

				output.m_data[1][i] =
					sign *
					crossProduct.y /
					determinantValue;

				output.m_data[2][i] =
					sign *
					crossProduct.z /
					determinantValue;

				output.m_data[3][i] =
					sign *
					crossProduct.w /
					determinantValue;
			}

			return output;
		}

		static f32 determinant(
			const Mat4x4& rhs
		) noexcept
		{
			Vec4 first
			{
				rhs.m_data[0][0],
				rhs.m_data[1][0],
				rhs.m_data[2][0],
				rhs.m_data[3][0]
			};

			Vec4 second
			{
				rhs.m_data[0][1],
				rhs.m_data[1][1],
				rhs.m_data[2][1],
				rhs.m_data[3][1]
			};

			Vec4 third
			{
				rhs.m_data[0][2],
				rhs.m_data[1][2],
				rhs.m_data[2][2],
				rhs.m_data[3][2]
			};

			const auto minor =
				Vec4::cross(
					first,
					second,
					third
				);

			return -(
				rhs.m_data[0][3] * minor.x +
				rhs.m_data[1][3] * minor.y +
				rhs.m_data[2][3] * minor.z +
				rhs.m_data[3][3] * minor.w
				);
		}

		Vec4 row(
			ui32 index
		) const
		{
			assert(
				index < 4 &&
				"Matrix row index out of range"
			);

			return
			{
				m_data[index][0],
				m_data[index][1],
				m_data[index][2],
				m_data[index][3]
			};
		}

		Vec4 column(
			ui32 index
		) const
		{
			assert(
				index < 4 &&
				"Matrix column index out of range"
			);

			return
			{
				m_data[0][index],
				m_data[1][index],
				m_data[2][index],
				m_data[3][index]
			};
		}

		Vec4 transform(
			const Vec4& vector
		) const noexcept
		{
			return
			{
				vector.x * m_data[0][0] +
					vector.y * m_data[1][0] +
					vector.z * m_data[2][0] +
					vector.w * m_data[3][0],

				vector.x * m_data[0][1] +
					vector.y * m_data[1][1] +
					vector.z * m_data[2][1] +
					vector.w * m_data[3][1],

				vector.x * m_data[0][2] +
					vector.y * m_data[1][2] +
					vector.z * m_data[2][2] +
					vector.w * m_data[3][2],

				vector.x * m_data[0][3] +
					vector.y * m_data[1][3] +
					vector.z * m_data[2][3] +
					vector.w * m_data[3][3]
			};
		}

		Mat4x4 operator*(
			const Mat4x4& rhs
			) const noexcept
		{
			Mat4x4 result{};

			for (
				auto rowIndex = 0u;
				rowIndex < 4u;
				++rowIndex
				)
			{
				for (
					auto sourceIndex = 0u;
					sourceIndex < 4u;
					++sourceIndex
					)
				{
					const auto value =
						m_data[
							rowIndex
						][
							sourceIndex
						];

					for (
						auto columnIndex = 0u;
						columnIndex < 4u;
						++columnIndex
						)
					{
						result.m_data[
							rowIndex
						][
							columnIndex
						] +=
								value *
								rhs.m_data[
									sourceIndex
								][
									columnIndex
								];
					}
				}
			}

			return result;
		}

	private:
		f32 m_data[4][4]{};
	};
}
