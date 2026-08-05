#include "Layer/LayerStack.h"

LayerStack::LayerStack() = default;

LayerStack::~LayerStack()
{
	Clear();
}

void LayerStack::Clear()
{
	for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
	{
		if (*it)
			(*it)->OnDetach();
	}
	m_layers.clear();
	m_layerInsertIndex = 0;
}

void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
{
	if (!layer)
		return;

	layer->OnAttach();
	m_layers.insert(m_layers.begin() + static_cast<std::ptrdiff_t>(m_layerInsertIndex), std::move(layer));
	++m_layerInsertIndex;
}

void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
{
	if (!overlay)
		return;

	overlay->OnAttach();
	m_layers.push_back(std::move(overlay));
}

std::unique_ptr<Layer> LayerStack::PopLayer()
{
	if (m_layerInsertIndex == 0)
		return nullptr;

	std::size_t index = m_layerInsertIndex - 1;
	std::unique_ptr<Layer> popped = std::move(m_layers[index]);
	if (popped)
		popped->OnDetach();

	m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
	--m_layerInsertIndex;
	return popped;
}

std::unique_ptr<Layer> LayerStack::PopOverlay()
{
	if (m_layers.size() <= m_layerInsertIndex)
		return nullptr;

	std::size_t index = m_layers.size() - 1;
	std::unique_ptr<Layer> popped = std::move(m_layers[index]);
	if (popped)
		popped->OnDetach();

	m_layers.pop_back();
	return popped;
}

void LayerStack::OnUpdate(float deltaTime)
{
	for (auto& layer : m_layers)
	{
		if (layer)
			layer->OnUpdate(deltaTime);
	}
}

void LayerStack::OnFixedUpdate(float fixedDeltaTime)
{
	for (auto& layer : m_layers)
		if (layer) layer->OnFixedUpdate(fixedDeltaTime);
}

void LayerStack::OnEvent(Event& e)
{
	// By convention, dispatch events from top to bottom so top-most layer can consume them first.
	for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
	{
		if (*it)
		{
			(*it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}
}

void LayerStack::OnImGuiRender()
{
	for (auto& layer : m_layers)
	{
		if (layer)
			layer->OnImGuiRender();
	}
}

std::size_t LayerStack::Size() const noexcept
{
	return m_layers.size();
}
