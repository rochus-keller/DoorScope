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

#include "MainFrame.h"
#include "AppContext.h"
#include <Gui2/AutoMenu.h>
#include <QMenuBar>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QResizeEvent>
#include <Gui2/AutoShortcut.h>
#include "DirViewer.h"
using namespace Ds;
using namespace Gui2;

MainFrame::MainFrame(QWidget *parent)
	: QMainWindow(parent)
{
	setAttribute( Qt::WA_QuitOnClose );

	d_dir = new DirViewer( this );
	setCentralWidget( d_dir );

	new Gui2::AutoShortcut( tr("CTRL+Q"), this, qApp, SLOT( quit() ) );

	setCaption();
	show();

	QVariant state = AppContext::inst()->getSet()->value( "MainFrame/State" );
	if( !state.isNull() )
		restoreState( state.toByteArray() );
	QSize s = AppContext::inst()->getSet()->value("MainFrame/Size" ).toSize();
	if( s.isValid() )
		resize( s);
}

MainFrame::~MainFrame()
{

}

void MainFrame::setCaption()
{
	QFileInfo info( AppContext::inst()->getDb()->getFilePath() );
	setWindowTitle( tr("%1 - DoorScope").arg( info.baseName() ) );
}

void MainFrame::closeEvent ( QCloseEvent * event )
{
	AppContext::inst()->getSet()->setValue("MainFrame/State", saveState() );
	QMainWindow::closeEvent( event );
	if( event->isAccepted() )
		qApp->quit();
}

void MainFrame::resizeEvent ( QResizeEvent * e )
{
	QMainWindow::resizeEvent( e );
	if( e->spontaneous() )
		AppContext::inst()->getSet()->setValue("MainFrame/Size", e->size() );
}

void MainFrame::openDatabase()
{
	ENABLED_IF( true );

	QString path = QFileDialog::getOpenFileName( this, tr("Open Repository"), 
		QString(), "*.dsdb", 0 );
	if( !path.isNull() )
		AppContext::inst()->open( path );
	setCaption();
}

void MainFrame::newDatabase()
{
	ENABLED_IF( true );

	QString path = QFileDialog::getSaveFileName( this, tr("New Repository"), 
		QString(), "*.dsdb", 0 );
	if( !path.isNull() )
		AppContext::inst()->open( path );
	setCaption();
}
