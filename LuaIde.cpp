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

#include "LuaIde.h"
#include <Script/Terminal2.h>
#include <Script/StackView.h>
#include <Script/LocalsView.h>
#include <Script/CodeEditor.h>
#include <Script/SyntaxHighlighter.h>
#include <Script/Lua.h>
#include <Sdb/Database.h>
#include <Gui2/AutoMenu.h>
#include <Gui2/AutoShortcut.h>
#include <QDockWidget>
#include <QFileInfo>
#include <QToolButton>
#include <QTabWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QMessageBox>
#include <QFontDialog>
#include <QInputDialog>
#include <QFileDialog>
#include <QApplication>
#include <QtDebug>
#include "AppContext.h"
#include "TypeDefs.h"
#include "ScriptSelectDlg.h"
#include <memory>
using namespace Ds;

static LuaIde* s_inst = 0;
static const int s_fixed = 1;

class _MyDockWidget : public QDockWidget
{
public:
	_MyDockWidget( const QString& title, QWidget* p ):QDockWidget(title,p){}
	virtual void setVisible( bool visible )
	{
		QDockWidget::setVisible( visible );
		if( visible )
			raise();
	}
};

static inline QDockWidget* createDock( QMainWindow* parent, const QString& title, bool visi )
{
	QDockWidget* dock = new _MyDockWidget( title, parent );
	dock->setObjectName( QChar('[') + title );
	dock->setAllowedAreas( Qt::AllDockWidgetAreas );
	dock->setFeatures( QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable );
	dock->setFloating( false );
	dock->setVisible( visi );
	return dock;
}

struct _ScriptViewItem : public Gui::ListViewItem
{
	_ScriptViewItem( Gui::ListView* p, const Sdb::Obj& s ):ListViewItem( p ),d_script(s) {}
	Sdb::Obj d_script;
	QString text( int f ) const
	{
		if( f == 0 )
			return d_script.getValue(AttrScriptName).getStr();
		return QString();
	}
};

Lua::CodeEditor* LuaIde::createEditor(bool readOnly, const QString &title, const QString &id, const QString &text )
{
	Lua::CodeEditor* e = new Lua::CodeEditor( d_tab );
	QSettings set;
	e->setFont( set.value( "LuaEditor/Font", QVariant::fromValue( Lua::CodeEditor::defaultFont() ) ).value<QFont>() );
	e->setFocus();
	e->setText( text );
	e->setName( title );
	e->setObjectName( id );
	e->setReadOnly( readOnly );
	const int i = d_tab->addTab( e, title );
	d_tab->setCurrentIndex( i );
	if( !readOnly )
		d_tab->setTabIcon( i, QIcon(":/DoorScope/Images/stift.png" ) );
	e->document()->setModified( false );
	connect( e, SIGNAL(modificationChanged(bool)), this, SLOT( onTextChanged(bool) ) );
	connect( e, SIGNAL( cursorPositionChanged() ), this, SLOT(  onUpdateCursor() ) );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( e, true );
	pop->addCommand( "Save", this, SLOT(handleSave()), tr("CTRL+S"), false );
	pop->addSeparator();
	pop->addCommand( "Check Syntax", this, SLOT(handleCheck()) );
	pop->addCommand( "&Execute",this, SLOT(handleExecute()), tr("CTRL+E"), false );
	pop->addCommand( "Continue", this, SLOT( handleContinue() ), tr("F5"), false );
	pop->addCommand( "Step", this, SLOT( handleSingleStep() ), tr("F11"), false );
	pop->addCommand( "Terminate", this, SLOT( handleAbort() ) );
	pop->addCommand( "Debugging", this, SLOT(handleSetDebug() ) );
	pop->addCommand( "Break at first line", this, SLOT( handleBreakAtFirst() ) );
	pop->addCommand( "Toggle Breakpoint", this, SLOT(handleBreakpoint() ), tr("F9"), false );
	pop->addCommand( "Remove all Breakpoints", this, SLOT(handleRemoveAllBreaks() ) );
	pop->addSeparator();
	pop->addCommand( "Undo", e, SLOT(handleEditUndo()), tr("CTRL+Z"), true );
	pop->addCommand( "Redo", e, SLOT(handleEditRedo()), tr("CTRL+Y"), true );
	pop->addSeparator();
	pop->addCommand( "Cut", e, SLOT(handleEditCut()), tr("CTRL+X"), true );
	pop->addCommand( "Copy", e, SLOT(handleEditCopy()), tr("CTRL+C"), true );
	pop->addCommand( "Paste", e, SLOT(handleEditPaste()), tr("CTRL+V"), true );
	pop->addCommand( "Select all", e, SLOT(handleEditSelectAll()), tr("CTRL+A"), true  );
	pop->addSeparator();
	pop->addCommand( "Find...", e, SLOT(handleFind()), tr("CTRL+F"), true );
	pop->addCommand( "Find again", e, SLOT(handleFindAgain()), tr("F3"), true );
	pop->addCommand( "Replace...", e, SLOT(handleReplace()), tr("CTRL+R"), true );
	pop->addCommand( "&Goto...", e, SLOT(handleGoto()), tr("CTRL+G"), true );
	pop->addSeparator();
	pop->addCommand( "Indent", e, SLOT(handleIndent()) );
	pop->addCommand( "Unindent", e, SLOT(handleUnindent()) );
	pop->addCommand( "Set Indentation Level...", e, SLOT(handleSetIndent()) );
	pop->addSeparator();
	pop->addCommand( "Print...", e, SLOT(handlePrint()) );
	pop->addCommand( "Export PDF...", e, SLOT(handleExportPdf()) );
	pop->addSeparator();
	pop->addCommand( "Set Font...", this, SLOT(handleSetFont()) );
	return e;
}

Lua::CodeEditor *LuaIde::getOrCreateEditor(bool readOnly, const QString &title, const QString &id, const QString &text)
{
	int pos;
	Lua::CodeEditor* e = getEditor( id, &pos );
	if( e != 0 )
	{
		d_tab->setCurrentIndex( pos );
		return e;
	}
	return createEditor( readOnly, title, id, text );
}

void LuaIde::fillScriptList()
{
	d_scriptList->clear();
	Sdb::Obj sub = AppContext::inst()->getRoot().getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == TypeLuaScript )
			new _ScriptViewItem( d_scriptList, sub );
	}while( sub.next() );
	d_scriptList->setSorting( 0 );
}

bool LuaIde::isUniqueName(const QString & name) const
{
	Sdb::Obj sub = AppContext::inst()->getRoot().getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == TypeLuaScript &&
			sub.getValue( AttrScriptName ).getStr().compare( name, Qt::CaseInsensitive) == 0 )
			return false;
	}while( sub.next() );
	return true;
}

Lua::CodeEditor *LuaIde::currentEditor() const
{
	return dynamic_cast<Lua::CodeEditor*>( d_tab->currentWidget() );
}

void LuaIde::toggleBreakPoint(Lua::CodeEditor * e, int line)
{
	const QByteArray n = e->objectName().toLatin1();
	if( Lua::Engine2::getInst()->getBreaks( n ).contains( line + 1 ) )
	{
		// Remove
		Lua::Engine2::getInst()->removeBreak( n, line + 1 );
		e->removeBreakPoint( line );
	}else
	{
		// Add
		Lua::Engine2::getInst()->addBreak( n, line + 1 );
		e->addBreakPoint( line );
	}
}

void LuaIde::updateBreakpoints(Lua::CodeEditor * e)
{
	e->clearBreakPoints();
	const Lua::Engine2::Breaks& b = Lua::Engine2::getInst()->getBreaks( e->objectName().toLatin1() );
	foreach( quint32 l, b )
		e->addBreakPoint( l - 1 );
}

void LuaIde::saveData(Lua::CodeEditor * e)
{
	if( e->objectName().startsWith( QChar(':') ) )
	{
		const QString name = e->objectName().mid( 1 );
		for( int i = 0; i < d_scriptList->count(); i++ )
		{
			_ScriptViewItem* item = static_cast<_ScriptViewItem*>( d_scriptList->child( i ) );
			if( item->text(0) == name )
			{
				item->d_script.setValue( AttrScriptSource, Stream::DataCell().setString( e->getText() ) );
				item->d_script.setValue( AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
				e->document()->setModified(false);
				break;
			}
		}
	}
}

LuaIde::LuaIde()
{
	QFileInfo info( AppContext::inst()->getDb()->getFilePath() );
	setWindowTitle( tr("%1 - DoorScope Lua IDE").arg( info.baseName() ) );

	d_tab = new QTabWidget( this );
	d_tab->setIconSize( QSize(10,10) );
	d_tab->setElideMode( Qt::ElideRight );
	connect( d_tab, SIGNAL( currentChanged ( int ) ), this, SLOT( onCurrentTabChanged(int) ) );

	d_closer = new QToolButton( d_tab );
	d_closer->setEnabled(false);
	d_closer->setIcon( QPixmap( ":/DoorScope/Images/close.png" ) );
	d_closer->setAutoRaise( true );
	connect( d_closer, SIGNAL( clicked() ), this, SLOT( onClose() ) );
	d_tab->setCornerWidget( d_closer );
	setCentralWidget( d_tab );

	getOrCreateEditor( false, tr("<Default>") );

	setDockNestingEnabled(true);
	setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

	QDockWidget* dock = createDock( this, tr("Scripts" ), true );
	d_scriptList = new Gui::ListView( dock );
	d_scriptList->addColumn( "" );
	d_scriptList->header()->hide();
	d_scriptList->setMinimumHeight( 200 );
	d_scriptList->setRootIsDecorated( false );
	fillScriptList();
	connect( d_scriptList, SIGNAL(  doubleClicked( Gui::ListViewItem * ) ),
		this, SLOT( onItemDoubleClicked( Gui::ListViewItem * ) ) );
	connect( d_scriptList, SIGNAL(  returnPressed( Gui::ListViewItem * ) ),
		this, SLOT( onItemDoubleClicked( Gui::ListViewItem * ) ) );
	dock->setWidget( d_scriptList );
	addDockWidget( Qt::LeftDockWidgetArea, dock );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_scriptList, true );
	pop->addCommand(tr("Show"), this, SLOT(handleShow()) );
	pop->addCommand(tr("Edit"), this, SLOT(handleEdit()) );
	pop->addCommand(tr("Check Syntax..."), this, SLOT(handleCheck2()) );
	pop->addCommand(tr("Execute"), this, SLOT(handleExecute2()) );
	pop->addSeparator();
	pop->addCommand(tr("New..."), this, SLOT(handleCreate()) );
	pop->addCommand(tr("Duplicate..."), this, SLOT(handleDuplicate()) );
	pop->addCommand(tr("Rename..."), this, SLOT(handleRename()) );
	pop->addCommand(tr("Delete..."), this, SLOT(handleDelete()) );
	pop->addSeparator();
	pop->addCommand(tr("Show File..."), this, SLOT(handleOpenFile()) );
	pop->addCommand(tr("Import from File..."), this, SLOT(handleImport()) );
	pop->addCommand(tr("Execute File..."), this, SLOT(handleRunFile()) );
	pop->addCommand(tr("Import from Repository..."), this, SLOT(handleImport2()) );
	pop->addCommand(tr("Export..."), this, SLOT(handleExport()) );
	pop->addCommand(tr("Export Binary..."), this, SLOT(handleExportBin()) );

	dock = createDock( this, tr("Terminal" ), true );
	d_term = new Lua::Terminal2( dock, Lua::Engine2::getInst() );
	dock->setWidget( d_term );
	addDockWidget( Qt::BottomDockWidgetArea, dock );

	dock = createDock( this, tr("Stack" ), true );
	d_stack = new Lua::StackView( Lua::Engine2::getInst(), dock );
	d_stack->setMinimumWidth(200);
	dock->setWidget( d_stack );
	addDockWidget( Qt::RightDockWidgetArea, dock );

	dock = createDock( this, tr("Locals" ), true );
	d_locals = new Lua::LocalsView( Lua::Engine2::getInst(), dock );
	d_locals->setMinimumWidth(200);
	dock->setWidget( d_locals );
	addDockWidget( Qt::RightDockWidgetArea, dock );

	QVariant state = AppContext::inst()->getSet()->value( "LuaIDE/State" );
	if( !state.isNull() )
		restoreState( state.toByteArray() );
	bringToFront();

	Lua::Engine2::getInst()->setDbgShell( this );
	statusBar()->showMessage( tr("Idle") );

	new Gui2::AutoShortcut( tr("CTRL+S"), this, this, SLOT(handleSave()) );
	new Gui2::AutoShortcut( tr("CTRL+E"), this, this, SLOT(handleExecute()) );
	new Gui2::AutoShortcut( tr("F5"), this, this, SLOT(handleContinue()) );
	new Gui2::AutoShortcut( tr("F11"), this, this, SLOT(handleSingleStep()) );
	new Gui2::AutoShortcut( tr("F9"), this, this, SLOT(handleBreakpoint()) );
}

LuaIde::~LuaIde()
{
	s_inst = 0;
}

LuaIde *LuaIde::inst()
{
	if( s_inst == 0 )
		s_inst = new LuaIde();
	return s_inst;
}

void LuaIde::handleBreak(Lua::Engine2 * lua, const QByteArray &source, int line)
{
	bringToFront();
	Lua::CodeEditor* curEdit = getEditor(source);
	if( curEdit == 0 )
	{
		if( source.startsWith( ':' ) )
		{
			const QString name = source.mid( 1 );
			for( int i = 0; i < d_scriptList->count(); i++ )
			{
				_ScriptViewItem* item = static_cast<_ScriptViewItem*>( d_scriptList->child( i ) );
				if( item->text(0) == name )
				{
					curEdit = createEditor( true, name, source, item->d_script.getValue( AttrScriptSource ).getStr() );
					break;
				}
			}
		}else if( source.startsWith( '@' ) )
		{
			QFileInfo fi( source.mid( 1 ) );
			curEdit = createEditor( true, fi.fileName(), source );
			curEdit->loadFromFile( fi.filePath(), true );
			curEdit->document()->setModified(false);
		}else if( source.startsWith( '#' ) )
		{
			curEdit = createEditor( true, source.mid(1), source );
		}else if( false ) // source.startsWith( '[') )
		{
			const int pos = source.indexOf( ']' );
			if( pos != -1 )
			{
				Sdb::Obj o = AppContext::inst()->getTxn()->getObject( source.mid( 1, pos ).toULongLong(0,16) );
				if( o.getType() == TypeLuaFilter )
				{
					curEdit = createEditor( true, source.mid(pos), source, o.getValue( AttrScriptSource ).getStr() );
				}
			}
		}else
		{
			lua_getfield( lua->getCtx(), LUA_REGISTRYINDEX, source );
			if( !lua_isnil( lua->getCtx(), -1 ) )
			{
				curEdit = createEditor( true, source, source, lua_tostring( lua->getCtx(), -1 ) );
			}
			lua_pop( lua->getCtx(), 1 );
		}
	}
	if( curEdit )
	{
		if( source.startsWith( '#' ) )
		{
			curEdit->loadFromString( lua->getCurBinary(), true );
			curEdit->document()->setModified(false);
		}
		curEdit->setPositionMarker( line - 1 );
		curEdit->setFocus();
	}
	while( lua->isWaiting() )
	{
		QApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents );
		QApplication::flush();
	}
	if( curEdit )
		curEdit->setPositionMarker( -1 );
}

Lua::CodeEditor *LuaIde::getEditor(const QString &id, int *pos)
{
	for( int i = 0; i < d_tab->count(); i++ )
	{
		Lua::CodeEditor* e = dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) );
		Q_ASSERT( e != 0 );
		if( e->objectName() == id )
		{
			if( pos )
				*pos = i;
			return e;
		}
	}
	return 0;
}

void LuaIde::bringToFront()
{
	showMaximized();
	activateWindow();
	raise();
}

void LuaIde::closeEvent(QCloseEvent *event)
{
	bool dirty = false;
	for( int i = 0; i < d_tab->count(); i++ )
		dirty |= dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) )->document()->isModified();

	if( dirty )
	{
		const int res = QMessageBox::warning( this, tr("Closing Lua IDE"),
											  tr("There are modified scripts. Do you want to save changes?"),
											  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes );
		switch( res )
		{
		case QMessageBox::Yes:
			for( int i = 0; i < d_tab->count(); i++ )
			{
				Lua::CodeEditor* e = dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) );
				if( e->document()->isModified() )
					saveData( e );
			}
			AppContext::inst()->getTxn()->commit();
			break;
		case QMessageBox::No:
			break;
		default:
			event->ignore();
			return;
		}
	}

	AppContext::inst()->getSet()->setValue("LuaIDE/State", saveState() );
	QMainWindow::closeEvent( event );
}

void LuaIde::onCurrentTabChanged(int i)
{
	d_closer->setEnabled( i >= s_fixed );
}

void LuaIde::onClose()
{
	Lua::CodeEditor* e = currentEditor();
	if( e != 0 && e != d_tab->widget(0) )
	{
		if( e->document()->isModified() )
		{
			const int res = QMessageBox::warning( this, tr("Closing Script"),
												  tr("The script was modified. Do you want to save changes?"),
												  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes );
			switch( res )
			{
			case QMessageBox::Yes:
				saveData( e );
				AppContext::inst()->getTxn()->commit();
				break;
			case QMessageBox::No:
				break;
			default:
				return;
			}
		}
		d_tab->removeTab( d_tab->currentIndex() );
		e->deleteLater();
	}
}

void LuaIde::onTextChanged(bool changed)
{
	Lua::CodeEditor* e = 0;
	int pos = -1;
	for( int i = 0; i < d_tab->count(); i++ )
	{
		if( sender() == d_tab->widget( i ) )
		{
			e = dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) );
			pos = i;
			break;
		}
	}
	Q_ASSERT( e != 0 );

	if( changed )
		d_tab->setTabText( pos, "*" + e->getName() );
	else
		d_tab->setTabText( pos, e->getName() );
}

void LuaIde::onItemDoubleClicked(Gui::ListViewItem * item)
{
	if( item )
	{
		_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( item);
		const QString name = svi->d_script.getValue( AttrScriptName ).getStr();
		updateBreakpoints( getOrCreateEditor( true, name, ":" + name,
											  svi->d_script.getValue( AttrScriptSource ).getStr() ) );
	}
}

void LuaIde::onUpdateCursor()
{
	Lua::CodeEditor* e = currentEditor();
	if( e == 0 )
		return;
	int line, col;
	e->getCursorPosition( &line, &col );
	const QString msg = tr("Line: %1  Column: %2  %3").arg(line + 1).arg(col + 1).
								arg( Lua::SyntaxHighlighter::format( e->getTokenTypeAtCursor() ) );
	statusBar()->showMessage( msg );
}

void LuaIde::handleCheck()
{
	Lua::CodeEditor* e = currentEditor();
	ENABLED_IF( e != 0 );
	if( Lua::Engine2::getInst()->pushFunction( e->text().toLatin1(), e->objectName().toLatin1() ) )
	{
		Lua::Engine2::getInst()->pop();
		QMessageBox::information( this, tr("Checking Syntax"), tr("No errors found!") );
	}else
		QMessageBox::critical( this, tr("Checking Syntax"), Lua::Engine2::getInst()->getLastError() );
}

void LuaIde::handleExecute()
{
	Lua::CodeEditor* e = currentEditor();
	ENABLED_IF( !Lua::Engine2::getInst()->isExecuting() && e != 0 );
	Lua::Engine2::getInst()->executeCmd( e->text().toLatin1(), ( e->objectName().isEmpty() ) ?
											 QByteArray("#Editor") : e->objectName().toLatin1() );
}

void LuaIde::handleContinue()
{
	ENABLED_IF( Lua::Engine2::getInst()->isDebug() && Lua::Engine2::getInst()->isWaiting() );

	Lua::Engine2::getInst()->runToBreakPoint();
}

void LuaIde::handleSingleStep()
{
	ENABLED_IF( Lua::Engine2::getInst()->isDebug() && Lua::Engine2::getInst()->isWaiting() );

	Lua::Engine2::getInst()->runToNextLine();
}

void LuaIde::handleAbort()
{
	ENABLED_IF( Lua::Engine2::getInst()->isWaiting() && Lua::Engine2::getInst()->isDebug() );

	Lua::Engine2::getInst()->terminate();
}

void LuaIde::handleSetDebug()
{
	CHECKED_IF( !Lua::Engine2::getInst()->isExecuting(), Lua::Engine2::getInst()->isDebug() );
	Lua::Engine2::getInst()->setDebug( !Lua::Engine2::getInst()->isDebug() );
}

void LuaIde::handleBreakAtFirst()
{
	CHECKED_IF( true, Lua::Engine2::getInst()->getDefaultCmd() == Lua::Engine2::RunToNextLine );
	if( Lua::Engine2::getInst()->getDefaultCmd() == Lua::Engine2::RunToNextLine )
		Lua::Engine2::getInst()->setDefaultCmd( Lua::Engine2::RunToBreakPoint );
	else
		Lua::Engine2::getInst()->setDefaultCmd( Lua::Engine2::RunToNextLine );
}

void LuaIde::handleBreakpoint()
{
	Lua::CodeEditor* e = currentEditor();
	ENABLED_IF( e != 0 && !e->objectName().isEmpty() );

	int line;
	e->getCursorPosition( &line );
	if( line == -1 )
		return;
	toggleBreakPoint( e, line );
}

void LuaIde::handleRemoveAllBreaks()
{
	Lua::CodeEditor* e = currentEditor();
	if( e == 0 )
		return;
	const QSet<int>& bps = e->getBreakPoints();
	ENABLED_IF( !bps.isEmpty() );
	foreach( int l, bps )
	{
		Lua::Engine2::getInst()->removeBreak( e->objectName().toLatin1(), l );
		e->removeBreakPoint( l );
	}
}

void LuaIde::handleSetFont()
{
	ENABLED_IF( true );

	bool ok;
	QFont f = Lua::CodeEditor::defaultFont();
	if( currentEditor() )
		f = currentEditor()->font();
	f = QFontDialog::getFont( &ok, f, this );
	if( !ok )
		return;
	QSettings set;
	set.setValue( "LuaEditor/Font", QVariant::fromValue(f) );
	set.sync();
	for( int i = 0; i < d_tab->count(); i++ )
		dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) )->setFont(f);
}

void LuaIde::handleCreate()
{
	ENABLED_IF( true );

	bool ok	= false;
	const QString name = QInputDialog::getText( this, tr("New Script"),
		tr("Please enter a unique name:"), QLineEdit::Normal, "", &ok );
	if( !ok )
		return;

	if( name.isEmpty() || !isUniqueName( name ) )
	{
		QMessageBox::critical( this, tr("New Script"),
			tr("The selected name was empty or not unique!"), QMessageBox::Cancel );
		return;
	}
	Sdb::Obj root = AppContext::inst()->getRoot();
	Sdb::Obj script = root.createAggregate( TypeLuaScript );
	script.setValue( AttrScriptName, Stream::DataCell().setString( name ) );
	script.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	AppContext::inst()->getTxn()->commit();
	_ScriptViewItem* item = new _ScriptViewItem( d_scriptList, script ); // TODO: steuern ber DbNotify?
	d_scriptList->onNotifyInsert();
	item->setCurrent();
	Lua::CodeEditor* editor = getOrCreateEditor( false, name, ":" + name );
	editor->setFocus();
}

void LuaIde::handleDuplicate()
{
	ENABLED_IF( d_scriptList->currentItem() );
	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );

	bool ok	= false;
	const QString name = QInputDialog::getText( this, tr("Duplicate Script"),
										 tr("Please enter a unique name:"), QLineEdit::Normal, svi->text(0), &ok );
	if( !ok )
		return;

	if( name.isEmpty() || !isUniqueName( name ) )
	{
		QMessageBox::critical( this, tr("Duplicate Script"),
			tr("The selected name was empty or not unique!"), QMessageBox::Cancel );
		return;
	}
	Sdb::Obj root = AppContext::inst()->getRoot();
	Sdb::Obj script = root.createAggregate( TypeLuaScript );
	script.setValue( AttrScriptName, Stream::DataCell().setString( name ) );
	const QString source = svi->d_script.getValue(AttrScriptSource).getStr();
	script.setValue( AttrScriptSource, Stream::DataCell().setString( source ) );
	script.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	AppContext::inst()->getTxn()->commit();
	_ScriptViewItem* item = new _ScriptViewItem( d_scriptList, script ); // TODO: steuern ber DbNotify?
	d_scriptList->onNotifyInsert();
	item->setCurrent();
	Lua::CodeEditor* editor = getOrCreateEditor( false, name, ":" + name, source );
	editor->setFocus();
}

void LuaIde::handleRename()
{
	ENABLED_IF( d_scriptList->currentItem() );

	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );
	bool ok	= false;
	const QString oldName = svi->text(0);
	const QString name = QInputDialog::getText( this, tr("Rename Script"),
										tr("Please enter a unique name:"), QLineEdit::Normal, oldName, &ok );
	if( !ok || oldName == name )
		return;
	if( name.isEmpty() || !isUniqueName( name) )
	{
		QMessageBox::critical( this, tr("Rename Script"),
			tr("The selected name was empty or not unique!"), QMessageBox::Cancel );
		return;
	}
	svi->d_script.setValue( AttrScriptName, Stream::DataCell().setString( name ) );

	int pos;
	Lua::CodeEditor* e = getEditor( ":" + oldName, &pos );
	if( e != 0 )
	{
		e->setName( name );
		e->setObjectName( ":" + name );
		if( e->document()->isModified() )
			d_tab->setTabText( pos, "*" + e->getName() );
		else
			d_tab->setTabText( pos, e->getName() );
	}
}

void LuaIde::handleDelete()
{
	ENABLED_IF( d_scriptList->currentItem() );
	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );

	if( QMessageBox::information( this, tr("Delete Script"),
			  tr("Do you really want to delete this script (cannot be undone)?"),
			  QMessageBox::Yes | QMessageBox::Cancel ) != QMessageBox::Yes )
		return;

	int pos;
	Lua::CodeEditor* e = getEditor( ":" + svi->d_script.getValue(AttrScriptName).getStr(), &pos );
	if( e != 0 )
	{
		d_tab->removeTab(pos);
		delete e;
	}
	svi->d_script.erase();
	svi->removeMe();
	AppContext::inst()->getTxn()->commit();
}

void LuaIde::handleShow()
{
	ENABLED_IF( d_scriptList->currentItem() );

	onItemDoubleClicked( d_scriptList->currentItem() );
}

void LuaIde::handleEdit()
{
	ENABLED_IF( d_scriptList->currentItem() );

	onItemDoubleClicked( d_scriptList->currentItem() );
	Lua::CodeEditor* e = currentEditor();
	if( e )
	{
		e->setReadOnly( false );
		d_tab->setTabIcon( d_tab->currentIndex(), QIcon(":/DoorScope/Images/stift.png" ) );
	}
}

void LuaIde::handleCheck2()
{
	ENABLED_IF( d_scriptList->currentItem() );

	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );
	if( Lua::Engine2::getInst()->pushFunction( svi->d_script.getValue(AttrScriptSource).getStr().toLatin1(),
									":" + svi->text(0).toLatin1() ) )
	{
		Lua::Engine2::getInst()->pop();
		QMessageBox::information( this, tr("Checking Syntax"), tr("No errors found.") );
	}else
		QMessageBox::critical( this, tr("Checking Syntax"), Lua::Engine2::getInst()->getLastError() );
}

void LuaIde::handleExecute2()
{
	ENABLED_IF( d_scriptList->currentItem() && !Lua::Engine2::getInst()->isExecuting() );
	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );

	if( Lua::Engine2::getInst()->executeCmd( svi->d_script.getValue(AttrScriptSource).getStr().toLatin1(),
											 ":" + svi->text(0).toLatin1() ) )
		; // svi->d_script->setCompiled();
	else if( !Lua::Engine2::getInst()->isSilent() )
		QMessageBox::critical( this, tr("Run Script"), Lua::Engine2::getInst()->getLastError() );
	else
		qWarning() << "LuaIde::handleExecute2: silently terminated:" << Lua::Engine2::getInst()->getLastError();
}

void LuaIde::handleImport()
{
	ENABLED_IF( true );
	const QString fileName = QFileDialog::getOpenFileName( this, tr("Import Lua Script"),
														   QString(), tr("Lua Script (*.lua)") );
	if( fileName.isEmpty() )
		return;

	QFileInfo info( fileName );

	bool ok	= false;
	const QString name = QInputDialog::getText( this, tr("Import Script"),
			tr("Please enter a unique name:"), QLineEdit::Normal, info.baseName(), &ok );
	if( !ok )
		return;

	QFile in( fileName );
	if( !in.open( QIODevice::ReadOnly ) )
	{
		QMessageBox::critical( this, tr("Open File"),
			tr("Cannot read from selected file!"), QMessageBox::Cancel );
		return;
	}
	QByteArray code = in.readAll();
	in.close();
	if( code.length() > 4 && code[ 0 ] == char(0x1b) && code[ 1 ] == 'L' &&
		code[ 2 ] == 'u' && code[ 3 ] == 'a' )
	{
		QMessageBox::critical( this, tr("Import Script"),
			tr("Cannot import binary Lua scripts!"), QMessageBox::Cancel );
		return;
	}
	if( name.isEmpty() || !isUniqueName(name) )
	{
		QMessageBox::critical( this, tr("Import Script"),
			tr("The selected name was not unique!"), QMessageBox::Cancel );
		return;
	}
	Sdb::Obj root = AppContext::inst()->getRoot();
	Sdb::Obj script = root.createAggregate( TypeLuaScript );
	script.setValue( AttrScriptName, Stream::DataCell().setString( name ) );
	script.setValue( AttrScriptSource, Stream::DataCell().setString( code ) );
	script.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	AppContext::inst()->getTxn()->commit();
	_ScriptViewItem* item = new _ScriptViewItem( d_scriptList, script ); // TODO: steuern ber DbNotify?
	d_scriptList->onNotifyInsert();
	item->setCurrent();
	Lua::CodeEditor* editor = getOrCreateEditor( false, name, ":" + name, code );
	editor->setFocus();
}

void LuaIde::handleImport2()
{
	ENABLED_IF( true );

	Sdb::Obj extRoot;
	std::auto_ptr<Sdb::Database> db;
	std::auto_ptr<Sdb::Transaction> txn;
	const QString path = QFileDialog::getOpenFileName( this, tr("Select Source Repository"), QString(), tr("*.dsdb") );
	if( path.isEmpty() )
		return;
	try
	{
		db.reset( new Sdb::Database() );
		db->open( path );
		txn.reset( new Sdb::Transaction( db.get() ) );
		extRoot = txn->getObject( QUuid( AppContext::s_rootUuid ) );
		if( extRoot.isNull() )
		{
			QMessageBox::critical( this, tr("Import from other Repository"),
								   tr("This seems to be an invalid DoorScope repository!") );
			return;
		}
	}catch( ... )
	{
		QMessageBox::critical( this, tr("Import from other Repository"), tr("Cannot import due to internal error!") );
		return;
	}
	if( extRoot.isNull() )
		return;
	ScriptSelectDlg dlg2( this );
	ScriptSelectDlg::Result res;
	Sdb::Obj root = AppContext::inst()->getRoot();
	if( dlg2.select( extRoot, res ) )
	{
		foreach( Sdb::OID id, res )
		{
			Sdb::Obj source = extRoot.getTxn()->getObject( id );
			Q_ASSERT( !source.isNull() );
			Sdb::Obj script = root.createAggregate( TypeLuaFilter );
			script.setValue( AttrScriptName, source.getValue(AttrScriptName) );
			script.setValue( AttrScriptSource, source.getValue(AttrScriptSource) );
			script.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
			AppContext::inst()->getTxn()->commit();
			_ScriptViewItem* item = new _ScriptViewItem( d_scriptList, script );
		}
		d_scriptList->onNotifyInsert();
	}
	extRoot = Sdb::Obj();
}

void LuaIde::handleOpenFile()
{
	ENABLED_IF( true );
	const QString fileName = QFileDialog::getOpenFileName(this, tr("Show File"), QString(), tr("Lua Script (*.lua)"));
	if( fileName.isNull() )
		return;

	QFileInfo info( fileName );
	Lua::CodeEditor* e = getOrCreateEditor(true, info.fileName(), "@" + fileName );
	e->loadFromFile( fileName, false );
	updateBreakpoints( e );
}

void LuaIde::handleRunFile()
{
	ENABLED_IF( !Lua::Engine2::getInst()->isExecuting() );
	const QString fileName = QFileDialog::getOpenFileName(this, tr("Run From File"), QString(),
		tr("Lua Script (*.lua)") );
	if( fileName.isNull() )
		return;

	QFileInfo info( fileName );

	if( !Lua::Engine2::getInst()->executeFile( fileName.toLatin1() ) )
	{
		if( !Lua::Engine2::getInst()->isSilent() )
			QMessageBox::critical( this, tr("Run From File"), Lua::Engine2::getInst()->getLastError() );
		else
			qWarning() << "ScriptView::handleRunFromFile: silently terminated:" << Lua::Engine2::getInst()->getLastError();
	}
}

void LuaIde::handleExport()
{
	ENABLED_IF( d_scriptList->currentItem() );
	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );

	QDir dir;
	QString fileName = dir.absoluteFilePath( svi->text(0) );
	fileName = QFileDialog::getSaveFileName( this, tr("Export Lua Script"), fileName,
		tr("Lua Script (*.lua)"), 0, QFileDialog::DontConfirmOverwrite );
	if( fileName.isNull() )
		return;

	QFileInfo info( fileName );

	if( info.suffix().toUpper() != "LUA" )
		fileName += ".lua";
	info.setFile( fileName );
	if( info.exists() )
	{
		if( QMessageBox::warning( this, tr("Save As"),
			tr("This file already exists. Do you want to overwrite it?"),
			QMessageBox::Ok | QMessageBox::Cancel ) != QMessageBox::Ok )
			return;
	}
	QFile out( fileName );
	if( !out.open( QIODevice::WriteOnly ) )
	{
		QMessageBox::critical( this, tr("Save As"),
			tr("Cannot write to selected file!"), QMessageBox::Cancel );
		return;
	}
	out.write( svi->d_script.getValue(AttrScriptSource).getStr().toLatin1() );
}

void LuaIde::handleExportBin()
{
	ENABLED_IF( d_scriptList->currentItem() );
	_ScriptViewItem* svi = static_cast<_ScriptViewItem*>( d_scriptList->currentItem() );

	QDir dir;
	QString fileName = dir.absoluteFilePath( svi->text(0) );
	fileName = QFileDialog::getSaveFileName( this, tr("Export Lua Binary"), fileName,
		tr("Lua Binary (*.lua)"), 0, QFileDialog::DontConfirmOverwrite );
	if( fileName.isNull() )
		return;

	QFileInfo info( fileName );

	if( info.suffix().toUpper() != "LUA" )
		fileName += ".lua";
	info.setFile( fileName );
	if( info.exists() )
	{
		if( QMessageBox::warning( this, tr("Save As"),
			tr("This file already exists. Do you want to overwrite it?"),
								  QMessageBox::Ok | QMessageBox::Cancel ) != QMessageBox::Ok )
			return;
	}

	if( !Lua::Engine2::getInst()->saveBinary( svi->d_script.getValue( AttrScriptSource ).getStr().toLatin1(),
											  fileName.toLatin1() ) )
		QMessageBox::critical( this, tr("Export Binary Script"), Lua::Engine2::getInst()->getLastError() );
}

void LuaIde::handleSave()
{
	Lua::CodeEditor* e = currentEditor();
	ENABLED_IF( e != 0 && e->document()->isModified() );

	saveData( e );
	AppContext::inst()->getTxn()->commit();
}
