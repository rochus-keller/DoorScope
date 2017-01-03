#include "LinksMdl.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include <Sdb/Transaction.h>
using namespace Ds;
using namespace Stream;

LinksMdl::LinksMdl(QObject *parent)
	: QAbstractItemModel(parent), d_root( 0 ), d_plainText( false )
{
	d_root = new Slot();
}

LinksMdl::~LinksMdl()
{

}

LinksMdl::Slot::~Slot()
{
	for( int i = 0; i < d_subs.size(); i++ )
		delete d_subs[i];
}

void LinksMdl::setObj( const Sdb::Obj& o )
{
	if( d_obj.getId() == o.getId() )
		return;
	d_obj = o;
	refill();
}

void LinksMdl::refill()
{
	if( !d_root->d_subs.isEmpty() )
	{
		beginRemoveRows( QModelIndex(), 0, d_root->d_subs.size() - 1 );
		d_map.clear();
		delete d_root;
		d_root = new Slot();
		endRemoveRows();
	}else
		*d_root = Slot();
	if( !d_obj.isNull() )
		d_root->d_oid = d_obj.getOid();
	fetchAll();
	reset();
}

LinksMdl::Slot* LinksMdl::createSlot( Slot* p, quint64 oid, quint8 kind )
{
	Slot* s = new Slot();
	s->d_oid = oid;
	s->d_kind = kind;
	s->d_super = p;
	p->d_subs.append( s );
	d_map[ s->d_oid ] = s;
	return s;
}

void LinksMdl::fetchAll()
{
	if( d_obj.isNull() )
		return;
	Sdb::Obj o = d_obj.getFirstObj();
	if( !o.isNull() ) do
	{
		if( o.getType() == TypeOutLink )
		{
			Slot* s = createSlot( d_root, o.getOid(), OutLinkKind );
			fetchStub( s, o );
		}else if( o.getType() == TypeInLink )
		{
			Slot* s = createSlot( d_root, o.getOid(), InLinkKind );
			fetchStub( s, o );
		}
	}while( o.next() );
}

void LinksMdl::fetchStub( Slot* p, const Sdb::Obj& link )
{
	Sdb::Obj o = link.getFirstObj();
	if( !o.isNull() ) do
	{
		if( o.getType() == TypeStub )
		{
			createSlot( p, o.getOid(), StubKind );
		}
	}while( o.next() );
}

quint64 LinksMdl::getOid( const QModelIndex & index ) const
{
	if( d_obj.isNull() )
		return 0;
	Slot* p;
	if( index.isValid() )
		p = static_cast<Slot*>( index.internalPointer() );
	else
		p = d_root;
	return p->d_oid;
}

QModelIndex LinksMdl::index( int row, int column, const QModelIndex & parent ) const
{
	if( d_obj.isNull() )
		return QModelIndex();

	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;

	Q_ASSERT( row < p->d_subs.size() );
	return createIndex( row, column, p->d_subs[row] );

	return QModelIndex();
}

int LinksMdl::rowCount ( const QModelIndex & parent ) const
{
	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	Q_ASSERT( p != 0 );
	return p->d_subs.size();
}

QModelIndex LinksMdl::parent(const QModelIndex & index) const
{
	if( index.isValid() )
	{
		Slot* s = static_cast<Slot*>( index.internalPointer() );
		Slot* p = s->d_super;
		if( p != d_root ) // im Fall p == 0 müsste s == d_root sein
		{
			Slot* pp = p->d_super;
			const int row = pp->d_subs.indexOf( p );
			Q_ASSERT( row >= 0 );
			return createIndex( row, index.column(), p );
		}
	}
	return QModelIndex();
}

QVariant LinksMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_obj.isNull() )
		return QVariant();

	Slot* s = static_cast<Slot*>( index.internalPointer() );
	if( index.column() == 0 )
	{
		switch( role )
		{
		case Qt::DisplayRole:
		case Qt::EditRole:
			{
				Sdb::Obj o = d_obj.getTxn()->getObject( s->d_oid );
				switch( s->d_kind )
				{
				case InLinkKind:
					return QString( "[%1] %2: %3" ).
						arg( o.getValue( AttrLnkModName ).toString(true) ).
						arg( TypeDefs::extractName( o.getValue( AttrSrcDocName ).toString(true) ) ).
						arg( o.getValue( AttrSrcObjId ).toPrettyString() );
				case OutLinkKind:
					return QString( "[%1] %2: %3" ).
						arg( o.getValue( AttrLnkModName ).toString(true) ).
						arg( TypeDefs::extractName( o.getValue( AttrTrgDocName ).toString(true) ) ).
						arg( o.getValue( AttrTrgObjId ).toPrettyString() );
				case StubKind:
					return data( index, BodyTextRole );
				}
			}
			break;
		case TitleRole:
			if( s->d_kind == StubKind && !d_plainText )
			{
				Sdb::Obj o = d_obj.getTxn()->getObject( s->d_oid );
				Stream::DataCell v = o.getValue( AttrStubTitle );
				if( v.isStr() && !v.getStr().isEmpty() )
					return o.getValue( AttrObjNumber ).toString(true) + " " + v.toString(true);
			}
			break;
		case BodyRole:
			if( s->d_kind == StubKind )
			{
				if( d_plainText )
					return data( index, BodyTextRole );
				else
				{
					Sdb::Obj o = d_obj.getTxn()->getObject( s->d_oid );
					DataCell v = o.getValue( AttrObjText );
					if( v.isNull() )
						return QVariant();
					else if( v.isBml() )
						return v.getArr();
					else
						return v.toPrettyString();
				}
			}
			break;
		case BodyTextRole:
			{
				Slot* t = s;
				if( s->d_kind != StubKind )
				{
					if( !s->d_subs.empty() )
						t = s->d_subs.first();
					else
						return "";
				}
				QString str;
				Sdb::Obj o = d_obj.getTxn()->getObject( t->d_oid );
				Stream::DataCell v = o.getValue( AttrStubTitle );
				if( v.isStr() && !v.getStr().isEmpty() )
					str = o.getValue( AttrObjNumber ).toString(true) + " " + v.toString(true) + "\r\n";
				v = o.getValue( AttrObjText );
				if( v.isBml() )
				{
					Stream::DataReader r( v );
					str += r.extractString();
				}else
					str += v.toPrettyString();
				return str;
			}
			break;
		case OidRole:
			return s->d_oid;
		case KindRole:
			return (int)s->d_kind;
		case Qt::ToolTipRole:
			{
				Sdb::Obj o = d_obj.getTxn()->getObject( s->d_oid );
				switch( s->d_kind )
				{
				case InLinkKind:
					return QString( "[%4] %1 %2 %3: %5\r\n%6" ).
						arg( o.getValue( AttrSrcDocName ).toString(true) ).
						arg( o.getValue( AttrSrcDocVer ).toString(true) ).
						arg( TypeDefs::formatDate( 
								o.getValue( AttrSrcDocModDate ).getDateTime() ) ).
						arg( o.getValue( AttrLnkModName ).toString(true) ).
						arg( o.getValue( AttrSrcObjId ).toPrettyString() ).
						arg( data( index, BodyTextRole ).toString() );
					break;
				case OutLinkKind:
					return QString( "[%4] %1 %2 %3: %5\r\n%6" ).
						arg( o.getValue( AttrTrgDocName ).toString(true) ).
						arg( o.getValue( AttrTrgDocVer ).toString(true) ).
						arg( TypeDefs::formatDate( 
								o.getValue( AttrTrgDocModDate ).getDateTime() ) ).
						arg( o.getValue( AttrLnkModName ).toString(true) ).
						arg( o.getValue( AttrTrgObjId ).toPrettyString() ).
						arg( data( index, BodyTextRole ).toString() );
					break;
				default:
					return data( index, BodyTextRole );
				}
			}
			break;
		case CommentRole:
			{
				Sdb::Obj o = d_obj.getTxn()->getObject( s->d_oid );
				return o.getValue( AttrAnnotated ).getInt32() > 0;
			}
			break;
		}
	}
	return QVariant();
}

QModelIndex LinksMdl::findIndex( quint64 oid ) const
{
	for( int row = 0; row < d_root->d_subs.size(); row++ )
		if( d_root->d_subs[row]->d_oid == oid )
			return index( row, 0 );
	return QModelIndex();
}
