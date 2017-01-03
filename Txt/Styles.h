#ifndef _TEXT_STYLES
#define _TEXT_STYLES

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

#include <QObject>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextFrameFormat>

class QTextCursor;

namespace Txt
{
	class Styles : public QObject
	{
		Q_OBJECT
	public:
		Styles( QObject*, float pointSize = 10.0, float titleFactor = 1.0, 
			float margin = 5.0, bool parFirstIndent = true );
		void setup( float pointSize = 10.0, float titleFactor = 1.0, 
			float margin = 5.0, bool parFirstIndent = true );
		void setFontStyle( const QString& family, float pointSize, float titleFactor = 1.0 );

		// Folgende Konstanten werden als IDs für QTextFormat::setProperty() verwendet.
		// Jeder QTextBlock bzw. sein QTextBlockFormat hat genau ein PropParagraph, das
		// die Funktion des Blocks festlegt. Tables und Listen umfassen einen oder
		// mehrere QTextBlocks, die jeweils dasselbe PropParagraph haben.
		// Listen können verschachtelt werden. 
		// Tabellen können nicht verschachtelt werden und keine Listen enthalten.

		// Ein Paragraph ist ein Zeichenstrom, der Links, Inline-Objects und Inline-
		// Images enthalten kann. Was ist mit Formeln?
		// Ein Link umfasst mehrere Zeichen, die als ganzes geschützt sind. 
		// Inline-Objects/Images werden als Zeichen aufgefasst (by Value).


		enum Property { 
			// Property für QTextBlockFormat:
			PropBlockType = QTextFormat::UserProperty + 1, // Wert aus ParagraphStyle
			PropFrameType, // Werte aus FrameType

			// Properties für QTextCharFormat:
			PropLink, // Wert: BML, gespeichert als QByteArray

			PropListOwner, // Wert objectIndex() der umgebenden Liste

			PropSize,	// Für Image, optional
			PropImage	// Für Image, wie ein CharProperty, d.h. kein ParagraphStyle
		};
	
		enum ParagraphStyle {
			INVALID_PARAGRAPH_STYLE,
			H1, H2, H3, H4, H5, H6, // Titles
			PAR,	// Paragraph
			// Hilfsformate
			_CODE,	// Non-Breakable Line, nicht in Menü angeboten
			MAX_PARAGRAPH_STYLE
		};
		static const float s_titleFactor[];

		enum FrameType {
			PLAIN,
			LST,
			TBL,
			CODE,
			MAX_FRAME_TYPE
		};

		static bool getLink( const QTextCharFormat&, QByteArray& );
		static void setLink( QTextCharFormat&, const QByteArray& );
		static bool isSameLink( const QTextCharFormat&, const QTextCharFormat& );

		const QTextCharFormat& getEm() const { return d_em; }
		static bool isEm( const QTextCharFormat& );
		const QTextCharFormat& getStrong() const { return d_strong; }
		static bool isStrong( const QTextCharFormat& );
		const QTextCharFormat& getFixed() const { return d_fixed; }
		static bool isFixed( const QTextCharFormat& );
		const QTextCharFormat& getSuper() const { return d_super; }
		static bool isSuper( const QTextCharFormat& );
		const QTextCharFormat& getSub() const { return d_sub; }
		static bool isSub( const QTextCharFormat& );
		const QTextCharFormat& getUnder() const { return d_under; }
		static bool isUnder( const QTextCharFormat& );
		const QTextCharFormat& getStrike() const { return d_strike; }
		static bool isStrike( const QTextCharFormat& );

		const QTextBlockFormat& getBlockFormat(ParagraphStyle) const;
		const QTextCharFormat& getCharFormat(ParagraphStyle) const;
		const QTextCharFormat& getDefaultFormat( const QTextCursor& ) const;
		QFont getFont( quint8 level = 0 ) const; // level=0...Body, level=1..6 Header

		const QTextFrameFormat& getCodeFrameFormat() const { return d_codeFrame; }
		const QTextFrameFormat& getListFrameFormat() const { return d_listFrame; }
		const QTextCharFormat& getUrl() const { return d_url; }
		const QTextCharFormat& getLink() const { return d_link; }
		
		ParagraphStyle getNextStyle( ParagraphStyle s ) const { return d_para[s].d_next; }

		static const Styles* inst();
		static void setInst( Styles* );
	signals:
		void sigFontStyleChanged();
	private:
		struct BlockCharFormat
		{
			QTextBlockFormat d_block;
			/// Default-Format, wenn kein TextStyle vorhanden
			QTextCharFormat d_char;

			/// Legt fest, mit welchem Style ein neuer, nachfolgender Paragraph 
			/// formatiert wird
			ParagraphStyle d_next;

			BlockCharFormat():d_next(INVALID_PARAGRAPH_STYLE){}
		};

		BlockCharFormat d_para[ MAX_PARAGRAPH_STYLE ];
		QTextCharFormat d_em;
		QTextCharFormat d_strong;
		QTextCharFormat d_fixed;
		QTextCharFormat d_super;
		QTextCharFormat d_sub;
		QTextCharFormat d_under;
		QTextCharFormat d_strike;
		QTextCharFormat d_url;
		QTextCharFormat d_link;
		QTextFrameFormat d_listFrame;
		QTextFrameFormat d_codeFrame;
	}; 

}

#endif // _TEXT_STYLES
