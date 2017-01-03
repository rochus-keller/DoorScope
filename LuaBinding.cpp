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

#include "LuaBinding.h"
#include <Script2/ValueBinding.h>
#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Sdb/Database.h>
#include <Sdb/Transaction.h>
#include <Txt/TextOutHtml.h>
#include <Txt/TextInStream.h>
#include <QTextDocument>
#include <QFileDialog>
#include <QApplication>
#include <QSettings>
#include <Script2/QtValue.h>
#include "TypeDefs.h"
#include "HistMdl.h"
#include "DocSelectorDlg.h"
#include "AppContext.h"
using namespace Lua;
using namespace Ds;

/* TODO
    Annotation Handling
    Set Review Status
    guarded Pointer to History und Annotations
    Document Indexer
    Object List in Viewer, deren Inhalt mit einem Lua-Statement gefllt werden kann
    Allgemeine Ergebnisliste mit Doppelklick-Navigation und ev. entfernen aus der Liste
    Spter:
    Table Aggregation (kann man auch selber in Lua)
    Attr Selector Dlg
	Hourglass
  */

struct _String : public QString
{
	_String(){}
	_String( const QString& str ):QString(str) {}
	bool operator<( const _String& rhs ) const { return compare(rhs) < 0; }
	bool operator<=( const _String& rhs ) const { return compare(rhs) <= 0; }
	bool operator==( const _String& rhs ) const { return compare(rhs) == 0; }
	static int toLatin1( lua_State * L)
    {
        if( lua_isuserdata( L, 1 ) )
        {
			QString* obj = QtValue<QString>::check( L, 1 );
			lua_pushstring( L, obj->toLatin1() );
        }else
        {
            lua_pushstring( L, lua_tostring( L, 1 ) );
        }
        return 1;
    }
    static int toUtf8( lua_State * L)
    {
		QString* obj = QtValue<QString>::check( L, 1 );
		lua_pushstring( L, obj->toUtf8() );
        return 1;
    }
	static int init( lua_State * L)
	{
		QString* obj = QtValue<QString>::check( L, 1 );
		*obj = lua_tostring( L, 2 );
		return 0;
	}
    static int fromUtf8( lua_State * L)
    {
		QString* obj = QtValue<QString>::check( L, 1 );
		*obj = QString::fromUtf8( lua_tostring( L, 2 ) );
        return 0;
    }
    static int fromLatin1( lua_State * L)
    {
		QString* obj = QtValue<QString>::check( L, 1 );
		*obj = QString::fromLatin1( lua_tostring( L, 2 ) );
        return 0;
    }

};
static const luaL_reg _String_reg[] =
{
    { "toLatin1", _String::toLatin1 },
    { "toUtf8", _String::toUtf8 },
    { "fromUtf8", _String::fromUtf8 },
    { "fromLatin1", _String::fromLatin1 },
	{ "init", _String::init },
    { 0, 0 }
};

struct _SpecialValue
{
    // Reprsentiert HTML, BML RichText, Date, DateTime, Time, Image
    _SpecialValue() { d_value.setNull(); }
    Stream::DataCell d_value;
    bool operator==( const _SpecialValue& rhs ) const { return d_value.equals( rhs.d_value ); }

    static QByteArray toIsoDate( const Stream::DataCell& v )
    {
        switch( v.getType() )
        {
        case Stream::DataCell::TypeDateTime:
            return v.getDateTime().toString( Qt::ISODate ).toLatin1();
        case Stream::DataCell::TypeDate:
            return v.getDate().toString( Qt::ISODate ).toLatin1();
        case Stream::DataCell::TypeTime:
            return v.getTime().toString( Qt::ISODate ).toLatin1();
        default:
            return QByteArray();
        }
    }

    static QString renderString( const Stream::DataCell& v, const QByteArray& format = QByteArray() )
    {
        if( !format.isEmpty() )
        {
            switch( v.getType() )
            {
            case Stream::DataCell::TypeDate:
                return v.getDate().toString( format );
            case Stream::DataCell::TypeTime:
                return v.getTime().toString( format );
            case Stream::DataCell::TypeDateTime:
                return v.getDateTime().toString( format );
			default:
				break;
            }
        }
        switch( v.getType() )
        {
        case Stream::DataCell::TypeImg:
            return QString::fromAscii( v.getArr().toBase64() );
        case Stream::DataCell::TypeNull:
            return "nil";
        case Stream::DataCell::TypeHtml:
            {
                QTextDocument doc;
                doc.setHtml( v.getStr() );
                return doc.toPlainText();
            }
            break;
        case Stream::DataCell::TypeDate:
        case Stream::DataCell::TypeTime:
        case Stream::DataCell::TypeDateTime:
            return QString::fromLatin1( toIsoDate( v ) );
        default:
            return TypeDefs::prettyValue( v ).toString();
        }
    }

    static int toLatin1( lua_State * L)
    {
        if( lua_isuserdata( L, 1 ) )
        {
            _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
            QByteArray format;
            if( lua_isstring( L, 2 ) )
                format = lua_tostring( L, 2 );
            if( obj->d_value.isImg() )
                lua_pushstring( L, obj->d_value.getArr().toBase64() );
            else
                lua_pushstring( L, renderString( obj->d_value, format ).toLatin1() );
        }else
        {
            lua_pushstring( L, lua_tostring( L, 1 ) );
        }
        return 1;
    }
    static int toString( lua_State * L)
    {
        _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
        QByteArray format;
        if( lua_isstring( L, 2 ) )
            format = lua_tostring( L, 2 );
		_String* str = QtValue<_String>::create( L );
		*str = renderString( obj->d_value, format );
        return 1;
    }
    static int getValueType( lua_State* L )
    {
        _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
        lua_pushstring( L, obj->d_value.getTypeName() );
        return 1;
    }
    static int getDate( lua_State* L )
    {
        _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
        QDate d = obj->d_value.getDate(); // wandelt intern
        lua_pushinteger( L, d.year() );
        lua_pushinteger( L, d.month() );
        lua_pushinteger( L, d.day() );
        return 3;
    }
    static int getTime( lua_State* L )
    {
        _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
        QTime t = obj->d_value.getTime();
        lua_pushinteger( L, t.hour() );
        lua_pushinteger( L, t.minute() );
        lua_pushinteger( L, t.second() );
        return 3;
    }
    static int toHtml( lua_State* L )
    {
        if( lua_isuserdata(L, 1 ) )
        {
            // Params: this, fragmentOnly, charset
            _SpecialValue* obj = ValueBinding<_SpecialValue>::check( L, 1 );
			_String* str = QtValue<_String>::create( L );
            QByteArray charset = "latin-1";
            if( lua_isstring( L, 3 ) )
                charset = lua_tostring( L, 3 );
            if( obj->d_value.isHtml() )
            {
                if( lua_toboolean( L, 2 ) )
                {
                    QTextDocument doc;
                    doc.setHtml( obj->d_value.getStr() );
                    QTextDocumentFragment f( &doc );
					*str = f.toHtml( charset );
                }else
					*str = obj->d_value.getStr();
            }else if( obj->d_value.isBml() )
            {
                QTextDocument doc;
                Txt::TextInStream in;
                in.readFromTo( obj->d_value, &doc );
                Txt::TextOutHtml out( &doc );
				*str = out.toHtml( lua_toboolean( L, 2 ), charset );
            }else if( obj->d_value.isDate() || obj->d_value.isTime() || obj->d_value.isDateTime() )
            {
                if( lua_toboolean( L, 2 ) )
					*str = QString::fromAscii(toIsoDate( obj->d_value ));
                else
					*str = QString( "<html><body>%1</body></html>" ).arg(
                                        QString::fromLatin1( toIsoDate( obj->d_value ) ) );
            }else if( obj->d_value.isImg() )
            {
                if( lua_toboolean( L, 2 ) )
					*str = QString( "<img src=\"data:image/png;base64,%1\"/>" ).arg(
                            QLatin1String( obj->d_value.getArr().toBase64() ) );
                else
					*str = QString(
                        "<html><body><img src=\"data:image/png;base64,%1\"/></body></html>" ).arg(
                            QLatin1String( obj->d_value.getArr().toBase64() ) );
            }else
            {
                if( lua_toboolean( L, 2 ) )
					*str = obj->d_value.toString();
                else
					*str = QString( "<html><body>%1</body></html>" ).arg( obj->d_value.toString() );
            }
        }else
        {
			_String* str = QtValue<_String>::create( L );
			*str = QString::fromLatin1( lua_tostring( L, 1 ) );
        }
        return 1;
    }
    static int toHtmlDiff( lua_State* L )
    {
        // Params: new, old, fragment, charset
        QString newText, oldText;
        if( lua_isuserdata(L, 1 ) )
        {
            if( _SpecialValue* obj = ValueBinding<_SpecialValue>::cast( L, 1 ) )
                newText = renderString( obj->d_value );
            else
				newText = *QtValue<_String>::check( L, 1 );
        }else
            newText = lua_tostring( L, 1 );
        if( lua_isuserdata(L, 2 ) )
        {
            if( _SpecialValue* obj = ValueBinding<_SpecialValue>::cast( L, 2 ) )
                oldText = renderString( obj->d_value );
            else
				oldText = *QtValue<_String>::check( L, 2 );
        }else
            oldText = lua_tostring( L, 1 );
        QByteArray charset = "latin-1";
        if( lua_isstring( L, 4 ) )
            charset = lua_tostring( L, 4 );
        QTextDocument doc;
        QTextCursor cur( &doc );
        HistMdl::printDiff( cur, newText, oldText );
		_String* str = QtValue<_String>::create( L );
        if( lua_toboolean( L, 3 ) )
        {
            QTextDocumentFragment f( &doc );
			*str = f.toHtml(charset);
        }else
			*str = doc.toHtml(charset);
        return 1;
    }
};

static const luaL_reg SpecialValue_reg[] =
{
    { "toHtmlDiff", _SpecialValue::toHtmlDiff },
    { "toLatin1", _SpecialValue::toLatin1 },
    { "toString", _SpecialValue::toString },
    { "getValueType", _SpecialValue::getValueType },
    { "getDate", _SpecialValue::getDate },
    { "getTime", _SpecialValue::getTime },
    { "toHtml", _SpecialValue::toHtml },
    { 0, 0 }
};

struct _ContentObject
{
    Sdb::Obj d_obj;
    void checkValid( lua_State *L )
    {
        if( d_obj.isNull() )
            luaL_error( L, "accessing null reference");
        if( d_obj.isDeleted() )
            luaL_error( L, "accessing deleted object");
    }

    static int index(lua_State *L)
    {
        _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
        obj->checkValid(L);
        const char* fieldName = luaL_checkstring( L, 2 );
        const quint32 atom = TypeDefs::findAtom( obj->d_obj.getDb(), fieldName, obj->d_obj.getType() );
        if( atom )
            return pushValue( L, atom, obj->d_obj );
        else
			return ValueBinding<_ContentObject>::fetch( L, true, true );
    }
    static int newindex(lua_State *L)
    {
        _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
        obj->checkValid(L);
        const char* fieldName = luaL_checkstring( L, 2 );
        // Hier prfen wir nur die eingebauten Atome
        const quint32 atom = TypeDefs::findAtom( fieldName, obj->d_obj.getType() );
        if( atom )
        {
            luaL_error( L, "cannot write to '%s'", fieldName );
            return 0;
        }else
            return ValueBinding<_ContentObject>::newindex( L );
    }
    static int pushValue(lua_State *L, quint32 atom, const Sdb::Obj& obj )
    {
        if( obj.isNull() )
            lua_pushnil( L );
        else
        {
            // Reprsentiert HTML, BML RichText, Date, DateTime, Time, Image
            const Stream::DataCell v = obj.getValue( atom );
            switch( v.getType() )
            {
            case Stream::DataCell::TypeNull:
            case Stream::DataCell::TypeInvalid:
                lua_pushnil( L );
                break;
            case Stream::DataCell::TypeTrue:
            case Stream::DataCell::TypeFalse:
                lua_pushboolean( L, v.getBool() );
                break;
            case Stream::DataCell::TypeDouble:
                lua_pushnumber( L, v.getDouble() );
                break;
            case Stream::DataCell::TypeLatin1:
            case Stream::DataCell::TypeAscii:
                lua_pushstring( L, v.toString().toLatin1() );
                break;
            case Stream::DataCell::TypeString:
                {
					_String* obj = QtValue<_String>::create( L );
					*obj = v.getStr();
                }
                break;
            case Stream::DataCell::TypeUInt8:
            case Stream::DataCell::TypeUInt16:
            case Stream::DataCell::TypeInt32:
            case Stream::DataCell::TypeUInt32:
                lua_pushinteger( L, v.toVariant().toInt() );
                break;
            case Stream::DataCell::TypeOid:
				LuaBinding::pushObject( L, obj.getObject( atom ) );
                break;
            case Stream::DataCell::TypeAtom:
                lua_pushstring( L, TypeDefs::getSimpleName( atom, obj.getDb() ) );
                break;
            default:
                {
                    _SpecialValue* obj = ValueBinding<_SpecialValue>::create( L );
                    obj->d_value = v;
                }
                break;
            }
        }
        return 1;
    }
    static int getFolders(lua_State *L);
    static int getDocuments(lua_State *L);
    static int getContent( lua_State *L);
    static int getAnnotations( lua_State *L);
    static int getValues(lua_State *L)
    {
        _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
        obj->checkValid(L);
        Sdb::Orl::Names names = obj->d_obj.getNames();
        lua_newtable( L );
        const int table = lua_gettop( L );
        foreach( Sdb::Atom a, names )
        {
            lua_pushstring( L, TypeDefs::getSimpleName( a, obj->d_obj.getDb() ));
            pushValue( L, a, obj->d_obj );
            lua_rawset( L, table );
        }
        return 1;
    }
    typedef QList<Sdb::Atom> TypeList;
    static int getObjectsOfType( lua_State *L, const TypeList& types )
    {
        _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
        obj->checkValid(L);
        lua_newtable( L );
        const int table = lua_gettop( L );
        Sdb::Obj o = obj->d_obj.getFirstObj();
        int i = 1;
        if( !o.isNull() ) do
        {
            if( types.contains( o.getType() ) )
            {
				LuaBinding::pushObject( L, o );
                lua_rawseti( L, table, i );
                i++;
            }
        }while( o.next() );
        return 1;
    }
    static int getHistory( lua_State *L );
    static int getParent(lua_State *L)
    {
        _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
        obj->checkValid(L);
		LuaBinding::pushObject( L, obj->d_obj.getOwner() );
        return 1;
    }
};
static const luaL_reg ContentObject_reg[] =
{
    { "getParent", _ContentObject::getParent },
    { "getHistory", _ContentObject::getHistory },
    { "getAnnotations", _ContentObject::getAnnotations },
    { "getContent", _ContentObject::getContent },
    { "getValues", _ContentObject::getValues },
    { "getFolders", _ContentObject::getFolders },
    { "getDocuments", _ContentObject::getDocuments },
    { 0, 0 }
};
static const luaL_reg Meta_reg[] =
{
    { "__index", _ContentObject::index },
    { "__newindex", _ContentObject::newindex },
    { 0, 0 }
};

struct _Folder : public _ContentObject
{
};

int _ContentObject::getFolders(lua_State *L)
{
    return getObjectsOfType( L, TypeList() << TypeFolder );
}

struct _Document : public _ContentObject
{
};

int _ContentObject::getDocuments(lua_State *L)
{
    return getObjectsOfType( L, TypeList() << TypeDocument );
}

int _ContentObject::getAnnotations(lua_State *L)
{
    return getObjectsOfType( L, TypeList() << TypeAnnotation );
}

int _ContentObject::getContent(lua_State *L)
{
    return getObjectsOfType( L, TypeList() << TypeSection << TypeTitle << TypePicture
                             << TypeTable << TypeTableRow << TypeTableCell );
}

struct _Repository : public _ContentObject
{
    static int selectDocument(lua_State *L)
    {
        _Repository* obj = ValueBinding<_Repository>::check( L, 1 );
        obj->checkValid(L);
        DocSelectorDlg dlg( QApplication::activeWindow() );
        QString title = "Script: Select Document - DoorScope";
        if( lua_isstring( L, 2 ) )
            title = lua_tostring( L, 2 );
        dlg.setWindowTitle( title );
        dlg.resize( AppContext::inst()->getSet()->value(
                        "DocSelectorDlg/Size", QSize( 400, 400 ) ).toSize() ); // RISK
        const quint64 res = dlg.select( obj->d_obj );
		LuaBinding::pushObject( L, AppContext::inst()->getTxn()->getObject( res ) );
        return 1;
    }
};
static const luaL_reg _Repository_reg[] =
{
    { "selectDocument", _Repository::selectDocument },
    { 0, 0 }
};
struct _Annotation : public _ContentObject
{
    // RISK: kann gelscht werden via GUI
};
struct _Object : public _ContentObject
{
    static int getLinks(lua_State *L)
    {
        return getObjectsOfType( L, TypeList() << TypeOutLink << TypeInLink );
    }
};
static const luaL_reg _Object_reg[] =
{
    { "getLinks", _Object::getLinks },
    { 0, 0 }
};
struct _Title : public _Object
{
};
struct _Section : public _Object
{
};
struct _Table : public _Object
{
};
struct _TableRow : public _Object
{
};
struct _TableCell : public _Object
{
};
struct _Picture : public _Object
{
};
struct _Link : public _ContentObject
{
    static int getStub(lua_State *L)
    {
        _Link* obj = ValueBinding<_Link>::check( L, 1 );
        obj->checkValid(L);
        Sdb::Obj o = obj->d_obj.getFirstObj();
        if( !o.isNull() ) do
        {
            if( o.getType() == TypeStub )
            {
				LuaBinding::pushObject( L, o );
                return 1;
            }
        }while( o.next() );
        lua_pushnil(L);
        return 1;
    }
};
static const luaL_reg _Link_reg[] =
{
    { "getStub", _Link::getStub },
    { 0, 0 }
};
struct _OutLink : public _Link
{
};
struct _InLink : public _Link
{
};
struct _Stub : public _Object
{
};
struct _History : public _ContentObject
{
    // RISK: kann gelscht werden via GUI
};
int _ContentObject::getHistory( lua_State *L )
{
    _ContentObject* obj = ValueBinding<_ContentObject>::check( L, 1 );
    obj->checkValid(L);
    lua_newtable( L );
    const int table = lua_gettop( L );
    Sdb::Qit i = obj->d_obj.getFirstSlot();
    int n = 1;
	if( !i.isNull() ) do
	{
        Sdb::Obj o = obj->d_obj.getTxn()->getObject( i.getValue() );
		if( !o.isNull() && o.getType() == TypeHistory )
		{
			LuaBinding::pushObject( L, o );
            lua_rawseti( L, table, n );
            n++;
		}
	}while( i.next() );
    return 1;
}

QByteArray _escape(const QByteArray& plain)
{
    // Adaption von Qt::escape aus qtextdocument.cpp
    QByteArray rich;
    rich.reserve(int(plain.length() * 1.1));
    for (int i = 0; i < plain.length(); ++i) {
        if (plain.at(i) == '<')
            rich += "&lt;";
        else if (plain.at(i) == '>')
            rich += "&gt;";
        else if (plain.at(i) == '&')
            rich += "&amp;";
        else
            rich += plain.at(i);
    }
    return rich;
}

struct _File
{
    QFile d_file;
    _File& operator=( const _File& rhs ) { return *this; } // Dummy-Operator

    static int openForWriting(lua_State *L)
    {
        QString title( "Script: Open File - DoorScope" );
        if( lua_isstring( L, 1 ) )
            title = QString::fromLatin1( lua_tostring( L, 1 ) );
        QString path = QFileDialog::getSaveFileName( QApplication::activeWindow(), title );
        if( path.isEmpty() )
        {
            lua_pushnil( L );
            return 1;
        }
        _File* obj = ValueBinding<_File>::create( L );
        obj->d_file.setFileName( path );
        if( !obj->d_file.open( QFile::WriteOnly ) )
            luaL_error( L, "cannot open file for writing: %s", qPrintable( path ) );
        return 1;
    }
    static int openForReading(lua_State *L)
    {
        QString title( "Script: Open File - DoorScope" );
        if( lua_isstring( L, 1 ) )
            title = QString::fromLatin1( lua_tostring( L, 1 ) );
        QString path = QFileDialog::getOpenFileName( QApplication::activeWindow(), title );
        if( path.isEmpty() )
        {
            lua_pushnil( L );
            return 1;
        }
        _File* obj = ValueBinding<_File>::create( L );
        obj->d_file.setFileName( path );
        if( !obj->d_file.open( QFile::ReadOnly ) )
            luaL_error( L, "cannot open file for reading: %s", qPrintable( path ) );
        return 1;
    }
    static int write(lua_State *L)
    {
        _File* obj = ValueBinding<_File>::check( L );
        if( !obj->d_file.isWritable() )
            luaL_error( L, "cannot write to file: %s", qPrintable( obj->d_file.fileName() ) );
        size_t len = 0;
        const char* data = lua_tolstring( L, 2, &len );
        if( !data )
            luaL_argerror ( L, 2, "expecting a string value" );
        if( lua_toboolean( L, 3 ) )
            // Escape String
            len = obj->d_file.write( _escape( QByteArray::fromRawData( data, len ) ) );
        else
            len = obj->d_file.write( data, len ); // Es wird ohne Nullzeichen geschrieben!
        lua_pushinteger( L, len );
        return 1;
    }
    static int read(lua_State *L)
    {
        _File* obj = ValueBinding<_File>::check( L );
        if( !obj->d_file.isReadable() )
            luaL_error( L, "cannot read from file: %s", qPrintable( obj->d_file.fileName() ) );
        const int len = lua_tointeger( L, 2 );
        QByteArray data;
        if( len <= 0 )
            data = obj->d_file.readAll();
        else
            data = obj->d_file.read( len );
        lua_pushlstring( L, data.data(), data.length() + 1 ); // inkl. Nullzeichen
        return 1;
    }
    static int isEof(lua_State *L)
    {
        _File* obj = ValueBinding<_File>::check( L );
        if( !obj->d_file.isReadable() )
            luaL_error( L, "cannot read from file: %s", qPrintable( obj->d_file.fileName() ) );
        lua_pushboolean( L, obj->d_file.atEnd() );
        return 1;
    }
    static int close(lua_State *L)
    {
        _File* obj = ValueBinding<_File>::check( L );
        obj->d_file.close();
        return 0;
    }
};
static const luaL_reg _File_reg[] =
{
    { "openForWriting", _File::openForWriting },
    { "openForReading", _File::openForReading },
    { "write", _File::write },
    { "read", _File::read },
    { "isEof", _File::isEof },
    { "close", _File::close },
    { 0, 0 }
};

int LuaBinding::pushObject(lua_State *L, const Sdb::Obj& obj )
{
    // TODO: wiederverwenden von Objekten fr gegebene OID
    _ContentObject* co = 0;
    switch( obj.getType() )
    {
    case 0:
        lua_pushnil( L );
        break;
    case TypeDocument:
        co = ValueBinding<_Document>::create( L );
        break;
    case TypeFolder:
        co = ValueBinding<_Folder>::create( L );
        break;
    case TypeTitle:
        co = ValueBinding<_Title>::create( L );
        break;
    case TypeSection:
        co = ValueBinding<_Section>::create( L );
        break;
    case TypeTable:
        co = ValueBinding<_Table>::create( L );
        break;
    case TypeTableRow:
        co = ValueBinding<_TableRow>::create( L );
        break;
    case TypeTableCell:
        co = ValueBinding<_TableCell>::create( L );
        break;
    case TypePicture:
        co = ValueBinding<_Picture>::create( L );
        break;
    case TypeAnnotation:
        co = ValueBinding<_Annotation>::create( L );
        break;
    case TypeOutLink:
        co = ValueBinding<_OutLink>::create( L );
        break;
    case TypeInLink:
        co = ValueBinding<_InLink>::create( L );
        break;
    case TypeStub:
        co = ValueBinding<_Stub>::create( L );
        break;
    case TypeHistory:
        co = ValueBinding<_History>::create( L );
        break;
    default:
        co = ValueBinding<_ContentObject>::create( L );
        break;
    }
    if( co )
        co->d_obj = obj;
    return 1;
}

static int loader(lua_State * L)
{
	const QByteArray name = lua_tostring( L, 1 );
	if( name.isEmpty() || !name.startsWith( ':' ) )
		return 0;
	const QString plain = name.mid(1);
	Sdb::Obj sub = AppContext::inst()->getRoot().getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == TypeLuaScript && sub.getValue(AttrScriptName).getStr() == plain )
		{
			if( !Lua::Engine2::getInst()->pushFunction( sub.getValue(AttrScriptSource).getStr().toLatin1(), name ) )
				luaL_error( L, Lua::Engine2::getInst()->getLastError() );
			return 1;
		}
	}while( sub.next() );
	return 0;
}

static int type(lua_State * L)
{
	luaL_checkany(L, 1);
	if( lua_type(L,1) == LUA_TUSERDATA )
	{
		ValueBindingBase::pushTypeName( L, 1 );
		if( !lua_isnil( L, -1 ) )
			return 1;
		lua_pop( L, 1 );
	}
	lua_pushstring(L, luaL_typename(L, 1) );
	return 1;
}

void LuaBinding::install(lua_State * L)
{
    ValueBinding<_SpecialValue>::install( L, "SpecialValue", SpecialValue_reg );
    ValueBinding<_SpecialValue>::addMetaMethod( L, "__tostring", _SpecialValue::toLatin1 );
	QtValue<_String,QString>::install( L, "String", _String_reg );
	QtValue<_String>::addMetaMethod( L, "__tostring", _String::toLatin1 );
    ValueBinding<_ContentObject>::install( L, "ContentObject", ContentObject_reg, false );
    ValueBinding<_ContentObject>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Repository, _ContentObject>::install( L, "Repository", _Repository_reg, false );
    ValueBinding<_Repository>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Folder,_ContentObject>::install( L, "Folder", 0, false );
    ValueBinding<_Folder>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Document,_ContentObject>::install( L, "Document", 0, false );
    ValueBinding<_Document>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Annotation,_ContentObject>::install( L, "Annotation", 0, false );
    ValueBinding<_Annotation>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Object,_ContentObject>::install( L, "Object", _Object_reg, false );
    ValueBinding<_Object>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Title,_Object>::install( L, "Title", 0, false );
    ValueBinding<_Title>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Section,_Object>::install( L, "Section", 0, false );
    ValueBinding<_Section>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Table,_Object>::install( L, "Table", 0, false );
    ValueBinding<_Table>::addMetaMethods( L, Meta_reg );
    ValueBinding<_TableRow,_Object>::install( L, "TableRow", 0, false );
    ValueBinding<_TableRow>::addMetaMethods( L, Meta_reg );
    ValueBinding<_TableCell,_Object>::install( L, "TableCell", 0, false );
    ValueBinding<_TableCell>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Picture,_Object>::install( L, "Picture", 0, false );
    ValueBinding<_Picture>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Link,_ContentObject>::install( L, "Link", _Link_reg, false );
    ValueBinding<_Link>::addMetaMethods( L, Meta_reg );
    ValueBinding<_InLink,_Link>::install( L, "InLink", 0, false );
    ValueBinding<_InLink>::addMetaMethods( L, Meta_reg );
    ValueBinding<_OutLink,_Link>::install( L, "OutLink", 0, false );
    ValueBinding<_OutLink>::addMetaMethods( L, Meta_reg );
    ValueBinding<_Stub,_Object>::install( L, "StubObject", 0, false );
    ValueBinding<_Stub>::addMetaMethods( L, Meta_reg );
    ValueBinding<_History,_ContentObject>::install( L, "ChangeEvent", 0, false );
    ValueBinding<_History>::addMetaMethods( L, Meta_reg );
    ValueBinding<_File>::install( L, "File", _File_reg, false );

	lua_getfield( L, LUA_GLOBALSINDEX, "package" );
	if( lua_istable(L, -1 ) )
	{
		lua_getfield( L, -1, "loaders" );
		if( lua_istable(L, -1 ) )
		{
			const int loaders = lua_gettop(L);
			for( int i = lua_objlen( L, loaders ) + 1; i > 1; i-- )
			{
				// Verschiebe alle Loaders um eins nach hinten
				lua_rawgeti(L, loaders, i - 1 );
				lua_rawseti(L, loaders, i ); // setzt intern automatisch #n
			}
			lua_pushcfunction( L, loader );
			lua_rawseti( L, loaders, 1 );
		}
		lua_pop( L, 1 ); // loaders
	}
	lua_pop( L, 1 ); // package
	lua_pushcfunction( L, type );
	lua_setfield( L, LUA_GLOBALSINDEX, "type" );
}

void LuaBinding::setRoot(lua_State *L, const Sdb::Obj &root)
{
	_Repository* obj = ValueBinding<_Repository>::create( L );
    obj->d_obj = root;
    lua_setfield( L, LUA_GLOBALSINDEX, "DoorScope" );
}

