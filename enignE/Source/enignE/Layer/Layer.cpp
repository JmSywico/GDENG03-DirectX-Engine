#include "Layer/Layer.h"

Layer::Layer(const std::string& name)
	: m_name(name)
{
}

Layer::~Layer() = default;

void Layer::OnAttach()
{
}

void Layer::OnDetach()
{
}

void Layer::OnUpdate(float)
{
}

void Layer::OnFixedUpdate(float)
{
}

void Layer::OnEvent(Event&)
{
}

void Layer::OnImGuiRender()
{
}

const std::string& Layer::GetName() const noexcept
{
	return m_name;
}
