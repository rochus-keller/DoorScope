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

#include "Styles.h"
#include <QTextCursor>
#include <QTextBlock>
#include <QTextList>
#include <QPointer>
using namespace Txt;

static QPointer<Styles> s_styles;

Styles::Styles( QObject* p, float pointSize, float titleFactor, 
			   float margin, bool parFirstIndent ):QObject(p)
{
	setup( pointSize, titleFactor, margin, parFirstIndent );
}

const Styles* Styles::inst()
{
	if( s_styles == 0 )
		s_styles = new Styles(0);
	return s_styles;
}

void Styles::setInst( Styles* s )
{
	if( s_styles != 0 && s_styles->parent() == 0 )
		delete s_styles;
	s_styles = s;
}

const float Styles::s_titleFactor[] = 
{
	0.0,		//INVALID_PARAGRAPH_STYLE,
	0.4,		//H1, 
	0.2,		//H2, 
	0.1,		//H3, 
	0.1,		//H4, 
	0.0,		//H5, 
	0.0,		//H6, // Titles
	0.0,		//PAR,	// Paragraph
	0.0,		//_CODE,	// Non-Breakable Line, nicht in Menü angeboten
};

void Styles::setup( float pointSize, float titleFactor, float margin, bool parFirstIndent )
{
	QTextCharFormat stdChar;
	stdChar.setFontFamily( "Arial" );
	stdChar.setFontPointSize( pointSize );
	stdChar.setFontWeight( QFont::Normal );
	stdChar.setFontUnderline( false );
	stdChar.setFontItalic( false );

	QTextBlockFormat stdBlock;
	stdBlock.setLeftMargin( 0 );
	stdBlock.setNonBreakableLines( false );
	stdBlock.setProperty( PropBlockType, PAR );
	stdBlock.setAlignment( Qt::AlignLeft );
	stdBlock.setTopMargin( margin );
	stdBlock.setBottomMargin( margin );
	stdBlock.setTextIndent( 0 );

	d_em.setFontItalic( true );

	d_strong.setFontWeight( QFont::Bold );

	d_under.setFontUnderline( true );

	d_strike.setFontStrikeOut( true );

	//d_fixed.setProperty( PropFixed, true );
	d_fixed.setFontFamily( "courier" ); // sonst passiert nicht bei Änderung, erst bei Reload
	d_fixed.setFontFixedPitch( true );

	d_super.setVerticalAlignment( QTextCharFormat::AlignSuperScript );

	d_sub.setVerticalAlignment( QTextCharFormat::AlignSubScript );

	d_url.setFontUnderline( true );
	d_url.setForeground( Qt::blue );

	d_link.setBackground( QColor( 204, 255, 255 ) );

	d_para[H1].d_char = stdChar;
	d_para[H1].d_block = stdBlock;
	d_para[H1].d_block.setProperty( PropBlockType, H1 );
	d_para[H1].d_char.setFontWeight( QFont::Bold );
	d_para[H1].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H1] * titleFactor ) );
	d_para[H1].d_next = PAR;

	d_para[H2].d_char = stdChar;
	d_para[H2].d_block = stdBlock;
	d_para[H2].d_block.setProperty( PropBlockType, H2 );
	d_para[H2].d_char.setFontWeight( QFont::Bold );
	d_para[H2].d_char.setFontItalic( true );
	d_para[H2].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H2] * titleFactor ) );
	d_para[H2].d_next = PAR;

	d_para[H3].d_char = stdChar;
	d_para[H3].d_block = stdBlock;
	d_para[H3].d_block.setProperty( PropBlockType, H3 );
	d_para[H3].d_char.setFontUnderline( true );
	d_para[H3].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H3] * titleFactor ) );
	d_para[H3].d_next = PAR;

	d_para[H4].d_char = stdChar;
	d_para[H4].d_block = stdBlock;
	d_para[H4].d_block.setProperty( PropBlockType, H4 );
	d_para[H4].d_char.setFontWeight( QFont::Bold );
	d_para[H4].d_char.setFontItalic( true );
	d_para[H4].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H4] * titleFactor ) );
	d_para[H4].d_next = PAR;

	d_para[H5].d_char = stdChar;
	d_para[H5].d_block = stdBlock;
	d_para[H5].d_block.setProperty( PropBlockType, H5 );
	d_para[H5].d_char.setFontUnderline( true );
	d_para[H5].d_char.setFontItalic( true );
	d_para[H5].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H5] * titleFactor ) );
	d_para[H5].d_next = PAR;

	d_para[H6].d_char = stdChar;
	d_para[H6].d_block = stdBlock;
	d_para[H6].d_block.setProperty( PropBlockType, H6 );
	d_para[H6].d_char.setFontUnderline( true );
	d_para[H6].d_char.setFontWeight( QFont::Bold );
	d_para[H6].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[H6] * titleFactor ) );
	d_para[H6].d_next = PAR;

	d_para[PAR].d_char = stdChar;
	d_para[PAR].d_block.setProperty( PropBlockType, PAR );
	d_para[PAR].d_block = stdBlock;
	if( parFirstIndent )
		d_para[PAR].d_block.setTextIndent( pointSize );
	d_para[PAR].d_next = PAR;

	d_para[_CODE].d_char = stdChar;
	d_para[_CODE].d_block = stdBlock;
	d_para[_CODE].d_block.setProperty( PropBlockType, _CODE );
	d_para[_CODE].d_block.setNonBreakableLines( true );
	d_para[_CODE].d_block.setTopMargin( 0 );
	d_para[_CODE].d_block.setBottomMargin( 0 );
	d_para[_CODE].d_char.setFontFamily( "courier" );
	d_para[_CODE].d_next = _CODE;


	d_listFrame.setPosition( QTextFrameFormat::InFlow ); 
	d_listFrame.setBorder( 0 ); // TEST
	d_listFrame.setMargin( pointSize );
	d_listFrame.setProperty( PropFrameType, LST );

	d_codeFrame.setPosition( QTextFrameFormat::InFlow ); 
	d_codeFrame.setBorder( 0 ); // TEST
	d_codeFrame.setMargin( pointSize );
	d_codeFrame.setProperty( PropFrameType, CODE );
	d_codeFrame.setBackground( QColor( 210, 210, 210 ) );

	// TODO emit
}

void Styles::setFontStyle( const QString& family, float pointSize, float titleFactor )
{
	for( int i = H1; i < MAX_PARAGRAPH_STYLE; i++ )
	{
		d_para[i].d_char.setFontFamily( family );
		d_para[i].d_char.setFontPointSize( pointSize * ( 1.0 + s_titleFactor[i] * titleFactor ) );
	}
	emit sigFontStyleChanged();
}

const QTextBlockFormat& Styles::getBlockFormat(ParagraphStyle s) const
{
	return d_para[s].d_block;
}

const QTextCharFormat& Styles::getCharFormat(ParagraphStyle s) const
{
	return d_para[s].d_char;
}

bool Styles::isEm( const QTextCharFormat& f ) 
{
	return f.fontItalic();
}

bool Styles::isStrong( const QTextCharFormat& f ) 
{
	return f.fontWeight() == QFont::Bold;
}

bool Styles::isFixed( const QTextCharFormat& f ) 
{
	return f.fontFixedPitch();
}

bool Styles::isSuper( const QTextCharFormat& f ) 
{
	return f.verticalAlignment() == QTextCharFormat::AlignSuperScript;
}

bool Styles::isSub( const QTextCharFormat& f ) 
{
	return f.verticalAlignment() == QTextCharFormat::AlignSubScript;
}

bool Styles::isUnder( const QTextCharFormat& f ) 
{
	return f.fontUnderline();
}

bool Styles::isStrike( const QTextCharFormat& f ) 
{
	return f.fontStrikeOut();
}

bool Styles::getLink( const QTextCharFormat& f, QByteArray& ba )
{
	if( f.isAnchor() )
	{
		if( f.hasProperty( PropLink ) )
		{
			ba = f.property( PropLink ).toByteArray();
			return true;
		}else
			return false;
	}else
		return false;
}

void Styles::setLink( QTextCharFormat& f, const QByteArray& path )
{
	f.setAnchor( true );
	f.setProperty( PropLink, path );
}

bool Styles::isSameLink( const QTextCharFormat& l, const QTextCharFormat& r )
{
	const bool lLink = l.hasProperty( PropLink );
	const bool rLink = r.hasProperty( PropLink );
	if( l.isAnchor() && r.isAnchor() )
	{
		if( lLink && rLink )
			return l.property( PropLink ) == r.property( PropLink );
		else if( !lLink && !rLink )
			return l.anchorHref() == r.anchorHref();
	}
	return false;
}

const QTextCharFormat& Styles::getDefaultFormat( const QTextCursor& cur ) const
{
	QTextBlockFormat f = cur.blockFormat();
	int s = f.intProperty( PropBlockType );
	if( s > INVALID_PARAGRAPH_STYLE && s < MAX_PARAGRAPH_STYLE )
		return getCharFormat( (ParagraphStyle)s );
	else
		return getCharFormat( PAR );
}

QFont Styles::getFont( quint8 level ) const
{
	QTextCharFormat f;
	if( level == 0 )
		f = getCharFormat( PAR );
	else
	{
		switch( level )
		{
		case 1:
			f = getCharFormat( H1 );
			break;
		case 2:
			f = getCharFormat( H2 );
			break;
		case 3:
			f = getCharFormat( H3 );
			break;
		case 4:
			f = getCharFormat( H4 );
			break;
		case 5:
			f = getCharFormat( H5 );
			break;
		default:
			f = getCharFormat( H6 );
			break;
		}
	}
	return f.font();
}
