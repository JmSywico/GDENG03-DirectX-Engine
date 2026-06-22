#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>

namespace dx3d
{
	class PlaneComponent final : public Component
	{
		dx3d_typeid(PlaneComponent)

	public:
		explicit PlaneComponent(const ComponentDesc& data);
	};
}