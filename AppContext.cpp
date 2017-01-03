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

#include "AppContext.h"
#include "TypeDefs.h"
#include <Sdb/Exceptions.h>
#include <QApplication>
#include <QIcon>
#include <QPlastiqueStyle>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>
#include "Exceptions.h"
#include "LuaBinding.h"
#include <Qtl2/Objects.h>
#include <Qtl2/Variant.h>
#include <Script2/QtObject.h>
using namespace Ds;
using namespace Sdb;

static AppContext* s_inst;
const char* AppContext::s_company = "Dr. Rochus Keller";
const char* AppContext::s_domain = "rochus.keller@doorscope.ch";
const char* AppContext::s_appName = "DoorScope";
const char* AppContext::s_rootUuid = "{f5d71bcb-a4ae-42c6-a13d-600334590705}";
const QUuid AppContext::s_reqIfMappings( "{9B17B05E-EFB1-44af-9CE5-E369F097BFF7}" );
const char* AppContext::s_version = "1.2.1";
const char* AppContext::s_date = "2017-01-03";
const char* AppContext::s_about =
        "<html><body><p>Release: <strong>%1</strong>   Date: <strong>%2</strong> </p>"
		"<p>Purpose: DoorScope can be used in the specification review process.</p>"
		"<p>Author: Dr. Rochus Keller, <a href='http://doorscope.ch'>rochus.keller@doorscope.ch</a></p>"
		"<p>Copyright (c) 2005-2017</p>"
        "<h4>Additional Credits:</h4>"
        "<p>Qt GUI Toolkit 4.4 (c) 1995-2008 Trolltech AS, 2008 Nokia Corporation<br>"
        "Sqlite 3.5, <a href='http://sqlite.org/copyright.html'>dedicated to the public domain by the authors</a><br>"
        "DoorScope icon by courtesy of <a href='http://www.flickr.com/photos/koalaloha/122272335/'>koalaloha</a><br>"
        "<a href='http://code.google.com/p/fugue-icons-src/'>Fugue Icons</a>  2012 by Yusuke Kamiyamane<br>"
        "Lua 5.1 by R. Ierusalimschy, L. H. de Figueiredo & W. Celes (c) 1994-2006 Tecgraf, PUC-Rio<p>"
        "<h4>Terms of use:</h4>"
		"<p>This version of DoorScope is freeware, i.e. it can be used for free by anyone. "
		"The software and documentation are provided \"as is\", without warranty of any kind, "
		"expressed or implied, including but not limited to the warranties of merchantability, "
		"fitness for a particular purpose and noninfringement. In no event shall the author or "
        "copyright holders be "
		"liable for any claim, damages or other liability, whether in an action of contract, tort "
        "or otherwise, "
		"arising from, out of or in connection with the software or the use or other dealings in "
        "the software.</p></body></html>";


AppContext::AppContext(QObject *parent)
	: QObject(parent),d_db(0),d_txn(0)
{
	s_inst = this;
	qApp->setOrganizationName( s_company );
	qApp->setOrganizationDomain( s_domain );
	qApp->setApplicationName( s_appName );

	QIcon icon;
	icon.addFile( ":/DoorScope/Images/Scope16.png" );
	icon.addFile( ":/DoorScope/Images/Scope32.png" );
	icon.addFile( ":/DoorScope/Images/Scope48.png" );
	icon.addFile( ":/DoorScope/Images/Scope64.png" );
	icon.addFile( ":/DoorScope/Images/Scope128.png" );
	qApp->setWindowIcon( icon );

	qApp->setStyle( new QPlastiqueStyle() );

	d_set = new QSettings( s_appName, s_appName, this );

	d_styles = new Txt::Styles( this );
	Txt::Styles::setInst( d_styles );
	if( d_set->contains( "DocViewer/Font" ) )
	{
		QFont f = d_set->value( "DocViewer/Font" ).value<QFont>();
		d_styles->setFontStyle( f.family(), f.pointSize() );
	}

	Lua::Engine2::setInst( new Lua::Engine2() );
	Lua::Engine2::getInst()->addStdLibs();
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::PACKAGE );
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::OS );
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::IO );
	Lua::QtObjectBase::install( Lua::Engine2::getInst()->getCtx() );
	Qtl::Variant::install( Lua::Engine2::getInst()->getCtx() );
	Qtl::Objects::install( Lua::Engine2::getInst()->getCtx() );
	LuaBinding::install( Lua::Engine2::getInst()->getCtx() );
}

AppContext::~AppContext()
{
	Lua::Engine2::setInst(0);
	d_root = Obj();
	if( d_txn )
		delete d_txn;
	if( d_db )
		delete d_db;
	s_inst = 0;
}

void AppContext::setDocFont( const QFont& f )
{
	d_set->setValue( "DocViewer/Font", QVariant::fromValue( f ) );
	d_styles->setFontStyle( f.family(), f.pointSize() );
}

AppContext* AppContext::inst()
{
	return s_inst;
}

bool AppContext::open( QString path )
{
	if( !path.toLower().endsWith( QLatin1String( ".dsdb" ) ) )
		path += QLatin1String( ".dsdb" );
	d_db = new Database( this );
	try
	{
		d_db->open( path );
		d_txn = new Transaction( d_db, this );
		TypeDefs::init( *d_db );
		d_root = d_txn->getObject( QUuid( s_rootUuid ) );
		if( d_root.isNull() )
			d_root = d_txn->createObject(QUuid( s_rootUuid ));
		d_txn->commit();
		LuaBinding::setRoot( Lua::Engine2::getInst()->getCtx(), d_root );
		return true;
	}catch( DatabaseException& e )
	{
		QMessageBox::critical( 0, tr("Create/Open Repository"),
			tr("Error <%1>: %2").arg( e.getCodeString() ).arg( e.getMsg() ) );
		return false;	
	}
}

Sdb::Database* AppContext::getDb() const
{
	return d_txn->getDb();
}

QString AppContext::getUserName() const
{
	if( d_user.isNull() )
	{
		QVariant v = d_set->value( "Reviewer" );
		if( !v.isNull() )
			d_user = v.toString();
	}
	return d_user;
}

void AppContext::setUserName( const QString& u )
{
	d_user = u;
	if( !u.isEmpty() )
	{
		d_set->setValue( "Reviewer", u );
	}
}

QString AppContext::askUserName( QWidget* p, bool override )
{
	QString user = getUserName();
	if( user.isNull() || override )
	{
		bool ok;
		user = QInputDialog::getText( p, tr("Reviewer Name"),
			tr("Please enter your name:"), QLineEdit::Normal,
			user, &ok );
		if( ok )
			setUserName( user );
		else if( !override )
			setUserName( "" );
	}
	return user;
}

QString AppContext::getIndexPath() const
{
	if( d_db == 0 )
		return QDir::current().absoluteFilePath( QLatin1String( "DoorScope.index" ) );
	QFileInfo info( d_db->getFilePath() );
	return info.absoluteDir().absoluteFilePath( info.completeBaseName() + QLatin1String( ".index" ) );
}
