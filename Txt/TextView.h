#ifndef _TextView
#define _TextView

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

#include <Txt/TextCursor.h>
#include <QTextFormat>
#include <QBasicTimer>
#include <QTextLine>

class QMimeData;

namespace Txt
{
	class TextView : public QObject
	{
		Q_OBJECT
	public:
		enum CursorAction {
			DontMove,
			MoveBackward,
			MoveForward,
			MoveWordBackward,
			MoveWordForward,
			MoveUp,
			MoveDown,
			MoveLineStart,
			MoveLineEnd,
			MoveHome,
			MoveEnd,
			MovePageUp,
			MovePageDown,
			MoveNextCell,
			MovePrevCell
		};

		TextView( QObject* owner, const Styles* s = 0, bool singleBlock = false, bool plainText = false );
		
		TextCursor& getCursor() { return cursor; }

		void setWidth( qreal width, qreal visibleHeight = 0.0 );
		const QSizeF& getExtent() const { return d_extent; }

		void ensureViewportLayouted( qreal visibleHeight = 0.0 ); // Top bis len im Dokument
		bool isReadOnly() const { return readOnly; }
		void setReadOnly( bool on ) { readOnly = on; }

		void clear();

		QRectF cursorRect(const QTextCursor &cursor) const;
		QRectF cursorRect() const;
		bool moveCursor( CursorAction, bool select, qreal pageHeight = 0 );
		bool find( const QString& pattern, bool forward = true, bool nocase = true, bool select = true, bool extend = false );
		void setCursorVisible( bool yes );
		void setCursorTo( const QPointF& pos, bool select ); // pos relativ zu 0,0
		void setBlinkingCursorEnabled(bool enable);
		void repaintCursor();

		void repaintSelection() { invalidate( selectionRect() ); }
		void selectTo( const QPointF& pos ); // pos relativ zu 0,0
		void selectWordAt( const QPointF& ); // pos relativ zu 0,0
		void selectAll();

		void setDocument(QTextDocument *document = 0);
		QTextDocument* getDocument() const { return doc; }
		bool isPlainText() const { return plainText; }
		bool isSingleBlock() const { return singleBlock; }
		void setPlainText( bool on ) { plainText = on; }
		void setSingleBlock( bool on ) { singleBlock = on; }

		// Zeichnet relativ zu orig, clip relativ zu 0,0
		void paint(QPainter *p, const QPalette&, const QRectF& clip = QRectF() );
		void invalidate() { invalidate( QRectF( QPointF(0,0), d_extent ) ); }

		static qreal widhtToHeight( QTextDocument* doc, qreal width );
	public slots:
		void undo();
		void redo();
		void copy();

	signals:
		void documentChanged();
		void extentChanged();
		void invalidate( const QRectF& );
		void textChanged();
		void undoAvailable(bool b);
		void redoAvailable(bool b);
		void currentCharFormatChanged(const QTextCharFormat &format);
		void copyAvailable(bool b);
		void selectionChanged();
		void cursorPositionChanged();
		void updateMicroFocus();

	protected:

		//* Overrides
		void timerEvent(QTimerEvent *e);
	private:
		void relayoutDocument(qreal visibleLength = 0.0 );
		void pageDown(QTextCursor::MoveMode moveMode, qreal pageHeight);
		void pageUp(QTextCursor::MoveMode moveMode, qreal pageHeight);
		static QTextLine currentTextLine(const QTextCursor &cursor);

		void updateCurrentCharFormat();

		void init();

		void setCursorPosition(const QPointF &pos);
		void setCursorPosition(int pos, QTextCursor::MoveMode mode = QTextCursor::MoveAnchor);
		void setCursor( const QTextCursor &cursor );

		void emitSelectionChanged();

		void setClipboard(bool selection);

		void extendWordwiseSelection(int suggestedNewPosition, qreal mouseXPosition);

		QRectF rectForPosition(int position) const;
		QRectF selectionRect() const;

		bool isWordDoubleClicked() const { return selectedWordOnDoubleClick.hasSelection(); }

		void wordDoubleClicked();

		class Cursor : public TextCursor
		{
		public:
			void setStyles( const Styles* s )
			{
				d_styles = s;
			}
			const Styles* getStyles() const { return d_styles; }
			const QTextCursor& get() const { return d_cur; }
			void set( const QTextCursor &c )
			{
				d_cur = c;
				adjustSelection();
			}
			void setPosition( int pos )
			{
				d_cur.setPosition( pos );
				adjustSelection();
			}
			void setPosition( int pos, QTextCursor::MoveMode mode )
			{
				d_cur.setPosition(pos, mode);
				adjustSelection();
			}
			bool movePosition(QTextCursor::MoveOperation op, 
				QTextCursor::MoveMode mm = QTextCursor::MoveAnchor, int n = 1)
			{
				bool res = d_cur.movePosition( op, mm);
				adjustSelection();
				return res;
			}
			void mergeBlockFormat( const QTextBlockFormat& fmt )
			{
				d_cur.mergeBlockFormat( fmt );
			}
			void mergeBlockCharFormat( const QTextCharFormat& fmt )
			{
				d_cur.mergeBlockCharFormat( fmt );
			}
			void mergeCharFormat( const QTextCharFormat& fmt )
			{
				d_cur.mergeCharFormat( fmt );
			}
			void setCharFormat( const QTextCharFormat& fmt )
			{
				d_cur.setCharFormat( fmt );
			}
			void setBlockFormat( const QTextBlockFormat& fmt )
			{
				d_cur.setBlockFormat( fmt );
			}
			void moveToStart()
			{
				d_cur.movePosition(QTextCursor::Start);
				adjustSelection();
			}
			void selectAll()
			{
				d_cur.movePosition(QTextCursor::Start);
				d_cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
			}
			void selectBlock()
			{
				d_cur.movePosition(QTextCursor::StartOfBlock);
				d_cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
			}
			void selectWord()
			{
				d_cur.select(QTextCursor::WordUnderCursor);
				adjustSelection();
			}
			void removeSelectedText()
			{
				d_cur.removeSelectedText();
				adjustSelection();
			}
			void insertText( const QString& txt )
			{
				d_cur.insertText( txt );
			}
			void insertFragment( const QTextDocumentFragment& f )
			{
				d_cur.insertFragment(f);
				adjustSelection();
			}
		};
		Cursor cursor;

	private:
		QTextDocument *doc;
		QTextCharFormat lastCharFormat;

		bool cursorOn;
		bool readOnly; 
		bool mousePressed;
		bool lastSelectionState;
		bool ignoreAutomaticScrollbarAdjustement;
		bool singleBlock;
		bool plainText;


		QBasicTimer cursorBlinkTimer;
		
		QSizeF d_extent;
	private:
		QTextCursor selectedWordOnDoubleClick;
	private slots:
		void updateCurrentCharFormatAndSelection();
		void emitExtentChanged();
		void emitCursorPosChanged(const QTextCursor &);
		void updateCursorAfterUndoRedo(int, int, int);
		void emitInvalidate( QRectF );
	};
}

#endif // _TextView
