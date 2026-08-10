#pragma once

#include <DX3D/Core/Base.h>
#include <DX3D/Core/Common.h>

#include <string>
#include <vector>

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

		std::vector<std::string>
			consumeDroppedFiles();

	protected:
		void* m_handle{};
		Rect m_size{};
	};
}
