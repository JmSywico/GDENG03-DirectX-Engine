#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>

namespace dx3d
{
	class CircleComponent final : public Component
	{
		dx3d_typeid(CircleComponent)

	public:
		explicit CircleComponent(const ComponentDesc& desc);
	};
}