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

#include "HistMdl.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include "StringDiff.h"
#include <QMap>
#include <Sdb/Transaction.h>
#include <Stream/DataReader.h>
#include <Txt/Styles.h>
#include <QtDebug>
#include <QTextDocument>
#include <QTextCursor>
using namespace Ds;
using namespace Sdb;
using namespace Stream;

Q_DECLARE_METATYPE( QTextDocument* ) 

HistMdl::HistMdl(QObject *parent)
	: QAbstractItemModel(parent)
{

}

HistMdl::~HistMdl()
{

}

void HistMdl::setObj( const Sdb::Obj& o )
{
	if( d_obj.getId() == o.getId() )
		return;
	d_obj = o;
	refill();
}

void HistMdl::refill()
{
	if( !d_rows.isEmpty() )
	{
		beginRemoveRows( QModelIndex(), 0, d_rows.size() - 1 );
		for( int n = 0; n < d_rows.size(); n++ )
			if( d_rows[n].d_doc )
				delete d_rows[n].d_doc;
		d_rows.clear();
		endRemoveRows();
	}
	if( d_obj.isNull() )
	{
		reset();
		return;
	}

	QMap<quint32,Slot> map;
	QMap<quint8,Slot> map2;
	Qit i = d_obj.getFirstSlot();
	if( !i.isNull() ) do
	{
		Obj o = d_obj.getTxn()->getObject( i.getValue() );
		if( !o.isNull() && o.getType() == TypeHistory )
		{
			const quint32 atom = o.getValue( AttrHistAttr ).getAtom();
			const quint8 op = o.getValue( AttrHistType ).getUInt8();
			if( atom != 0 && op == HistoryType_modifyObject )
			{
				QMap<quint32,Slot>::iterator j = map.find( atom );
				if( j == map.end() )
					map[atom] = Slot( o.getId(), FieldIndex );
				else
					j.value().d_last = o.getId();
			}else
			{
				QMap<quint8,Slot>::iterator j = map2.find( op );
				if( j == map2.end() )
					map2[op] = Slot( o.getId(), OpIndex );
				else
					j.value().d_last = o.getId();
			}
		}
	}while( i.next() );
	QMap<QByteArray,quint32> dir;
	QMap<quint32,Slot>::const_iterator l;
	for( l = map.begin(); l != map.end(); ++l )
		dir[ TypeDefs::getPrettyName( l.key(), d_obj.getDb() ) ] = l.key();
	QMap<QByteArray,quint32>::const_iterator j;
	for( j = dir.begin(); j != dir.end(); ++j )
	{
		if( !j.key().isEmpty() )
			d_rows.append( map[j.value()] );
	}
	dir.clear();
	QMap<quint8,Slot>::const_iterator m;
	for( m = map2.begin(); m != map2.end(); ++m )
		dir[ TypeDefs::historyTypePrettyString[ m.key() ] ] = m.key();
	for( j = dir.begin(); j != dir.end(); ++j )
	{
		if( !j.key().isEmpty() )
			d_rows.append( map2[j.value()] );
	}
	/* TEST
	for( int k = 0; k < d_rows.size(); k++ )
	{
		Obj o = d_obj.getTxn()->getObject( d_rows[k].d_last );
		const quint8 op = o.getValue( AttrHistType ).getUInt8();
		qDebug() << d_rows[k].d_first << " " << d_rows[k].d_last << 
			" " << d_rows[k].d_type << " " <<
			TypeDefs::historyTypeString[op];
	}
	*/
	reset();
}

int HistMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
		return d_rows.count();
	else if( parent.internalId() == FieldIndex )
		return 1;
	else
		return 0;
}

QModelIndex HistMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_rows.size() && column == 0 )
			return createIndex( row, 0, int(d_rows[row].d_type) );
	}else if( parent.internalId() == FieldIndex && row == 0 && column == 0 )
	{
		return createIndex( 0, 0, ValueIndex + parent.row() );
	}
	return QModelIndex();
}

QVariant HistMdl::data ( const QModelIndex & index, int role ) const
{
	if( d_obj.isNull() || !index.isValid() || index.column() != 0 )
		return QVariant();
	if( role == Qt::DisplayRole || role == Qt::EditRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == OpIndex )
		{
			Obj o = d_obj.getTxn()->getObject( d_rows[index.row()].d_last );
			QString name = "<unknown>";
			if( index.internalId() == FieldIndex )
				name = TypeDefs::getPrettyName( 
					o.getValue( AttrHistAttr ).getAtom(), d_obj.getDb() );
			else if( index.internalId() == OpIndex )
				name = TypeDefs::historyTypePrettyString[
						o.getValue( AttrHistType ).getUInt8() ];
			return QString( "%1: %2 %3 %4" ).
				arg( name ).
				arg( TypeDefs::formatDate( o.getValue( AttrHistDate ).getDateTime() ) ).
				arg( o.getValue( AttrHistAuthor ).getStr() ).
				arg( o.getValue( AttrHistInfo ).getStr() );
		}else if( index.internalId() >= ValueIndex )
		{
			const int row = index.internalId() - ValueIndex;
			if( row < d_rows.size() )
			{
				if( d_rows[row].d_doc == 0 )
					calcDiff( row );
				return QVariant::fromValue( d_rows[row].d_doc );
			}
		}
	}else if( role == Qt::ToolTipRole )
	{
		if( index.internalId() == FieldIndex )
		{
			if( d_rows[index.row()].d_doc == 0 )
				calcDiff( index.row() );
			if( d_rows[index.row()].d_doc )
				return d_rows[index.row()].d_doc->toHtml();
		}else if( index.internalId() == OpIndex )
		{
			Obj o = d_obj.getTxn()->getObject( d_rows[index.row()].d_last );
			if( o.hasValue( AttrHistInfo ) )
				return o.getValue( AttrHistInfo ).getStr();
			else
				return index.data();
		}
	}else if( role == IndexTypeRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == OpIndex )
			return index.internalId();
		else if( index.internalId() >= ValueIndex )
			return ValueIndex;
	}else if( role == FirstRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == OpIndex )
			return d_rows[index.row()].d_first;
	}else if( role == LastRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == OpIndex )
			return d_rows[index.row()].d_last;
	}else if( role == SystemRole )
	{
		QModelIndex i = index;
		if( index.internalId() >= ValueIndex )
			i = index.parent();
		if( d_rows[i.row()].d_type == OpIndex )
			return true; // Grundsätzlich System
		if( d_rows[i.row()].d_type == FieldIndex )
		{
			Obj o = d_obj.getTxn()->getObject( d_rows[i.row()].d_last );
			return ( o.getValue( AttrHistAttr ).getAtom() < DsMax );
		}
	}
	return QVariant();
}

QModelIndex HistMdl::parent(const QModelIndex & index) const
{
	if( d_obj.isNull() || !index.isValid() )
		return QModelIndex();
	if( index.internalId() == FieldIndex || index.internalId() == OpIndex )
		return QModelIndex();
	else if( index.internalId() >= ValueIndex )
		return createIndex( index.internalId() - ValueIndex, 0, FieldIndex );
	else
		return QModelIndex();
}

void HistMdl::printDiff( QTextCursor& cur, const QString& newText, const QString& oldText )
{
    QTextCharFormat normal = Txt::Styles::inst()->getCharFormat( Txt::Styles::PAR );
    QTextCharFormat fNew = normal;
    fNew.setFontUnderline( true );
    fNew.setForeground( Qt::red );
    QTextCharFormat fDel = normal;
    fDel.setFontStrikeOut( true );
    fDel.setForeground( Qt::blue );
    StringDiff::Items res = StringDiff::Diff( oldText, newText );
    int pos = 0;
    for( int i = 0; i < res.size(); i++ )
    {
        StringDiff::Item& it = res[i];

        // TODO: folgendes ist noch nicht maximal effizient

        // write unchanged chars
        QString str;
        while( ( pos < it.StartB ) && ( pos < newText.size() ) )
        {
            str += newText[pos];
            pos++;
        } // while
        if( !str.isEmpty() )
            cur.insertText( str, normal );
        str.clear();

        // write deleted chars
        if( it.deletedA > 0 )
        {
            for( int m = 0; m < it.deletedA; m++ )
                str += oldText[it.StartA + m];
            if( !str.isEmpty() )
                cur.insertText( str, fDel );
            str.clear();
        }

        // write inserted chars
        if( pos < ( it.StartB + it.insertedB ) )
        {
            while( pos < ( it.StartB + it.insertedB ) )
            {
                str += newText[pos];
                pos++;
            }
            if( !str.isEmpty() )
                cur.insertText(str,fNew);
        }
    }

    // write rest of unchanged chars
    if( pos < newText.size() )
    {
        cur.insertText( newText.mid( pos ), normal );
    }
}

void HistMdl::calcDiff( int row ) const
{
	if( row < d_rows.size() && d_rows[row].d_doc == 0 )
	{
		Obj first = d_obj.getTxn()->getObject( d_rows[row].d_first );
		Obj last = first;
		if( d_rows[row].d_first != d_rows[row].d_last )
			last = d_obj.getTxn()->getObject( d_rows[row].d_last );

		QString oldVal, newVal;
		DataCell v = first.getValue( AttrHistOld ); 
		if( v.isBml() )
		{
			DataReader r( v );
			oldVal = r.extractString();
		}else if( !v.isNull() )
			oldVal = v.toPrettyString();
		v = last.getValue( AttrHistNew );
		if( v.isBml() )
		{
			DataReader r( v );
			newVal = r.extractString();
		}else if( !v.isNull() )
			newVal = v.toPrettyString();

		d_rows[row].d_doc = new QTextDocument( const_cast<HistMdl*>(this) );
		QTextCursor cur( d_rows[row].d_doc );
        printDiff( cur, newVal, oldVal );
	}
}

