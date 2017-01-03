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

#include "TypeDefs.h"
#include <Sdb/Database.h>
#include <Sdb/Obj.h>
#include <Stream/DataReader.h>
#include <QHash>
#include <QtDebug>
#include "AppContext.h"
using namespace Ds;
using namespace Sdb;

const char* IndexDefs::IdxDocId = "<doorscope>IdxDocId";
const char* IndexDefs::IdxDocObjId = "<doorscope>IdxDocObjId";
const char* IndexDefs::IdxAnnDocNr = "<doorscope>IdxAnnDocNr";

struct _NameMapping
{
	quint32 d_id;
	const char* d_name;
	const char* d_pretty;
    int d_type;
};
static const _NameMapping s_names[] =
{
    {AttrCreatedBy, "CreatedBy", "Created By", 0  },
    {AttrCreatedOn, "CreatedOn", "Created On", 0  },
    {AttrModifiedBy, "ModifiedBy", "Last Modified By", 0  },
    {AttrModifiedOn, "ModifiedOn", "Last Modified On", 0  },
    {AttrCreatedThru, "CreatedThru", "Created Thru", 0  },
    {AttrReviewStatus, "ReviewStatus", 0, 0  },
    {AttrReviewNeeded, "ReviewNeeded", 0, 0  },
	{AttrAnnotated, "Annotated", 0, 0  },
    {AttrReviewer, "Reviewer", 0, 0  },
    {AttrStatusDate, "StatusDate", 0, 0  },
    {AttrConsRevStat, "ConsRevStat", 0, 0  },
	{TypeDocument, "Document", 0, 0 },
	{AttrDocId, "Id", "Doc. ID", TypeDocument  },
    {AttrDocPath, "Path", 0, 0  },
    {AttrDocVer, "Ver", "Version", 0  },
    {AttrDocDesc, "Desc", "Description", 0  },
    {AttrDocName, "Name", 0, TypeDocument  },
    {AttrDocType, "Type", 0, TypeDocument  },
    {AttrDocBaseline, "Baseline", 0, 0  },
    {AttrDocVerMaj, "VerMaj", 0, 0  },
    {AttrDocVerMin, "VerMin", 0, 0  },
    {AttrDocVerSuff, "VerSuff", 0, 0  },
    {AttrDocVerAnnot, "VerAnnot", "Baseline Annotation", 0  },
    {AttrDocVerDate, "VerDate", 0, 0  },
    {AttrDocPrefix, "Prefix", 0, 0  },
	{AttrDocExported, "Exported", 0, 0  },
    {AttrDocImported, "Imported", 0, 0  },
    {AttrDocStream, "Source File", 0, 0  },
    {AttrDocAttrs, "Attrs", 0, 0  },
    {AttrDocObjAttrs, "ObjAttrs", 0, 0  },
    {AttrDocOwning, "Owning", 0, 0  },
    {AttrDocMaxAnnot, "MaxAnnot", 0, 0  },
    {AttrDocDiffSource, "DiffSource", 0, 0  },
    {AttrDocAltName, "AltName", 0, 0  },
    {AttrDocIndex, "Index", 0, 0  },
    {AttrObjIdent, "RelId", "Absolute Number", 0  },
    {AttrObjNumber, "Number", "Object Number", 0  },
    {AttrObjAttrChanged, "AttrChanged", 0, 0  },
    {AttrObjTextChanged, "TextChanged", 0, 0  },
	{AttrObjShort, "Short", "Obj. Short Text", 0  },
    {AttrObjText, "Text", "Object Text", TypeObject },
    {AttrObjHomeDoc, "HomeDoc", 0, TypeObject  },
    {AttrObjDeleted, "Deleted", 0, 0  },
    {AttrObjDocId, "DocId", 0, TypeObject  },
	{TypeTitle, "Heading", 0, 0 },
    {AttrTitleSplit, "TitleSplit", 0, 0  },
	{TypeSection, "Section", 0, 0 },
    {AttrSecSplit, "SecSplit", 0, 0  },
    {AttrLnkModId, "ModId", 0, 0  },
    {AttrLnkModName, "ModName", "Link Module", 0  },
    {TypeOutLink, "OutLink", 0, 0  },
    {AttrTrgObjId, "ObjId", "Target Object", TypeOutLink  },
    {AttrTrgDocId, "DocId", "Doc. ID", TypeOutLink  },
    {AttrTrgDocName, "DocName", "Doc. Name", TypeOutLink  },
    {AttrTrgDocVer, "DocVer", "Doc. Version", TypeOutLink  },
    {AttrTrgDocModDate, "DocModDate", "Doc. Version Date", TypeOutLink  },
    {AttrTrgDocIsBl, "DocIsBl", "Doc. Baseline", TypeOutLink  },
	{TypeInLink, "InLink", 0, 0  },
    {AttrSrcObjId, "ObjId", "Source Object", TypeInLink  },
    {AttrSrcDocId, "DocId", "Doc. ID", TypeInLink  },
    {AttrSrcDocName, "DocName", "Doc. Name", TypeInLink  },
    {AttrSrcDocVer, "DocVer", "Doc. Version", TypeInLink  },
    {AttrSrcDocModDate, "DocModDate", "Doc. Version Date", TypeInLink  },
    {AttrSrcDocIsBl, "DocIsBl", "Doc. Baseline", TypeInLink  },
	{TypeTable, "Table", 0, 0 },
	{TypeTableRow, "TableRow", "Table Row", 0  },
	{TypeTableCell, "TableCell", "Table Cell", 0  },
	{TypePicture, "Picture", 0, 0 },
    {AttrPicImage, "Image", 0, 0  },
    {AttrPicWidth, "Width", 0, 0  },
    {AttrPicHeight, "Height", 0, 0  },
    {TypeHistory, "History", 0, 0  },
    {AttrHistAuthor, "Author", 0, 0  },
    {AttrHistDate, "Date", 0, 0  },
    {AttrHistType, "Type", 0, TypeHistory  },
    {AttrHistObjId, "ObjId", 0, TypeHistory  },
    {AttrHistAttr, "Attr", 0, TypeHistory  },
    {AttrHistOld, "Old", 0, 0  },
    {AttrHistNew, "New", 0, 0  },
    {AttrHistInfo, "Info", 0, 0  },
    {TypeFolder, "Folder", 0, 0  },
    {AttrFldrName, "Name", 0, TypeFolder },
    {AttrFldrExpanded, "Expanded", 0, 0  },
	{TypeAnnotation, "Annotation", 0, 0 },
    {AttrAnnNr, "Nr", 0, 0  },
    {AttrAnnAttr, "Attr", 0, TypeAnnotation  },
    {AttrAnnPos, "Pos", 0, 0  },
    {AttrAnnLen, "Len", 0, 0  },
    {AttrAnnText, "Text", 0, TypeAnnotation  },
    {AttrAnnHomeDoc, "HomeDoc", 0, TypeAnnotation  },
    {AttrAnnPrio, "Prio", 0, 0  },
    {AttrAnnOldNr, "OldNr", 0, 0  },
	{TypeStub, "Stub", "Linked Object", 0 },
	{AttrStubTitle, "Title", "Object Heading", 0 },
	{TypeLuaScript, "Script", 0, 0  },
	{TypeLuaFilter, "Filter", 0, 0  },
	{AttrScriptName, "Name", 0, TypeLuaScript },
	{AttrScriptSource, "Source", 0, TypeLuaScript  },
	{ 0, 0, 0, 0 }
};

typedef QHash<QPair<quint32,QByteArray>,quint32> NameToAtom; // [ type, name | pretty ] -> atom
static NameToAtom s_nameToAtom;

void TypeDefs::init( Database& db )
{
    for( int i = 0; s_names[i].d_id; i++ )
    {
        QByteArray key = QByteArray::fromRawData( s_names[i].d_name, ::strlen(s_names[i].d_name)+1 );
        s_nameToAtom.insert( qMakePair( quint32(0), key ), s_names[i].d_id );
        if( s_names[i].d_type )
            s_nameToAtom.insert( qMakePair( quint32(s_names[i].d_type), key ), s_names[i].d_id );
        if( s_names[i].d_pretty )
        {
            key = QByteArray::fromRawData( s_names[i].d_pretty, ::strlen(s_names[i].d_pretty)+1 );
            s_nameToAtom.insert( qMakePair( quint32(0), key ), s_names[i].d_id );
            if( s_names[i].d_type )
                s_nameToAtom.insert( qMakePair( quint32(s_names[i].d_type), key ), s_names[i].d_id );
        }
    }

	Database::Lock lock( &db, true );

	// Root
	//db.presetAtom( "popAccountList", AttrPopAccountList );

	// ContentObject

	if( db.findIndex( IndexDefs::IdxDocId ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrDocId ) );
		db.createIndex( IndexDefs::IdxDocId, def );
	}
	if( db.findIndex( IndexDefs::IdxDocObjId ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrObjDocId ) );
		def.d_items.append( IndexMeta::Item( AttrObjIdent ) );
		db.createIndex( IndexDefs::IdxDocObjId, def );
	}
	if( db.findIndex( IndexDefs::IdxAnnDocNr ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrAnnHomeDoc ) );
		def.d_items.append( IndexMeta::Item( AttrAnnNr ) );
		db.createIndex( IndexDefs::IdxAnnDocNr, def );
	}

    db.presetAtom( "DsMax", DsMax );
}

quint32 TypeDefs::findAtom(const char *name, quint32 type)
{
	// Dieser Aufruf von fromRawData ergibt eine Size des ByteArray, die das 
	// Nullzeichen einschliesst. Auch beim Import des DSDX durch DocManager
	// resultiert diese size. Wenn wir es nicht so machen, wird ein neues Atom 
	// zum selben String nochmals erstellt.
    QByteArray key = QByteArray::fromRawData( name, ::strlen( name ) + 1 );
    quint32 res = 0;
    if( type )
        res = s_nameToAtom.value( qMakePair( type, key ) );
    if( res )
        return res;
    else
        return s_nameToAtom.value( qMakePair( quint32(0), key ) );
}

quint32 TypeDefs::findAtom(Database * db, const char *name, quint32 type)
{
    quint32 res = findAtom( name, type );
    if( res )
        return res;
    // unnötig QByteArray key = QByteArray::fromRawData( name, ::strlen( name ) + 1 );
    return db->getAtom( name, false );
}

const char* TypeDefs::historyTypeString[] =
{
	"unknown",
	"createType",
	"modifyType", 
	"deleteType", 
	"createAttr", 
	"modifyAttr", 
	"deleteAttr", 
	"createObject", 
	"copyObject", 
	"modifyObject", 
	"deleteObject", 
	"unDeleteObject", 
	"purgeObject", 
	"clipCutObject", 
	"clipMoveObject", 
	"clipCopyObject", 
	"createModule", 
	"baselineModule", 
	"partitionModule", 
	"acceptModule", 
	"returnModule", 
	"rejoinModule", 
	"createLink", 
	"modifyLink", 
	"deleteLink", 
	"insertOLE", 
	"removeOLE", 
	"changeOLE", 
	"pasteOLE", 
	"cutOLE", 
	"readLocked", 
	0,
};

const char* TypeDefs::historyTypePrettyString[] =
{
	"unknown",
	"Create Type",
	"Modify Type", 
	"Delete Type", 
	"Create Attribute", 
	"Modify Attribute", 
	"Delete Attribute", 
	"Create Object", 
	"Copy Object", 
	"Modify Object", 
	"Delete Object", 
	"Undelete Object", 
	"Purge Object", 
	"Cut Object", 
	"Move Object", 
	"Copy Object", 
	"Create Module", 
	"Baseline Module", 
	"Partition Module", 
	"Accept Module", 
	"Return Module", 
	"Rejoin Module", 
	"Create Link", 
	"Modify Link", 
	"Delete Link", 
	"Insert OLE", 
	"Remove OLE", 
	"Change OLE", 
	"Paste OLE", 
	"Cut OLE", 
	"Read Locked", 
};

static inline bool _isNull( const QTime& t )
{
	return t.hour() == 0 && t.minute() == 0 && t.second() == 0;
}

QString TypeDefs::formatDate( const QDateTime& dt )
{
	if( _isNull( dt.time() ) )
		return dt.date().toString( Qt::SystemLocaleDate );
	else
		return dt.toString( Qt::SystemLocaleDate );
}

QString TypeDefs::extractName( const QString& fullName )
{
	QStringList l = fullName.split( '/' );
	return l.back();
}

QByteArray TypeDefs::getPrettyName( quint32 atom, Sdb::Database* db )
{
	if( atom < DsMax )
		return getPrettyName( atom );
	else if( db )
		return db->getAtomString( atom );
	else
        return "<unknown>";
}

QByteArray TypeDefs::getSimpleName(quint32 atom, Database * db)
{
    for( int i = 0; s_names[i].d_id; i++ )
    {
        if( s_names[i].d_id == atom )
        {
             return s_names[i].d_name;
        }
    }
    if( db )
            return db->getAtomString( atom );
    else
        return "<unknown>";
}

const char* TypeDefs::getPrettyName( quint32 atom )
{
    for( int i = 0; s_names[i].d_id; i++ )
    {
        if( s_names[i].d_id == atom )
        {
            if( s_names[i].d_pretty )
                return s_names[i].d_pretty;
            else
                return s_names[i].d_name;
        }
    }
	return "";
}

QString TypeDefs::elided( const Stream::DataCell& v, quint32 len )
{
	QString res;
	if( v.isNull() )
		return "";
	if( v.isStr() )
	{
		res = v.getStr();
		res.replace( '\n', ' ' );
		res.replace( '\r', "" );
	}else if( v.isBml() )
	{
		Stream::DataReader r( v );
		res = r.extractString();
	}else
		res = v.toPrettyString();
	if( len != 0 && res.size() > int(len) )
		res = res.left( len - 3 ) + "...";
	return res;
}

QString TypeDefs::formatDocName( const Sdb::Obj& doc, bool full )
{
	if( doc.isNull() || doc.getType() != TypeDocument )
		return "";
	QString name = doc.getValue( AttrDocAltName ).toString(true);
	if( name.isEmpty() )
		name = doc.getValue( AttrDocName ).toString(true);
	if( full )
		return QObject::tr( "%1 [%2 %3 %4]" ).arg( name ).
				arg( doc.getValue( AttrDocVer ).toString(true) ).
				arg( (doc.getValue( AttrDocBaseline ).getBool())?
					"Baseline":"Current" ).
				arg( formatDate( doc.getValue( AttrModifiedOn ).getDateTime() ) );
	else
		return name;
}

QVariant TypeDefs::prettyValue( const Stream::DataCell& v )
{
    // Es wird ein einfacher String ohne Markup erzeugt.
	switch( v.getType() )
	{
	case Stream::DataCell::TypeDateTime:
		return TypeDefs::formatDate( v.getDateTime() );
		break;
	case Stream::DataCell::TypeDate:
		return v.getDate().toString( Qt::SystemLocaleDate );
		break;
	case Stream::DataCell::TypeTime:
		return v.getTime().toString( Qt::SystemLocaleDate );
		break;
	case Stream::DataCell::TypeImg:
		{
			QImage img;
			v.getImage( img );
			return QVariant::fromValue( img );
		}
		break;
    case Stream::DataCell::TypeOid:
        {
            Sdb::Obj obj = AppContext::inst()->getTxn()->getObject(v);
            if( obj.isNull() )
                return "nil";
            else
            {
                if( obj.hasValue( AttrObjIdent ) )
                    return obj.getValue( AttrObjIdent ).toString(true);
                else if( obj.hasValue( AttrDocId ) )
                    return obj.getValue( AttrDocId ).toString(true);
                else
                    return getPrettyName( obj.getType() );
            }
        }
        break;
	default:
		return v.toString(true);
	}
	return "";
}
