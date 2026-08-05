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

		void* getNativeHandle() const noexcept;

		Rect getClientSize() const noexcept;

		Rect getClientAreaInScreenSpace();
		void minimize();
		void toggleMaximizeRestore();
		void close();
		void beginTitleBarDrag();
		bool isMaximized() const noexcept;

	protected:
		void* m_handle{};
		Rect m_size{};
		Rect m_restoreRect{};
		bool m_maximized{ true };
	};
}
