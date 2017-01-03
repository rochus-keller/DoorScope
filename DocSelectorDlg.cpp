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

#include "DocSelectorDlg.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <Sdb/Transaction.h>
using namespace Ds;

enum Type { FOLDER, DOC };
enum Role { _OID = Qt::UserRole + 1, DT }; 

DocSelectorDlg::DocSelectorDlg(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle( tr("Select Document - DoorScope") );

	QVBoxLayout* vbox = new QVBoxLayout( this );
	
	d_tree = new QTreeWidget( this );
	d_tree->setAlternatingRowColors( true );
	d_tree->setHeaderLabels( QStringList() << tr("Folder/Document") );
	d_tree->setSortingEnabled(true);
	d_tree->setAllColumnsShowFocus(true);
	vbox->addWidget( d_tree );

	d_bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
		Qt::Horizontal, this );
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( false );
	vbox->addWidget( d_bb );

	connect(d_bb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(d_bb, SIGNAL(rejected()), this, SLOT(reject()));

	connect( d_tree, SIGNAL(currentItemChanged(  QTreeWidgetItem *, QTreeWidgetItem * )),
		this, SLOT( onCurrentItemChanged(  QTreeWidgetItem *, QTreeWidgetItem * ) ) );
	connect(d_tree, SIGNAL( itemDoubleClicked ( QTreeWidgetItem *, int )), this,
		SLOT( onItemDoubleClicked ( QTreeWidgetItem *, int ) ) );

}

DocSelectorDlg::~DocSelectorDlg()
{

}

void DocSelectorDlg::loadFolder( QTreeWidgetItem* i, const Sdb::Obj& o )
{
	i->setText( 0, o.getValue( AttrFldrName ).toPrettyString() );
	i->setData( 0, _OID, o.getId() );
	i->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
	i->setExpanded( true );
	i->setIcon( 0, QIcon(":/DoorScope/Images/folder.png") );
}

void DocSelectorDlg::loadDoc( QTreeWidgetItem* i, const Sdb::Obj& o )
{
	i->setIcon( 0, QIcon(":/DoorScope/Images/document.png") );
	i->setData( 0, _OID, o.getId() );
	i->setText( 0, TypeDefs::formatDocName( o ) );
	i->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
}

void DocSelectorDlg::createChild( QTreeWidgetItem* parent, const Sdb::Obj& o, quint64 sel )
{
	QTreeWidgetItem* sub;
	if( o.getType() == TypeFolder )
	{
		if( parent )
			sub = new QTreeWidgetItem( parent, FOLDER );
		else 
			sub = new QTreeWidgetItem( d_tree, FOLDER );
		loadFolder( sub, o );
		loadChildren( sub, o, sel );
	}else if( o.getType() == TypeDocument )
	{
		if( parent )
			sub = new QTreeWidgetItem( parent, DOC );
		else 
			sub = new QTreeWidgetItem( d_tree, DOC );
		loadDoc( sub, o );
		if( o.getId() == sel )
			sub->setSelected( true );
		loadChildren( sub, o, sel );
	}
}

void DocSelectorDlg::loadChildren( QTreeWidgetItem* i, const Sdb::Obj& p, quint64 sel )
{
	Sdb::Obj o = p.getFirstObj();
	if( !o.isNull() ) do
	{
		createChild( i, o, sel );
	}while( o.next() );
}

quint64 DocSelectorDlg::select( Sdb::Obj root, quint64 sel )
{
	if( root.isNull() )
		return 0;
	d_tree->clear();
	loadChildren( 0, root, sel );
	d_tree->resizeColumnToContents ( 0 );
	d_tree->sortItems( 0, Qt::AscendingOrder ); 
	QList<QTreeWidgetItem *> items = d_tree->selectedItems();
	if( !items.empty() )
		d_tree->setCurrentItem( items[0] );
	//d_bb->button( QDialogButtonBox::Ok )->setEnabled( d_tree->currentItem() && d_tree->currentItem()->type() == DOC );

	if( exec() != QDialog::Accepted || d_tree->currentItem() == 0 )
		return 0;
	Sdb::Obj o = root.getTxn()->getObject( d_tree->currentItem()->data(0,_OID).toULongLong() );
	if( o.getType() == TypeDocument )
		return o.getId();
	else
		return 0;
}

void DocSelectorDlg::onCurrentItemChanged(  QTreeWidgetItem * item, QTreeWidgetItem * previous )
{
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( item->type() == DOC );
}

void DocSelectorDlg::onItemDoubleClicked ( QTreeWidgetItem * item, int )
{
	if( item->type() == DOC )
		accept();
}
