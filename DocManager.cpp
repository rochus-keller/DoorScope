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

#include "DocManager.h"
#include "TypeDefs.h"
#include "AppContext.h"
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Sdb/Transaction.h>
#include <Sdb/Exceptions.h>
#include <Txt/TextInStream.h>
#include <QFile>
#include <QDateTime>
#include <QImage>
#include <QtDebug>
#include <bitset>
#include <cassert>
using namespace Ds;
using namespace Sdb;
using namespace Stream;
using namespace Txt;

struct _NameSlot
{
	const char* s_name;
	quint32 s_id;
};

class _MyException : public std::exception
{
	QByteArray d_msg;
public:
	_MyException( const char* msg = "" ):d_msg(msg) {}
	~_MyException() throw() {}
	const char* what() const throw() { return d_msg; }
};


static _NameSlot s_ContentObject[] =
{
	{ "Created By", AttrCreatedBy },
	{ "Created On", AttrCreatedOn },
	{ "Last Modified By", AttrModifiedBy },
	{ "Last Modified On", AttrModifiedOn },
	{ "Created Thru", AttrCreatedThru },
	{ 0, 0 },
};

static _NameSlot s_Document[] =
{
	{ "~moduleID", AttrDocId },
	{ "~modulePath", AttrDocPath },
	{ "~moduleVersion", AttrDocVer },
	{ "~moduleDescription", AttrDocDesc },
	{ "Name", AttrDocName },
	{ "~moduleType", AttrDocType },
	{ "~isBaseline", AttrDocBaseline },
	{ "~baselineMajor", AttrDocVerMaj },
	{ "~baselineMinor", AttrDocVerMin },
	{ "~baselineSuffix", AttrDocVerSuff },
	{ "~baselineAnnotation", AttrDocVerAnnot },
	{ "~baselineDate", AttrDocVerDate },
	{ "Prefix", AttrDocPrefix },
	{ "~moduleFullName", DsMax }, // ignorieren
	{ "~moduleName", DsMax }, // ignorieren
	{ 0, 0 },
};

static _NameSlot s_Object[] =
{
	{ "Absolute Number", AttrObjIdent },
	{ "~number", AttrObjNumber },
	{ "Object Short Text", AttrObjShort },
	{ "Object Heading", AttrStubTitle },
	{ "Object Text", AttrObjText },
	{ "~deleted", AttrObjDeleted },
	{ "~level", DsMax }, // ignorieren
	{ "~outline", DsMax }, // ignorieren
	{ 0, 0 },
};

static _NameSlot s_Picture[] =
{
	{ "~width", AttrPicWidth },
	{ "~height", AttrPicHeight },
	{ 0, 0 },
};

static _NameSlot s_OutLink[] =
{
	{ "~linkModuleID", AttrLnkModId },
	{ "~linkModuleName", AttrLnkModName },
	{ "~targetObjAbsNo", AttrTrgObjId },
	{ "~targetModID", AttrTrgDocId },
	{ "~targetModName", AttrTrgDocName },
	{ "~targetModVersion", AttrTrgDocVer },
	{ "~targetModLastModified", AttrTrgDocModDate },
	{ "~isTargetModBaseline", AttrTrgDocIsBl },
	{ 0, 0 },
};

static _NameSlot s_InLink[] =
{
	{ "~linkModuleID", AttrLnkModId },
	{ "~linkModuleName", AttrLnkModName },
	{ "~sourceObjAbsNo", AttrSrcObjId },
	{ "~sourceModID", AttrSrcDocId },
	{ "~sourceModName", AttrSrcDocName },
	{ "~sourceModVersion", AttrSrcDocVer },
	{ "~sourceModLastModified", AttrSrcDocModDate },
	{ "~isSourceModBaseline", AttrSrcDocIsBl },
	{ 0, 0 },
};

static _NameSlot s_Hist[] =
{
	{ "~author", AttrHistAuthor },
	{ "~date", AttrHistDate },
	{ "~type", AttrHistType },
	{ "~absNo", AttrHistObjId },
	{ "~attrName", AttrHistAttr },
	{ "~oldValue", AttrHistOld },
	{ "~newValue", AttrHistNew },
	{ "~session", DsMax }, // ignore
	{ 0, 0 },
};

static inline void _put( QMap<QByteArray,quint32>& h, _NameSlot* s )
{
	while( s && s->s_name )
	{
		h[ s->s_name ] = s->s_id;
		s++;
	}
}

static inline void _put2( QMap<QByteArray,quint8>& h, const char** s )
{
	quint8 i = 0;
	while( *s )
	{
		h[ *s ] = i;
		s++;
		i++;
	}
}

DocManager::DocManager()
{
	_put( d_cache, s_ContentObject );
	_put( d_cache, s_Document );
	_put( d_cache, s_Object );
	_put( d_cache, s_Picture );
	_put( d_cache, s_OutLink );
	_put( d_cache, s_InLink );
	_put( d_cache, s_Hist );
	_put2( d_cache2, TypeDefs::historyTypeString );
}

DocManager::~DocManager()
{
}

Sdb::Obj DocManager::importStream( const QString& path )
{
	d_error.clear();
	d_nrToOid.clear();
	d_customObjAttr.clear();
	d_customModAttr.clear();

	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		d_error = tr("cannot open '%1' for reading").arg(path);
		return Obj();
	}
	Database::Lock lock( AppContext::inst()->getDb(), true );
	try
	{
		DataReader in( &f );

		Sdb::Obj doc = AppContext::inst()->getTxn()->createObject( TypeDocument );
		doc.setValue( AttrDocImported, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		Sdb::Obj own = AppContext::inst()->getTxn()->createObject();
		doc.setValue( AttrDocOwning, own );
		own.aggregateTo( doc );

		enum State { ExpectingModule, ReadingModule };
		State state = ExpectingModule;
		DataReader::Token t = in.nextToken();

		// Die ersten Drei Slots sind zur Protokollprüfung
		if( t != DataReader::Slot || !in.getName().isNull() || 
			in.readValue().getStr() != "DoorScopeExport" )
		{
			d_error = tr("unknown stream file format");
			throw _MyException("");
		}
		t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() || 
			in.readValue().getStr() != "0.3" )
		{
			d_error = tr("stream file version not supported");
			throw _MyException("");
		}
		t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() ||
			!in.readValue().isDateTime() )
		{
			throw _MyException("protocol error");
		}else
			doc.setValue( AttrDocExported, in.readValue() );
		t = in.nextToken();

		while( DataReader::isUseful( t ) )
		{
			switch( state )
			{
			case ExpectingModule:
				if( t == DataReader::BeginFrame )
				{
					if( in.getName().getArr() != "mod" )
						throw _MyException("protocol error");
					state = ReadingModule;
				}else
					throw _MyException("protocol error");
				break;
			case ReadingModule:
				if( t == DataReader::Slot )
				{
					if( !readModAttr(  in, doc ) )
						throw _MyException("");
				}else if( t == DataReader::EndFrame )
				{
					state = ExpectingModule;
				}else if( t == DataReader::BeginFrame )
				{
					const QByteArray name = in.getName().getArr();
					if( name == "obj" )
					{
						if( !readObj( in, doc, doc ) )
							throw _MyException("");
					}else if( name == "pic" )
					{
						if( !readPic( in, doc, doc ) )
							throw _MyException("");
					}else if( name == "tbl" )
					{
						if( !readTbl( in, doc, doc ) )
							throw _MyException("");
					}else if( name == "hist" )
					{
						if( !readHist( in, doc, own ) )
							throw _MyException("");
					}else
					{
						d_error += tr("unknown frame ") + name;
						throw _MyException("");
					}
				}
				break;
			}
			t = in.nextToken();
		}

		DataWriter w;
		QSet<quint32>::const_iterator i;
		for( i = d_customObjAttr.begin(); i != d_customObjAttr.end(); ++i )
			w.writeSlot( DataCell().setAtom( *i ) );
		doc.setValue( AttrDocObjAttrs, w.getBml() );

		w.setDevice();
		for( i = d_customModAttr.begin(); i != d_customModAttr.end(); ++i )
			w.writeSlot( DataCell().setAtom( *i ) );
		doc.setValue( AttrDocAttrs, w.getBml() );

		AppContext::inst()->getTxn()->commit();
		lock.commit();
		return doc;
	}catch( DatabaseException& e )
	{
		d_error += tr("invalid stream file format\n");
		d_error += e.getCodeString() + " " + e.getMsg();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return Obj();
	}catch( std::exception& e )
	{
		d_error += e.what();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return Obj();
	}
}

bool DocManager::readAttr( Stream::DataReader& in, Sdb::Obj& obj, quint32 id )
{
	if( id == DsMax )
		return true; // Attribut wird ignoriert
	DataCell v;
	in.readValue( v );
	if( v.isNull() || !v.isValid() )
		return true;
	if( v.isStr() && v.getStr().isEmpty() )
		return true;
	if( v.isArr() && v.getArr().isEmpty() )
		return true;
	if( v.isBml() )
	{
		const QByteArray temp = transformBml( v.getBml() );
		if( !temp.isEmpty() )
			v.setBml( temp );
	}
	if( id == AttrHistAttr )
		v.setAtom( getHistAttr( v.getStr().toLatin1() ) );
	else if( id == AttrHistType )
		v.setUInt8( d_cache2.value( v.getStr().toLatin1() ) );
	obj.setValue( id, v ); 
	if( id == AttrObjIdent )
		d_nrToOid[v.getInt32()] = obj.getId();
	return true;
}

bool DocManager::readModAttr( Stream::DataReader& in, Sdb::Obj& doc )
{
	quint32 id = d_cache.value( in.getName().getArr() );
	QByteArray tmp = in.getName().getArr();
	if( id == 0 )
	{
		id = AppContext::inst()->getTxn()->getAtom( in.getName().getArr() );
		d_customModAttr.insert( id );
	}
	return readAttr( in, doc, id );
}

quint32 DocManager::getHistAttr( const QByteArray& name ) const
{
	quint32 id = d_cache.value( name );
	if( id == 0 )
		id = AppContext::inst()->getTxn()->getAtom( name );
	return id;
}

bool DocManager::readObjAttr( Stream::DataReader& in, Sdb::Obj& o )
{
	quint32 id = d_cache.value( in.getName().getArr() );
	if( id == 0 )
	{
		id = AppContext::inst()->getTxn()->getAtom( in.getName().getArr() );
		d_customObjAttr.insert( id );
	}
	return readAttr( in, o, id );
}

static inline void _aggr( Sdb::Obj& obj, Sdb::Obj& super, const Sdb::Obj& doc )
{
	obj.aggregateTo( super );
	obj.setValue( AttrObjDocId, doc.getValue( AttrDocId ) );
	obj.setValue( AttrObjHomeDoc, doc );
}

bool DocManager::readPic( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj obj = AppContext::inst()->getTxn()->createObject( TypePicture );
	_aggr( obj, super, doc );
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( in.getName().isNull() )
			{
				QImage img;
				in.readValue().getImage( img );
				obj.setValue( AttrPicImage, DataCell().setImage( img ) );
			}else
			{
				if( !readObjAttr( in, obj ) )
					return false;
			}
		}else if( t == DataReader::EndFrame )
		{
			return true;
		}else
		{
			d_error += tr("invalid token ");
			return false;
		}

		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

bool DocManager::readTbl( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj tbl = AppContext::inst()->getTxn()->createObject( TypeTable );
	_aggr( tbl, super, doc );
	enum State { ReadingHead, ReadingRow, ReadingCell };
	State state = ReadingHead;
	Obj row; 
	Obj cell;
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		switch( state )
		{
		case ReadingHead:
			if( t == DataReader::Slot )
			{
				if( !readObjAttr( in, tbl ) )
					return false;
			}else if( t == DataReader::BeginFrame )
			{
				const QString name = in.getName().getArr();
				if( name == "row" )
				{
					row = AppContext::inst()->getTxn()->createObject( TypeTableRow );
					_aggr( row, tbl, doc );
					state = ReadingRow;
				}else
				{
					d_error += tr("unknown frame ") + name;
					return false;
				}
			}else if( t == DataReader::EndFrame )
			{
				return true;
			}
			break;
		case ReadingRow:
			if( t == DataReader::Slot )
			{
				if( !readObjAttr( in, row ) )
					return false;
			}else if( t == DataReader::BeginFrame )
			{
				const QString name = in.getName().getArr();
				if( name == "cell" )
				{
					cell = AppContext::inst()->getTxn()->createObject( TypeTableCell );
					_aggr( cell, row, doc );
					state = ReadingCell;
				}else
				{
					d_error += tr("unknown frame ") + name;
					return false;
				}
			}else if( t == DataReader::EndFrame )
			{
				state = ReadingHead;
			}
			break;
		case ReadingCell:
			if( t == DataReader::Slot )
			{
				if( !readObjAttr( in, cell ) )
					return false;
			}else if( t == DataReader::EndFrame )
			{
				state = ReadingRow;
			}else
			{
				d_error += tr("invalid token");
				return false;
			}
			break;
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

Obj DocManager::splitTitleBody( Sdb::Obj& title, const Sdb::Obj& doc )
{
	const bool hasTitle = !title.getValue( AttrStubTitle ).isNull();
	const bool hasBody = !title.getValue( AttrObjText ).isNull();
    Sdb::Obj body = title;
	if( !hasBody )
	{
		title.setValue( AttrObjText, title.getValue( AttrStubTitle ) );
		title.setValue( AttrStubTitle, DataCell().setNull() );
	}else if( !hasTitle && hasBody )
		title.setType( TypeSection );
	else // if hasTitle && hasBody
	{
		// Hier wird Titel und Body in zwei Objekte aufgespalten, so dass Body
		// das erste Subobject von Titel ist.
		body = AppContext::inst()->getTxn()->createObject( TypeSection );
		d_nrToOid[ title.getValue( AttrObjIdent ).getInt32() ] = body.getId();
		_aggr( body, title, doc );
		body.setValue( AttrObjText, title.getValue( AttrObjText ) );
		title.setValue( AttrObjText, title.getValue( AttrStubTitle ) );
		title.setValue( AttrStubTitle, DataCell().setNull() );
		title.setValue( AttrTitleSplit, body );

		body.setValue( AttrSecSplit, title );
		body.setValue( AttrObjIdent, title.getValue( AttrObjIdent ) ); // dupliziert
		body.setValue( AttrObjShort, title.getValue( AttrObjShort ) );
		body.setValue( AttrObjDeleted, title.getValue( AttrObjDeleted ) );
		body.setValue( AttrObjHomeDoc, title.getValue( AttrObjHomeDoc ) );
		body.setValue( AttrObjNumber, title.getValue( AttrObjNumber ) );
		body.setValue( AttrObjDocId, title.getValue( AttrObjDocId ) );

		body.setValue( AttrCreatedBy, title.getValue( AttrCreatedBy ) );
		body.setValue( AttrCreatedOn, title.getValue( AttrCreatedOn ) );
		body.setValue( AttrModifiedBy, title.getValue( AttrModifiedBy ) );
		body.setValue( AttrModifiedOn, title.getValue( AttrModifiedOn ) );
		body.setValue( AttrCreatedThru, title.getValue( AttrCreatedThru ) );
	
		Sdb::Obj::Names n = title.getNames();
		Sdb::Obj::Names::const_iterator i;
		for( i = n.begin(); i != n.end(); ++i )
		{
			if( *i > DsMax )
				body.setValue( *i, title.getValue( *i ) );
		}
	}
    return body;
}

bool DocManager::readObj( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj title = AppContext::inst()->getTxn()->createObject( TypeTitle );
	_aggr( title, super, doc );
	Obj body = title;
	DataReader::Token t = in.nextToken();
	bool titleBodyChecked = false;
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( !readObjAttr( in, title ) )
				return false;
		}else if( t == DataReader::EndFrame )
		{
			if( !titleBodyChecked )
			{
				body = splitTitleBody( title, doc );
				titleBodyChecked = true;
			}
			return true;
		}else if( t == DataReader::BeginFrame )
		{
			if( !titleBodyChecked )
			{
				body = splitTitleBody( title, doc );
				titleBodyChecked = true;
			}
			const QString name = in.getName().getArr();
			if( name == "obj" )
			{
				if( !readObj( in, title, doc ) )
					return false;
			}else if( name == "pic" )
			{
				if( !readPic( in, title, doc ) )
					return false;
			}else if( name == "tbl" )
			{
				if( !readTbl( in, title, doc ) )
					return false;
			}else if( name == "lnk" )
			{
				if( !readLout( in, body, doc ) )
					return false;
			}else if( name == "lin" )
			{
				if( !readLin( in, body, doc ) )
					return false;
			}else
			{
				d_error += tr("unknown frame ") + name;
				return false;
			}
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

bool DocManager::readStub( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj obj = AppContext::inst()->getTxn()->createObject( TypeStub );
	_aggr( obj, super, doc );
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( !readObjAttr( in, obj ) )
				return false;
		}else if( t == DataReader::EndFrame )
		{
			return true;
		}else
		{
			d_error += tr("invalid token");
			return false;
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

bool DocManager::readLout( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj lnk = AppContext::inst()->getTxn()->createObject( TypeOutLink );
	_aggr( lnk, super, doc );
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( !readObjAttr( in, lnk ) )
				return false;
		}else if( t == DataReader::EndFrame )
		{
			return true;
		}else if( t == DataReader::BeginFrame )
		{
			const QString name = in.getName().getArr();
			if( name == "tobj" )
			{
				if( !readStub( in, lnk, doc ) )
					return false;
			}else
			{
				d_error += tr("unknown frame ") + name;
				return false;
			}
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

bool DocManager::readLin( DataReader& in, Sdb::Obj& super, const Sdb::Obj& doc )
{
	Obj lnk = AppContext::inst()->getTxn()->createObject( TypeInLink );
	_aggr( lnk, super, doc );
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( !readObjAttr( in, lnk ) )
				return false;
		}else if( t == DataReader::EndFrame )
		{
			return true;
		}else if( t == DataReader::BeginFrame )
		{
			const QString name = in.getName().getArr();
			if( name == "sobj" )
			{
				if( !readStub( in, lnk, doc ) )
					return false;
			}else
			{
				d_error += tr("unknown frame ") + name;
				return false;
			}
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

bool DocManager::readHistAttr( Stream::DataReader& in, Sdb::Obj& o )
{
	quint32 id = d_cache.value( in.getName().getArr() );
	if( id == 0 )
	{
		id = AppContext::inst()->getTxn()->getAtom( in.getName().getArr() );
	}
	return readAttr( in, o, id );
}

static inline void _injectHr( Sdb::Obj& hr, Sdb::Obj& o )
{
	o.appendSlot( hr );
	const quint32 atom = hr.getValue( AttrHistAttr ).getAtom();
	const quint8 type = hr.getValue( AttrHistType ).getUInt8();
	if( atom != 0 || 
		type == HistoryType_createObject ||
		type == HistoryType_unDeleteObject ||
		type == HistoryType_createLink ||
		type == HistoryType_modifyLink || 
		type == HistoryType_deleteLink )
	{
		o.setValue( AttrObjAttrChanged, DataCell().setBool( true ) );
	}
	if( atom == AttrObjText || atom == AttrStubTitle ||
		type == HistoryType_createObject ||
		type == HistoryType_unDeleteObject )
	{
		o.setValue( AttrObjTextChanged, DataCell().setBool( true ) );
	}
}

bool DocManager::readHist( DataReader& in, Sdb::Obj& doc, Sdb::Obj& own )
{
	Obj hr = AppContext::inst()->getTxn()->createObject( TypeHistory );
	own.appendSlot( hr );
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( !readHistAttr( in, hr ) )
				return false;
		}else if( t == DataReader::EndFrame )
		{
			const qint32 nr = hr.getValue( AttrHistObjId ).getInt32();
			if( nr != 0 )
			{
				Obj o = AppContext::inst()->getTxn()->getObject( d_nrToOid.value( nr ) );
				if( !o.isNull() )
				{
					_injectHr( hr, o );
					if( !o.getValue( AttrSecSplit ).isNull() )
					{
						o = o.getOwner();
						if( !o.isNull() )
						{
							Obj hr2 = AppContext::inst()->getTxn()->createObject( TypeHistory );
							own.appendSlot( hr2 );
							hr2.setValue( AttrHistAuthor, hr.getValue( AttrHistAuthor ) );
							hr2.setValue( AttrHistDate, hr.getValue( AttrHistDate ) );
							hr2.setValue( AttrHistType, hr.getValue( AttrHistType ) );
							hr2.setValue( AttrHistObjId, hr.getValue( AttrHistObjId ) );
							hr2.setValue( AttrHistAttr, hr.getValue( AttrHistAttr ) );
							hr2.setValue( AttrHistOld, hr.getValue( AttrHistOld ) );
							hr2.setValue( AttrHistNew, hr.getValue( AttrHistNew ) );
							_injectHr( hr2, o );
						}
					}
				}
				// else
					// hr.erase();  // Gibt einen SQL-Lock
			}else
				doc.appendSlot( hr ); 
			return true;
		}else
		{
			d_error += tr("invalid token");
			return false;
		}
		t = in.nextToken();
	}
	d_error = "Unexpected end of stream";
	return false;
}

enum TypeFace { TypeAnsi, TypeSymbol, TypeGreek, TypeUnknown };

static inline quint16 _toUnicode( quint16 c )
{
	const quint16 square = 0x2750;
	switch( c )
	{
		// Reihenfolge entspricht dem DOORS Insert Symbol Dialog von oben links nach unten rechts
	case 0x22: return 0x2200; // für alle
	case 0x24: return 0x2203; // es gibt ein
	case 0x27: return 0x220d; // invers element
	case 0x40: return 0x2245; // gleich welle
	case 0x41: return 0x391; // gross Alpha
	case 0x42: return 0x392; // B
	case 0x43: return 0x3A7; // X
	case 0x44: return 0x394; // Delta
	case 0x45: return 0x395; // E
	case 0x46: return 0x3A6; // Phi
	case 0x47: return 0x393; // Gamma
	case 0x48: return 0x397; // H
	case 0x49: return 0x399; // I
	case 0x4a: return 0x3D1; // Theta Variante
	case 0x4b: return 0x39A; // K
	case 0x4c: return 0x39B; // Lambda
	case 0x4d: return 0x39C; // M
	case 0x4e: return 0x39D; // N
	case 0x4f: return 0x39F; // O
	case 0x50: return 0x3A0;
	case 0x51: return 0x398;
	case 0x52: return 0x3A1; // P
	case 0x53: return 0x3A3; // Summe
	case 0x54: return 0x3A4; // T
	case 0x55: return 0x3A5; // Y
	case 0x56: return 0x3C2;
	case 0x57: return 0x3A9; // Omega
	case 0x58: return 0x39E;
	case 0x59: return 0x3A8;
	case 0x5a: return 0x396; // Z
	case 0x5b: return 0x5B; // large L-Brack
	case 0x5d: return 0x5D; // large R-Brack
	case 0x5e: return 0x22a5; // umgekehrtes T
	case 0x5f: return 0x2017; // Strich unten
	case 0x60: return 0x00af; // Strich oben
	case 0x61: return 0x3B1; // klein alpha
	case 0x62: return 0x3B2; // beta
	case 0x63: return 0x3C7; // x
	case 0x64: return 0x3B4; // delta
	case 0x65: return 0x3B5; // epsilon
	case 0x66: return 0x3D5;
	case 0x67: return 0x3B3; // gamma
	case 0x68: return 0x3B7;
	case 0x69: return 0x3B9; // i
	case 0x6a: return 0x3C6; // phi
	case 0x6b: return 0x3BA; // k
	case 0x6c: return 0x3BB; // l
	case 0x6d: return 0x3BC; // mycron
	case 0x6e: return 0x3BD; // n
	case 0x6f: return 0x3BF; // o
	case 0x70: return 0x3C0; // pi
	case 0x71: return 0x3B8;
	case 0x72: return 0x3C1;
	case 0x73: return 0x3C3;
	case 0x74: return 0x3C4; // tau
	case 0x75: return 0x3C5; // u
	case 0x76: return 0x3D6; // w quer
	case 0x77: return 0x3C9; // w
	case 0x78: return 0x3BE;
	case 0x79: return 0x3C8; // psi
	case 0x7a: return 0x3B6;
	case 0x7b: return 0x7B; // L-Curly
	case 0x7c: return 0x7C; // Bar
	case 0x7d: return 0x7D; // R-Curly
	case 0x7e: return 0x7E; // Tilde
	case 0xa1: return square; // T-Fontäne
	case 0xa2: return 0x27; // hochkomma
	case 0xa3: return 0x2264; // kleinergleich
	case 0xa4: return 0x2F; // Slash
	case 0xa5: return 0x221E; // unendlich
	case 0xa6: return 0x192; // geschwungenes f
	case 0xa7: return 0x2663; // Kreuz
	case 0xa8: return 0x2666; // Karo
	case 0xa9: return 0x2665; // Herz
	case 0xaa: return 0x2660; // Blatt
	case 0xab: return 0x2194; // Pfeil links-rechts
	case 0xac: return 0x2190; // Pfeil links
	case 0xad: return 0x2191; // Pfeil hoch
	case 0xae: return 0x2192; // Pfeil rechts
	case 0xaf: return 0x2193; // Pfeil runter
	case 0xb0: return 0xB0; // Kringel
	case 0xb1: return 0xB1; // Plusminus
	case 0xb2: return 0x22; // Gänsefüsschen?
	case 0xb3: return 0x2265; // Grössergleich
	case 0xb4: return 0x2217; // mal?
	case 0xb5: return 0x221D;
	case 0xb6: return 0x2202;
	case 0xb7: return 0x2022; // dicker Punkt
	case 0xb8: return 0xF7; // geteilt durch
	case 0xb9: return 0x2260; // ungleich
	case 0xba: return 0x2261;
	case 0xbb: return 0x2248; // gewellt gleich
	case 0xbc: return square; // drei punkte
	case 0xbd: return square; // Vertikalstrich
	case 0xbe: return square; // horizontalstrich
	case 0xbf: return square; // Returnpfeil
	case 0xc0: return square; // Altdeutsches N?
	case 0xc1: return square; // Altdeutsches J?
	case 0xc2: return square; // Altdeutsches R?
	case 0xc3: return square; // ?
	case 0xc4: return 0x2297; // mal im kreis
	case 0xc5: return 0x2295; // plus im kreis
	case 0xc6: return 0x2205; // Kreis querstrick
	case 0xc7: return 0x2229; // and
	case 0xc8: return 0x222A; // or
	case 0xc9: return 0x2283; // linke teilmenge
	case 0xca: return 0x2287;
	case 0xcb: return 0x2284;
	case 0xcc: return 0x2282;
	case 0xcd: return 0x2286;
	case 0xce: return 0x2208; // element
	case 0xcf: return 0x2209; // nicht element
	case 0xd0: return 0x2220;
	case 0xd1: return 0x2207; // nabla
	case 0xd2: return 0x00ae; // R
	case 0xd3: return 0x00a9; // C
	case 0xd4: return 0x2122; // TM
	case 0xd5: return 0x220F; // Produktzeichen
	case 0xd6: return 0x221A; // Wurzel
	case 0xd7: return 0x2219; // mal
	case 0xd8: return 0xAC; // not
	case 0xd9: return 0x2227; // and
	case 0xda: return 0x2228; // or
	case 0xdb: return 0x21d4; // left right double arrow
	case 0xdc: return 0x21d0; // left double arrow
	case 0xdd: return 0x21d1; // up double arrow
	case 0xde: return 0x21d2; // right double arrow
	case 0xdf: return 0x21d3; // down double arrow
	case 0xe0: return square; // leere Menge
	case 0xe1: return 0x27E8; // dreieckklammer rechts offen
	case 0xe2: return 0x00ae; // R
	case 0xe3: return 0x00a9; // C
	case 0xe4: return 0x2122; // TM
	case 0xe5: return 0x2211; // Summe
	case 0xe6: return square;
	case 0xe7: return square;
	case 0xe8: return square;
	case 0xe9: return square;
	case 0xea: return square;
	case 0xeb: return square;
	case 0xec: return square;
	case 0xed: return square;
	case 0xee: return square;
	case 0xef: return square;
	case 0xf1: return 0x27E9; // dreiecksklammer links offen
	case 0xf2: return 0x222B; // Integralzeichen
	case 0xf3: return square;
	case 0xf4: return square;
	case 0xf5: return square;
	case 0xf6: return square;
	case 0xf7: return square;
	case 0xf8: return square;
	case 0xf9: return square;
	case 0xfa: return square;
	case 0xfb: return square;
	case 0xfc: return square;
	case 0xfd: return square;
	case 0xfe: return square;
	default: return square;
	}
}
static bool _transformFrag( DataCell& v, DataWriter& out, std::bitset<8>& fmt, TypeFace& font )
{
	if( v.getType() == DataCell::TypeUInt8 )
	{
		if( v.getUInt8() == 'i' )
			fmt.set( TextInStream::Italic );
		else if( v.getUInt8() == 'b' )
			fmt.set( TextInStream::Bold );
		else if( v.getUInt8() == 'u' )
			fmt.set( TextInStream::Underline );
		else if( v.getUInt8() == 'k' )
			fmt.set( TextInStream::Strikeout );
		else if( v.getUInt8() == 'p' )
			fmt.set( TextInStream::Super );
		else if( v.getUInt8() == 's' )
			fmt.set( TextInStream::Sub );

		if( v.getUInt8() == 'y' ) // Symbol
			font = TypeSymbol;
		else if( v.getUInt8() == 'g' ) // Greek
			font = TypeGreek;
		else if( v.getUInt8() == '?' ) // Unknown
			font = TypeUnknown;

		return false;
	}else if( v.getType() == DataCell::TypeString )
	{
		if( !v.getStr().isEmpty() )
		{
			QString str = v.getStr();
			if( font == TypeSymbol )
			{
				//qDebug() << "**** Table";
				for( int i = 0; i < str.size(); i++ )
					str[i] = _toUnicode( str[i].unicode() );
					// qDebug( "case 0x%x: return 0x0;", str[i].unicode() );
				//qDebug() << "symbol: " << str;
			}else if( font == TypeGreek )
			{
				//qDebug() << "greek: " << str;
			}else if( font == TypeUnknown )
			{
				//qDebug() << "unknown: " << str;
			}
			out.writeSlot( DataCell().setUInt8( fmt.to_ulong() ) );
			out.writeSlot( DataCell().setString( str ) );
			return true;
		}else
			return false;
	}else
		throw _MyException( "invalid cell type in rich text fragment" );
}

QByteArray DocManager::transformBml( const QByteArray& bml )
{
	DataReader in( bml );
	DataWriter out;

	out.startFrame( NameTag("rtxt") );
	out.writeSlot( DataCell().setAscii( "0.1" ), NameTag("ver") );

	DataReader::Token t = in.nextToken();
	enum State { WaitPar, ReadPar, WaitRt, ReadRt, ReadImg };
	enum State2 { Idle, WritePar, WriteLst };
	State state = WaitPar;
	State2 state2 = Idle;
	// bool firstRtInPar = false;
	bool hasText = false;
	quint32 indent;
	std::bitset<8> format;
	TypeFace font;
	while( DataReader::isUseful( t ) )
	{
		switch( state )
		{
		case WaitPar:
			if( t == DataReader::BeginFrame && in.getName().getArr() == "par" )
			{
				state = ReadPar;
				indent = 0;
			}
			break;
		case ReadPar:
			if( t == DataReader::Slot )
			{
				const QByteArray name = in.getName().getArr();
				if( name == "bu" )
				{
					if( state2 == Idle )
					{
						out.startFrame( NameTag("lst") );
						state2 = WriteLst;
						out.writeSlot( DataCell().setUInt8( TextInStream::Disc ),
							NameTag( "ls" ) ); // List Style
						out.writeSlot( DataCell().setUInt8( indent ),
							NameTag( "il" ) ); // Indentation Level 0..255
					}
				}else if( name == "bs" )
					; // ignore
				else if( name == "il" )
				{
					indent = in.readValue().getInt32() / 360;
					if( state2 == WriteLst )
						out.writeSlot( DataCell().setUInt8( indent ),
							NameTag( "il" ) ); // Indentation Level 0..255
				}
				else
					throw _MyException("unexpected slot in paragraph: " + name);
			}else if( t == DataReader::BeginFrame && in.getName().getArr() == "rt" )
			{
				if( state2 == Idle )
				{
					out.startFrame( NameTag("par") );
					state2 = WritePar;
				}
				state = WaitRt;
			}else if( t == DataReader::EndFrame )
			{
				if( state2 == WritePar || state2 == WriteLst )
				{
					state2 = Idle;
					out.endFrame(); // lst oder par
				}
				state = WaitPar;
			}
			break;
		case WaitRt:
			if( t == DataReader::Slot )
			{
				const QByteArray name = in.getName().getArr();
				DataCell v;
				in.readValue( v );
				if( name == "ole" )
				{
					if( v.getType() == DataCell::TypeImg )
					{
						out.startFrame( NameTag("img") );
						state = ReadImg;
						hasText = true;
						out.writeSlot( v );
					}else
						throw _MyException( "invalid ole slot in rich text" );
				}else if( name == "url" )
				{
					out.startFrame( NameTag("anch") );
					out.writeSlot( v, NameTag("url") );
					hasText = true;
					out.endFrame(); // url
				}else if( name.isEmpty() && 
					( v.getType() == DataCell::TypeUInt8 || 
					v.getType() == DataCell::TypeString ) )
				{
					out.startFrame( NameTag("frag") );
					format.reset();
					font = TypeAnsi;
					if( _transformFrag( v, out, format, font ) )
						hasText = true;
					state = ReadRt;
				}else
					throw _MyException( "invalid slot in rich text: " + name );
			}else if( t == DataReader::EndFrame )
			{
				state = ReadPar;
			}else
				throw _MyException( "invalid frame in rich text" );
			break;
		case ReadRt:
			if( t == DataReader::Slot )
			{
				DataCell v;
				in.readValue( v );
				assert( in.getName().isNull() );
				if( v.getType() == DataCell::TypeUInt8 || v.getType() == DataCell::TypeString )
				{
					if( _transformFrag( v, out, format, font ) )
						hasText = true;
				}else
					throw _MyException( "invalid fragment type in rich text" );
			}else if( t == DataReader::EndFrame )
			{
				out.endFrame(); // frag
				state = ReadPar;
			}else
				throw _MyException( "invalid frame in rich text" );
			break;
		case ReadImg:
			if( t == DataReader::Slot )
			{
				const QByteArray name = in.getName().getArr();
				DataCell v;
				in.readValue( v );
				if( name == "~width" || name == "~height" )
					; // RISK ignore for the moment
				else
					throw _MyException( "invalid slot in rich text image: " + name );
			}else if( t == DataReader::EndFrame )
			{
				out.endFrame(); // frag
				state = ReadPar;
			}else
				throw _MyException( "invalid frame in rich text" );
			break;
		}
		t = in.nextToken();
	}
	out.endFrame(); // rtxt
	if( !hasText )
		return QByteArray();
	else
		return out.getStream();
}

bool DocManager::deleteHistoOfObj( Sdb::Obj obj )
{
	Sdb::Qit i = obj.getFirstSlot(); 
	if( !i.isNull() ) do
	{
		Sdb::Obj hr = obj.getTxn()->getObject( i.getValue() );
		if( !hr.isNull() && hr.getType() == TypeHistory )
		{
			hr.erase();
			i.erase();
		}
	}while( i.next() );
	obj.setValue( AttrObjAttrChanged, DataCell() );
	obj.setValue( AttrObjTextChanged, DataCell() );
	Sdb::Obj o = obj.getFirstObj();
	if( !o.isNull() ) do
	{
		if( !deleteHistoOfObj( o ) )
			return false;
	}while( o.next() );
	return true;
}

bool DocManager::deleteHisto( Sdb::Obj doc )
{
	if( doc.getType() != TypeDocument )
		return false;

	// Hierarchisch durch das Dokument runter.
	if( !deleteHistoOfObj( doc ) )
		return false;

	Sdb::Qit i = doc.getObject(AttrDocOwning).getFirstSlot(); // referenziert alle TypeHistory
	if( !i.isNull() ) do
	{
		i.erase();
	}while( i.next() );
	return true;
}

static inline qint32 _pred( Sdb::Obj o )
{
	o.prev();
	if( !o.isNull() ) do
	{
		DataCell v = o.getValue( AttrObjIdent );
		if( v.isInt32() )
			return v.getInt32();
	}while( o.prev() );
	return 0;
}

bool DocManager::createDiff( Sdb::Obj lhs, Sdb::Obj rhs, Sdb::Obj own, const QList<quint32>& attrs )
{
	bool diff = false;
	if( lhs.isNull() )
	{
		Sdb::Obj hr = AppContext::inst()->getTxn()->createObject( TypeHistory );
		own.appendSlot( hr );
		rhs.appendSlot( hr );
		hr.setValue( AttrHistType, DataCell().setUInt8( HistoryType_createObject ) );
		hr.setValue( AttrHistObjId, rhs.getValue( AttrObjIdent ) );
		hr.setValue( AttrHistAuthor, rhs.getValue( AttrModifiedBy ) );
		hr.setValue( AttrHistDate, rhs.getValue( AttrModifiedOn ) );
		rhs.setValue( AttrObjAttrChanged, DataCell().setBool( true ) );
		rhs.setValue( AttrObjTextChanged, DataCell().setBool( true ) );
		diff = true;
	}else
	{
		for( int i = 0; i < attrs.size(); i++ )
		{
			DataCell l = lhs.getValue( attrs[i] );
			DataCell r = rhs.getValue( attrs[i] );
			if( !l.equals( r ) )
			{
				diff = true;
				Sdb::Obj hr = AppContext::inst()->getTxn()->createObject( TypeHistory );
				own.appendSlot( hr );
				rhs.appendSlot( hr );
				if( attrs[i] == AttrObjText )
					rhs.setValue( AttrObjTextChanged, DataCell().setBool( true ) );

				hr.setValue( AttrHistType, DataCell().setUInt8( HistoryType_modifyObject ) );
				hr.setValue( AttrHistObjId, rhs.getValue( AttrObjIdent ) );
				hr.setValue( AttrHistAttr, DataCell().setAtom( attrs[i] ) );
				hr.setValue( AttrHistOld, l );
				hr.setValue( AttrHistNew, r );
				hr.setValue( AttrHistAuthor, rhs.getValue( AttrModifiedBy ) );
				hr.setValue( AttrHistDate, rhs.getValue( AttrModifiedOn ) );
			}
		}
		if( !lhs.getOwner().getValue( AttrObjIdent ).equals( rhs.getOwner().getValue( AttrObjIdent ) ) ||
			_pred( lhs ) != _pred( rhs ) )
		{
			Sdb::Obj hr = AppContext::inst()->getTxn()->createObject( TypeHistory );
			own.appendSlot( hr );
			rhs.appendSlot( hr );
			hr.setValue( AttrHistType, DataCell().setUInt8( HistoryType_clipMoveObject ) );
			hr.setValue( AttrHistObjId, rhs.getValue( AttrObjIdent ) );
			hr.setValue( AttrHistAuthor, rhs.getValue( AttrModifiedBy ) );
			hr.setValue( AttrHistDate, rhs.getValue( AttrModifiedOn ) );
			diff = true;
		}
		if( diff )
		{
			rhs.setValue( AttrObjAttrChanged, DataCell().setBool( true ) );
		}
	}
	return diff;
}

bool DocManager::createHistoOfObj( Sdb::Obj super, Sdb::Obj doc, Sdb::Obj own, const QList<quint32>& attrs )
{
	// Jedes Objekt rekursiv durchlaufen und Existenz, Attribute, und Position vergleichen
	Sdb::Obj obj = super.getFirstObj();
	if( !obj.isNull() ) do
	{
		// TODO: Links, Stubs

		DataCell v = obj.getValue( AttrObjIdent );
		if( !v.isNull() )
		{
			QMap<quint32,quint64>::iterator j = d_nrToOid.find( v.getInt32() );
			if( j == d_nrToOid.end() )
			{
				// Das Objekt wurde neu erzeugt
				createDiff( Sdb::Obj(), obj, own, attrs );
			}else if( obj.getValue( AttrTitleSplit ).isNull() )
			{
				// Im Falle von Split wird erst Body betrachtet, dann Title
				if( j.value() == 0 )
					return false; // Darf nicht vorkommen
				// Das Objekt existierte bereits
				Sdb::Obj lhs = obj.getTxn()->getObject( j.value() );
				if( lhs.getType() != obj.getType() )
					lhs = Sdb::Obj(); // Nicht Diff zwischen Title und Section
				createDiff( lhs, obj, own, attrs );
				j.value() = 0; // Markiere Objekt als konsumiert

				if( !lhs.isNull() && !lhs.getValue( AttrSecSplit ).isNull() && !obj.getValue( AttrSecSplit ).isNull() )
					// Wenn beide Splits sind, wurde Owner noch nicht verglichen (da AbsNo gleich mit Sub)
					createDiff( lhs.getOwner(), obj.getOwner(), own, attrs );
			}
			createHistoOfObj( obj, doc, own, attrs );
		}
	}while( obj.next() );
	return true;
}

static void _fillMap( QMap<quint32,quint64>& map, Sdb::Obj obj )
{
	// Da Top Down wird jeweils Title durch Body überschrieben im Falle AttrSecSplit
	Sdb::Obj i = obj.getFirstObj();
	if( !i.isNull() ) do
	{
		DataCell v = i.getValue( AttrObjIdent );
		if( i.getType() != TypeStub && !v.isNull() )
		{
			map[v.getInt32()] = i.getId();
			_fillMap( map, i );
		}
	}while( i.next() );
}

bool DocManager::createHisto( Sdb::Obj prev, Sdb::Obj doc, const QList<quint32>& attrs )
{
	if( prev.getType() != TypeDocument )
		return false;
	if( !deleteHisto( doc ) )
		return false;
	doc.setValue( AttrDocDiffSource, prev );
	d_nrToOid.clear();
	_fillMap( d_nrToOid, prev );
	Sdb::Obj own = doc.getObject(AttrDocOwning);
	if( !createHistoOfObj( doc, doc, own, attrs ) )
		return false;
	QMap<quint32,quint64>::iterator j;
	for( j = d_nrToOid.begin(); j != d_nrToOid.end(); ++j )
	{
		if( j.value() != 0 )
		{
			// Objekt existierte im alten aber nicht mehr im neuen Dokument
			Sdb::Obj hr = AppContext::inst()->getTxn()->createObject( TypeHistory );
			own.appendSlot( hr );
			doc.appendSlot( hr );
			hr.setValue( AttrHistType, DataCell().setUInt8( HistoryType_deleteObject ) );
			hr.setValue( AttrHistObjId, DataCell().setOid( j.value() ) );
			hr.setValue( AttrHistAuthor, doc.getValue( AttrModifiedBy ) );
			hr.setValue( AttrHistDate, doc.getValue( AttrModifiedOn ) );
			Sdb::Obj o = hr.getTxn()->getObject( j.value() );
			if( !o.isNull() )
				hr.setValue( AttrHistInfo, DataCell().setString( 
					QString( "%1 %2" ).arg( o.getValue( AttrObjNumber ).getStr() ).
					arg( TypeDefs::elided( o.getValue( AttrObjText ), 0 ) ) ) );
		}
	}
	d_nrToOid.clear();
	return true;
}

bool DocManager::deleteDoc( Sdb::Obj doc )
{
	if( doc.getType() != TypeDocument )
		return false;
	// Zuerst alle in der Owning-Liste löschen
	Sdb::Qit i = doc.getObject(AttrDocOwning).getFirstSlot();
	if( !i.isNull() ) do
	{
		Sdb::Obj o = doc.getTxn()->getObject( i.getValue() );
		if( !o.isNull() )
			o.erase();
		i.erase();
	}while( i.next() );

	// Dann das doc selber löschen
	doc.erase();
	return true;
}

bool DocManager::deleteFolder( Sdb::Obj folder )
{
	if( folder.getType() != TypeFolder )
		return false;
	Sdb::Obj sub = folder.getFirstObj();
	bool ok = true;
	if( !sub.isNull() ) do
	{
		Sdb::Obj me = sub;
		ok = sub.next();
		if( me.getType() == TypeDocument )
		{
			if( !deleteDoc( me ) ) 
				return false;
		}else if( me.getType() == TypeFolder )
		{
			if( !deleteFolder( me ) ) 
				return false;
		}else
			me.erase();
	}while( ok );

	folder.erase();
	return true;
}

bool DocManager::deleteObj( Sdb::Obj o )
{
	d_error.clear();
	try
	{
		if( o.getType() == TypeFolder )
			return deleteFolder( o );
		else if( o.getType() == TypeDocument )
			return deleteDoc( o );
		else
			return false;
	}catch( DatabaseException& e )
	{
		d_error += e.getCodeString() + " " + e.getMsg();
		return false;
	}
}

static void _descentAnnot( Sdb::Obj sec, bool reset )
{
	Sdb::Obj o = sec.getFirstObj();
	bool ok = true;
	if( !o.isNull() ) do
	{
		Sdb::Obj me = o;
		ok = o.next(); // muss hier stehen, da me.erase auch o ungültig macht
		switch( o.getType() )
		{
		case TypeTitle:
		case TypeSection:
		case TypeTable:
		case TypeTableRow:
		case TypeTableCell:
		case TypePicture:
		case TypeOutLink:
		case TypeInLink:
			_descentAnnot( me, reset );
			break;
		case TypeAnnotation:
			me.erase();
			break;
		}
	}while( ok );
	sec.setValue( AttrAnnotated, DataCell().setInt32( 0 ) );
	if( reset )
	{
		if( sec.getValue(AttrReviewNeeded).getBool() )
			sec.setValue(AttrReviewStatus, Stream::DataCell().setUInt8(ReviewStatus_Pending) );
		else
			sec.setValue(AttrReviewStatus, Stream::DataCell().setNull() );
		sec.setValue( AttrReviewer, DataCell().setNull() );
		sec.setValue( AttrStatusDate, DataCell().setNull() );
		sec.setValue( AttrConsRevStat, DataCell().setNull() );
	}
}

bool DocManager::deleteAnnots( Sdb::Obj doc, bool reset )
{
	if( doc.isNull() || doc.getType() != TypeDocument )
		return false;
	d_error.clear();
	try
	{
		_descentAnnot( doc, reset );
		doc.setValue( AttrDocMaxAnnot, DataCell().setUInt32( 0 ) );
		doc.setValue( AttrAnnotated, DataCell().setInt32( 0 ) );
		return true;
	}catch( DatabaseException& e )
	{
		d_error += e.getCodeString() + " " + e.getMsg();
		return false;
	}
}
