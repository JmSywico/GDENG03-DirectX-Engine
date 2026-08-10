#pragma once

#include "../pch.h"

class LayerStack
{
public:
	LayerStack();
	~LayerStack();

	/** @brief Inserts owned layers below overlays. */
	void PushLayer(std::unique_ptr<Layer> layer);

	/** @brief Inserts owned overlays above regular layers. */
	void PushOverlay(std::unique_ptr<Layer> overlay);

	std::unique_ptr<Layer> PopLayer();
	std::unique_ptr<Layer> PopOverlay();

	void OnUpdate(float deltaTime);
	void OnFixedUpdate(float fixedDeltaTime);
	void OnEvent(Event& e);
	void OnImGuiRender();
	void Clear();

	std::size_t Size() const noexcept;

private:
	std::vector<std::unique_ptr<Layer>> m_layers;
	// Layers are [0, m_layerInsertIndex); overlays are [m_layerInsertIndex, end).
	std::size_t m_layerInsertIndex{0};
};
