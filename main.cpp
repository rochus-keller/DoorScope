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

#include <QtGui/QApplication>
#include <QScrollArea>
#include <QSplashScreen>
#include <Sdb/Transaction.h>
#include <Sdb/Database.h>
#include <Sdb/Idx.h>
#include <Sdb/Exceptions.h>
#include <Sdb/BtreeCursor.h>
#include <Sdb/BtreeStore.h>
#include <QBuffer>
#include <QtDebug>
#include <QFile>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QtPlugin>
#include "AppContext.h"
#include "MainFrame.h"
#include "TypeDefs.h"
using namespace Stream;
using namespace Ds;
 
Q_IMPORT_PLUGIN(qgif)
Q_IMPORT_PLUGIN(qjpeg)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifndef _DEBUG
	try
	{
#endif
		AppContext ctx;
		QString path;

		if( ctx.getSet()->contains( "App/Font" ) )
		{
			QFont f1 = QApplication::font();
			QFont f2 = ctx.getSet()->value( "App/Font" ).value<QFont>();
			f1.setFamily( f2.family() );
			f1.setPointSize( f2.pointSize() );
			QApplication::setFont( f1 );
		}
		
		QStringList args = QCoreApplication::arguments();
		for( int i = 1; i < args.size(); i++ ) // arg 0 enthält Anwendungspfad
		{
			if( !args[ i ].startsWith( '-' ) )
			{
				path = args[ i ];
				break;
			}
		}
		
		if( path.isEmpty() )
		{
			QFileDialog dlg( 0 );
			dlg.setWindowTitle( AppContext::tr("Create/Open Repository - DoorScope") );
			dlg.setFileMode( QFileDialog::AnyFile );
			dlg.setAcceptMode( QFileDialog::AcceptSave );
			dlg.setConfirmOverwrite( false );
			dlg.setLabelText( QFileDialog::Accept, AppContext::tr( "&Open" ) );
			dlg.setNameFilter( "*.dsdb" );
			dlg.setReadOnly( true );
			dlg.setViewMode( QFileDialog::List );
			dlg.setDefaultSuffix( "dsdb" );
			if( dlg.exec() )
			{
				QStringList l = dlg.selectedFiles();
				if( !l.isEmpty() )
					path = l.first();
			}
		}
		if( path.isEmpty() )
			return 0;
		if( !ctx.open( path ) )
			return -1;

		MainFrame mf;
		const int res = app.exec();
		//ctx.getTxn()->getDb()->dump();
		return res;
#ifndef _DEBUG
	}catch( Sdb::DatabaseException& e )
	{
		QMessageBox::critical( 0, MainFrame::tr("DoorSope Error"),
			QString("Database Error: [%1] %2").arg( e.getCodeString() ).arg( e.getMsg() ), QMessageBox::Abort );
		return -1;
	}catch( std::exception& e )
	{
		QMessageBox::critical( 0, MainFrame::tr("DoorSope Error"),
			QString("Generic Error: %1").arg( e.what() ), QMessageBox::Abort );
		return -1;
	}catch( ... )
	{
		QMessageBox::critical( 0, MainFrame::tr("DoorSope Error"),
			QString("Generic Error: unexpected internal exception"), QMessageBox::Abort );
		return -1;
	}
#endif
	return 0;
}
