#pragma once

#include <DX3D/Game/Component.h>

namespace dx3d
{
	class CapsuleComponent final : public Component
	{
		dx3d_typeid(CapsuleComponent)
	public:
		explicit CapsuleComponent(const ComponentDesc& data);
	};
}
