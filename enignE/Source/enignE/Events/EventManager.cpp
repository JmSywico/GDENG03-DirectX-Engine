#include "Events/EventManager.h"

EventManager& EventManager::Get()
{
	static EventManager instance;
	return instance;
}

EventManager::EventManager()
	: m_nextListenerId(1)
{
}

EventManager::ListenerID EventManager::AddListener(EventType type, EventCallback callback)
{
	std::lock_guard<std::mutex> lock(m_listenersMutex);

	ListenerID id = m_nextListenerId++;
	auto& list = m_listeners[type];
	list.push_back({id, std::move(callback)});
	return id;
}

void EventManager::RemoveListener(EventType type, ListenerID id)
{
	std::lock_guard<std::mutex> lock(m_listenersMutex);

	auto it = m_listeners.find(type);
	if (it == m_listeners.end())
		return;

	auto& vec = it->second;
	for (auto vit = vec.begin(); vit != vec.end(); ++vit)
	{
		if (vit->Id == id)
		{
			vec.erase(vit);
			break;
		}
	}
}

void EventManager::QueueEvent(std::unique_ptr<Event> evt)
{
	if (!evt)
		return;

	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_eventQueue.push(std::move(evt));
}

void EventManager::FireEvent(Event& evt)
{
	std::vector<ListenerEntry> listenersCopy;

	{
		std::lock_guard<std::mutex> lock(m_listenersMutex);
		auto it = m_listeners.find(evt.Type);
		if (it != m_listeners.end())
		{
			listenersCopy = it->second;
		}
	}

	for (auto& entry : listenersCopy)
	{
		if (evt.Handled)
			break;

		if (entry.Callback)
			entry.Callback(evt);
	}
}

void EventManager::DispatchQueuedEvents()
{
	std::queue<std::unique_ptr<Event>> localQueue;

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		std::swap(localQueue, m_eventQueue);
	}

	while (!localQueue.empty())
	{
		std::unique_ptr<Event> evt = std::move(localQueue.front());
		localQueue.pop();

		if (evt)
			FireEvent(*evt);
	}
}
