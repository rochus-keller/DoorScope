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

#include "DocImporter.h"
#include "TypeDefs.h"
#include "AppContext.h"
#include "DocManager.h"
#include <QFile>
#include <QBuffer>
#include <QtDebug>
#include <Sdb/Database.h>
#include <Txt/TextInStream.h>
#include <private/qtexthtmlparser_p.h>
#include <bitset>
#include <QStack>
#include <QFileInfo>
#include <QDir>
using namespace Ds;
using namespace Sdb;
using namespace Stream;
using namespace Txt;

class _MyException : public std::exception
{
	QByteArray d_msg;
public:
	_MyException( const char* msg = "" ):d_msg(msg) {}
	~_MyException() throw() {}
	const char* what() const throw() { return d_msg; }
};

DocImporter::DocImporter(QObject *parent)
	: QObject(parent)
{

}

DocImporter::~DocImporter()
{

}

bool DocImporter::checkCompatible( Sdb::Obj& doc, const QString& path )
{
	d_error.clear();
	d_info.clear();
	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		d_error = tr( "Cannot open file for reading" );
		return false;
	}
	try
	{
		Stream::DataReader in( &f );

		Stream::DataReader::Token t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() || 
			in.readValue().getStr() != "DoorScopeAnnot" )
		{
			d_error = tr("unknown stream file format");
			throw std::exception();
		}
		t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() || 
			( in.readValue().getStr() != "0.2" && in.readValue().getStr() != "0.1" ) )
		{
			d_error = tr("stream file version not supported");
			throw std::exception();
		}
		t = in.nextToken();
		QString did;
		QString dver;
		QString dnm;
		QString danm;
		QDateTime dmod;
		bool dbl = false;
		while( t == DataReader::Slot )
		{
			const NameTag name = in.getName().getTag();
			if( name.equals( "did" ) )
				did = in.readValue().getStr();
			else if( name.equals( "dver" ) )
				dver = in.readValue().getStr();
			else if( name.equals( "dnm" ) )
				dnm = in.readValue().getStr();
			else if( name.equals( "danm" ) )
				danm = in.readValue().getStr();
			else if( name.equals( "dmod" ) )
				dmod = in.readValue().getDateTime();
			else if( name.equals( "dbl" ) )
				dbl = in.readValue().getBool();
			t = in.nextToken(); 
		}
		if( doc.getValue( AttrDocId ).getStr() != did ||
			doc.getValue( AttrDocVer ).getStr() != dver ||
			doc.getValue( AttrModifiedOn ).getDateTime() != dmod ||
			doc.getValue( AttrDocBaseline ).getBool() != dbl )
		{
			if( !danm.isEmpty() )
				dnm = danm;
			d_info = QObject::tr( "%1 [%2 %3 %4]" ).arg( dnm ).
				arg( dver ).
				arg( (dbl)?"Baseline":"Current" ).
				arg( TypeDefs::formatDate( dmod ) ); 
			return false;
		}else
			return true;
	}catch( std::exception& e )
	{
		d_error += e.what();
		return false;
	}
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

bool DocImporter::readAnnot( Sdb::Obj& doc, const QString& path, bool overwriteReviewStatus )
{
	d_error.clear();
	d_info.clear();

	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		d_error = tr( "Cannot open file for reading" );
		return false;
	}
	d_nrToOid.clear();
	_fillMap( d_nrToOid, doc );
	d_nrToOid[0] = doc.getId(); // ansonsten werden Annotations des Dokuments nicht importiert
	d_misses.clear();
	Database::Lock lock( AppContext::inst()->getDb(), true );
	try
	{
		Stream::DataReader in( &f );

		Stream::DataReader::Token t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() || 
			in.readValue().getStr() != "DoorScopeAnnot" )
		{
			d_error = tr("unknown stream file format");
			throw std::exception();
		}
		t = in.nextToken();
		if( t != DataReader::Slot || !in.getName().isNull() || 
			( in.readValue().getStr() != "0.2" && in.readValue().getStr() != "0.1" ) )
		{
			d_error = tr("stream file version not supported");
			throw std::exception();
		}
		t = in.nextToken();
		while( t == DataReader::Slot )
			t = in.nextToken(); // überlese alles bis erstes Frame

		while( DataReader::isUseful( t ) )
		{
			if( t == DataReader::BeginFrame )
			{
				const NameTag name = in.getName().getTag();
				if( name.equals( "stat" ) )
					readStat( in, doc, overwriteReviewStatus );
				else if( name.equals( "olan" ) || name.equals( "ilan" ) || name.equals( "oan" ) )
					readAnnot( in, doc );
				else
				{
					d_error += tr("unknown frame ") + name.toString();
					throw std::exception();
				}
			}else
				throw _MyException("protocol error");
			t = in.nextToken();
		}

		AppContext::inst()->getTxn()->commit();
		lock.commit();
		return true;
	}catch( DatabaseException& e )
	{
		d_error += tr("invalid stream file format\n");
		d_error += e.getCodeString() + " " + e.getMsg();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return false;
	}catch( std::exception& e )
	{
		d_error += e.what();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return false;
	}
}

void DocImporter::readStat( Stream::DataReader& in, Sdb::Obj& doc, bool overwriteReviewStatus )
{
	Stream::DataReader::Token t = in.nextToken();
	qint32 anr = 0;
	quint32 ot = 0;
	quint8 stat = 0;
	QString who;
	QDateTime when;
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			const NameTag name = in.getName().getTag();
			if( name.equals( "anr" ) )
				anr = in.readValue().getInt32();
			else if( name.equals( "ot" ) )
				ot = in.readValue().getAtom();
			else if( name.equals( "stat" ) )
				stat = in.readValue().getUInt8();
			else if( name.equals( "who" ) )
				who = in.readValue().getStr();
			else if( name.equals( "when" ) )
				when = in.readValue().getDateTime();
			else
			{
				d_error += tr("unknown slot ") + name.toString();
				throw _MyException("protocol error");
			}
		}else if( t == DataReader::EndFrame )
		{
			break;
		}else
			throw _MyException("protocol error");
		t = in.nextToken();
	}
	Obj o = doc.getTxn()->getObject( d_nrToOid.value( anr ) );
	if( !o.isNull() )
	{
		if( o.getType() == TypeSection && ot == TypeTitle )
			o = o.getOwner();
		if( overwriteReviewStatus )
		{
			const quint8 old_stat = o.getValue( AttrReviewStatus ).getUInt8();
			const QString old_who = o.getValue( AttrReviewer ).getStr();
			const QDateTime old_when = o.getValue( AttrStatusDate ).getDateTime();
			o.setValue( AttrReviewStatus, DataCell().setUInt8( stat ) );
			o.setValue( AttrStatusDate, DataCell().setDateTime( when ) );
			o.setValue( AttrReviewer, DataCell().setString( who ) );
			stat = old_stat;
			when = old_when;
			who = old_who;
		} 
		if( stat >= ReviewStatus_Recheck )
		{
			QByteArray bml = o.getValue( AttrConsRevStat ).getBml();
			QBuffer buf( &bml );
			buf.open( QIODevice::Append );
			DataWriter w( &buf );
			w.startFrame();
			w.writeSlot( DataCell().setUInt8( stat ) );
			if( !who.isEmpty() )
				w.writeSlot( DataCell().setString( who ) );
			if( when.isValid() )
				w.writeSlot( DataCell().setDateTime( when ) );
			w.endFrame();
			buf.close();
			o.setValue( AttrConsRevStat, DataCell().setBml( bml ) );
		}
	}else
		d_misses.append( anr );
}

void DocImporter::readAnnot( Stream::DataReader& in, Sdb::Obj& doc )
{
	Stream::DataReader::Token t = in.nextToken();
	qint32 anr = 0;
	quint32 ot = 0;
	bool inlink = false;
	qint32 tanr = 0;
	QString tdid;
	QString tmid;
	quint32 annr = 0;
	quint32 attr = 0;
	QString text;
	quint8 prio = 0;
	QString whom;
	QDateTime mod;
	QString whoc;
	QDateTime cre;
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			const NameTag name = in.getName().getTag();
			if( name.equals( "anr" ) )
				anr = in.readValue().getInt32();
			else if( name.equals( "ot" ) )
				ot = in.readValue().getAtom();
			else if( name.equals( "tanr" ) )
				tanr = in.readValue().getInt32();
			else if( name.equals( "sanr" ) )
			{
				inlink = true;
				tanr = in.readValue().getInt32();
			}else if( name.equals( "tdid" ) || name.equals( "sdid" ) )
				tdid = in.readValue().getStr();
			else if( name.equals( "tmid" ) || name.equals( "smid" ) )
				tmid = in.readValue().getStr();
			else if( name.equals( "annr" ) )
				annr = in.readValue().getUInt32();
			else if( name.equals( "attr" ) )
				attr = in.readValue().getAtom();
			else if( name.equals( "text" ) )
				text = in.readValue().getStr();
			else if( name.equals( "prio" ) )
				prio = in.readValue().getUInt8();
			else if( name.equals( "whom" ) )
				whom = in.readValue().getStr();
			else if( name.equals( "mod" ) )
				mod = in.readValue().getDateTime();
			else if( name.equals( "whoc" ) )
				whoc = in.readValue().getStr();
			else if( name.equals( "cre" ) )
				cre = in.readValue().getDateTime();
			else
			{
				d_error += tr("unknown slot ") + name.toString();
				throw _MyException("protocol error");
			}
		}else if( t == DataReader::EndFrame )
		{
			break;
		}else
			throw _MyException("protocol error");
		t = in.nextToken();
	}
	Obj o = doc.getTxn()->getObject( d_nrToOid.value( anr ) );
	if( !o.isNull() )
	{
		if( o.getType() == TypeSection && ot == TypeTitle )
			o = o.getOwner();
		if( tanr )
		{
			Sdb::Obj i = o.getFirstObj();
			//const quint32 type = (inlink)? quint32(TypeInLink) : quint32(TypeOutLink);
			if( !i.isNull() ) do
			{
				if( !inlink && i.getType() == TypeOutLink )
				{
					if( i.getValue( AttrTrgObjId ).getInt32() == tanr &&
						i.getValue( AttrTrgDocId ).getStr() == tdid &&
						i.getValue( AttrLnkModId ).getStr() == tmid )
					{
						o = i;
						break;
					}
				}else if( inlink && i.getType() == TypeInLink )
				{
					if( i.getValue( AttrSrcObjId ).getInt32() == tanr &&
						i.getValue( AttrSrcDocId ).getStr() == tdid &&
						i.getValue( AttrLnkModId ).getStr() == tmid )
					{
						o = i;
						break;
					}
				}
			}while( i.next() );
		}
		
		Sdb::Obj a = doc.getTxn()->createObject( TypeAnnotation );
		if( attr )
			a.setValue( AttrAnnAttr, Stream::DataCell().setAtom( attr ) );
		a.setValue( AttrAnnHomeDoc, doc );
		a.setValue( AttrAnnOldNr, DataCell().setString( QString::number( annr ) ) );
		a.setValue( AttrAnnText, DataCell().setString( text ) );
		a.setValue( AttrAnnPrio, DataCell().setUInt8( prio ) );
		const quint32 nr = doc.getValue( AttrDocMaxAnnot ).getUInt32() + 1;
		a.setValue( AttrAnnNr, DataCell().setUInt32( nr ) );
		doc.setValue( AttrDocMaxAnnot, DataCell().setUInt32( nr ) );
		a.setValue( AttrCreatedBy, DataCell().setString( whoc ) );
		a.setValue( AttrModifiedBy, DataCell().setString( whom ) );
		a.setValue( AttrCreatedOn, DataCell().setDateTime( cre ) );
		a.setValue( AttrModifiedOn, DataCell().setDateTime( mod ) );
		a.aggregateTo( o );
		o.setValue( AttrAnnotated, DataCell().setInt32( o.getValue( AttrAnnotated ).getInt32() + 1 ) );
		if( o.getId() != doc.getId() )
			doc.setValue( AttrAnnotated, DataCell().setInt32( doc.getValue( AttrAnnotated ).getInt32() + 1 ) );
	}else
		d_misses.append( anr );
}

struct Context
{
	Context():nextId(1){}
	QStack< QPair<Sdb::Obj,int> > trace;
	quint32 nextId;
	Sdb::Obj doc;
	QTextHtmlParser parser;
};

static inline void _aggr( Sdb::Obj& obj, Context& ctx )
{
	obj.aggregateTo( ctx.trace.top().first );
	obj.setValue( AttrObjDocId, ctx.doc.getValue( AttrDocId ) );
	obj.setValue( AttrObjHomeDoc, ctx.doc );
	obj.setValue( AttrObjIdent, DataCell().setInt32( ctx.nextId++ ) );
	obj.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
	obj.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
	obj.setValue( AttrCreatedThru, DataCell().setString( "HTML Import" ) );
}

static void readImg( Context& ctx, const QTextHtmlParserNode& n, DataWriter& out )
{
    // NOTE n.imageName ist noch kodiert und enthält z.B. %20
	QFileInfo path;
    QImage img;
	if( n.imageName.startsWith( QLatin1String( "file:///" ), Qt::CaseInsensitive ) ) // Windows-Spezialität
		// siehe http://en.wikipedia.org/wiki/File_URI_scheme
		path = n.imageName.mid( 8 );
    else if( n.imageName.startsWith( "data:" ) )
    {
        // Daten sind direkt in das Attribut eingebettet
        // image/png;base64, siehe http://de.wikipedia.org/wiki/Data-URL
        const int pos1 = n.imageName.indexOf(QLatin1String("image/"),4, Qt::CaseInsensitive);
        if( pos1 != -1 )
        {
            const int pos2 = n.imageName.indexOf( QChar(';'), pos1 + 6 );
            if( pos2 != -1 )
            {
                const QString imgType = n.imageName.mid( pos1 + 6, pos2 - pos1 -6 ).toUpper();
                const int pos3 = n.imageName.indexOf(QLatin1String("base64,"),pos2 + 1, Qt::CaseInsensitive);
                if( pos3 != -1 )
                {
                    const QByteArray base64 = n.imageName.mid( pos3 + 7 ).toAscii();
                    img.loadFromData( QByteArray::fromBase64( base64 ), imgType.toAscii() );
                }
            }
        }
	}else
	{
		const QUrl url = QUrl::fromEncoded( n.imageName.toAscii(), QUrl::TolerantMode );
		path = url.toLocalFile();
	}
	if( img.isNull() )
        img.load( path.absoluteFilePath() );

    if( img.isNull() ) // auch bei fehlerhaftem Load null
    {
		img.load( ":/DoorScope/Images/img_placeholder.png" );
		qDebug() << "Cannot find " << n.imageName << " " << path.absoluteFilePath();
	}

	out.startFrame( NameTag("img") );
	if( n.imageWidth > 0.0 && n.imageHeight > 0.0 )
	{
		//out.writeSlot( DataCell().setUInt16( n.imageWidth ), NameTag("w") );
		//out.writeSlot( DataCell().setUInt16( n.imageHeight ), NameTag("h") );
		img = img.scaled( QSize( n.imageWidth, n.imageHeight ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
	}
	out.writeSlot( DataCell().setImage( img ) );
	out.endFrame();
}

static int f2i( QTextHTMLElements f )
{
	switch( f )
	{
	case Html_em:
	case Html_i:
	case Html_cite:
	case Html_var:
	case Html_dfn:
		return TextInStream::Italic;
	case Html_big:
	case Html_small:
	case Html_nobr: // unbekannt
		// ignorieren
		break;
	case Html_strong:
	case Html_b:
		return TextInStream::Bold;
	case Html_u:
		return TextInStream::Underline;
	case Html_s:
		return TextInStream::Strikeout;
	case Html_code:
	case Html_tt:
	case Html_kbd:
	case Html_samp:
		return TextInStream::Fixed;
	case Html_sub:
		return TextInStream::Sub;
	case Html_sup:
		return TextInStream::Super;
	default:
		break;
	}
	return -1;
}

static QString simplify( QString str )
{
	if ( str.isEmpty() )
        return str;
    QString result;
	result.resize( str.size() );
    const QChar* from = str.data();
    const QChar* fromend = from + str.size();
    int outc = 0;
    QChar* to = result.data();

	// Ersetzte den Whitespace am Anfang durch einen Space
	while( from != fromend && from->isSpace() )
		from++;
    if( from != str.data() )
        to[ outc++ ] = QLatin1Char(' ');
    for(;from != fromend ;) 
	{
		// Ersetze den Whitespace im Rest des Strings durch einen Space inkl. dem Ende
        while( from != fromend && from->isSpace() )
            from++;
        while( from != fromend && !from->isSpace() )
            to[ outc++ ] = *from++;
        if( from != fromend )
            to[ outc++ ] = QLatin1Char(' ');
        else
            break;
    }
    result.truncate( outc );
    return result;
}

static void consumeFollowers( Context& ctx, int n, DataWriter& out, std::bitset<8>& format, int fidx = -1 )
{
	const int parent = ctx.parser.at(n).parent;
	n++;
	while( n < ctx.parser.count() && ctx.parser.at(n).id == Html_unknown )
	{
		// Setzte Format sofort zurück, sobald Scope des Formats endet
		if( fidx != -1 && parent != ctx.parser.at(n).parent )
			format.set( fidx, false );
		const QString str = simplify( ctx.parser.at(n).text );
		if( !str.isEmpty() )
		{
			out.startFrame( NameTag("frag") );
			out.writeSlot( DataCell().setUInt8( format.to_ulong() ) );
			out.writeSlot( DataCell().setString( str ) );
			out.endFrame();
		}
		n++;
	}
}

static void readFrag( Context& ctx, const QTextHtmlParserNode& n, DataWriter& out, std::bitset<8>& format )
{
	if( n.id == Html_img )
	{
		readImg( ctx, n, out );
		return;
	}else if( n.id == Html_br )
	{
		// TODO: setzt <br> das Format zurück?
		out.startFrame( NameTag("frag") );
		out.writeSlot( DataCell().setUInt8( format.to_ulong() ) );
		out.writeSlot( DataCell().setString( "\n" ) );
		out.endFrame();
		return;
	} // else

	const int fidx = f2i( n.id );
	if( fidx != -1 )
		format.set( fidx );

	const QString str = simplify( n.text );
	if( !str.isEmpty() )
	{
		out.startFrame( NameTag("frag") );
		out.writeSlot( DataCell().setUInt8( format.to_ulong() ) );
		out.writeSlot( DataCell().setString( str ) );
		out.endFrame();
	}

	for( int i = 0; i < n.children.size(); i++ )
	{
		readFrag( ctx, ctx.parser.at( n.children[i] ), out, format );
		consumeFollowers( ctx, n.children[i], out, format, fidx );
	}

	if( fidx != -1 )
		format.set( fidx, false );
}

static void readFrags( Context& ctx, const QTextHtmlParserNode& p, DataWriter& out )
{
	std::bitset<8> format;
	for( int i = 0; i < p.children.size(); i++ )
	{
		// Starte Neues frag für je imp und alle übrigen
		// img hat nie Children
		// a hat selten Children. Falls dann sup oder sub.
		// Hier sollte a einfach als Text behandelt werden, da wir sonst die Art der URL interpretieren müssen.
		readFrag( ctx, ctx.parser.at( p.children[i] ), out, format );
		consumeFollowers( ctx, p.children[i], out, format );
	}
}

static QString collectText( const QTextHtmlParser& parser, const QTextHtmlParserNode& p )
{
	// Hier verwenden wir normales QString::simplified(), da Formatierung ignoriert wird.
	QString text;
	if( p.id == Html_img )
		text += "<img>";
	text += p.text.simplified();
	for( int i = 0; i < p.children.size(); i++ )
	{
		text += collectText( parser, parser.at( p.children[i] ) );
		int n = p.children[i] + 1;
		while( n < parser.count() && parser.at(n).id == Html_unknown )
		{
			text += parser.at(n).text.simplified();
			n++;
		}
	}
	return text;
}

static QString coded( QString str )
{
	str.replace( QChar('&'), "&amp;" );
	str.replace( QChar('>'), "&gt;" );
	str.replace( QChar('<'), "&lt;" );
	return str;
}

static QString generateHtml( Context& ctx, const QTextHtmlParserNode& p )
{
	const QStringList blocked = QStringList() << "width" << "height" << "lang" << "class" << "size" << 
		"style" << "align" << "valign";

	QString html;
	if( p.id != Html_unknown && p.id != Html_font )
	{
		// qDebug() << p.tag << p.attributes; // TEST
		html += QString( "<%1" ).arg( p.tag );
		for( int i = 0; i < p.attributes.size() / 2; i++ )
		{
			const QString name = p.attributes[ 2 * i ].toLower();
			if( !blocked.contains( name ) )
				html += QString( " %1=\"%2\"" ).arg( name ).arg( p.attributes[ 2 * i + 1 ] );
		}
		html += ">";
	}
	html += coded( p.text ); 
	for( int i = 0; i < p.children.size(); i++ )
	{
		html += generateHtml( ctx, ctx.parser.at( p.children[i] ) );
		int n = p.children[i] + 1;
		while( n < ctx.parser.count() && ctx.parser.at(n).id == Html_unknown )
		{
			html += coded( ctx.parser.at(n).text ); 
			n++;
		}
	}
	if( p.id != Html_unknown && p.id != Html_font )
	{
		html += QString( "</%1>" ).arg( p.tag );
	}
	return html;
}

static QByteArray readParagraph( Context& ctx, const QTextHtmlParserNode& p )
{
	// Gehe durch alle Nodes innerhalb p und baue Richtext zusammen
	// TODO: falls nur einfacher Text enthalten, sollte nicht BML geschrieben werden.
	DataWriter out;

	out.startFrame( NameTag("rtxt") );
	out.writeSlot( DataCell().setAscii( "0.1" ), NameTag("ver") );

	if( p.id == Html_pre )
	{
		out.startFrame( NameTag( "code" ) );
		out.writeSlot( DataCell().setString( p.text ) );
		out.endFrame(); 
	}else
	{
		out.startFrame( NameTag( "par" ) );
		const QString str = simplify( p.text );
		if( !str.isEmpty() )
		{
			out.startFrame( NameTag("frag") );
			out.writeSlot( DataCell().setString( str ) );
			out.endFrame(); // frag
		}
		readFrags( ctx, p, out );
		out.endFrame(); // par
	}
	out.endFrame(); // rtxt
	return out.getStream();
}

static int h2i( QTextHTMLElements f )
{
	switch( f )
	{
	case Html_h1:
		return 1;
	case Html_h2:
		return 2;
	case Html_h3:
		return 3;
	case Html_h4:
		return 4;
	case Html_h5:
		return 5;
	case Html_h6:
		return 6;
	default:
		return 0;
	}
}

static int findNumber( const QString& title )
{
	if( title.isEmpty() || !title[0].isDigit() )
		return -1;
	enum State { Digit, Dot };
	State s = Digit;
	int i = 0;
	while( i < title.size() )
	{
		switch( s )
		{
		case Digit:
			if( title[i].isDigit() )
				; // ignore
			else if( title[i] == QChar('.') )
				s = Dot;
			else if( title[i].isSpace() )
				return i + 1;
			else
				return i;
			break;
		case Dot:
			if( title[i].isDigit() )
				s = Digit;
			else if( title[i].isSpace() )
				return i + 1;
			else
				return -1; // Wenn nach einem Dot etwas anderes kommt als eine Zahl oder ein Space, breche die Übung ab.
			break;
		}
		i++;
	}
	return -1;
}

static void readHtmlNode( Context& ctx, const QTextHtmlParserNode& p )
{
	switch( p.id )
	{
	case Html_html:
	case Html_body:
	case Html_div:
	case Html_span:
	default:
		for( int i = 0; i < p.children.count(); i++ )
			readHtmlNode( ctx, ctx.parser.at( p.children[i] ) );
		break;
	case Html_p:
	case Html_address:
	case Html_a:
		if( !collectText( ctx.parser, p ).isEmpty() ) // RISK: teuer
		{
			Obj body = AppContext::inst()->getTxn()->createObject( TypeSection );
			_aggr( body, ctx );
			QByteArray bml = readParagraph( ctx, p );
			/* TEST
			DataReader r( bml );
			qDebug() << "************** " << p.tag;
			r.dump();
			*/
			body.setValue( AttrObjText, DataCell().setBml( bml ) );
		}
		break;
	// case Html_center: // gibt es nicht
	case Html_ul:
	case Html_ol:
	case Html_table:
	case Html_dl:
    case Html_pre: // TODO: pre sollte via BML laufen. Später auch alle übrigen
		{
			Obj body = AppContext::inst()->getTxn()->createObject( TypeSection );
			_aggr( body, ctx );
			body.setValue( AttrObjText, DataCell().setHtml( generateHtml( ctx, p ) ) );
		}
		break;

	case Html_h1:
	case Html_h2:
	case Html_h3:
	case Html_h4:
	case Html_h5:
	case Html_h6:
		{
			const QString str = simplify( collectText( ctx.parser, p ) ); // ignoriere Formatierung
			if( !str.isEmpty() )
			{
				const int l = h2i( p.id );
				while( l <= ctx.trace.top().second && ctx.trace.size() > 1 )
					ctx.trace.pop();
				Obj title = AppContext::inst()->getTxn()->createObject( TypeTitle );
				_aggr( title, ctx );
				const int split = findNumber( str );
				if( split != -1 )
				{
					title.setValue( AttrObjNumber, DataCell().setString( str.left( split ) ) );
					title.setValue( AttrObjText, DataCell().setString( str.mid( split ) ) );
				}else
					title.setValue( AttrObjText, DataCell().setString( str ) );
				ctx.trace.push( qMakePair(title,l) );
			}
		}
		break;
	// case Html_hr: // Horizontal rule ignorieren
	// case Html_caption: // Deprecated

	case Html_dt:
	case Html_dd:
	case Html_li:
	case Html_tr:
	case Html_td:
	case Html_th:
	case Html_thead:
	case Html_tbody:
	case Html_tfoot:
		// NOTE: sollen hier nicht vorkommen
		break;
	case Html_title:
		ctx.doc.setValue( AttrDocName, DataCell().setString( simplify( p.text ) ) );
		break;
	}
}

Sdb::Obj DocImporter::importHtmlFile( const QString& path )
{
	d_error.clear();

    QFileInfo info( path );
	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		d_error = tr("cannot open '%1' for reading").arg(path);
		return Obj();
	}
    QDir::setCurrent( info.absolutePath() ); // für relative images
    Sdb::Obj doc = importHtmlString( QString::fromLatin1( f.readAll() ) );
    if( !doc.isNull() )
    {
        doc.setValue( AttrDocStream, DataCell().setString( path ) );
        if( doc.getValue( AttrDocName ).getStr().isEmpty() )
			doc.setValue( AttrDocName, DataCell().setString( info.fileName() ) );
    }
    return doc;
}

Obj DocImporter::importHtmlString(const QString &html )
{
    Context ctx;
	// TODO <META HTTP-EQUIV="CONTENT-TYPE" CONTENT="text/html; charset=windows-1252">
	ctx.parser.parse( html, 0 );
	if( ctx.parser.count() == 0 )
	{
		d_error = "HTML stream has no contents!";
		return Obj();
	}
	//parser.dumpHtml(); // TEST
	// dump2( parser );
	Database::Lock lock( AppContext::inst()->getDb(), true );
	try
	{
		Sdb::Obj doc = AppContext::inst()->getTxn()->createObject( TypeDocument );
		doc.setValue( AttrDocImported, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		doc.setValue( AttrDocId, DataCell().setString( QUuid::createUuid().toString() ) );
		doc.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
		doc.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		doc.setValue( AttrCreatedThru, DataCell().setString( "HTML Import" ) );

		const QTextHtmlParserNode& n = ctx.parser.at(0);
		// dump( parser, n, 0 );
		ctx.trace.push( qMakePair(doc,0) );
		ctx.doc = doc;
		readHtmlNode( ctx, n );

		AppContext::inst()->getTxn()->commit();
		lock.commit();
		return doc;
	}catch( DatabaseException& e )
	{
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
    return Sdb::Obj();
}
