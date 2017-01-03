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

#include "DocExporter.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include "AppContext.h"
#include <Txt/TextOutStream.h>
#include <QFile>
#include <QTextStream>
using namespace Ds;
using namespace Stream;

DocExporter::DocExporter(QObject *parent)
	: QObject(parent)
{

}

DocExporter::~DocExporter()
{

}

static QString _statusString( quint8 id )
{
	switch( id )
	{
	case ReviewStatus_Pending:
		return "Review Pending";
	case ReviewStatus_Recheck:
		return "Clarification Pending";
	case ReviewStatus_Accepted:
		return "Approved";
	case ReviewStatus_AcceptedWithChanges:
		return "Approved with Changes";
	case ReviewStatus_Rejected:
		return "Disapproved";
	}
	return "";
}

static QString _prioString( quint8 id )
{
	switch( id )
	{
	case AnnPrio_NiceToHave:
		return "Nice to have";
	case AnnPrio_NeedToHave:
		return "Need to have";
	case AnnPrio_Urgent:
		return "Urgent";
	}
	return "";
}

static bool _hasAnnotations( const Sdb::Obj& o )
{
	if( o.getValue( AttrAnnotated ).getInt32() > 0 )
		return true;
	Sdb::Obj sub = o.getFirstObj();
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeOutLink || sub.getType() == TypeInLink )
		{
			if( sub.getValue( AttrAnnotated ).getInt32() > 0 )
				return true;
		}
	}while( sub.next() );
	return false;
}

static QString _notEmpty( const QString& str )
{
	if( !str.isEmpty() )
		return str;
	else
		return "-";
}

static void _writeAnnot( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj sub = sec.getFirstObj();
	bool first = true;
	bool close = false;
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeAnnotation )
		{
			if( first )
			{
				out << "<tr><td>.<td colspan=4 border=0>";
				out << "<table border=1 cellspacing=0>";
				out << "<tr><th>An.Nr.<th>Attribute<th>Annotation<th>Priority<th>Who<th>When";
				first = false;
				close = true;
			}
			out << "<tr>";
			out << "<td>" << sub.getValue(AttrAnnNr).getUInt32();
			out << "<td>" << _notEmpty( TypeDefs::getPrettyName( sub.getValue(AttrAnnAttr).getAtom(), sub.getDb() ) );
			out << "<td>" << _notEmpty( sub.getValue(AttrAnnText).getStr() );
			out << "<td>" << _notEmpty( _prioString( sub.getValue( AttrAnnPrio ).getUInt8() ) );
			out << "<td>" << _notEmpty( sub.getValue( AttrModifiedBy ).getStr() );
			out << "<td>" << TypeDefs::formatDate( sub.getValue( AttrModifiedOn ).getDateTime() );
			
		}
	}while( sub.next() );
	if( close )
	{
		out << "</table>";
		close = false;
	}
	sub = sec.getFirstObj();
	if( !sub.isNull() ) do 
	{
		if( ( sub.getType() == TypeOutLink || sub.getType() == TypeInLink ) && 
			sub.getValue( AttrAnnotated ).getInt32() > 0 )
		{
			out << "<tr><td>.<td>-<td>" << TypeDefs::getPrettyName( sub.getType(), sub.getDb() );
			if( sub.getType() == TypeOutLink )
				out << QString( "<td>[%1] %2: %3" ).
						arg( sub.getValue( AttrLnkModName ).getStr() ).
						arg( TypeDefs::extractName( sub.getValue( AttrTrgDocName ).getStr() ) ).
						arg( sub.getValue( AttrTrgObjId ).toPrettyString() );
			else
				out << QString( "<td>[%1] %2: %3" ).
						arg( sub.getValue( AttrLnkModName ).getStr() ).
						arg( TypeDefs::extractName( sub.getValue( AttrSrcDocName ).getStr() ) ).
						arg( sub.getValue( AttrSrcObjId ).toPrettyString() );
			out << "<td>-";
			_writeAnnot( sub, out );
		}
	}while( sub.next() );
}

static void _writeSection( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		const quint8 status = o.getValue( AttrReviewStatus ).getUInt8();
		switch( o.getType() )
		{
		case TypeTitle:
		case TypeSection:
		case TypeTable:
		case TypeTableRow:
		case TypeTableCell:
		case TypePicture:
			// <th>Abs.Nr.<th>Obj.Nr.<th>Type<th>Title/Text<th>Status
			if( status != ReviewStatus_None || _hasAnnotations( o ) )
			{
				out << "<tr>";
				out << "<td>" << o.getValue( AttrObjIdent ).toPrettyString();
				out << "<td>" << _notEmpty( o.getValue( AttrObjNumber ).getStr() );
				out << "<td>" << TypeDefs::getPrettyName( o.getType(), o.getDb() );
				out << "<td>" << _notEmpty( TypeDefs::elided( o.getValue( AttrObjText ) ) );
				out << "<td>" << _notEmpty( _statusString( status ) );
				_writeAnnot( o, out );
			}
			_writeSection( o, out );
			break;
		}
	}while( o.next() );
}

static void _writeHeader( const Sdb::Obj& doc, QTextStream& out, const QString& title )
{
	const QString docTitle = TypeDefs::formatDocName( doc );
	out << "<html><head><meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\">";
	out << "<meta name=\"Publisher\" content=\"DoorScope " << AppContext::s_version << "\">";
	out << QString( "<title>%1 - %2</title>" ).arg( docTitle ).arg( title );
	out << "</head>";
	out << "<h1>" << title << "</h1>";
	out << "<table border=0>";
	out << "<tr><th align='left'>Document: <td>" << docTitle;
	out << "<tr><th align='left'>Path: <td>" << doc.getValue(AttrDocPath).getStr();
	out << "<tr><th align='left'>Prefix: <td>" << doc.getValue(AttrDocPrefix).getStr();
	out << "<tr><th align='left'>Document ID: <td>" << doc.getValue(AttrDocId).getStr();
	out << "<tr><th align='left'>Description: <td>" << doc.getValue(AttrDocDesc).getStr();
	out << "<tr><th align='left'>Report Date: <td>" << TypeDefs::formatDate( QDateTime::currentDateTime() );
	out << "</table><br>";
}

bool DocExporter::saveStatusAnnotReportHtml( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("UTF-8");
	_writeHeader( doc, out, "Review Status and Annotation Report" );

	out << "<table border=1 cellspacing=0>";
	out << "<tr><th>Abs.Nr.<th>Obj.Nr.<th>Type<th>Title/Text<th>Status";
	if( _hasAnnotations( doc ) )
		_writeAnnot( doc, out );
	_writeSection( doc, out );
	out << "</table></html>";
	return true;
}

static void _writeAnnot2( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj sub = sec.getFirstObj();
	//bool first = true;
	//bool close = false;
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeAnnotation )
		{
			out << "<tr><td colspan=4>.";
			out << "<td>" << sub.getValue(AttrAnnNr).getUInt32();
			out << "<td>" << _notEmpty( TypeDefs::getPrettyName( sub.getValue(AttrAnnAttr).getAtom(), sub.getDb() ) );
			out << "<td>" << _notEmpty( sub.getValue(AttrAnnText).getStr() );
			out << "<td>" << _notEmpty( _prioString( sub.getValue( AttrAnnPrio ).getUInt8() ) );
			out << "<td>" << _notEmpty( sub.getValue( AttrModifiedBy ).getStr() );
			out << "<td>" << TypeDefs::formatDate( sub.getValue( AttrModifiedOn ).getDateTime() );
			
		}
	}while( sub.next() );
}

static void _writeSection2( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		const quint8 status = o.getValue( AttrReviewStatus ).getUInt8();
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
			if( status != ReviewStatus_None || _hasAnnotations( o ) )
			{
				out << "<tr>";
				if( o.getType() == TypeOutLink )
				{
					out << "<td>.<td>" << TypeDefs::getPrettyName( o.getType(), o.getDb() );
					out << QString( "<td>%1:%2" ).
							arg( o.getValue( AttrTrgDocId ).getStr() ).
							arg( o.getValue( AttrTrgObjId ).toPrettyString() );
					out << "<td>-";
				}else if( o.getType() == TypeInLink )
				{
					out << "<td>.<td>" << TypeDefs::getPrettyName( o.getType(), o.getDb() );
					out << QString( "<td>%1:%2" ).
							arg( o.getValue( AttrSrcDocId ).getStr() ).
							arg( o.getValue( AttrSrcObjId ).toPrettyString() );
					out << "<td>-";
				}else
				{
					out << "<td>" << o.getValue( AttrObjIdent ).toPrettyString();
					out << "<td>" << TypeDefs::getPrettyName( o.getType(), o.getDb() );
					out << "<td>" << _notEmpty( o.getValue( AttrObjNumber ).getStr() );
					out << "<td>" << _notEmpty( _statusString( status ) );
				}
				out << "<td colspan=6>.";
				_writeAnnot2( o, out );
			}
			_writeSection2( o, out );
			break;
		}
	}while( o.next() );
}

bool DocExporter::saveStatusAnnotReportHtml2( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("UTF-8");
	_writeHeader( doc, out, "Review Status and Annotation Report" );

	out << "<table border=1 cellspacing=0>";
	out << "<tr><th>Abs.Nr.<th>Type<th>Obj.Nr.<th>Status<th>An.Nr.<th>Attribute<th>Annotation<th>Priority<th>Who<th>When";
	if( _hasAnnotations( doc ) )
		_writeAnnot2( doc, out );
	_writeSection2( doc, out );
	out << "</table></html>";
	return true;
}

static inline QString _headerLevel( int level )
{
	switch( level )
	{
	case 0:
		return "h1";
	case 1:
		return "h2";
	case 2:
		return "h3";
	case 3:
		return "h4";
	case 4:
		return "h5";
	default:
		return "h6";
	}
}

static void _writeRich( const Stream::DataCell& v, QTextStream& out )
{
	if( v.isBml() )
	{
		Stream::DataReader r( v );
		Txt::TextOutStream::toHtml( r, out ); 
	}else if( v.isNull() )
		out << "."; // RISK
	else
	{
		QString str = v.toPrettyString();
		str.replace( QLatin1String( "\n" ), QLatin1String( "<br>" ) );
		out << str;
	}
}

static void _writeTable( const Sdb::Obj& tbl, QTextStream& out )
{
	Sdb::Obj r = tbl.getFirstObj();
	out << "<table border=1 cellspacing=0>";
	if( !r.isNull() ) do
	{
		if( r.getType() == TypeTableRow )
		{
			out << "<tr>";
			Sdb::Obj c = r.getFirstObj();
			if( !c.isNull() ) do
			{
				if( c.getType() == TypeTableCell )
				{
					Stream::DataCell v = c.getValue( AttrObjText );
					out << "<td>";
					_writeRich( v, out );
				}
			}while( c.next() );
		}
	}while( r.next() );
	out << "</table>";
}

static QString _notNull( const Stream::DataCell& v )
{
	if( v.isNull() )
		return QString();
	else
		return v.toPrettyString();
}

static void _writeImg( const Stream::DataCell& v, QTextStream& out )
{
	if( !v.isImg() )
		return;
	out << "<img alt=\"<embedded image>\" src=\"data:image/png;base64," << v.getArr().toBase64() << "\">";
}

static void _writeAnnot3( const Sdb::Obj& obj, QTextStream& out, const QByteArray& attrName )
{
	const QString name = obj.getValue( AttrModifiedBy ).getStr();
	const QString prio = _prioString( obj.getValue( AttrAnnPrio ).getUInt8() );
	out << "<dt><strong>Annotation [";
	out << obj.getValue(AttrAnnNr).getUInt32();
	out << "] ";
	if( !attrName.isEmpty() )
		out << "of '" << QString::fromLatin1( attrName ) << "' ";
	else
		out << "of Object ";
	out << "</strong>";
	if( !name.isEmpty() )
		out << "by " << name << " ";
	out << "on " << TypeDefs::formatDate( obj.getValue( AttrModifiedOn ).getDateTime() );
	if( !prio.isEmpty() )
		out << ", " << prio;
	out << "<dd>";
	// out << _notEmpty( obj.getValue(AttrAnnText).getStr() );
	_writeRich( obj.getValue(AttrAnnText), out );
}

static void _writeAttr( const Sdb::Obj& obj, QTextStream& out, const QByteArray& attrName, quint32 a )
{
	out << "<dt><strong>Attribute ";
	out << "'" << QString::fromLatin1( attrName ) << "' ";
	out << "</strong>";
	out << "<dd>";
	_writeRich( obj.getValue( a ), out );
}


static void _writeAnnots( const Sdb::Obj& sec, QTextStream& out, bool attr )
{
	typedef QMap<QByteArray,QList<Sdb::Obj> > Sort;
	Sort sort;
	out << "<dl>";
	Sdb::Obj sub = sec.getFirstObj();
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeAnnotation )
		{
			const quint32 a = sub.getValue(AttrAnnAttr).getAtom();
			const QByteArray attrName = TypeDefs::getPrettyName( a, sub.getDb() );
			if( a == AttrObjText || a == AttrStubTitle )
				_writeAnnot3( sub, out, attrName );
			else
				sort[attrName].append( sub );
		}
	}while( sub.next() );
	Sort::const_iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		for( int j = 0; j < i.value().size(); j++ )
		{
			const quint32 a = i.value()[j].getValue(AttrAnnAttr).getAtom();
			if( attr && j == 0 && a )
				_writeAttr( sec, out, i.key(), a );
			_writeAnnot3( i.value()[j], out, i.key() );
		}
	}
	out << "</dl>";
}

static void _writeSection3( const Sdb::Obj& sec, QTextStream& out, int level, bool annot, bool attr )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		const quint8 status = o.getValue( AttrReviewStatus ).getUInt8();
		switch( o.getType() )
		{
		case TypeTitle:
			out << "<tr>";
			out << "<td>" << _notEmpty(_notNull(o.getValue( AttrObjIdent ) ) );
			out << "<td><" << _headerLevel( level ) << ">" <<
				o.getValue( AttrObjNumber ).getStr() << " " <<
				o.getValue( AttrObjText ).getStr() << 
				"</" << _headerLevel( level ) << ">";
			if( annot )
			{
				_writeAnnots( o, out, attr );
				out << "<td>" << _notEmpty( _statusString( status ) );
			}
			_writeSection3( o, out, level + 1, annot, attr );
			break;
		case TypeSection:
			out << "<tr>";
			out << "<td>" << _notEmpty(_notNull(o.getValue( AttrObjIdent ) ) );
			out << "<td>";
			_writeRich( o.getValue( AttrObjText ), out );
			if( annot )
			{
				_writeAnnots( o, out, attr );
				out << "<td>" << _notEmpty( _statusString( status ) );
			}
			_writeSection3( o, out, level + 1, annot, attr );
			break;
		case TypeTable:
			out << "<tr>";
			out << "<td>" << _notEmpty(_notNull(o.getValue( AttrObjIdent ) ) );
			out << "<td>";
			_writeTable( o, out );
			if( annot )
			{
				_writeAnnots( o, out, attr );
				out << "<td>" << _notEmpty( _statusString( status ) );
			}
			break;
		case TypePicture:
			out << "<tr>";
			out << "<td>" << _notEmpty(_notNull(o.getValue( AttrObjIdent ) ) );
			out << "<td>";
			_writeImg( o.getValue( AttrPicImage ), out );
			if( annot )
			{
				_writeAnnots( o, out, attr );
				out << "<td>" << _notEmpty( _statusString( status ) );
			}
			break;
		}
	}while( o.next() );
}

bool DocExporter::saveFullDocHtml( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("UTF-8");
	_writeHeader( doc, out, "Document Report" );
	out << "<table border=1 cellspacing=0 align='left'>";
	out << "<tr><th>Abs.Nr.<th>Title/Text";
	_writeSection3( doc, out, 0, false, false );
	out << "</table></html>";
	return true;
}

bool DocExporter::saveFullDocAnnotHtml( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f ); 
	out.setCodec("UTF-8");
	_writeHeader( doc, out, "Annotated Document Report" );
	_writeAnnots( doc, out, false );
	out << "<table border=1 cellspacing=0 align='left'>";
	out << "<tr><th>Abs.Nr.<th>Title/Text<th>Status";
	_writeSection3( doc, out, 0, true, false );
	out << "</table></html>";
	return true;
}

bool DocExporter::saveFullDocAnnotAttrHtml( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("UTF-8");
	_writeHeader( doc, out, "Annotated Document Report" );
	_writeAnnots( doc, out, true );
	out << "<table border=1 cellspacing=0 align='left'>";
	out << "<tr><th>Abs.Nr.<th>Title/Text<th>Status";
	_writeSection3( doc, out, 0, true, true );
	out << "</table></html>";
	return true;
}

static void _writeCsv( QTextStream& out, QString str )
{
	str.replace("\"", "'" );
	out << "\"" << str << "\"";
}

static void _writeRow( const Sdb::Obj& sec, const QList<quint32>& attrs, QTextStream& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		switch( o.getType() )
		{
		case TypeTitle:
		case TypeSection:
		case TypeTable:
		case TypeTableRow:
		case TypeTableCell:
		case TypePicture:
			out << o.getValue( AttrObjIdent ).toPrettyString(); // vorher getInt32();
			for( int i = 0; i < attrs.size(); i++ )
			{
				Stream::DataCell v = o.getValue( attrs[i] );
				out << ",";
				if( v.isBml() )
				{
					Stream::DataReader r( v );
					_writeCsv( out, r.extractString() );
				}else if( v.isStr() )
					_writeCsv( out, v.getStr() );
				else if( !v.isNull() )
					_writeCsv( out, v.toPrettyString() );
			}
			out << "\n";
			_writeRow( o, attrs, out );
			break;
		}
	}while( o.next() );
}

bool DocExporter::saveAttrCsv( const Sdb::Obj& doc, const QList<quint32>& attrs, 
							  const QString& path, bool utf8 )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	if( utf8 )
		out.setCodec("UTF-8");
	else
		out.setCodec("Latin1");
	out << "\"Abs.Nr.\"";
	for( int i = 0; i < attrs.size(); i++ )
		out << ",\"" << doc.getDb()->getAtomString( attrs[i] ) << "\"";
	out << "\n";
	_writeRow( doc, attrs, out );
	return true;
}
static void _writeReviewStatus( const Sdb::Obj& o, Stream::DataWriter& out )
{
	const quint8 status = o.getValue( AttrReviewStatus ).getUInt8();
	if( status >= ReviewStatus_Recheck )
	{
		out.startFrame( NameTag("stat") );
		out.writeSlot( o.getValue( AttrObjIdent ), NameTag( "anr" ) );
		out.writeSlot( DataCell().setAtom( o.getType() ), NameTag("ot") );
		out.writeSlot( o.getValue( AttrReviewStatus ), NameTag("stat") );
		DataCell v = o.getValue( AttrReviewer );
		if( !v.isNull() )
			out.writeSlot( v, NameTag("who") );
		else
			out.writeSlot( DataCell().setString( AppContext::inst()->getUserName() ), NameTag("who") ); // RISK
		v = o.getValue( AttrStatusDate );
		if( !v.isNull() )
			out.writeSlot( v, NameTag( "when" ) );
		else
			out.writeSlot( DataCell().setDateTime( QDateTime::currentDateTime() ), NameTag( "when" ) ); // RISK
		out.endFrame();
	}
}

static void _writeAnnot( const Sdb::Obj& owner, const Sdb::Obj& a, Stream::DataWriter& out )
{
	if( owner.getType() == TypeOutLink )
	{
		out.startFrame( NameTag("olan") );
		out.writeSlot( owner.getOwner().getValue( AttrObjIdent ), NameTag( "anr" ) );
		out.writeSlot( DataCell().setAtom( owner.getOwner().getType() ), NameTag("ot") );
		out.writeSlot( owner.getValue( AttrTrgObjId ), NameTag( "tanr" ) );
		out.writeSlot( owner.getValue( AttrTrgDocId ), NameTag( "tdid" ) );
		out.writeSlot( owner.getValue( AttrLnkModId ), NameTag( "tmid" ) );
	}else if( owner.getType() == TypeInLink )
	{
		out.startFrame( NameTag("ilan") );
		out.writeSlot( owner.getOwner().getValue( AttrObjIdent ), NameTag( "anr" ) );
		out.writeSlot( DataCell().setAtom( owner.getOwner().getType() ), NameTag("ot") );
		out.writeSlot( owner.getValue( AttrSrcObjId ), NameTag( "sanr" ) );
		out.writeSlot( owner.getValue( AttrSrcDocId ), NameTag( "sdid" ) );
		out.writeSlot( owner.getValue( AttrLnkModId ), NameTag( "smid" ) );
	}else
	{
		out.startFrame( NameTag("oan") );
		out.writeSlot( owner.getValue( AttrObjIdent ), NameTag( "anr" ) );
		out.writeSlot( DataCell().setAtom( owner.getType() ), NameTag("ot") );
	}
	out.writeSlot( a.getValue(AttrAnnNr), NameTag( "annr" ) );
	DataCell v = a.getValue(AttrAnnAttr);
	if( !v.isNull() )
		out.writeSlot( v, NameTag( "attr" ) );
	out.writeSlot( a.getValue(AttrAnnText), NameTag( "text" ) );
	out.writeSlot( a.getValue( AttrAnnPrio ), NameTag( "prio" ) );
	out.writeSlot( a.getValue( AttrModifiedBy ), NameTag( "whom" ) );
	out.writeSlot( a.getValue( AttrModifiedOn ), NameTag( "mod" ) );
	out.writeSlot( a.getValue( AttrCreatedBy ), NameTag( "whoc" ) );
	out.writeSlot( a.getValue( AttrCreatedOn ), NameTag( "cre" ) );

	out.endFrame();
}

static void _descent( const Sdb::Obj& sec, Stream::DataWriter& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
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
			_writeReviewStatus( o, out );
			_descent( o, out );
			break;
		case TypeAnnotation:
			_writeAnnot( sec, o, out );
			break;
		}
	}while( o.next() );
}

bool DocExporter::saveAnnot( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	Stream::DataWriter out( &f );

	// Header
	out.writeSlot( DataCell().setString("DoorScopeAnnot") );
	out.writeSlot( DataCell().setString("0.2") );
	out.writeSlot( DataCell().setDateTime( QDateTime::currentDateTime() ) );

	// Document Info
	out.writeSlot( doc.getValue( AttrDocId ), NameTag( "did" ) );
	out.writeSlot( doc.getValue( AttrDocVer ), NameTag( "dver" ) );
	out.writeSlot( doc.getValue( AttrDocName ), NameTag( "dnm" ) );
	DataCell v = doc.getValue( AttrDocAltName );
	if( !v.isNull() )
		out.writeSlot( v, NameTag( "danm" ) );
	out.writeSlot( doc.getValue( AttrDocPath ), NameTag( "dpa" ) );
	out.writeSlot( doc.getValue( AttrModifiedOn ), NameTag( "dmod" ) );
	out.writeSlot( doc.getValue( AttrCreatedOn ), NameTag( "dcre" ) );
	out.writeSlot( doc.getValue( AttrDocExported ), NameTag( "dexp" ) );
	out.writeSlot( doc.getValue( AttrDocBaseline ), NameTag( "dbl" ) );

	_descent( doc, out );
	return true;
}

static void _writeAnnot2Csv( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj sub = sec.getFirstObj();
	//bool first = true;
	//bool close = false;
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeAnnotation )
		{
			out << ",,,,,";
			out << sub.getValue(AttrAnnNr).getUInt32();
			out << ",";
			_writeCsv( out, TypeDefs::getPrettyName( sub.getValue(AttrAnnAttr).getAtom(), sub.getDb() ) );
			out << ",";
			_writeCsv( out, sub.getValue(AttrAnnText).getStr() );
			out << ",";
			_writeCsv( out, _prioString( sub.getValue( AttrAnnPrio ).getUInt8() ) );
			out << ",";
			_writeCsv( out, sub.getValue( AttrModifiedBy ).getStr() );
			out << ",";
			_writeCsv( out, TypeDefs::formatDate( sub.getValue( AttrModifiedOn ).getDateTime() ) );
			out << "\n";
			
		}
	}while( sub.next() );
}


static void _writeAnnot3Csv( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj sub = sec.getFirstObj();
	QString anr;
	if( !sec.getValue( AttrObjIdent ).isNull() )
		anr = sec.getValue( AttrObjIdent ).toPrettyString();
	if( !sub.isNull() ) do 
	{
		if( sub.getType() == TypeAnnotation )
		{
			out << anr;
			out << ",";
			out << sub.getValue(AttrAnnNr).getUInt32();
			out << ",";
			_writeCsv( out, TypeDefs::getPrettyName( sub.getValue(AttrAnnAttr).getAtom(), sub.getDb() ) );
			out << ",";
			_writeCsv( out, sub.getValue(AttrAnnText).getStr() );
			out << ",";
			_writeCsv( out, _prioString( sub.getValue( AttrAnnPrio ).getUInt8() ) );
			out << ",";
			_writeCsv( out, sub.getValue( AttrModifiedBy ).getStr() );
			out << ",";
			_writeCsv( out, TypeDefs::formatDate( sub.getValue( AttrModifiedOn ).getDateTime() ) );
			out << "\n";
			
		}
	}while( sub.next() );
}

static void _writeSection2Csv( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		const quint8 status = o.getValue( AttrReviewStatus ).getUInt8();
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
			if( status != ReviewStatus_None || _hasAnnotations( o ) )
			{
				if( o.getType() == TypeOutLink )
				{
					out << ",";
					_writeCsv( out, TypeDefs::getPrettyName( o.getType(), o.getDb() ) );
					out << ",";
					_writeCsv( out, QString( "%1:%2" ).
							arg( o.getValue( AttrTrgDocId ).getStr() ).
							arg( o.getValue( AttrTrgObjId ).toPrettyString() ) );
					out << ",";
					_writeCsv( out, QString( "%1 %2 %3" ).
						arg( o.getValue( AttrTrgDocName ).getStr() ).
						arg( o.getValue( AttrTrgDocVer ).getStr() ).
						arg( TypeDefs::formatDate( o.getValue( AttrTrgDocModDate ).getDateTime() ) ) );
					out << ",";
				}else if( o.getType() == TypeInLink )
				{
					out << ",";
					_writeCsv( out, TypeDefs::getPrettyName( o.getType(), o.getDb() ) );
					out << ",";
					_writeCsv( out, QString( "%1:%2" ).
							arg( o.getValue( AttrSrcDocId ).getStr() ).
							arg( o.getValue( AttrSrcObjId ).toPrettyString() ) );
					out << ",";
					_writeCsv( out, QString( "%1 %2 %3" ).
						arg( o.getValue( AttrSrcDocName ).getStr() ).
						arg( o.getValue( AttrSrcDocVer ).getStr() ).
						arg( TypeDefs::formatDate( o.getValue( AttrSrcDocModDate ).getDateTime() ) ) );
					out << ",";
				}else
				{
					out << o.getValue( AttrObjIdent ).toPrettyString();
					out << ",";
					_writeCsv( out, TypeDefs::getPrettyName( o.getType(), o.getDb() ) );
					out << ",";
					_writeCsv( out, o.getValue( AttrObjNumber ).getStr() );
					out << ",";
					_writeCsv( out, TypeDefs::elided( o.getValue( AttrObjText ) ) );
					out << ",";
					_writeCsv( out, _statusString( status ) );
				}
				out << "\n";
				_writeAnnot2Csv( o, out );
			}
			_writeSection2Csv( o, out );
			break;
		}
	}while( o.next() );
}

static void _writeSection3Csv( const Sdb::Obj& sec, QTextStream& out )
{
	Sdb::Obj o = sec.getFirstObj();
	if( !o.isNull() ) do
	{
		switch( o.getType() )
		{
		case TypeTitle:
		case TypeSection:
		case TypeTable:
		case TypeTableRow:
		case TypeTableCell:
		case TypePicture:
			if( _hasAnnotations( o ) )
				_writeAnnot3Csv( o, out );
			_writeSection3Csv( o, out );
			break;
		}
	}while( o.next() );
}

bool DocExporter::saveStatusAnnotReportCsv( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("Latin1");
	out << "\"Abs.Nr.\",\"Type\",\"Obj.Nr.\",\"Text\",\"Status\",\"An.Nr.\",\"Attribute\",\"Annotation\",\"Priority\",\"Who\",\"When\"\n";
	if( _hasAnnotations( doc ) )
		_writeAnnot2Csv( doc, out );
	_writeSection2Csv( doc, out );
	return true;
}

bool DocExporter::saveAnnotCsv( const Sdb::Obj& doc, const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::WriteOnly ) )
		return false;
	QTextStream out( &f );
	out.setCodec("Latin1");
	out << "\"Abs.Nr.\",\"An.Nr.\",\"Attribute\",\"Annotation\",\"Priority\",\"Who\",\"When\"\n";
	if( _hasAnnotations( doc ) )
		_writeAnnot3Csv( doc, out );
	_writeSection3Csv( doc, out );
	return true;
}
