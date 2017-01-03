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

#include "DocMdl.h"
#include "TypeDefs.h"
#include "AppContext.h"
#include "PropsMdl.h"
#include <Sdb/Transaction.h>
#include <Txt/TextInStream.h>
#include <Txt/TextCursor.h>
#include <Script/Engine2.h>
#include <Script/Lua.h>
#include <QtDebug>
#include <QApplication>
#include "LuaBinding.h"
using namespace Ds;
using namespace Sdb;

DocMdl::DocMdl(QObject *parent)
	: QAbstractItemModel(parent), d_root(0), d_filter( TitleAndBody ), 
	  d_onlyHdrTxtChanges( false ), d_luaFilter(LUA_NOREF)
{
	AppContext::inst()->getDb()->addObserver( this, SLOT(onDbUpdate( Sdb::UpdateInfo )));
	d_root = new Slot();
}

DocMdl::~DocMdl()
{
	if( d_root )
		delete d_root;
}

DocMdl::Slot::~Slot()
{
	if( d_text )
		delete d_text;
	for( int i = 0; i < d_subs.size(); i++ )
		delete d_subs[i];
}

void DocMdl::setDoc( const Sdb::Obj& doc )
{
	d_doc = doc;
	refill();
}

void DocMdl::setDoc( const Sdb::Obj& doc, Filter f )
{
	d_filter = f;
	d_doc = doc;
	refill();
}

void DocMdl::setFilter( Filter f )
{
	d_filter = f;
	refill();
}

bool DocMdl::setLuaFilter(const QByteArray& code, const QByteArray &name)
{
	Lua::Engine2* e = Lua::Engine2::getInst();
	Q_ASSERT( e != 0 );

	bool changed = false;
	if( d_luaFilter != LUA_NOREF )
	{
		// Unregister
		luaL_unref( e->getCtx(), LUA_REGISTRYINDEX, d_luaFilter );
		lua_pushnil( e->getCtx() );
		d_luaFilter = LUA_NOREF;
		lua_setfield( e->getCtx(), LUA_REGISTRYINDEX, d_luaFilterName );
		changed = true;
	}
	if( code.isEmpty() )
	{
		if( changed )
			refill();
		return true;
	}
	if( !e->pushFunction( code, name ) )
	{
		if( changed )
			refill();
		return false;
	}

	lua_pushstring( e->getCtx(), code );
	lua_setfield( e->getCtx(), LUA_REGISTRYINDEX, name );
	d_luaFilterName = name;
	d_luaFilter = luaL_ref( e->getCtx(), LUA_REGISTRYINDEX );
	refill();
	return true;
}

bool DocMdl::hasLuaFilter() const
{
	return d_luaFilter != LUA_NOREF;
}

void DocMdl::addCol( const QString& name, quint32 attr )
{
	beginInsertColumns( QModelIndex(), d_cols.size(), d_cols.size() );
	d_cols.append( qMakePair( name, attr ) );
	endInsertColumns();
}

void DocMdl::clearCols()
{
	beginRemoveColumns( QModelIndex(), 1, d_cols.size() );
	d_cols.clear();
	endRemoveColumns();
}

void DocMdl::refill()
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
	if( !d_doc.isNull() )
		d_root->d_oid = d_doc.getOid();
	reset();
}

Qt::ItemFlags DocMdl::flags(const QModelIndex &index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QModelIndex DocMdl::index( int row, int column, const QModelIndex & parent ) const
{
	if( d_doc.isNull() )
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

int DocMdl::rowCount ( const QModelIndex & parent ) const
{
	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	Q_ASSERT( p != 0 );
	return p->d_subs.size();
}

QModelIndex DocMdl::parent(const QModelIndex & index) const
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

QVariant DocMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_doc.isNull() )
		return QVariant();

	Slot* s = static_cast<Slot*>( index.internalPointer() );
	Q_ASSERT( s != 0 );
	switch( role )
	{
		// Spaltenunabhängige Information
	case OidRole:
		return s->d_oid;
	case LevelRole:
		return (int)s->d_level;
	case ChangedRole:
		{
			Obj o = d_doc.getTxn()->getObject( s->d_oid );
			if( d_onlyHdrTxtChanges )
				return o.getValue(AttrObjTextChanged).getBool();
			else
				return o.getValue(AttrObjAttrChanged).getBool();
		}
		break;
	case StatusRole:
		{
			Obj o = d_doc.getTxn()->getObject( s->d_oid );
			Stream::DataCell v = o.getValue( AttrReviewStatus );
			if( v.getType() == Stream::DataCell::TypeUInt8 )
				return int( v.getUInt8() );
		}
		break;
	case ConsStatRole:
		{
			Obj o = d_doc.getTxn()->getObject( s->d_oid );
			Stream::DataCell v = o.getValue( AttrConsRevStat );
			if( v.getType() == Stream::DataCell::TypeBml )
				return v.getBml();
		}
		break;

	case CommentRole:
		{
			Obj o = d_doc.getTxn()->getObject( s->d_oid );
			return o.getValue( AttrAnnotated ).getInt32() > 0;
		}
		break;
	}
	if( index.column() == 0 )
	{
		switch( role )
		{
		case Qt::DisplayRole:
		case Qt::EditRole:
			if( s->d_text )
			{
				return QVariant::fromValue( s->d_text );
			}else
			{
				Obj o = d_doc.getTxn()->getObject( s->d_oid );
				if( s->d_level > 0 ) // Title
					return o.getValue( AttrObjNumber ).toString(true) + " " + // TODO: AttrObjNumber kann leer sein
						o.getValue( AttrObjText ).toString(true);
				else
					return o.getValue( AttrObjText ).getStr(); // kann auch html sein
			}
			break;
		case Qt::ToolTipRole:
			if( d_filter == TitleOnly )
				return data( index, Qt::DisplayRole );
			break;
		}
	}else if( index.column() > 0 && ( role == Qt::DisplayRole || role == Qt::ToolTipRole ) )
	{
		Obj o = d_doc.getTxn()->getObject( s->d_oid );
		Q_ASSERT( (index.column()-1) < d_cols.size() );
		return TypeDefs::prettyValue( o.getValue( d_cols[index.column()-1].second ) );
	}
	return QVariant();
}

bool DocMdl::hasChildren( const QModelIndex & parent ) const
{
	if( d_doc.isNull() )
		return false;
	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	if( !p->d_subs.isEmpty() )
		return true;
	else if( p->d_empty )
		return false;

	const bool res = canFetchMore( parent );
	if( !res )
		p->d_empty = true;
	return res;
}

bool DocMdl::canFetchMore ( const QModelIndex & parent ) const
{
	if( d_doc.isNull() )
		return false;
	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	if( p->d_empty )
		return false;
	Obj o;
	if( p->d_subs.isEmpty() )
	{
		o = d_doc.getTxn()->getObject( p->d_oid );
		o = o.getFirstObj();
	}else
	{
		o = d_doc.getTxn()->getObject( p->d_subs.last()->d_oid );
		if( !o.isNull() )
		{
			if( !o.next() )
				return 0;
		}
	}
	if( !o.isNull() ) do
	{
		const quint32 type = o.getType();
		switch( d_filter )
		{
		case TitleAndBody:
			if( ( type == TypeSection || type == TypeTitle ||
				  type == TypeTable || type == TypePicture ) && callLuaFilter( o ) )
				return true;
			break;
		case TitleOnly:
			if( type == TypeTitle && callLuaFilter( o ) )
				return true;
			break;
		case BodyOnly:
			if( ( type == TypeSection || type == TypeTable || type == TypePicture ) && callLuaFilter( o ) )
				return true;
			break;
		}
	}while( o.next() );
	return false;
}

DocMdl::Slot* DocMdl::createSlot( Slot* p, quint64 oid, quint32 type )
{
	Slot* s = new Slot();
	s->d_oid = oid;
	s->d_super = p;
	p->d_subs.append( s );
	d_map[ s->d_oid ] = s;
	s->d_level = ( type == TypeTitle )? p->d_level + 1 : 0;
	return s;
}

void DocMdl::fetchPic( Slot* s, const Obj& o )
{
	Stream::DataCell v = o.getValue( AttrPicImage );
	if( v.isImg() )
	{
		s->d_text = new TextDocument( this );
		Txt::TextCursor cur( s->d_text );
		QImage img;
		v.getImage( img );
		cur.insertImg( img );
	}
}

void DocMdl::fetchText( Slot* s, const Obj& o )
{
	Stream::DataCell v = o.getValue( AttrObjText );
	if( v.isBml() )
	{
		Txt::TextInStream in;
		Stream::DataReader r( v );
		s->d_text = in.readFrom( r, this );
		if( s->d_text == 0 )
			qWarning() << in.getError();
	}else if( v.isHtml() )
	{
		s->d_text = new TextDocument( this );
		s->d_text->setDefaultFont( Txt::Styles::inst()->getFont( 0 ) );
		s->d_text->setHtml( v.getStr() ); // TODO: embedded Images
	}
}

void DocMdl::fetchTable( Slot* s, const Obj& o )
{
	Obj r = o.getFirstObj();
	int rows = 0;
	int cols = 0;
	if( !r.isNull() ) do
	{
		if( r.getType() == TypeTableRow )
		{
			rows++;
			int count = 0;
			Obj c = r.getFirstObj();
			if( !c.isNull() ) do
			{
				if( c.getType() == TypeTableCell )
					count++;
			}while( c.next() );
			cols = qMax( cols, count );
		}
	}while( r.next() );
	s->d_text = new TextDocument( this );
	s->d_text->setDefaultFont( Txt::Styles::inst()->getFont( 0 ) );
	if( rows && cols )
	{
		Txt::TextCursor cur( s->d_text );
		cur.addTable( rows, cols );
		int row = 0;
		r = o.getFirstObj();
		if( !r.isNull() ) do
		{
			if( r.getType() == TypeTableRow )
			{
				int col = 0;
				Obj c = r.getFirstObj();
				if( !c.isNull() ) do
				{
					if( c.getType() == TypeTableCell )
					{
						cur.gotoRowCol( row, col );
						Stream::DataCell v = c.getValue( AttrObjText );
						if( v.isBml() )
						{
							Txt::TextInStream in;
							Stream::DataReader r( v );
							in.readFromTo( r, cur );
						}else if( v.isStr() )
							cur.insertText( v.getStr() ); // kann auch html sein
                        // TODO: verschachtelte Tabellen?
						col++;
					}
				}while( c.next() );
				row++;
			}
		}while( r.next() );
	}
}

int DocMdl::fetch( Slot* p, bool all )
{
	if( p->d_empty )
		return 0;
	Obj o;
	if( p->d_subs.isEmpty() )
	{
		o = d_doc.getTxn()->getObject( p->d_oid );
		o = o.getFirstObj();
	}else
	{
		o = d_doc.getTxn()->getObject( p->d_subs.last()->d_oid );
		if( !o.isNull() )
		{
			if( !o.next() )
				return 0;
		}
	}

	const int maxBatch = 20; // RISK
	int n = 0;
	if( !o.isNull() ) do
	{
		const quint32 type = o.getType();
		switch( type )
		{
		case TypeSection:
			if( ( d_filter == TitleAndBody || d_filter == BodyOnly ) && callLuaFilter( o ) )
			{
				fetchText( createSlot( p, o.getOid(), type ), o );
				n++;
			}
			break;
		case TypeTitle:
			if( ( d_filter == TitleAndBody || d_filter == TitleOnly ) && callLuaFilter( o ) )
			{
				fetchText( createSlot( p, o.getOid(), type ), o );
				n++;
			}
			break;
		case TypeTable:
			if( ( d_filter == TitleAndBody || d_filter == BodyOnly ) && callLuaFilter( o ) )
			{
				fetchTable( createSlot( p, o.getOid(), type ), o );
				n++;
			}
			break;
		case TypePicture:
			if( ( d_filter == TitleAndBody || d_filter == BodyOnly ) && callLuaFilter( o ) )
			{
				fetchPic( createSlot( p, o.getOid(), type ), o );
				n++;
			}
			break;
		}
	}while( o.next() && ( all || n < maxBatch ) );
	return n;
}

bool DocMdl::callLuaFilter(const Obj & o) const
{
	if( d_luaFilter == LUA_NOREF )
		return true;
	Lua::Engine2* e = Lua::Engine2::getInst();
	Q_ASSERT( e != 0 );
	lua_rawgeti( e->getCtx(), LUA_REGISTRYINDEX, d_luaFilter );
	LuaBinding::pushObject( e->getCtx(), o );
	LuaBinding::pushObject( e->getCtx(), d_doc );
	if( !e->runFunction( 2, 1 ) )
	{
		try
		{
			qDebug() << "DocMdl::callLuaFilter:" << e->getLastError();
			e->error( e->getLastError() );
		}catch( const std::exception& e )
		{
			qDebug( "DocMdl::callLuaFilter: Error calling host: %s", e.what() );
		}catch( ... )
		{
			qDebug( "DocMdl::callLuaFilter: unknown exception while calling host" );
		}
	}else
	{
		const bool res = lua_toboolean( e->getCtx(), -1 );
		lua_pop( e->getCtx(), 1 ); // result
		return res;
	}
	return true;
}

void DocMdl::fetchMore ( const QModelIndex & parent )
{
	fetchLevel( parent );
}

void DocMdl::fetchLevel( const QModelIndex & parent, bool all )
{
	if( d_doc.isNull() )
		return;

	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	const int n = fetch( p, all );
	if( n == 0 )
		return;

	beginInsertRows( parent, p->d_subs.size() - n, p->d_subs.size() - 1 );
	endInsertRows();
}

void _fetchAll( DocMdl* mdl, const QModelIndex & parent )
{
	mdl->fetchLevel( parent, true );
	const int count = mdl->rowCount( parent );
	for( int row = 0; row < count; row++ )
		_fetchAll( mdl, mdl->index( row, 0, parent ) );
}

void DocMdl::fetchAll()
{
	_fetchAll( this, QModelIndex() );
}

void DocMdl::onDbUpdate( Sdb::UpdateInfo info )
{
	if( d_doc.isNull() )
		return;
	switch( info.d_kind )
	{
	case UpdateInfo::DbClosing:
		setDoc( Obj() );
		break;
	case UpdateInfo::ValueChanged:
		if( d_map.contains( info.d_id ) && 
			( info.d_name == AttrReviewStatus || info.d_name == AttrAnnotated ) )
		{
			Slot* s = d_map.value( info.d_id ); 
			const int row = s->d_super->d_subs.indexOf( s );
			Q_ASSERT( row >= 0 );
			const QModelIndex i = createIndex( row, 0, s );
			emit dataChanged( i, i );
		}
		break;
	}
}

QVariant DocMdl::headerData ( int section, Qt::Orientation orientation, int role ) const
{
	if( orientation == Qt::Horizontal && ( role == Qt::DisplayRole || role == Qt::ToolTipRole ) )
	{
		if( section == 0 )
			return tr("Title/Text");
		else if( section - 1 < d_cols.size() )
			return d_cols[ section - 1 ].first;
	}
	return QVariant();
}

bool DocMdl::setData ( const QModelIndex & index, const QVariant & value, int role )  
{
	if( role == Qt::SizeHintRole )
	{
		// Trick um Height-Cache zu invalidieren
		emit dataChanged( index, index );
		return true;
	}
	return false;
}

quint64 DocMdl::getOid( const QModelIndex & index ) const
{
	if( d_doc.isNull() )
		return 0;
	Slot* p;
	if( index.isValid() )
		p = static_cast<Slot*>( index.internalPointer() );
	else
		p = d_root;
	return p->d_oid;
}

QModelIndex DocMdl::findIndex( quint64 oid, bool title )
{
	if( d_doc.isNull() || oid == 0 || d_doc.getId() == oid )
		return QModelIndex();

	Slot* s = d_map.value( oid ); // Best Guess
	if( s != 0 )
	{
		// Das Objekt wurde bereits geladen.
		if( title )
		{
			while( s && s->d_super && s->d_super->d_super && s->d_level == 0 )
				s = s->d_super;
		}
		const int row = s->d_super->d_subs.indexOf( s );
		Q_ASSERT( row >= 0 );
		return createIndex( row, 0, s );
	}
	// Das Objekt wurde noch nicht geladen
	QModelIndex res;
	QList<quint64> path;
	Obj o = d_doc.getTxn()->getObject( oid );
	while( !o.isNull() && d_doc.getOid() != o.getOid() )
	{
		path.prepend( o.getOid() );
		o = o.getOwner();
	}
	if( o.isNull() )
		return QModelIndex();
	// path beinhaltet nun Grossvater, Vater, Sohn, ...
	for( int i = 0; i < path.size(); i++ )
	{
		// Suche von oben nach unten und lade jeweils die Levels soweit erforderlich
		QModelIndex sub = findInLevel( res, path[i] );
		if( !sub.isValid() )
			break;
		res = sub;
	}
	if( title )
	{
		while( res.isValid() && res.data( LevelRole ).toInt() == 0 )
			res = res.parent();
	}
	return res;
}

QModelIndex DocMdl::findInLevel( const QModelIndex & parent, quint64 oid )
{
	if( d_doc.isNull() || oid == 0 || d_doc.getId() == oid )
		return QModelIndex();
	Slot* p;
	if( parent.isValid() )
		p = static_cast<Slot*>( parent.internalPointer() );
	else
		p = d_root;
	int i = 0;
	do
	{
		if( i >= p->d_subs.size() )
		{
			const int n = fetch( p );
			if( n == 0 )
				break; // Nichts gefunden auf Level, trotz vollständigem Fetch
			beginInsertRows( parent, p->d_subs.size() - n, p->d_subs.size() - 1 );
			endInsertRows();
		}
		if( p->d_subs[i]->d_oid == oid )
			return createIndex( i, 0, p->d_subs[i] );
		i++;
	}while( true ); // NOTE: ansonsten keine Chance für fetch: i < p->d_subs.size() );
	return QModelIndex();
}

QModelIndex DocMdl::getFirstIndex() const
{
	if( d_doc.isNull() || d_root->d_subs.isEmpty() )
		return QModelIndex();
	else
        return createIndex( 0, 0, d_root->d_subs.first() );
}

QVariant TextDocument::loadResource(int type, const QUrl &name)
{
    if( type == QTextDocument::ImageResource && name.scheme() == "data" )
    {
        // Es soll ein embedded image geladen werden
        // name.path() enthält "image/png;base64,..."
        const QString src = name.path();
        const int pos1 = src.indexOf(QLatin1String("image/"),0, Qt::CaseInsensitive);
        if( pos1 != -1 )
        {
            const int pos2 = src.indexOf( QChar(';'), pos1 + 6 );
            if( pos2 != -1 )
            {
                const QString imgType = src.mid( pos1 + 6, pos2 - pos1 -6 ).toUpper();
                const int pos3 = src.indexOf(QLatin1String("base64,"),pos2 + 1, Qt::CaseInsensitive);
                if( pos3 != -1 )
                {
                    const QByteArray base64 = src.mid( pos3 + 7 ).toAscii();
                    QImage img;
                    if( img.loadFromData( QByteArray::fromBase64( base64 ), imgType.toAscii() ) )
                    {
                        return img;
                    }
                }
            }
        }
    }
    return QTextDocument::loadResource( type, name );
}
