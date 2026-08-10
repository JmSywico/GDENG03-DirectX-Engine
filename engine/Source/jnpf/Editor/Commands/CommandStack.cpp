#include "Editor/Commands/CommandStack.h"

namespace jnpf::Editor
{
	void CommandStack::Execute(std::unique_ptr<EditorCommand> command)
	{
		m_coalescing = false;
		if (!command)
			return;
		command->Execute();
		const std::uint64_t before = m_currentRevision;
		m_currentRevision = m_nextRevision++;
		m_undo.push_back({std::move(command), before, m_currentRevision});
		m_redo.clear();
	}

	void CommandStack::ExecuteCoalesced(std::unique_ptr<EditorCommand> command)
	{
		if (!command)
			return;
		command->Execute();
		if (m_coalescing && !m_undo.empty() && m_undo.back().Command->MergeWith(*command))
		{
			m_currentRevision = m_nextRevision++;
			m_undo.back().AfterRevision = m_currentRevision;
			m_redo.clear();
			return;
		}
		const std::uint64_t before = m_currentRevision;
		m_currentRevision = m_nextRevision++;
		m_undo.push_back({std::move(command), before, m_currentRevision});
		m_redo.clear();
		m_coalescing = true;
	}

	void CommandStack::RecordExecuted(std::unique_ptr<EditorCommand> command)
	{
		m_coalescing = false;
		if (!command)
			return;
		const std::uint64_t before = m_currentRevision;
		m_currentRevision = m_nextRevision++;
		m_undo.push_back({std::move(command), before, m_currentRevision});
		m_redo.clear();
	}

	void CommandStack::Undo()
	{
		m_coalescing = false;
		if (m_undo.empty())
			return;
		auto entry = std::move(m_undo.back());
		m_undo.pop_back();
		entry.Command->Undo();
		m_currentRevision = entry.BeforeRevision;
		m_redo.push_back(std::move(entry));
	}

	void CommandStack::Redo()
	{
		m_coalescing = false;
		if (m_redo.empty())
			return;
		auto entry = std::move(m_redo.back());
		m_redo.pop_back();
		entry.Command->Execute();
		m_currentRevision = entry.AfterRevision;
		m_undo.push_back(std::move(entry));
	}

	void CommandStack::Clear()
	{
		m_undo.clear();
		m_redo.clear();
		m_coalescing = false;
		m_currentRevision = 0;
		m_savedRevision = 0;
		m_nextRevision = 1;
	}
}
