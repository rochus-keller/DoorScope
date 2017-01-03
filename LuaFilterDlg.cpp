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

#include "LuaFilterDlg.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <Script/CodeEditor.h>
#include <Gui2/AutoMenu.h>
#include "AppContext.h"
#include "TypeDefs.h"
using namespace Ds;

LuaFilterDlg::LuaFilterDlg(QWidget *parent) :
	QDialog(parent)
{
	QVBoxLayout* vbox = new QVBoxLayout( this );
	QHBoxLayout* hbox = new QHBoxLayout();
	hbox->setMargin(0);
	vbox->addLayout( hbox );

	hbox->addWidget( new QLabel( tr("Name:"), this ) );

	d_title = new QLineEdit( this );
	hbox->addWidget( d_title );

	QLabel* l = new QLabel( tr("The following Lua code is called for each content object of the document. "
							   "If the function returns 'false' the content object is excluded "
							   "from the result set." ), this );
	l->setWordWrap(true);
	vbox->addWidget( l );
	d_code = new Lua::CodeEditor( this );
	d_code->setShowNumbers(true);
	d_code->setMinimumHeight(30);
	QSettings set;
	d_code->setFont( set.value( "LuaEditor/Font", QVariant::fromValue( Lua::CodeEditor::defaultFont() ) ).value<QFont>() );
	vbox->addWidget( d_code );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_code, true );
	pop->addCommand( "Check Syntax", this, SLOT(handleCheck()) );
	pop->addSeparator();
	pop->addCommand( "Undo", d_code, SLOT(handleEditUndo()), tr("CTRL+Z"), true );
	pop->addCommand( "Redo", d_code, SLOT(handleEditRedo()), tr("CTRL+Y"), true );
	pop->addSeparator();
	pop->addCommand( "Cut", d_code, SLOT(handleEditCut()), tr("CTRL+X"), true );
	pop->addCommand( "Copy", d_code, SLOT(handleEditCopy()), tr("CTRL+C"), true );
	pop->addCommand( "Paste", d_code, SLOT(handleEditPaste()), tr("CTRL+V"), true );
	pop->addCommand( "Select all", d_code, SLOT(handleEditSelectAll()), tr("CTRL+A"), true  );
	pop->addSeparator();
	pop->addCommand( "Find...", d_code, SLOT(handleFind()), tr("CTRL+F"), true );
	pop->addCommand( "Find again", d_code, SLOT(handleFindAgain()), tr("F3"), true );
	pop->addCommand( "Replace...", d_code, SLOT(handleReplace()), tr("CTRL+R"), true );
	pop->addCommand( "&Goto...", d_code, SLOT(handleGoto()), tr("CTRL+G"), true );
	pop->addSeparator();
	pop->addCommand( "Indent", d_code, SLOT(handleIndent()) );
	pop->addCommand( "Unindent", d_code, SLOT(handleUnindent()) );
	pop->addCommand( "Set Indentation Level...", d_code, SLOT(handleSetIndent()) );

	QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	vbox->addWidget( bb );
	connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

	d_title->setFocus();

	setMinimumSize( 600, 600 );
}

bool LuaFilterDlg::edit(QString &title, QString &code)
{
	d_title->setText( title );
	d_code->setText( code );
	while( exec() == QDialog::Accepted )
	{
		// Check
		if( d_title->text().isEmpty() )
		{
			QMessageBox::critical( this, windowTitle(), tr("The name of the filter must not be empty!") );
			continue;
		}
		if( !checkSyntax() )
			continue;
		title = d_title->text();
		code = d_code->text();
		return true;
	}
	return false;
}

void LuaFilterDlg::handleCheck()
{
	ENABLED_IF( !d_code->getText().isEmpty() );
	if( checkSyntax() )
		QMessageBox::information( this, tr("Checking Syntax"), tr("No errors found!") );
}

bool LuaFilterDlg::checkSyntax()
{
	if( Lua::Engine2::getInst()->pushFunction( d_code->text().toLatin1(), "Lua Filter" ) )
	{
		Lua::Engine2::getInst()->pop();
		return true;
	}else
	{
		QMessageBox::critical( this, tr("Syntax Error"), Lua::Engine2::getInst()->getLastError() );
		return false;
	}
}

