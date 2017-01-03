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

#include "TextInStream.h"
#include "TextCursor.h"
#include <bitset>
#include <QTextDocument>
#include <QTextStream>
#include <QtDebug>
using namespace Txt;
using namespace Stream;

const char* TextInStream::s_mimeRichText = "application/richtext/bml";

class _MyException : public std::exception
{
	QByteArray d_msg;
public:
	_MyException( const char* msg ):d_msg(msg) {}
	~_MyException() throw() {}
	const char* what() const throw() { return d_msg; }
};

TextInStream::TextInStream( const Styles* s )
{
	if( s == 0 )
		d_styles = Styles::inst();
	else
		d_styles = s;
}

QTextDocument* TextInStream::readFrom( DataReader& in, QObject* owner )
{
	QTextDocument* doc = new QTextDocument( owner );
	doc->setDefaultFont( d_styles->getFont() );
	TextCursor cur( doc, d_styles );
	if( !readFromTo( in, cur ) )
	{
		delete doc;
		return 0;
	}
	return doc;
}

bool TextInStream::readFromTo( DataReader& in, TextCursor& cur, bool insert )
{
	d_error.clear();
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

		enum State { WaitBlock, ReadPar, ReadLst, ReadFrag, ReadImg, ReadAnch, ReadCode };
		State state = WaitBlock;
		State oldBlockState;
		std::bitset<8> format;
		QSize size;
		bool first = insert;
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
						if( cur.inList() )
							cur.addItemLeft(true);
						else if( !first )
							cur.addParagraph();
					}else if( name.equals( "code" ) )
					{
						state = ReadCode;
						if( cur.inList() )
							cur.addItemLeft(true);
						else
							cur.addCodeBlock();
					}else if( name.equals( "lst" ) )
					{
						state = ReadLst;
						if( cur.inList() )
							cur.addParagraph();
						else
							cur.addList( QTextListFormat::ListDisc );
					}else
						throw _MyException( "expecting frame 'par' or 'lst'" );
					first = false;
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
					state = WaitBlock;
				}else
					throw _MyException( "invalid token" );
				break;
			case ReadCode:
				if( t == DataReader::Slot )
				{
					oldBlockState = state;
				}else if( t == DataReader::EndFrame )
				{
					cur.insertText( v.getStr() );
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
						if( !cur.indentList( v.getUInt8() ) )
							throw _MyException( "invalid indentation level" );
					}else if( name.equals( "ls" ) )
					{
						switch( v.getUInt8() )
						{
						case NoList:
							break;
						case Disc:
							cur.setListStyle( QTextListFormat::ListDisc );
							break;
						case Circle:
							cur.setListStyle( QTextListFormat::ListCircle );
							break;
						case Square:
							cur.setListStyle( QTextListFormat::ListSquare );
							break;
						case Decimal:
							cur.setListStyle( QTextListFormat::ListDecimal );
							break;
						case LowAlpha:
							cur.setListStyle( QTextListFormat::ListLowerAlpha );
							break;
						case UpAlpha:
							cur.setListStyle( QTextListFormat::ListUpperAlpha );
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
						cur.setEm( false );
						cur.setUnder( false );
						cur.setStrike( false );
						cur.setSub( false );
						cur.setSuper( false );
						cur.setEm( false );
						cur.setStrong( false );
						cur.setFixed( false );
						if( format.test( Italic ) )
							cur.setEm( true );
						if( format.test( Bold ) )
							cur.setStrong( true );
						if( format.test( Underline ) )
							cur.setUnder( true );
						if( format.test( Strikeout ) )
							cur.setStrike( true );
						if( format.test( Super ) )
							cur.setSuper( true );
						if( format.test( Sub ) )
							cur.setSub( true );
						if( format.test( Fixed ) )
							cur.setFixed( true );
						cur.insertText( v.getStr() );
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
					const NameTag name = in.getName().getTag();
					in.readValue( v );
					if( name.isNull() && v.getType() == DataCell::TypeImg )
					{
						QImage img;
						v.getImage(img);
						if( size.isValid() )
							cur.insertImg( img, size.width(), size.height() );
						else
							cur.insertImg( img );
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
					//qDebug() << url << urlText; // TEST
					cur.insertUrl( url, true, urlText );
					state = oldBlockState;
				}else
					throw _MyException( "invalid token" );
				break;
			}
			t = in.nextToken();
		}
	}catch( _MyException& e )
	{
		d_error = e.what();
		return false;
	}

	// Der erste Block ist überschüssig
	if( !insert )
	{
		cur.gotoStart();
		cur.deleteCharOrSelection();
	}
    return true;
}

bool TextInStream::readFromTo(const DataCell &bml, QTextDocument *doc, bool insert)
{
    if( !bml.isBml() )
        return false;
	doc->setDefaultFont( d_styles->getFont() );
	TextCursor cur( doc, d_styles );
    Stream::DataReader in( bml );
	return readFromTo( in, cur, insert );
}

