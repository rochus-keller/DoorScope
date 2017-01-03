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

#include "TextOutStream.h"
#include "TextCursor.h"
#include "ImageGlyph.h"
#include <bitset>
#include <QTextDocument>
#include <QTextStream>
#include <QTextFrame>
#include <QTextTable>
#include <QTextList>
#include <QPicture>
#include <QtDebug>
#include <cassert>
using namespace Txt;
using namespace Stream;

class _MyException : public std::exception
{
	QByteArray d_msg;
public:
	_MyException( const char* msg ):d_msg(msg) {}
	~_MyException() throw() {}
	const char* what() const throw() { return d_msg; }
};

bool TextOutStream::toHtml( Stream::DataReader& in, QTextStream& out, bool onlyFragment )
{
	if( !onlyFragment )
		out << "<html><body>";
	try
	{
		DataCell v;
		DataReader::Token t = in.nextToken();
		if( !DataReader::isUseful( t ) )
			return true; // offensichtlich ein leerer Stream
		if( t != DataReader::BeginFrame )
			throw _MyException( "expecting frame 'rtxt'" );
		if( !in.getName().getTag().equals( "rtxt" ) )
			throw _MyException( "invalid frame type" );
		t = in.nextToken();
		if( t != DataReader::Slot )
			throw _MyException( "expecting slot 'ver'" );
		if( !in.getName().getTag().equals( "ver" ) )
			throw _MyException( "invalid slot" );
		in.readValue( v );
		if( v.getType() != DataCell::TypeAscii || v.getArr() != "0.1" )
			throw _MyException( "invalid stream version" );
		t = in.nextToken();

		enum State { WaitBlock, ReadPar, ReadLst, ReadFrag, ReadImg, ReadAnch };
		State state = WaitBlock;
		State oldBlockState;
		std::bitset<8> format;
		bool inList = false;
		QSize size;
		QByteArray url;
		QString urlText;
		while( DataReader::isUseful( t ) )
		{
			switch( state )
			{
			case WaitBlock:
				if( t == DataReader::BeginFrame )
				{
					const NameTag name = in.getName().getTag();
					if( name.equals( "par" ) )
					{
						state = ReadPar;
						out << "<p>"; // <p style="margin-left:3em">
					}else if( name.equals( "lst" ) )
					{
						state = ReadLst;
						if( !inList )
						{
							out << "<ul>"; // TODO: Listentypen und rekursive Listen
							inList = true;
						}
						out << "<li>";
					}else
						throw _MyException( "expecting frame 'par' or 'lst'" );
				}else if( t == DataReader::EndFrame )
				{
					; // nop
				}else
					throw _MyException( "expecting frame" );
				break;
			case ReadPar:
				if( t == DataReader::BeginFrame )
				{
					const NameTag name = in.getName().getTag();
					oldBlockState = state;
					if( name.equals( "frag" ) )
					{
						state = ReadFrag;
						format.reset();
					}else if( name.equals( "img" ) )
					{
						state = ReadImg;
						size = QSize();
					}else if( name.equals( "anch" ) )
					{
						state = ReadAnch;
						url.clear();
						urlText.clear();
					}else
						throw _MyException( "expecting frame 'frag', 'img' or 'anch'" );
				}else if( t == DataReader::EndFrame )
				{
					if( inList )
						out << "</ul>";
					else
						out << "</p>";
					state = WaitBlock;
				}else
					throw _MyException( "invalid token" );
				break;
			case ReadLst:
				if( t == DataReader::Slot )
				{
					const NameTag name = in.getName().getTag();
					in.readValue( v );
					if( name.equals( "il" ) )
					{
						// v.getUInt8() indentation
					}else if( name.equals( "ls" ) )
					{
						switch( v.getUInt8() ) // TODO
						{
						case TextInStream::NoList:
							break;
						case TextInStream::Disc:
							break;
						case TextInStream::Circle:
							break;
						case TextInStream::Square:
							break;
						case TextInStream::Decimal:
							break;
						case TextInStream::LowAlpha:
							break;
						case TextInStream::UpAlpha:
							break;
						default:
							throw _MyException( "unknown list style" );
						}
					}else
						throw _MyException( "expecting slot 'il' or 'ls'" );
				}else if( t == DataReader::BeginFrame )
				{
					const NameTag name = in.getName().getTag();
					oldBlockState = state;
					if( name.equals( "frag" ) )
					{
						state = ReadFrag;
						format.reset();
					}else if( name.equals( "img" ) )
					{
						state = ReadImg;
					}else if( name.equals( "anch" ) )
					{
						state = ReadAnch;
						url.clear();
						urlText.clear();
					}else
						throw _MyException( "expecting frame 'frag', 'img' or 'anch'" );
				}else if( t == DataReader::EndFrame )
				{
					state = WaitBlock;
				}else
					throw _MyException( "invalid token" );
				break;
			case ReadFrag:
				if( t == DataReader::Slot && in.getName().isNull() )
				{
					in.readValue( v );
					if( v.getType() == DataCell::TypeUInt8 )
						format = std::bitset<8>( v.getUInt8() );
					else if( v.getType() == DataCell::TypeString )
					{
						if( format.test( TextInStream::Italic ) )
							out << "<em>";
						if( format.test( TextInStream::Bold ) )
							out << "<strong>";
						if( format.test( TextInStream::Underline ) )
							out << "<ins>";
						if( format.test( TextInStream::Strikeout ) )
							out << "<del>";
						if( format.test( TextInStream::Super ) )
							out << "<sup>";
						if( format.test( TextInStream::Sub ) )
							out << "<sub>";
						if( format.test( TextInStream::Fixed ) )
							out << "<code>";
						out << v.getStr();
						if( format.test( TextInStream::Fixed ) )
							out << "</code>";
						if( format.test( TextInStream::Sub ) )
							out << "</sub>";
						if( format.test( TextInStream::Super ) )
							out << "</sup>";
						if( format.test( TextInStream::Strikeout ) )
							out << "</del>";
						if( format.test( TextInStream::Underline ) )
							out << "</ins>";
						if( format.test( TextInStream::Bold ) )
							out << "</strong>";
						if( format.test( TextInStream::Italic ) )
							out << "</em>";
					}else
						throw _MyException( "expecting format or text slot" );
				}else if( t == DataReader::EndFrame )
				{
					state = oldBlockState;
				}else
					throw _MyException( "expecting 'frag' slots or end" );
				break;
			case ReadImg:
				if( t == DataReader::Slot )
				{
					in.readValue( v );
					const NameTag name = in.getName().getTag();
					if( name.isNull() && v.getType() == DataCell::TypeImg )
					{
						out << "<img ";
						if( size.isValid() )
							out << "width=" << size.width() << " height=" << size.height() << " ";
						out << "alt=\"<embedded image>\" src=\"data:image/png;base64," << 
							v.getArr().toBase64() << "\">";
					}else if( name.equals( "w" ) )
						size.setWidth( v.getUInt16() );
					else if( name.equals( "h" ) )
						size.setHeight( v.getUInt16() );
					else
						throw _MyException( "invalid slot" );
				}else if( t == DataReader::EndFrame )
				{
					state = oldBlockState;
				}else
					throw _MyException( "invalid token" );
				break;
			case ReadAnch:
				if( t == DataReader::Slot )
				{
					const NameTag name = in.getName().getTag();
					in.readValue( v );
					if( name.equals( "url" ) )
					{
						url = v.getArr();
					}else if( name.equals( "text" ) )
					{
						urlText = v.toString();
					}else
						throw _MyException( "invalid slot" );
				}else if( t == DataReader::EndFrame )
				{
					if( urlText.isEmpty() )
						out << "<a href=\"" << url << "\">";
					else
						out << "<a href=\"" << url << ">" << urlText << "</a>";
					state = oldBlockState;
				}else
					throw _MyException( "invalid token" );
				break;
			}
			t = in.nextToken();
		}
	}catch( _MyException& e )
	{
		out << "ERROR: " << e.what();
	}
	if( !onlyFragment )
		out << "</body></html>";
	return true;
}

void TextOutStream::writeTo( Stream::DataWriter& w, const QTextDocument* doc )
{
	if( doc )
		emitDocument( doc, w );
}

void TextOutStream::emitDocument(const QTextDocument* doc, Stream::DataWriter& out)
{
	QTextFrame* root = doc->rootFrame();

	out.startFrame( Stream::NameTag( "rtxt" ) );
	out.writeSlot( Stream::DataCell().setAscii( "0.1" ), Stream::NameTag( "ver" ) );

    for( QTextFrame::Iterator it = root->begin(); it != root->end(); ++it ) 
	{
		// Gehe durch alle Frames des Root. Der Frame-Iter liefert entweder
		// Frames oder Blocks, die unmittelbar im Root sind.
		// Da wir keine verschachtelten Frames unterstützen, sollten wir hier
		// alles sehen, was es gibt.

        if( QTextFrame *f = it.currentFrame() ) 
		{
			QTextFrameFormat fmt = f->frameFormat();
			switch( fmt.intProperty( Styles::PropFrameType ) )
			{
			case Styles::PLAIN:
				// Sollte nicht vorkommen.
				break;
			case Styles::TBL:
				emitTable( qobject_cast<QTextTable*>(f), out );
				break;
			case Styles::LST:
				emitList( f, out );
				break;
			case Styles::CODE:
				emitCode( f, out );
				break;
			default:
				assert( false );
				break;
			}
        }else if( it.currentBlock().isValid() ) 
		{
			const int type = it.currentBlock().blockFormat().
				intProperty( Styles::PropBlockType );
			switch( type )
			{
			case Styles::H1:
				out.startFrame( Stream::NameTag( "h1" ) );
				break;
			case Styles::H2:
				out.startFrame( Stream::NameTag( "h2" ) );
				break;
			case Styles::H3:
				out.startFrame( Stream::NameTag( "h3" ) );
				break;
			case Styles::H4:
				out.startFrame( Stream::NameTag( "h4" ) );
				break;
			case Styles::H5:
				out.startFrame( Stream::NameTag( "h5" ) );
				break;
			case Styles::H6:
				out.startFrame( Stream::NameTag( "h6" ) );
				break;
			case Styles::PAR:
			default:
				out.startFrame( Stream::NameTag( "par" ) );
				break;
			}
			emitBlock( it.currentBlock(), out );
			out.endFrame();
        }
	}
	out.endFrame();
}

void TextOutStream::emitList( const QTextFrame* f, Stream::DataWriter& out )
{
    for( QTextFrame::Iterator it = f->begin(); it != f->end(); ++it ) 
	{
		QTextBlock block = it.currentBlock();
		assert( block.isValid() );
		QTextList *list = block.textList();
		assert( list );
		out.startFrame( NameTag( "lst" ) );
		const QTextListFormat format = list->format();
		TextInStream::ListStyle s = TextInStream::NoList;
		switch( format.style() ) 
		{
			case QTextListFormat::ListDecimal: 
				s = TextInStream::Decimal;
				break;
			case QTextListFormat::ListDisc: 
				s = TextInStream::Disc;
				break;
			case QTextListFormat::ListCircle: 
				s = TextInStream::Circle;
				break;
			case QTextListFormat::ListSquare: 
				s = TextInStream::Square;
				break;
			case QTextListFormat::ListLowerAlpha: 
				s = TextInStream::LowAlpha;
				break;
			case QTextListFormat::ListUpperAlpha: 
				s = TextInStream::UpAlpha;
				break;
			default: 
				assert( false );
		}
		out.writeSlot( DataCell().setUInt8( s ), NameTag( "ls" ) );
		out.writeSlot( DataCell().setUInt8( block.blockFormat().indent() ), NameTag( "il" ) );
		emitBlock( block, out );
		out.endFrame();
	}
}

void TextOutStream::emitBlock(const QTextBlock &block, Stream::DataWriter& out )
{
	QTextBlock::Iterator it = block.begin();

	for( ; !it.atEnd(); ++it )
	{
		const QTextFragment f = it.fragment();
		const QTextCharFormat tf = f.charFormat();
		if( tf.isAnchor() )
		{
			// Vereinfachung: wir gehen hier davon aus, dass Anchors nur
			// aus Text bestehen und über die gesamte Textlänge dasselbe Format
			// aufweisen. Wenn das nicht zutreffen würde, hätte man einfach denselben
			// Link mehrfach hintereinander als separates Objekt (Schönheitsfehler).
			if( !tf.anchorHref().isEmpty() )
			{
				out.startFrame( NameTag( "anch" ) );
				// qDebug() << "href: " << tf.anchorHref() << " Text: " << f.text();
				out.writeSlot( DataCell().setUrl( tf.anchorHref().toAscii() ), NameTag( "url" ) );
				if( f.text() != tf.anchorHref() )
					out.writeSlot( DataCell().setString( f.text() ), NameTag( "text" ) );
				out.endFrame();
			}
		}else if( f.length() == 1 && f.text().at(0) == QChar::ObjectReplacementCharacter )
		{
			out.startFrame( NameTag( "img" ) );
			if( tf.objectType() == ImageGlyph::Type )
			{
				if( tf.hasProperty( Styles::PropSize ) )
				{
					const QSize s = tf.property( Styles::PropSize ).value<QSize>();
					out.writeSlot( DataCell().setUInt16( s.width() ), NameTag( "w" ) );
					out.writeSlot( DataCell().setUInt16( s.height() ), NameTag( "h" ) );
	
				}
				QVariant var = tf.property( Styles::PropImage );
				DataCell v;
				if( var.canConvert<QImage>() )
					v.setImage( var.value<QImage>() );
				else if( var.canConvert<QPixmap>() )
					v.setImage( var.value<QPixmap>().toImage() );
				else if( var.canConvert<QPicture>() )
					v.setPicture( var.value<QPicture>() );
				out.writeSlot( v );
			}
			out.endFrame();
		}else
		{
			out.startFrame( NameTag( "frag" ) );
			emitTextFormat( tf, out );
			out.writeSlot( DataCell().setString( f.text() ) );
			out.endFrame();
		}
	}
}

void TextOutStream::emitTextFormat(const QTextCharFormat &tf, Stream::DataWriter& out)
{
	std::bitset<8> format;

	if( Styles::isEm( tf ) )
		format.set( TextInStream::Italic );
	if( Styles::isStrong( tf ) )
		format.set( TextInStream::Bold );
	if( Styles::isFixed( tf ) )
		format.set( TextInStream::Fixed );
	if( Styles::isUnder( tf ) )
		format.set( TextInStream::Underline );
	if( Styles::isStrike( tf ) )
		format.set( TextInStream::Strikeout );
	if( Styles::isSuper( tf ) )
		format.set( TextInStream::Super );
	if( Styles::isSub( tf ) )
		format.set( TextInStream::Sub );
	if( format.any() )
		out.writeSlot( DataCell().setUInt8( format.to_ulong() ) );
}

void TextOutStream::emitTable(const QTextTable *table, Stream::DataWriter& out )
{
	// TODO
}

void TextOutStream::emitCode(const QTextFrame * frame, Stream::DataWriter& out )
{
	out.startFrame( NameTag( "code" ) );
	QString lines;
    for( QTextFrame::Iterator it = frame->begin(); it != frame->end(); ++it ) 
	{
		QTextBlock block = it.currentBlock();
		assert( block.isValid() );
		QTextBlockFormat f = block.blockFormat();
		//v.setByte( (Root::UInt8)f.indent() ); // TODO: ist indent im block.text schon berücksichtigt?
		//out.writeSlot( v );
		lines += block.text() + QLatin1String( "\r\n" );
	}
	out.writeSlot( DataCell().setString( lines ) );
	out.endFrame();
}

bool TextOutStream::hasStyle( const QTextDocument* doc )
{
	QTextBlock b = doc->begin();
	while( b.isValid() )
	{
		// TODO: das ist ziemlich ineffizient
		QTextFormat f = b.blockFormat();
		for( int i = Styles::PropBlockType; i <= Styles::PropFrameType; i++ )
			if( f.hasProperty( i ) )
				return true;
		QTextBlock::iterator it;
		for (it = b.begin(); !(it.atEnd()); ++it) 
		{
			QTextFragment currentFragment = it.fragment();
			if (currentFragment.isValid())
			{
				QTextCharFormat f = currentFragment.charFormat();
				if( f.isAnchor() )
					return true;
				for( int i = Styles::PropLink; i <= Styles::PropImage; i++ )
					if( f.hasProperty( i ) )
						return true;
				if( f.fontItalic() || f.fontWeight() == QFont::Bold || f.fontFixedPitch() ||
					f.verticalAlignment() == QTextCharFormat::AlignSuperScript ||
					f.verticalAlignment() == QTextCharFormat::AlignSubScript ||
					f.fontUnderline() || f.fontStrikeOut() )
					return true;
			}

		}
		b = b.next();
	}
	return false;
}
