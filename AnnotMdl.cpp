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

#include "AnnotMdl.h"
#include "TypeDefs.h"
#include "AppContext.h"
#include "DocManager.h"
#include <Sdb/Obj.h>
#include <Sdb/Exceptions.h>
#include <Stream/DataReader.h>
using namespace Ds;

AnnotMdl::AnnotMdl(QObject *parent)
	: QAbstractItemModel(parent), d_sorted( false )
{
	AppContext::inst()->getDb()->addObserver( this, SLOT(onDbUpdate( Sdb::UpdateInfo )));
}

AnnotMdl::~AnnotMdl()
{
	clearAll();
}

QModelIndex AnnotMdl::prependAnnot( quint64 id )
{
	if( d_obj.isNull() )
		return QModelIndex();
	Sdb::Obj o = d_obj.getTxn()->getObject( id );
	if( o.getType() == TypeAnnotation )
	{
		beginInsertRows( QModelIndex(), 0, 0 );
		d_rows.prepend( new Slot( o ) );
		endInsertRows();
		return index( 0, 0, index( 0, 0 ) );
	}
	else
		return QModelIndex();
}

void AnnotMdl::onDbUpdate( Sdb::UpdateInfo info )
{
	if( d_obj.isNull() )
		return;
	switch( info.d_kind )
	{
	/*
	case Sdb::UpdateInfo::Aggregated:
		if( info.d_id2 == d_obj.getId() )
		{
			if( d_obj.getTxn()->getObject( info.d_id ).getType() == TypeAnnotation )
			{
				beginInsertRows( QModelIndex(), 0, 0 );
				d_rows.prepend( info.d_id );
				endInsertRows();
			}
		}
		break;
		*/
	case Sdb::UpdateInfo::ObjectErased:
		{
			const int row = find( info.d_id );
			if( row != -1 && row < d_rows.size() )
			{
				beginRemoveRows( QModelIndex(), row, row );
				delete d_rows[row];
				d_rows.removeAt( row );
				endRemoveRows();
			}
		}
		break;
	case Sdb::UpdateInfo::ValueChanged:
		{
			const int row = find( info.d_id );
			if( row != -1 && row < d_rows.size() )
			{
				QModelIndex i = index( row, 0 );
				if( info.d_name == AttrAnnText )
					i = index( 0, 0, i );
				dataChanged( i, i );
			}
		}
		break;
	case Sdb::UpdateInfo::DbClosing:
		clearAll();
		reset();
		break;
	}
}

int AnnotMdl::find( quint64 id ) const
{
	for( int i = 0; i < d_rows.size(); i++ )
		if( d_rows[i]->d_obj.getId() == id )
			return i;
	return -1;
}

void AnnotMdl::clearAll()
{
	if( !d_rows.isEmpty() )
	{
		beginRemoveRows( QModelIndex(), 0, d_rows.size() - 1 );
		for( int i = 0; i < d_rows.size(); i++ )
			delete d_rows[i];
		d_rows.clear();
		endRemoveRows();
	}
}

void AnnotMdl::setObj( const Sdb::Obj& o )
{
	if( d_obj.getId() == o.getId() )
		return;
	d_obj = o;
	refill();
}

void AnnotMdl::refill()
{
	clearAll();
	if( d_obj.isNull() )
	{
		reset();
		return;
	}
	Sdb::Obj o = d_obj.getFirstObj();
	if( d_sorted )
	{
		QMultiMap<QByteArray, Sdb::Obj> order;
		if( !o.isNull() ) do
		{
			if( o.getType() == TypeAnnotation )
			{
				QByteArray head = TypeDefs::getPrettyName( o.getValue( AttrAnnAttr ).getAtom(), d_obj.getDb() );
				if( head.isEmpty() )
					head = TypeDefs::getPrettyName( o.getOwner().getType(), d_obj.getDb() );
				order.insert( head, o );
			}
		}while( o.next() );
		QMultiMap<QByteArray, Sdb::Obj>::const_iterator i;
		for( i = order.begin(); i != order.end(); ++i )
		{
			d_rows.append( new Slot( i.value() ) );
		}
	}else
	{
		if( !o.isNull() ) do
		{
			if( o.getType() == TypeAnnotation )
			{
				d_rows.prepend( new Slot( o ) );
			}
		}while( o.next() );
	}
	reset();
}

void AnnotMdl::setSorted( bool on )
{
	if( d_sorted == on )
		return;
	d_sorted = on;
	refill();
}

int AnnotMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
		return d_rows.count();
	else if( parent.internalPointer() == 0 )
		return 1;
	else
		return 0;
}

QModelIndex AnnotMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_rows.size() && column == 0 )
			return createIndex( row, 0, 0 );
	}else if( parent.internalPointer() == 0 && row == 0 && column == 0 )
	{
		return createIndex( 0, 0, d_rows[ parent.row() ] );
	}
	return QModelIndex();
}

QModelIndex AnnotMdl::parent(const QModelIndex & index) const
{
	if( d_obj.isNull() || !index.isValid() )
		return QModelIndex();
	if( index.internalPointer() == 0 )
		return QModelIndex();
	else if( index.internalPointer() != 0 )
		return createIndex( d_rows.indexOf( static_cast<Slot*>(index.internalPointer()) ), 0, 0 );
	else
		return QModelIndex();
}

Qt::ItemFlags AnnotMdl::flags(const QModelIndex &index) const
{
	if( index.internalPointer() == 0 )
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	// Alle Felder grundsätzlich Editierbar machen, ansonsten wird Editor nicht geschlossen.
	else if( index.internalPointer() != 0 )
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	else
		return 0;
}

QVariant AnnotMdl::data ( const QModelIndex & idx, int role ) const
{
	if( d_obj.isNull() || !idx.isValid() || idx.column() != 0 )
		return QVariant();

	try
	{
		if( role == Qt::DisplayRole || role == Qt::EditRole  )
		{
			if( idx.internalPointer() == 0 )
			{
				Sdb::Obj o = d_rows[idx.row()]->d_obj;
				QByteArray head = TypeDefs::getPrettyName( o.getValue( AttrAnnAttr ).getAtom(),
					d_obj.getDb() );
				if( head.isEmpty() )
					head = TypeDefs::getPrettyName( o.getOwner().getType(), d_obj.getDb() );
				return QString( "%1: [%2] %3 %4" ).arg( head.data() ).
					arg( o.getValue( AttrAnnNr ).getUInt32() ).
					arg( TypeDefs::formatDate( o.getValue( AttrModifiedOn ).getDateTime() ) ).
					arg( o.getValue( AttrModifiedBy ).toString(true) );
			}else if( idx.internalPointer() != 0 )
			{
				Slot* s = static_cast<Slot*>( idx.internalPointer() );
				Sdb::Obj o = s->d_obj;
				Stream::DataCell v = o.getValue( AttrAnnText );
				if( v.isBml() )
				{
					Stream::DataReader r( v );
					return r.extractString();
				}else if( v.isNull() )
					return "";
				else
					return v.toPrettyString();
			}
		}else if( role == Qt::ToolTipRole )
		{
			if( idx.internalPointer() == 0  )
			{
				return data( index( 0, 0, idx ) );
			}
		}else if( role == IndexTypeRole )
		{
			if( idx.internalPointer() == 0 )
				return TitleIndex;
			else 
				return BodyIndex;
		}else if( role == OidRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			return s->d_obj.getId();
		}else if( role == AttrRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			return s->d_obj.getValue( AttrAnnAttr ).getAtom();
		}else if( role == NameRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			return s->d_obj.getValue( AttrModifiedBy ).toString(true);
		}else if( role == PrioRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			return (int)s->d_obj.getValue( AttrAnnPrio ).getUInt8();
		}else if( role == NrRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			return s->d_obj.getValue( AttrAnnNr ).getUInt32();
		}else if( role == BodyRole )
		{
			Slot* s;
			if( idx.internalPointer() == 0 )
				s = d_rows[idx.row()];
			else
				s = static_cast<Slot*>( idx.internalPointer() );
			Sdb::Obj o = s->d_obj;
			Stream::DataCell v = o.getValue( AttrAnnText );
			if( v.isBml() )
			{
				return v.getArr();
			}else if( v.isNull() )
				return "";
			else
				return v.toPrettyString();
		}
	}catch( Sdb::DatabaseException& e )
	{
		if( e.getCode() == Sdb::DatabaseException::RecordDeleted )
			return "<deleted>";
	}
	return QVariant();
}

bool AnnotMdl::setData ( const QModelIndex & index, const QVariant & value, int role )  
{
	if( role == Qt::SizeHintRole )
	{
		// Trick um Height-Cache zu invalidieren
		emit dataChanged( index, index );
		return true;
	}else if( role == BodyRole )
	{
		try
		{
			Slot* s;
			if( index.internalPointer() == 0 )
				s = d_rows[index.row()];
			else
				s = static_cast<Slot*>( index.internalPointer() );

			Sdb::Obj o = s->d_obj;
			Stream::DataCell old = o.getValue( AttrAnnText );
			Stream::DataCell nw;
			if( value.type() == QVariant::ByteArray )
			{
				nw.setBml( value.toByteArray() );
			}else
				nw.setString( value.toString() );
			if( !old.equals( nw ) )
			{
				o.setValue( AttrAnnText, nw );
				o.setValue( AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
				QString user = AppContext::inst()->getUserName();
				if( !user.isEmpty() )
					o.setValue( AttrModifiedBy, Stream::DataCell().setString( user ) );
				o.getTxn()->commit();
			}
		}catch( Sdb::DatabaseException& e )
		{
		}
		return true;
	}
	return false;
}

void AnnotMdl::deleteAnnot( const QModelIndex& i )
{
	if( d_obj.isNull() || !i.isValid() )
		return;
	Slot* s;
	if( i.internalPointer() == 0 )
		s = d_rows[i.row()];
	else
		s = static_cast<Slot*>( i.internalPointer() );
	Sdb::Obj p = s->d_obj.getOwner();
	const int n = p.getValue( AttrAnnotated ).getInt32();
	p.setValue( AttrAnnotated, Stream::DataCell().setInt32( n - 1 ) );
	Sdb::Obj doc = p.getObject( AttrObjHomeDoc );
	if( !doc.isNull() )
		doc.setValue( AttrAnnotated, Stream::DataCell().setInt32( doc.getValue( AttrAnnotated ).getInt32() - 1 ) );
	// else: wenn p == doc gibt es kein AttrObjHomeDoc
	s->d_obj.erase();
	d_obj.getTxn()->commit();
}

void AnnotMdl::setUserName( const QModelIndex& i, const QString& name )
{
	if( d_obj.isNull() || !i.isValid() )
		return;
	Slot* s;
	if( i.internalPointer() == 0 )
		s = d_rows[i.row()];
	else
		s = static_cast<Slot*>( i.internalPointer() );
	if( name.isEmpty() )
		s->d_obj.setValue( AttrModifiedBy, Stream::DataCell().setNull() );
	else
		s->d_obj.setValue( AttrModifiedBy, Stream::DataCell().setString( name ) );
	d_obj.getTxn()->commit();
	emit dataChanged( i, i );
}

quint64 AnnotMdl::getOid( const QModelIndex & i ) const
{
	return data( i, OidRole ).toULongLong();
}

QModelIndex AnnotMdl::findIndex( quint64 oid ) const
{
	const int row = find( oid );
	if( row != -1 )
		return index( row, 0 );
	else
		return QModelIndex();
}
