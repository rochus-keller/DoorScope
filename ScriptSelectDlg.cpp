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

#include "ScriptSelectDlg.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QListWidget>
#include <Gui2/AutoMenu.h>
#include "TypeDefs.h"
using namespace Ds;

ScriptSelectDlg::ScriptSelectDlg(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle( tr("Select Scripts - DoorScope") );

	QVBoxLayout* vbox = new QVBoxLayout( this );

	d_list = new QListWidget( this );
	QFont f = d_list->font();
	f.setBold( true );
	d_list->setFont( f );
	d_list->setAlternatingRowColors( true );
	vbox->addWidget( d_list );

	QDialogButtonBox* bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this );
	vbox->addWidget( bb );

	connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

	Gui2::AutoMenu* m = new Gui2::AutoMenu( d_list, true );
	m->addCommand( tr("Select all"), this, SLOT(selectAll()) );
	m->addCommand( tr("Deselect all"), this, SLOT(deselectAll()) );
}

bool ScriptSelectDlg::select(const Sdb::Obj &obj, ScriptSelectDlg::Result & res)
{
	Q_ASSERT( !obj.isNull() );

	QMultiMap<QString,Sdb::Obj> sort;

	Sdb::Obj sub = obj.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == TypeLuaScript || sub.getType() == TypeLuaFilter )
			sort.insertMulti( sub.getValue(AttrScriptName).getStr(), sub );
	}while( sub.next() );

	d_list->clear();
	res.clear();
	QMultiMap<QString,Sdb::Obj>::const_iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		QListWidgetItem* item = new QListWidgetItem( i.key(), d_list );
		item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
		item->setCheckState( Qt::Unchecked );
		item->setData( Qt::UserRole, i.value().getOid() );
	}
	if( exec() != QDialog::Accepted )
		return false;
	for( int j = 0; j < d_list->count(); j++ )
	{
		if( d_list->item( j )->checkState() == Qt::Checked )
			res.append( d_list->item( j )->data( Qt::UserRole ).toULongLong() );
	}
	return true;

}

void ScriptSelectDlg::selectAll()
{
	ENABLED_IF( true );
	for( int j = 0; j < d_list->count(); j++ )
		d_list->item( j )->setCheckState( Qt::Checked );
}

void ScriptSelectDlg::deselectAll()
{
	ENABLED_IF( true );
	for( int j = 0; j < d_list->count(); j++ )
		d_list->item( j )->setCheckState( Qt::Unchecked );
}
