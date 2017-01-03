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

#include "DirViewer.h"
#include "AppContext.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include "DocViewer.h"
#include "DocExporter.h"
#include "DocSelectorDlg.h"
#include "AttrSelectDlg.h"
#include "DocImporter.h"
#include <Gui2/AutoMenu.h>
#include <Gui2/AutoShortcut.h>
#include <Txt/Styles.h>
#include <QFileIconProvider>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMimeData>
#include <QFontDialog>
#include <QSettings>
#include <QInputDialog>
#include <QClipboard>
#include <QtDebug>
#include <QProgressDialog>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Script/Terminal2.h>
#include "Indexer.h"
#include "SearchView.h"
#include "ReqIfImport.h"
#include "LuaIde.h"
using namespace Stream;
using namespace Ds;
using namespace Sdb;

enum Type { FOLDER, DOC };
enum Role { _OID = Qt::UserRole + 1, DT }; 

/*
http://lists.trolltech.com/qt-interest/2007-06/thread00045-0.html
class TreeWidgetItem : public QTreeWidgetItem {
public:
   TreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent){}
private:
   bool operator<(const QTreeWidgetItem &other)const {
       int column = treeWidget()->sortColumn();
       return text(column).toLower() < other.text(column).toLower();
    }
};
*/

DirViewer::DirViewer(QWidget *parent)
	: QTreeWidget(parent), d_loadLock( false )
{
	setHeaderLabels( QStringList() << tr("Name") << tr("Version") << tr("Status") << 
		tr("Status Date") << tr("Import Date") << tr("Doc. ID") );
	
	setSortingEnabled(true);
	setUniformRowHeights(false);
	setWordWrap(true);
	setAlternatingRowColors(true);
	setSelectionMode( QAbstractItemView::ExtendedSelection );
	setEditTriggers( QAbstractItemView::SelectedClicked );
	setAllColumnsShowFocus(false);
	setDragDropMode( QAbstractItemView::DragDrop ); // vorher InternalMove );
	setDragEnabled( true );
	setDropIndicatorShown( true );

	connect( AppContext::inst()->getDb(), SIGNAL(notify( Sdb::UpdateInfo )),
		this, SLOT(onDbUpdate( Sdb::UpdateInfo )), Qt::QueuedConnection );
	connect( this, SIGNAL(itemChanged( QTreeWidgetItem *, int )),
		this, SLOT( onItemChanged( QTreeWidgetItem *, int ) ) );
	connect( this, SIGNAL( itemExpanded( QTreeWidgetItem * )),
		this, SLOT( onItemExpanded( QTreeWidgetItem * ) ) );
	connect( this, SIGNAL( itemCollapsed( QTreeWidgetItem * ) ), 
		this, SLOT( onItemCollapsed( QTreeWidgetItem * ) ) );
	connect( this, SIGNAL( itemDoubleClicked( QTreeWidgetItem *, int ) ),
		this, SLOT( onItemDoubleClicked( QTreeWidgetItem *, int ) ) );

	// Wegen QT_BUG
	connect( this, SIGNAL(  sigExpandItem ( const QTreeWidgetItem * ) ), 
		this, SLOT( expandItem( const QTreeWidgetItem * ) ), Qt::QueuedConnection );

	Gui2::AutoMenu* m = new Gui2::AutoMenu( this, true );
    m->addCommand( tr("Test..."), this, SLOT(onTest()) );
    m->addCommand( tr("Open Document..."), this, SLOT(onOpenDoc()), tr("CTRL+O"), true );
    m->addCommand( tr("Search..."), this, SLOT(onSearch()), tr("CTRL+F"), true );
	m->addSeparator();
	Gui2::AutoMenu* m2 = new Gui2::AutoMenu( tr("Export HTML Report"), m );
	m2->addCommand( tr("Recursive..." ), this, SLOT(onExpHtml()) );
	m2->addCommand( tr("Linear..." ), this, SLOT(onExpHtml2()) );
	m2->addCommand( tr("Document..." ), this, SLOT(onExpHtml3()) );
	m2->addCommand( tr("Annot. Document..." ), this, SLOT(onExpHtml4()) );
	m2->addCommand( tr("Annot. Document incl. Attributes..." ), this, SLOT(onExpHtml5()) );
	m->addMenu( m2 );
	m2 = new Gui2::AutoMenu( tr("Export Data"), m );
	m2->addCommand( tr("Attribute Table..." ), this, SLOT(onExpCsv()) );
	m2->addCommand( tr("Attribute Table (UTF-8)..." ), this, SLOT(onExpCsvUtf8()) );
	m2->addCommand( tr("Annotation Table (outlined)..." ), this, SLOT(onExpAnnotCsv()) );
	m2->addCommand( tr("Annotation Table (linear)..." ), this, SLOT(onExpAnnotCsv2()) );
	m2->addCommand( tr("Annotations..." ), this, SLOT(onExpAnnot()) );
	m->addMenu( m2 );
	m->addSeparator();
	m->addCommand( tr("New Folder"), this, SLOT(onNewFolder()) );
    m->addCommand( tr("Paste HTML Document"), this, SLOT(onPasteHtmlDoc()) );
	m->addCommand( tr("Import Documents..."), this, SLOT(onImportDoc()) );
	m->addCommand( tr("Import Annotations..."), this, SLOT(onImportAnnot()) );
	m->addSeparator();
	m->addCommand( tr("Reset History..."), this, SLOT(onCreateHistory()) );
    m->addCommand( tr("Delete Object..."), this, SLOT(onDeleteObject()), tr("Del"), true );
	m->addCommand( tr("Delete Annotations..."), this, SLOT(onDeleteAnnot()) );
	m->addCommand( tr("Rebuild Index..."), this, SLOT(onReindex()) );
#ifdef _DEBUG
	//m->addCommand( tr("&Dump Repository"), SLOT(dumpDatabase() ) );
#endif
	m->addSeparator();
	Gui2::AutoMenu* m3 = new Gui2::AutoMenu( tr("Set Font"), m );
	m3->addCommand( tr("Document..."), this, SLOT( onSetDocFont() ) );
	m3->addCommand( tr("Application..."), this, SLOT( onSetAppFont() ) );
	m->addMenu( m3 );
	m->addCommand( tr("Show Lua IDE..."), this, SLOT( onOpenIde() ), tr("CTRL+L" ), true );
    m->addCommand( tr("About DoorScope..."), this, SLOT(onAbout() ) );
    m->addSeparator();
	m->addAction( tr("&Quit"), qApp, SLOT(quit()), tr("CTRL+Q") );

	loadAll();
}

DirViewer::~DirViewer()
{

}

void DirViewer::onDbUpdate( Sdb::UpdateInfo i )
{
	if( i.d_kind == Sdb::UpdateInfo::DbClosing )
	{
		clear(); // TODO
	}
}

void DirViewer::onDumpDatabase()
{
	ENABLED_IF( true );

	//AppContext::inst()->getDb()->dump();
	for( int i = 0; i <  topLevelItemCount (); i++ )
		expandItem( topLevelItem(i) );
}

void DirViewer::loadAll()
{
	clear();
	d_map.clear();
	Obj root = AppContext::inst()->getRoot();
	d_loadLock = true;
	loadChildren( 0, root );
	d_loadLock = false;

	// Muss asynchron nach setExpanded aufgerufen werden wegen QT_BUG
	QMetaObject::invokeMethod( this, "adjustColumns", Qt::QueuedConnection );
}

void DirViewer::adjustColumns()
{
	resizeColumnToContents ( 0 );
	resizeColumnToContents ( 1 );
	resizeColumnToContents ( 2 );
	resizeColumnToContents ( 3 );
	resizeColumnToContents ( 4 );
    sortItems( 0, Qt::AscendingOrder );
}

void DirViewer::onOpenIde()
{
    ENABLED_IF( true );

	LuaIde* ide = LuaIde::inst();
	ide->bringToFront();
}

void DirViewer::onTest()
{
    ENABLED_IF( true );
    importDocs( 0, QStringList() << "reqif/example.reqif" );
}

void DirViewer::importDocs( QTreeWidgetItem* parentItem, const QStringList &paths)
{
    QApplication::processEvents();
	QStringList::const_iterator it = paths.begin();
	QProgressDialog dlg( this );
	dlg.setWindowTitle( tr("Import Documents") );
	dlg.setWindowModality(Qt::WindowModal);
	dlg.setMaximum( paths.size() );
	dlg.setMinimumDuration( 0 );
	dlg.setValue( 0 );
	QApplication::processEvents();
	QList<Sdb::Obj> docs;
	{
		Database::Lock lock( AppContext::inst()->getDb(), true );
		Obj parent;
		if( parentItem )
		{
			parent = AppContext::inst()->getTxn()->getObject( parentItem->data(0,_OID).toULongLong() );
			parent.setValue( AttrFldrExpanded, Stream::DataCell().setBool(true) );
		}else
			parent = AppContext::inst()->getRoot();
		while( it != paths.end() )
		{
			QFileInfo info( *it );
			dlg.setLabelText( tr("Importing %1" ).arg( info.fileName() ) );
			dlg.setValue( dlg.value() + 1 );
			d_lastPath = info.absolutePath();
			QApplication::setOverrideCursor( Qt::WaitCursor );
            QApplication::processEvents();
			QList<Sdb::Obj> res;
			QString error;
            const QString suff = info.suffix().toLower();
            if( suff == "dsdx" || suff == "stream" )
			{
				DocManager dm;
                Sdb::Obj d = dm.importStream( *it );
                if( !d.isNull() )
                    res.append( d );
				error = dm.getError();
            }else if( suff == "html" || suff == "htm" )
			{
				DocImporter dm;
                Sdb::Obj d = dm.importHtmlFile( *it );
                if( !d.isNull() )
                    res.append( d );
				error = dm.getError();
            }else if( suff == "reqif")
            {
				ReqIfImport r;
                res = r.importFile( *it, this ); // fr jedes File ein Dialog
                error = r.getError();
            }
			if( res.isEmpty() && !error.isEmpty() )
			{
				dlg.cancel();
                QApplication::processEvents();
				AppContext::inst()->getTxn()->rollback();
				lock.rollback();
				QApplication::restoreOverrideCursor();
                QMessageBox::critical( this, tr("Error importing document" ), error );
				return;
			}
            docs += res;
            foreach( Sdb::Obj o, res )
                o.aggregateTo( parent );
			AppContext::inst()->getTxn()->commit();

			QApplication::restoreOverrideCursor();
			QApplication::processEvents();
			if( dlg.wasCanceled() )
			{
				AppContext::inst()->getTxn()->rollback();
				lock.rollback();
				return;
			}
			++it;
		}
		lock.commit();
	}
	QTreeWidgetItem* item = 0;
	if( parentItem )
		item = parentItem;
	for( int i = 0; i < docs.size(); i++ )
	{
		QTreeWidgetItem* sub = 0;
		if( item )
		{
			// QT-BUG: wenn man das zweite Child in ein Item einfgt, crasht Qt
			if( item->childCount() == 1 )
			{
				;
			}else
				sub = new QTreeWidgetItem( item, DOC );
			item->setExpanded(true);
		}else
			sub = new QTreeWidgetItem( this, DOC );
		if( sub )
		{
			setCurrentItem( sub );
			loadDoc( sub, docs[i] );
		}else
		{
			loadAll();
			return;
		}
	}
}

void DirViewer::createChild( QTreeWidgetItem* parent, const Sdb::Obj& o )
{
	QTreeWidgetItem* sub;
	if( o.getType() == TypeFolder )
	{
		if( parent )
			sub = new QTreeWidgetItem( parent, FOLDER );
		else 
			sub = new QTreeWidgetItem( this, FOLDER );
		loadFolder( sub, o );
		loadChildren( sub, o );
		//d_map[o.getId()] = sub;
	}else if( o.getType() == TypeDocument )
	{
		if( parent )
			sub = new QTreeWidgetItem( parent, DOC );
		else 
			sub = new QTreeWidgetItem( this, DOC );
		loadDoc( sub, o );
		loadChildren( sub, o );
		//d_map[o.getId()] = sub;
	}
}

void DirViewer::loadChildren( QTreeWidgetItem* i, const Sdb::Obj& p )
{
	Obj o = p.getFirstObj();
	if( !o.isNull() ) do
	{
		createChild( i, o );
	}while( o.next() );
}

void DirViewer::loadFolder( QTreeWidgetItem* i, const Sdb::Obj& o )
{
	d_loadLock = true;
	i->setText( 0, o.getValue( AttrFldrName ).toPrettyString() );
	i->setData( 0, _OID, o.getId() );
	i->setFlags( Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled |
		Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled );
	i->setIcon( 0, QIcon(":/DoorScope/Images/folder.png") );
	if( o.getValue( AttrFldrExpanded ).getBool() )
	{
		//qDebug() << "Expanded" << o.getValue( AttrFldrName ).toPrettyString(); // TEST
		// Funktioniert nicht mehr ab Qt 4.4 i->setExpanded( true );  
		// Funktioniert nicht QMetaObject::invokeMethod( i->treeWidget(), "expandItem", Qt::QueuedConnection, Q_ARG(QTreeWidgetItem*, i ) );
		// Funktioniert nicht i->treeWidget()->setItemExpanded( i, true );
		// wenn man trotzdem aufruft, werden zufllige Elemente expandiert
#if QT_VERSION < 0x040400
		i->setExpanded( true );
#else
		sigExpandItem( i );
#endif
	}
	d_loadLock = false;
}

void DirViewer::loadDoc( QTreeWidgetItem* i, const Sdb::Obj& o )
{
	d_loadLock = true;
	i->setIcon( 0, QIcon(":/DoorScope/Images/document.png") );
	i->setData( 0, _OID, o.getId() );
	i->setText( 0, TypeDefs::formatDocName( o, false ) );
	i->setText( 1, o.getValue( AttrDocVer ).toString(true) );
	i->setToolTip( 1, o.getValue( AttrDocName ).getStr() ); // vertrgt auch HTML
	i->setText( 2, (o.getValue( AttrDocBaseline ).getBool())?"Baseline":"Current" );
	i->setFlags( Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled );
	// TODO: Nur Name sollte editierbar sein!
	QDateTime dt = o.getValue( AttrModifiedOn ).getDateTime();
	if( dt.isValid() ) // DisplayRole
	{
		if( dt.time().hour() == 0 && dt.time().minute() == 0 && dt.time().second() == 0 )
			i->setData( 3, Qt::DisplayRole, dt.date() );
		else
			i->setData( 3, Qt::DisplayRole, dt );
	}
	dt = o.getValue( AttrDocImported ).getDateTime();
	if( dt.isValid() )
		i->setData( 4, Qt::DisplayRole, dt );
	i->setText( 5, o.getValue( AttrDocId ).toString(true) );
	d_loadLock = false;
}

void DirViewer::onItemChanged( QTreeWidgetItem * item, int column )
{
	if( column == 0 && !d_loadLock )
	{
		Obj o = AppContext::inst()->getTxn()->getObject( item->data(0,_OID).toULongLong() );
		if( !o.isNull() )
		{
			if( item->type() == FOLDER )
				o.setValue( AttrFldrName, Stream::DataCell().setString( item->text(0) ) );
			else if( item->type() == DOC )
				o.setValue( AttrDocAltName, Stream::DataCell().setString( item->text(0) ) );
			AppContext::inst()->getTxn()->commit();
		}
	}
}

void DirViewer::onItemExpanded( QTreeWidgetItem * item )
{
	if( item->type() == FOLDER && !d_loadLock )
	{
		Obj o = AppContext::inst()->getTxn()->getObject( item->data(0,_OID).toULongLong() );
		if( !o.isNull() )
		{
			o.setValue( AttrFldrExpanded, Stream::DataCell().setBool( true ) );
			// qDebug() << "setExpanded" << o.getValue( AttrFldrName ).toPrettyString(); // TEST
			AppContext::inst()->getTxn()->commit();
		}
	}
}

void DirViewer::onItemCollapsed( QTreeWidgetItem * item )
{
	if( item->type() == FOLDER )
	{
		Obj o = AppContext::inst()->getTxn()->getObject( item->data(0,_OID).toULongLong() );
		if( !o.isNull() )
		{
			o.setValue( AttrFldrExpanded, Stream::DataCell().setBool( false ) );
			// qDebug() << "setCollapsed" << o.getValue( AttrFldrName ).toPrettyString(); // TEST
			AppContext::inst()->getTxn()->commit();
		}
	}
}

void DirViewer::onItemDoubleClicked( QTreeWidgetItem * item, int column )
{
	open( item );
}

void DirViewer::open( QTreeWidgetItem* item )
{
	if( item->type() == DOC )
	{
		Obj o = AppContext::inst()->getTxn()->getObject( item->data(0,_OID).toULongLong() );
		DocViewer::showDoc(o);
	}
}

void DirViewer::onOpenDoc()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );
	open( i );
}

void DirViewer::onNewFolder()
{
	QList<QTreeWidgetItem *> sel = selectedItems();
	ENABLED_IF( sel.isEmpty() || ( sel.size() == 1 && sel[0]->type() == FOLDER ) );

	Sdb::Transaction* txn = AppContext::inst()->getTxn();
	Obj folder = txn->createObject( TypeFolder );
	folder.setValue( AttrFldrName, Stream::DataCell().setString( tr("New Folder") ) );

	Obj parent;
	if( !sel.isEmpty() )
	{
		parent = txn->getObject( sel[0]->data(0,_OID).toULongLong() );
		parent.setValue( AttrFldrExpanded, Stream::DataCell().setBool(true) );
	}else
		parent = AppContext::inst()->getRoot();

	folder.aggregateTo( parent );
	txn->commit();

	QTreeWidgetItem* sub;
	if( !sel.isEmpty() )
	{
		// QT-BUG: wenn man das zweite Child in ein Item einfgt, crasht Qt
		if( sel[0]->childCount() == 1 )
		{
			loadAll();
			return;
		}else
			sub = new QTreeWidgetItem( sel[0], FOLDER );
		sel[0]->setExpanded(true);
	}else 
		sub = new QTreeWidgetItem( this, FOLDER );
	setCurrentItem( sub );
	loadFolder( sub, folder );
	//d_map[folder.getId()] = sub;
}

static void _closeAll( QTreeWidgetItem* item )
{
	if( item && item->type() == DOC )
		DocViewer::closeAll( AppContext::inst()->getTxn()->getObject( item->data(0,_OID).toULongLong() ) );
	for( int i = 0; i < item->childCount(); i++ )
		_closeAll( item->child( i ) );
}

void DirViewer::onDeleteObject()
{
	QList<QTreeWidgetItem *> sel = selectedItems();
	ENABLED_IF( sel.size() == 1 );

	if( QMessageBox::warning( this, tr("Deleting Object"),
		tr("Are you sure you want to permanently delete this object?\n"
		"This operation cannot be undone!"),
		QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) == QMessageBox::Cancel )
		return;

	_closeAll( sel[0] );
	{
		Obj o = AppContext::inst()->getTxn()->getObject( sel[0]->data(0,_OID).toULongLong() );
		DocManager dm;
		if( dm.deleteObj( o ) )
		{
			delete sel[0];
			AppContext::inst()->getTxn()->commit();
		}else
		{
			AppContext::inst()->getTxn()->rollback();
			QMessageBox::critical( this, tr("Error Deleting Object"),
				dm.getError() );
		}
	}
}

void DirViewer::onDeleteAnnot()
{
	QList<QTreeWidgetItem *> sel = selectedItems();
	ENABLED_IF( sel.isEmpty() || ( sel.size() == 1 && sel[0]->type() == DOC ) );

	if( QMessageBox::warning( this, tr("Deleting Annotations"),
		tr("Are you sure you want to permanently delete the annotations of this document?\n"
		"This operation cannot be undone!"),
		QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) == QMessageBox::Cancel )
		return;

	bool reset = true;
	switch( QMessageBox::warning( this, tr("Deleting Annotations"),
		tr("Do you also want to reset the review status of each object?"),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes ) )
	{
	case QMessageBox::Cancel:
		return;
	case QMessageBox::No:
		reset = false;
		break;
	default:
		break;
	}

	Obj o = AppContext::inst()->getTxn()->getObject( sel[0]->data(0,_OID).toULongLong() );
	DocManager dm;
	if( dm.deleteAnnots( o, reset ) )
	{
		AppContext::inst()->getTxn()->commit();
	}else
	{
		AppContext::inst()->getTxn()->rollback();
		QMessageBox::critical( this, tr("Error Deleting Annotations"),dm.getError() );
	}
}

void DirViewer::onImportDoc()
{
	QList<QTreeWidgetItem *> sel = selectedItems();
	ENABLED_IF( sel.isEmpty() || ( sel.size() == 1 && sel[0]->type() == FOLDER ) );

	QString filter;
	QStringList paths = QFileDialog::getOpenFileNames( this, tr("Import Documents"), d_lastPath,
		"All supported files (*.dsdx *.html *.reqif);;(DoorScope streams (*.stream *.dsdx);;"
		"HTML files (*.html *.htm);;ReqIF files (*.reqif)",
		&filter );  // gibt Probleme: QFileDialog::DontUseNativeDialog
	if( paths.isEmpty() || filter.isNull() )
		return;

    importDocs( (sel.isEmpty())?0:sel.first(), paths );
}

void DirViewer::onPasteHtmlDoc()
{
    QList<QTreeWidgetItem *> sel = selectedItems();
	ENABLED_IF( QApplication::clipboard()->mimeData() &&
                QApplication::clipboard()->mimeData()->hasHtml() &&
		( sel.isEmpty() || ( sel.size() == 1 && sel[0]->type() == FOLDER ) ) );

    QTreeWidgetItem* parentItem = (sel.isEmpty())?0:sel.first();

    Obj parent;
    if( parentItem )
    {
        parent = AppContext::inst()->getTxn()->getObject( parentItem->data(0,_OID).toULongLong() );
        parent.setValue( AttrFldrExpanded, Stream::DataCell().setBool(true) );
        parentItem->setExpanded(true);
    }else
        parent = AppContext::inst()->getRoot();

    Database::Lock lock( AppContext::inst()->getDb(), true );
    QApplication::setOverrideCursor( Qt::WaitCursor );
    DocImporter dm;
    Obj doc = dm.importHtmlString( QApplication::clipboard()->mimeData()->html() );
    QString error = dm.getError();

    if( doc.isNull() )
    {
        AppContext::inst()->getTxn()->rollback();
        lock.rollback();
        QApplication::restoreOverrideCursor();
        QMessageBox::critical( this, tr("Error importing document" ), error );
        return;
    }
    doc.setValue( AttrDocName, DataCell().setString( tr("<pasted html>") ) );
    doc.aggregateTo( parent );
    doc.getTxn()->commit();
    QApplication::restoreOverrideCursor();
    QTreeWidgetItem* sub = 0;
    if( parentItem )
        sub = new QTreeWidgetItem( parentItem, DOC );
    else
        sub = new QTreeWidgetItem( this, DOC );
    setCurrentItem( sub );
    loadDoc( sub, doc );
}

QStringList DirViewer::mimeTypes () const
{
	return QStringList() << "application/oid-list" << "text/uri-list";
}

bool DirViewer::dropMimeData ( QTreeWidgetItem * parent, int index, const QMimeData * data,
							  Qt::DropAction action )
{
	if( data->hasFormat("application/oid-list") )
    {
        if( parent && parent->type() != FOLDER )
            return false;
        Obj p = AppContext::inst()->getRoot();
        if( parent )
            p = AppContext::inst()->getTxn()->getObject( parent->data( 0, _OID ).toULongLong() );
        Q_ASSERT( !p.isNull() );
        DataReader r( data->data( "application/oid-list" ) );
        DataReader::Token t = r.nextToken();
        while( t == DataReader::Slot )
        {
            Obj o = AppContext::inst()->getTxn()->getObject( r.readValue() );
            Q_ASSERT( !o.isNull() );
            o.aggregateTo( p );
            createChild( parent, o );
            t = r.nextToken();
        }
        AppContext::inst()->getTxn()->commit();
        return true;
    }else if( data->hasUrls() )
    {
        if( parent && parent->type() != FOLDER )
            return false;
        QStringList paths;
        foreach( QUrl u, data->urls() )
        {
            QString fileName = u.toLocalFile();
            if (QFile::exists(fileName) && fileName.toLower().endsWith(".dsdx"))
                paths.append( fileName );
        }
        importDocs( parent, paths );
        return !paths.empty();
    }else
        return false;
}

Qt::DropActions DirViewer::supportedDropActions () const
{
	return Qt::MoveAction | Qt::CopyAction;
	// CopyAction fhrt zu eigenartigen Effekten wenn man Folder und Documents verschiebt
}

QMimeData * DirViewer::mimeData ( const QList<QTreeWidgetItem *> items ) const
{
	if( items.isEmpty() )
		return 0;
	QMimeData* data = new QMimeData();
	DataWriter w;
	for( int i = 0; i < items.size(); i++ )
	{
		w.writeSlot( DataCell().setOid( items[i]->data( 0, _OID ).toULongLong() ) );
	}
	data->setData( "application/oid-list", w.getStream() );
	return data;
}

void DirViewer::dropEvent(QDropEvent *event)
{
	// Wenn wir das nicht tun, wird dropMimeData oben nie aufgerufen
    QTreeView::dropEvent(event);
}

void DirViewer::startDrag(Qt::DropActions supportedActions)
{
    QTreeWidget::startDrag( Qt::MoveAction ); // wir untersttzen fr internal nur Moves
}

void DirViewer::onAbout()
{
	ENABLED_IF( true );

	QMessageBox::about( this, tr("About DoorScope"), 
		tr( AppContext::s_about )
		.arg( AppContext::s_version ).arg( AppContext::s_date ) );
}

void DirViewer::expReport( bool (*report)(const Sdb::Obj& doc, const QString& path ) )
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	QString path = QFileDialog::getSaveFileName( this, tr("Export Report"), 
		QString(), "*.html", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".html" ) ) )
		path += QLatin1String( ".html" );
	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	if( !report( o, path ) )
		QMessageBox::critical( this, tr("Export Report"),
			tr("Error saving file" ) );
}

void DirViewer::onExpHtml()
{
	expReport( DocExporter::saveStatusAnnotReportHtml );
}

void DirViewer::onExpHtml2()
{
	expReport( DocExporter::saveStatusAnnotReportHtml2 );
}

void DirViewer::onExpHtml3()
{
	expReport( DocExporter::saveFullDocHtml );
}

void DirViewer::onExpHtml4()
{
	expReport( DocExporter::saveFullDocAnnotHtml );
}

void DirViewer::onExpHtml5()
{
	expReport( DocExporter::saveFullDocAnnotAttrHtml );
}

void DirViewer::onExpCsv()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	QList<quint32> atts;
	AttrSelectDlg dlg( this );
	if( !dlg.select( o, atts ) )
		return;

	QString path = QFileDialog::getSaveFileName( this, tr("Export Attribute Table"), 
		QString(), "*.csv", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".csv" ) ) )
		path += QLatin1String( ".csv" );

	if( !DocExporter::saveAttrCsv( o, atts, path ) )
		QMessageBox::critical( this, tr("Export Report"),
			tr("Error saving file" ) );
}

void DirViewer::onExpCsvUtf8()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	QList<quint32> atts;
	AttrSelectDlg dlg( this );
	if( !dlg.select( o, atts ) )
		return;

	QString path = QFileDialog::getSaveFileName( this, tr("Export Attribute Table"), 
		QString(), "*.csv", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".csv" ) ) )
		path += QLatin1String( ".csv" );

	if( !DocExporter::saveAttrCsv( o, atts, path, true ) )
		QMessageBox::critical( this, tr("Export Report"),
			tr("Error saving file" ) );
}

void DirViewer::onExpAnnot()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	QString path = QFileDialog::getSaveFileName( this, tr("Export Annotations"), 
		QString(), "*.dsax", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".dsax" ) ) )
		path += QLatin1String( ".dsax" );

	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	if( !DocExporter::saveAnnot( o, path ) )
		QMessageBox::critical( this, tr("Export Annotations"),
			tr("Error saving file" ) );
}

void DirViewer::onExpAnnotCsv()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	QString path = QFileDialog::getSaveFileName( this, tr("Export Report"), 
		QString(), "*.csv", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".csv" ) ) )
		path += QLatin1String( ".csv" );

	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	if( !DocExporter::saveStatusAnnotReportCsv( o, path ) )
		QMessageBox::critical( this, tr("Export Report"),
			tr("Error saving file" ) );
}

void DirViewer::onExpAnnotCsv2()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	QString path = QFileDialog::getSaveFileName( this, tr("Export Report"), 
		QString(), "*.csv", 0 );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".csv" ) ) )
		path += QLatin1String( ".csv" );

	Sdb::Obj o = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	if( !DocExporter::saveAnnotCsv( o, path ) )
		QMessageBox::critical( this, tr("Export Report"),
			tr("Error saving file" ) );
}


void DirViewer::onCreateHistory()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	Sdb::Obj doc = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	DocSelectorDlg dlg( this );
	dlg.setWindowTitle( tr("Select previous Document Version - DoorScope" ) );
	dlg.resize( AppContext::inst()->getSet()->value( "DocSelectorDlg/Size", QSize( 400, 400 ) ).toSize() );
	Sdb::Obj prev = AppContext::inst()->getTxn()->getObject( 
		dlg.select( AppContext::inst()->getRoot(), doc.getValue( AttrDocDiffSource ).getOid() ) );
	AppContext::inst()->getSet()->setValue( "DocSelectorDlg/Size", dlg.size() );
	if( prev.isNull() )
		return;
	QList<quint32> atts;
	AttrSelectDlg dlg2( this );
	if( AppContext::inst()->getSet()->contains( "AttrSelectDlg/Size" ) )
		dlg2.resize( AppContext::inst()->getSet()->value( "AttrSelectDlg/Size" ).toSize() );
	const bool doit = dlg2.select( doc, atts );
	AppContext::inst()->getSet()->setValue( "AttrSelectDlg/Size", dlg2.size() );

	if( !doit )
		return;
	else
	{
		DocViewer::closeAll( doc );
		Sdb::Database::Lock lock( AppContext::inst()->getDb(), true ); // da sonst permanent in Db rattert.
		QApplication::setOverrideCursor( Qt::WaitCursor );
		DocManager dm;
		dm.createHisto( prev, doc, atts );
		AppContext::inst()->getTxn()->commit();
		QApplication::restoreOverrideCursor();
	}
}

void DirViewer::onSetDocFont()
{
	ENABLED_IF( true );

	QFont f = Txt::Styles::inst()->getFont();
	bool ok;
	f = QFontDialog::getFont( &ok, f, this, tr("Select Document Font - DoorScope" ) );
	if( ok )
		AppContext::inst()->setDocFont( f );
}

void DirViewer::onSetAppFont()
{
	ENABLED_IF( true );
	QFont f1 = QApplication::font();
	bool ok;
	QFont f2 = QFontDialog::getFont( &ok, f1, this, tr("Select Application Font - DoorScope" ) );
	if( !ok )
		return;
	f1.setFamily( f2.family() );
	f1.setPointSize( f2.pointSize() );
	QApplication::setFont( f1 );
	AppContext::inst()->getSet()->setValue( "App/Font", f1 );
}

void DirViewer::onImportAnnot()
{
	QTreeWidgetItem* i = currentItem();
	ENABLED_IF( i && i->type() == DOC );

	QString path = QFileDialog::getOpenFileName( this, tr("Import DoorScope Annotations"), 
		d_lastPath, "*.dsax", 0 );
	if( path.isNull() )
		return;

	QFileInfo info( path );
	d_lastPath = info.absolutePath();
	Sdb::Obj doc = AppContext::inst()->getTxn()->getObject( i->data(0,_OID).toULongLong() );
	DocImporter di;
	if( !di.checkCompatible( doc, path ) )
	{
		if( di.getError().isEmpty() )
		{
			if( QMessageBox::question( this, tr("Importing DoorScope Annotations" ), 
				QString( "Are you sure you want to import annotations of '%1' into '%2'?" ).
				arg( di.getInfo() ).arg( TypeDefs::formatDocName( doc ) ), 
				QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) == QMessageBox::Cancel )
				return;
		}else
		{
			QMessageBox::critical( this, tr("Error importing DoorScope Annotations" ), di.getError() );
			return;
		}
	}
	bool overwrite = false;
	switch( QMessageBox::question( this, tr("Importing DoorScope Annotations" ), 
				tr( "Do you want to overwrite the review status of the objects?" ), 
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No ) )
	{
	case QMessageBox::Cancel:
		return;
	case QMessageBox::Yes:
		overwrite = true;
		break;
	default:
		break;
	}
	QApplication::setOverrideCursor( Qt::WaitCursor );
	if( di.readAnnot( doc, path, overwrite ) )
	{
		doc.getTxn()->commit();
		QApplication::restoreOverrideCursor();
	}else
	{
		QApplication::restoreOverrideCursor();
		QMessageBox::critical( this, tr("Error importing DoorScope Annotations" ), di.getError() );
	}
}

void DirViewer::onSearch()
{
	ENABLED_IF( true );

	Indexer idx;
	if( !Indexer::exists() )
	{
		if( QMessageBox::question( this, tr("DoorScope Search"), 
			tr("The index does not yet exist. Do you want to build it? This will take some minutes." ),
			QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Cancel )
			return;
		if( !idx.indexRepository( this ) )
		{
			if( !idx.getError().isEmpty() )
				QMessageBox::critical( this, tr("DoorScope Indexer"), idx.getError() );
			return;
		}
	}

	SearchView* v = SearchView::inst();
	v->show();
	v->raise();
}

void DirViewer::onReindex()
{
	ENABLED_IF( true );
	Indexer idx;
	if( !idx.indexRepository( this ) )
	{
		if( !idx.getError().isEmpty() )
			QMessageBox::critical( this, tr("DoorScope Indexer"), idx.getError() );
	}
}
