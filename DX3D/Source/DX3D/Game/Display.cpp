#include <DX3D/Game/Display.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/SwapChain.h>

dx3d::Display::Display(
	const DisplayDesc& desc
)
	: Window(desc.window)
{
	m_size = getClientSize();

	m_swapChain =
		desc.graphicsDevice.createSwapChain(
			{
				m_handle,
				m_size
			}
		);
}

bool dx3d::Display::update()
{
	const Rect clientSize =
		getClientSize();

	if (
		clientSize.width <= 0 ||
		clientSize.height <= 0
		)
	{
		return false;
	}

	const bool sizeChanged =
		clientSize.width != m_size.width ||
		clientSize.height != m_size.height;

	if (sizeChanged)
	{
		m_size = clientSize;

		m_swapChain->resize(
			m_size
		);
	}

	return true;
}

dx3d::SwapChain&
dx3d::Display::getSwapChain() noexcept
{
	return *m_swapChain;
}