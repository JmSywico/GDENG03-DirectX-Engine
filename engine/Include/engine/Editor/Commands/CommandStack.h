#pragma once

#include "EditorCommand.h"

#include <memory>
#include <cstdint>
#include <vector>

namespace jnpf::Editor
{
	/**
	 * @brief Owns editor commands and provides linear undo/redo history.
	 * @ingroup editor
	 *
	 * Executing or recording a new command clears redo history. Commands use
	 * stable entity IDs where possible so history survives entity recreation.
	 */
	class CommandStack
	{
public:
		/** @brief Executes a command and pushes it onto undo history. */
		void Execute(std::unique_ptr<EditorCommand> command);
		void ExecuteCoalesced(std::unique_ptr<EditorCommand> command);
		void EndCoalescing() { m_coalescing = false; }
		/**
		 * @brief Records a command whose final state was already applied live.
		 *
		 * This is used by continuous interactions such as gizmo drags so the
		 * drag produces one undo step without executing the final transform twice.
		 */
		void RecordExecuted(std::unique_ptr<EditorCommand> command);
		void Undo();
		void Redo();
		void Clear();
		bool CanUndo() const { return !m_undo.empty(); }
		bool CanRedo() const { return !m_redo.empty(); }
		bool IsDirty() const { return m_currentRevision != m_savedRevision; }
		void MarkSaved() { m_savedRevision = m_currentRevision; }
		std::uint64_t GetRevision() const { return m_currentRevision; }

	private:
		struct Entry
		{
			std::unique_ptr<EditorCommand> Command;
			std::uint64_t BeforeRevision = 0;
			std::uint64_t AfterRevision = 0;
		};
		std::vector<Entry> m_undo;
		std::vector<Entry> m_redo;
		bool m_coalescing = false;
		std::uint64_t m_currentRevision = 0;
		std::uint64_t m_savedRevision = 0;
		std::uint64_t m_nextRevision = 1;
	};
}
