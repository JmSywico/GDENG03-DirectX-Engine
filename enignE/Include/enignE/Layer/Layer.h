#pragma once

#include <string>

struct Event;

class Layer
{
public:
	explicit Layer(const std::string& name = "Layer");
	virtual ~Layer();

	virtual void OnAttach();
	virtual void OnDetach();

	virtual void OnUpdate(float deltaTime);
	virtual void OnFixedUpdate(float fixedDeltaTime);

	virtual void OnEvent(Event& e);

	virtual void OnImGuiRender();

	const std::string& GetName() const noexcept;

protected:
	std::string m_name;
};
