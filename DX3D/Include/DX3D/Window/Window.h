#pragma once

#include <DX3D/Core/Base.h>
#include <DX3D/Core/Common.h>

namespace dx3d
{
	class Window : public Base
	{
	public:
		explicit Window(
			const WindowDesc& desc
		);

		virtual ~Window() override;
<<<<<<< Updated upstream
		dx3d::Rect getClientAreaInScreenSpace();
=======

		void* getNativeHandle() const noexcept;

		Rect getClientSize() const noexcept;

		Rect getClientAreaInScreenSpace();

>>>>>>> Stashed changes
	protected:
		void* m_handle{};
		Rect m_size{};
	};
}