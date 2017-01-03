#ifndef LUAIDE_H
#define LUAIDE_H

/*
* Copyright 2005-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the DoorScope application
* see <http://doorscope.ch/>).
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QMainWindow>
#include <Script/Engine2.h>
#include <Gui/ListView.h>

class QTabWidget;
class QToolButton;

namespace Lua
{
	class Terminal2;
	class CodeEditor;
	class StackView;
	class LocalsView;
}

namespace Ds
{
	class LuaIde : public QMainWindow, public Lua::Engine2::DbgShell
	{
		Q_OBJECT
	public:
		explicit LuaIde();
		~LuaIde();
		static LuaIde* inst();
		void bringToFront();
	protected:
		void closeEvent ( QCloseEvent * event );
		void handleBreak( Lua::Engine2*, const QByteArray& source, int line );
		Lua::CodeEditor* getEditor( const QString& id = QString(), int* pos = 0 );
		Lua::CodeEditor* createEditor(bool readOnly = true, const QString& title = QString(),
									  const QString& id = QString(), const QString& text = QString() );
		Lua::CodeEditor* getOrCreateEditor(bool readOnly = true, const QString& title = QString(),
									  const QString& id = QString(), const QString& text = QString() );
		void fillScriptList();
		bool isUniqueName( const QString& ) const;
		Lua::CodeEditor* currentEditor() const;
		void toggleBreakPoint( Lua::CodeEditor*, int line );
		void updateBreakpoints( Lua::CodeEditor* );
		void saveData( Lua::CodeEditor* );
	protected slots:
		void onCurrentTabChanged(int);
		void onClose();
		void onTextChanged(bool changed);
		void onItemDoubleClicked( Gui::ListViewItem * );
		void onUpdateCursor();
		void handleCheck();
		void handleExecute();
		void handleContinue();
		void handleSingleStep();
		void handleAbort();
		void handleSetDebug();
		void handleBreakAtFirst();
		void handleBreakpoint();
		void handleRemoveAllBreaks();
		void handleSetFont();
		void handleCreate();
		void handleDuplicate();
		void handleRename();
		void handleDelete();
		void handleShow();
		void handleEdit();
		void handleCheck2();
		void handleExecute2();
		void handleImport();
		void handleImport2();
		void handleOpenFile();
		void handleRunFile();
		void handleExport();
		void handleExportBin();
		void handleSave();
	private:
		QTabWidget* d_tab;
		QToolButton* d_closer;
		Lua::Terminal2* d_term;
		Gui::ListView* d_scriptList;
		Lua::StackView* d_stack;
		Lua::LocalsView* d_locals;
	};
}

#endif // LUAIDE_H
