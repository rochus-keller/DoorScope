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

#include "PropsMdl.h"
#include <Sdb/Database.h>
#include "DocManager.h"
#include "TypeDefs.h"
#include <QImage>
#include <QMap>
#include <QtDebug>
using namespace Ds;

PropsMdl::PropsMdl(QObject *parent)
	: QAbstractItemModel(parent)
{
}

PropsMdl::~PropsMdl()
{

}

void PropsMdl::setObj( const Sdb::Obj& o, bool autoFields )
{
	d_obj = o;
	if( autoFields )
	{
		d_fields.clear();
		// Felder setzen anhand effektiver Belegung
		if( !o.isNull() )
		{
			Sdb::Orl::Names n = o.getNames();
			Sdb::Orl::Names::const_iterator i;
			for( i = n.begin(); i != n.end(); ++i )
				d_fields.append( *i );
		}
	}
	refill();
}

void PropsMdl::setObj( const Sdb::Obj& o, const QList<quint32>& f )
{
	if( d_obj.getId() == o.getId() && d_fields == f )
		return;
	d_obj = o;
	d_fields = f;
	refill();
}

void PropsMdl::setFields( const QList<quint32>& f )
{
	d_fields = f;
	refill();
}

void PropsMdl::refill()
{
	d_rows.clear();
	if( !d_obj.isNull() )
	{
		QMap<QByteArray,quint32> dir;
		for( int i = 0; i < d_fields.size(); i++ )
			dir[ TypeDefs::getPrettyName( d_fields[i], d_obj.getDb() ) ] = d_fields[i];
		QMap<QByteArray,quint32>::const_iterator j;
		for( j = dir.begin(); j != dir.end(); ++j )
		{
			Stream::DataCell v = d_obj.getValue( j.value() );
			if( !j.key().isEmpty() && !v.isNull() )
			{
				const int fieldLimit = 50;
				if( j.value() == AttrObjIdent && v.isInt32() && v.getInt32() == 0 )
					continue; // Bei Links hat die Nummer keine Bedeutung
				switch( v.getType() )
				{
				case Stream::DataCell::TypeLatin1:
				case Stream::DataCell::TypeAscii:
					if( v.getArr().size() > fieldLimit || v.getArr().contains( '\n' ) )
						d_rows.append( QPair<quint32,int>( j.value(), FieldIndex ) );
					else
						d_rows.append( QPair<quint32,int>( j.value(), PairIndex ) );
					break;
				case Stream::DataCell::TypeString:
					if( v.getStr().size() > fieldLimit || v.getStr().contains( '\n' ) )
						d_rows.append( QPair<quint32,int>( j.value(), FieldIndex ) );
					else
						d_rows.append( QPair<quint32,int>( j.value(), PairIndex ) );
					break;
				case Stream::DataCell::TypeLob:
				case Stream::DataCell::TypeBml:
				case Stream::DataCell::TypeImg:
				case Stream::DataCell::TypePic:
				case Stream::DataCell::TypeHtml:
				case Stream::DataCell::TypeXml:
					d_rows.append( QPair<quint32,int>( j.value(), FieldIndex ) );
					break;
				default:
					d_rows.append( QPair<quint32,int>( j.value(), PairIndex ) );
					break;
				}
			}
		}
	}
	reset();
}

int PropsMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
		return d_rows.count();
	else if( parent.internalId() == FieldIndex )
		return 1;
	else
		return 0;
}

QModelIndex PropsMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_rows.size() && column == 0 )
			return createIndex( row, 0, d_rows[row].second );
	}else if( parent.internalId() == FieldIndex && row == 0 && column == 0 )
	{
		return createIndex( 0, 0, ValueIndex + parent.row() );
	}
	return QModelIndex();
}

quint32 PropsMdl::getAtom( const QModelIndex& i ) const
{
	if( d_obj.isNull() || !i.isValid() )
		return 0;
	if( ( i.internalId() == FieldIndex || i.internalId() == PairIndex ) && 
		i.row() < d_rows.size() )
		return d_rows[ i.row() ].first;
	if( i.internalId() >= ValueIndex )
		return d_rows[ i.parent().row() ].first;
	return 0;
}

QVariant PropsMdl::data ( const QModelIndex & index, int role ) const
{
	if( d_obj.isNull() || !index.isValid() || index.column() != 0 )
		return QVariant();

	if( role == Qt::DisplayRole || role == Qt::EditRole )
	{
		if( index.internalId() == FieldIndex )
			return QString( "%1: ..." ). 
				arg( TypeDefs::getPrettyName( d_rows[ index.row() ].first, d_obj.getDb() ).data() );
		else if( index.internalId() == PairIndex )
		{
			return QString( "%1: %2" ).
				arg( TypeDefs::getPrettyName( d_rows[ index.row() ].first, d_obj.getDb() ).data() ).
				arg( TypeDefs::prettyValue( d_obj.getValue( d_rows[ index.row() ].first ) ).toString() );
		}else if( index.internalId() >= ValueIndex )
		{
			return TypeDefs::prettyValue( d_obj.getValue( d_rows[ index.parent().row() ].first ) );
		}
	}else if( role == Qt::ToolTipRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == PairIndex )
		{
			return TypeDefs::prettyValue( d_obj.getValue( d_rows[ index.row() ].first ) );
		}
	}else if( role == IndexTypeRole )
	{
		if( index.internalId() == FieldIndex || index.internalId() == PairIndex )
			return index.internalId();
		else if( index.internalId() >= ValueIndex )
			return ValueIndex;
	}else if( role == TitleRole )
	{
		QModelIndex i = index;
		if( index.internalId() >= ValueIndex )
			i = index.parent();
		return QString::fromLatin1(
			TypeDefs::getPrettyName( d_rows[ i.row() ].first, d_obj.getDb() ) );
	}else if( role == ValueRole )
	{
		QModelIndex i = index;
		if( index.internalId() >= ValueIndex )
			i = index.parent();
		Stream::DataCell v = d_obj.getValue( d_rows[ i.row() ].first );
		if( v.isBml() )
			return v.getArr();
		else
			return TypeDefs::prettyValue( v );
	}else if( role == SystemRole )
	{
        // bool: handelt es sich um ein built-in oder custom Attribute?
		QModelIndex i = index;
		if( index.internalId() >= ValueIndex )
			i = index.parent();
		return ( d_rows[ i.row() ].first < DsMax );
	}
	return QVariant();
}

QModelIndex PropsMdl::parent(const QModelIndex & index) const
{
	if( d_obj.isNull() || !index.isValid() )
		return QModelIndex();
	if( index.internalId() == FieldIndex || index.internalId() == PairIndex )
		return QModelIndex();
	else if( index.internalId() >= ValueIndex )
		return createIndex( index.internalId() - ValueIndex, 0, FieldIndex );
	else
		return QModelIndex();
}

Qt::ItemFlags PropsMdl::flags(const QModelIndex &index) const
{
	if( index.internalId() == FieldIndex || index.internalId() == PairIndex )
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	else if( index.internalId() >= ValueIndex )
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	else
		return 0;
}
