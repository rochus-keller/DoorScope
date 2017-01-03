#ifndef __TEXT_MYTEXTCURSOR
#define __TEXT_MYTEXTCURSOR

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

#include <Txt/Styles.h>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QUrl>

class QImage;

namespace Txt
{
	class TextCursor
	{
	public:
		QTextDocumentFragment getSelection() const;
		QString getSelectedText() const;
		bool hasSelection() const { return d_cur.hasSelection(); }
		bool hasComplexSelection() const { return d_cur.hasComplexSelection(); }
		int getPosition() const { return d_cur.position(); }
		int getSelStartPos() const { return d_cur.selectionStart(); }
		int getSelEndPos() const { return d_cur.selectionEnd(); }
		void gotoStart();
		void gotoEnd();

		bool isUnder() const;
		void setUnder( bool on = true );
		bool isStrike() const;
		void setStrike( bool on = true );
		bool isSub() const;
		void setSub( bool on = true );
		bool isSuper() const;
		void setSuper( bool on = true );
		bool isEm() const;
		void setEm( bool on = true );
		bool isStrong() const;
		void setStrong( bool on = true );
		bool isFixed() const;
		void setFixed( bool on = true );
		bool canSetCharFormat() const;
		void setDefaultCharFormat( Styles::ParagraphStyle );	 
		QTextCharFormat getCharFormat () const;

		void insertText( const QString& );
		void insertImg( const QImage&, int w = 0, int h = 0 );
		void insertFragment(const QTextDocumentFragment& ); // RISK

		void insertUrl( const QString& uri, bool encoded = false, const QString& text = "" );
		void insertUrl( const QUrl&, const QString& text = "" );
		bool isUrl() const;
		QUrl getUrl() const;
		QByteArray getUrlEncoded() const;
		QString getUrlText() const;

		void deleteCharOrSelection( bool left = false );
		void removeSelectedText();

		void addParagraph( bool softBreak = false );
		bool setParagraphStyle( Styles::ParagraphStyle );	 
		bool canSetParagraphStyle( int ) const;
		Styles::ParagraphStyle getParagraphStyle() const;

		bool canAddList() const;
		bool addList( QTextListFormat::Style s );
		bool inList() const { return d_cur.currentList() != 0; }
		bool addItemLeft( bool jumpout = false );
		int getListStyle() const;
		void setListStyle( QTextListFormat::Style ); 
		bool indentList();
		bool indentList(quint8 level);
		bool unindentList(bool all = false);

		bool canAddTable() const;
		void addTable( quint16 rows, quint16 cols, bool fullWidth = false );
		bool inTable() const { return d_cur.currentTable() != 0; }
		void addTableRow();
		void gotoRowCol( quint16 row, quint16 col );
		QPair<quint16,quint16> getTableSize() const;
		void resizeTable( quint16 rows, quint16 cols, bool fullWidth = false );
		void mergeTableCells();

		void addCodeBlock();
		bool canAddCodeBlock() const;
		bool inCodeBlock() const;
		bool atBeginOfCodeLine() const;
		bool indentCodeLines();
		bool unindentCodeLines();

		void beginEditBlock() { d_cur.beginEditBlock(); }
		void endEditBlock() { d_cur.endEditBlock(); }

		bool isNull() const { return d_cur.isNull(); }
		const Styles* getStyles() const { return d_styles; }

		TextCursor( QTextCursor& cur, const Styles* s = 0 );
		TextCursor( QTextDocument*, const Styles* s = 0 );
		TextCursor( const TextCursor& );
		TextCursor& operator=( const TextCursor& );

		//* Utility
		static bool calcAnchorBounds( const QTextCursor& cur, int& left, int& right );
		static void dump(const QTextDocument*);
		void dump() const;
	protected:
		TextCursor() {}

		static bool applyStyle( QTextCursor&, const Styles*, Styles::ParagraphStyle );
		static bool canApplyStyle( const QTextCursor&, Styles::ParagraphStyle );
		static bool canAddObject( const QTextCursor& );

		static bool addList( QTextCursor&, const Styles*, QTextListFormat::Style s, bool create = true );
		static bool canAddListItem( const QTextCursor& );

		static bool addCodeBlock( QTextCursor&, const Styles* );

		void adjustSelection();
		QTextCharFormat getDefault() const;

        static QString createUrlText( const QUrl &uri );

		static bool isSelection( const QTextCursor&, Styles::ParagraphStyle );
		static bool isSelection( const QTextCursor&, Styles::FrameType );
		static void removeCharFormat( QTextCursor&, const QTextCharFormat& );

		QTextCursor d_cur;
		const Styles* d_styles;
	};
}

#endif //__TEXT_MYTEXTCURSOR
