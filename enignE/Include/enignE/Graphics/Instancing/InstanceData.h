#pragma once

#include <DirectXMath.h>

namespace enignE::Graphics
{
	struct InstanceData
	{
		DirectX::XMFLOAT4 WorldRow0 = {1.0f, 0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 WorldRow1 = {0.0f, 1.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 WorldRow2 = {0.0f, 0.0f, 1.0f, 0.0f};
		DirectX::XMFLOAT4 WorldRow3 = {0.0f, 0.0f, 0.0f, 1.0f};

		static InstanceData FromWorldMatrix(const DirectX::XMMATRIX& world)
		{
			DirectX::XMFLOAT4X4 matrix{};
			DirectX::XMStoreFloat4x4(&matrix, world);

			InstanceData data;
			data.WorldRow0 = {matrix._11, matrix._12, matrix._13, matrix._14};
			data.WorldRow1 = {matrix._21, matrix._22, matrix._23, matrix._24};
			data.WorldRow2 = {matrix._31, matrix._32, matrix._33, matrix._34};
			data.WorldRow3 = {matrix._41, matrix._42, matrix._43, matrix._44};
			return data;
		}
	};
}
