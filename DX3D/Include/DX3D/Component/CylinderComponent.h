#pragma once

#include <DX3D/Game/Component.h>

namespace dx3d
{
	class CylinderComponent final : public Component
	{
		dx3d_typeid(CylinderComponent)
	public:
		explicit CylinderComponent(const ComponentDesc& data);
	};
}
