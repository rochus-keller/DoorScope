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

#include "AttrSelectDlg.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include <Stream/DataReader.h>
#include <Sdb/Database.h>
#include <Gui2/AutoMenu.h>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QListWidget>
using namespace Ds;

AttrSelectDlg::AttrSelectDlg(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle( tr("Select Attributes - DoorScope") );

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

AttrSelectDlg::~AttrSelectDlg()
{

}

bool AttrSelectDlg::select( const Sdb::Obj& doc, Atoms& atts )
{
	QMap<QByteArray,quint32> sort;

	sort[TypeDefs::getPrettyName( AttrCreatedBy ) ] = AttrCreatedBy;
	sort[TypeDefs::getPrettyName( AttrCreatedOn ) ] = AttrCreatedOn;
	sort[TypeDefs::getPrettyName( AttrModifiedBy ) ] = AttrModifiedBy;
	sort[TypeDefs::getPrettyName( AttrModifiedOn ) ] = AttrModifiedOn;
	sort[TypeDefs::getPrettyName( AttrCreatedThru ) ] = AttrCreatedThru;
	sort[TypeDefs::getPrettyName( AttrObjNumber ) ] = AttrObjNumber;
	sort[TypeDefs::getPrettyName( AttrObjShort ) ] = AttrObjShort;
	sort["Heading/Text"] = AttrObjText;

	Stream::DataReader r( doc.getValue( AttrDocObjAttrs ) );
	Stream::DataReader::Token t = r.nextToken();
	while( t == Stream::DataReader::Slot )
	{
		const quint32 a = r.readValue().getAtom();
		if( a )
			sort[ doc.getDb()->getAtomString( a ) ] = a;
		t = r.nextToken();
	}

	d_list->clear();
	atts.clear();
	QMap<QByteArray,quint32>::const_iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		QListWidgetItem* item = new QListWidgetItem( i.key(), d_list, i.value() );
		item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
		item->setCheckState( Qt::Unchecked );
		if( i.value() >= DsMax )
			item->setTextColor( Qt::darkBlue );
	}
	if( exec() != QDialog::Accepted )
		return false;
	for( int j = 0; j < d_list->count(); j++ )
		if( d_list->item( j )->checkState() == Qt::Checked )
			atts.append( d_list->item( j )->type() );
	return true;
}

void AttrSelectDlg::selectAll()
{
	ENABLED_IF( true );
	for( int j = 0; j < d_list->count(); j++ )
		d_list->item( j )->setCheckState( Qt::Checked );
}

void AttrSelectDlg::deselectAll()
{
	ENABLED_IF( true );
	for( int j = 0; j < d_list->count(); j++ )
		d_list->item( j )->setCheckState( Qt::Unchecked );
}
