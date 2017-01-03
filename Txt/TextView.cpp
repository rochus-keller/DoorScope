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

#include "TextView.h"

#include <qfont.h>
#include <qpainter.h>
#include <qevent.h>
#include <qdebug.h>
#include <qmime.h>
#include <qdrag.h>
#include <qclipboard.h>
#include <qmenu.h>
#include <qstyle.h>
#include <qtimer.h>
#include "private/qtextdocumentlayout_p.h"
#include "qtextdocument.h"
#include "qtextlist.h"
#include "TextMimeData.h"

#include <qtextformat.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <limits.h>
#include <qtexttable.h>
#include <qvariant.h>
#include <qurl.h>
#include <cassert>
using namespace Txt;


TextView::TextView( QObject* owner, const Styles* s, bool sb, bool pt )
        : QObject( owner ), doc(0), cursorOn(false),
	  readOnly(false), lastSelectionState(false), 
	  ignoreAutomaticScrollbarAdjustement(false), singleBlock( sb ), plainText( pt )
{
	if( s == 0 )
		s = Styles::inst();
	cursor.setStyles( s );
    init();
}

// could go into QTextCursor...
QTextLine TextView::currentTextLine(const QTextCursor &cursor )
{
    const QTextBlock block = cursor.block();
    if (!block.isValid())
        return QTextLine();

    const QTextLayout *layout = block.layout();
    if (!layout)
        return QTextLine();

    const int relativePos = cursor.position() - block.position();
    return layout->lineForTextPosition(relativePos);
}


void TextView::updateCurrentCharFormat()
{
    

    QTextCharFormat fmt = cursor.get().charFormat();
    if (fmt == lastCharFormat)
        return;
    lastCharFormat = fmt;

    emit currentCharFormatChanged(fmt);
    emit updateMicroFocus();
}

void TextView::init()
{
    

    setDocument();

    // hbar->setSingleStep(20);
    // vbar->setSingleStep(20);

	/*
    viewport()->setBackgroundRole(QPalette::Base);
	// q->setAutoFillBackground( true );

    q->setAcceptDrops(true);
    q->setFocusPolicy(Qt::WheelFocus);
    q->setAttribute(Qt::WA_KeyCompression);

	q->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum ); // RISK

    viewport()->setCursor(Qt::IBeamCursor);
	*/
}

void TextView::setCursor( const QTextCursor &c )
{
    
	cursor.set( c ); 
    updateCurrentCharFormatAndSelection();
    invalidate(); 
    cursorPositionChanged();
}

void TextView::emitInvalidate( QRectF r )
{
	/* NOTE: Document liefert immer 0, 0, max, max, egal was passiert.
	QRect t=r.toRect(); // TEST
	qDebug( "x=%d y=%d w=%d h=%d", t.x(), t.y(), t.width(), t.height() );
	*/
	// Layout::update kommt bei jeglicher Änderung, von setDoc über KeyDown bis Format,
	// und immer wird gesamter Bereich invalidiert.
	emit invalidate( r );
}

void TextView::setDocument(QTextDocument *document)
{
    if( doc != 0 && doc == document)
        return;

	if( doc )
	{
		doc->disconnect(this);
		doc->documentLayout()->disconnect(this);
		doc->documentLayout()->setPaintDevice(0);

		if( doc->parent() == this )
			delete doc;
	}    

	doc = 0;

    // for use when called from setPlainText. we may want to re-use the currently
    // set char format then.
    const QTextCharFormat charFormatForInsertion = cursor.get().charFormat();

    bool clearDocument = true;
    if (!doc) {
        if (document) {
            doc = document;
            clearDocument = false;
        } else {
            doc = new QTextDocument( this );
			doc->setDefaultFont( cursor.getStyles()->getFont() );
        }

		// NOTE update ist eigentlich ein sinnloser Event.
		// Dummerweise erfährt man aber bei Cut/Copy/Paste/Undo/Redo ansonsten nichts vom Update
        ( QObject::connect(doc->documentLayout(), SIGNAL(update(QRectF)), this, SLOT(emitInvalidate(QRectF))));
        ( QObject::connect(doc->documentLayout(), SIGNAL(documentSizeChanged(QSizeF)), this, SLOT(emitExtentChanged())));
        cursor.set( QTextCursor(doc) );

        // doc->setDefaultFont(q->font());
        // doc->documentLayout()->setPaintDevice( viewport() );

        ( QObject::connect(doc, SIGNAL(contentsChanged()), this, SLOT(updateCurrentCharFormatAndSelection())));
        ( QObject::connect(doc, SIGNAL(cursorPositionChanged(QTextCursor)), this, SLOT(emitCursorPosChanged(QTextCursor))));

        // convenience signal forwards
		// textChanged brauchts vermutlich nicht.
        ( QObject::connect(doc, SIGNAL(contentsChanged()), this, SIGNAL(textChanged())));
        ( QObject::connect(doc, SIGNAL(undoAvailable(bool)), this, SIGNAL(undoAvailable(bool))));
        ( QObject::connect(doc, SIGNAL(redoAvailable(bool)), this, SIGNAL(redoAvailable(bool))));
    }

    doc->setUndoRedoEnabled(false);

    // TODO q->setAttribute(Qt::WA_InputMethodEnabled);

    // avoid multiple textChanged() signals being emitted
    QObject::disconnect(doc, SIGNAL(contentsChanged()), this, SIGNAL(textChanged()));

    if (clearDocument) {
        doc->clear();

        cursor.moveToStart();
        QTextBlockFormat blockFmt;
        // blockFmt.setLayoutDirection(q->layoutDirection());
        cursor.setBlockFormat(blockFmt);
    }

    // preserve the char format across clear()
    cursor.moveToStart();
    cursor.setCharFormat(charFormatForInsertion);

    ( QObject::connect(doc, SIGNAL(contentsChanged()), this, SIGNAL(textChanged())));
    emit textChanged();
    doc->setUndoRedoEnabled(!readOnly );
    updateCurrentCharFormatAndSelection();
    doc->setModified(false);
    emit cursorPositionChanged();
	emit documentChanged();
    relayoutDocument();
}

void TextView::setCursorPosition(const QPointF &pos)
{
    const int cursorPos = doc->documentLayout()->hitTest( pos, Qt::FuzzyHit);
    if (cursorPos == -1)
        return;
    cursor.setPosition(cursorPos);
}

void TextView::setCursorPosition(int pos, QTextCursor::MoveMode mode)
{
    cursor.setPosition(pos, mode);

    if (mode != QTextCursor::KeepAnchor) {
        selectedWordOnDoubleClick = QTextCursor();
    }
}

void TextView::repaintCursor()
{
    invalidate( cursorRect() );
}

void TextView::emitSelectionChanged()
{
    
    bool current = cursor.get().hasSelection();
    if (current == lastSelectionState)
        return;

    lastSelectionState = current;
    emit copyAvailable(current);
    emit selectionChanged();
    emit updateMicroFocus();
}

void TextView::updateCurrentCharFormatAndSelection()
{
    updateCurrentCharFormat();
    emitSelectionChanged();
}

void TextView::emitExtentChanged()
{
    if (ignoreAutomaticScrollbarAdjustement)
        return;

    QAbstractTextDocumentLayout *layout = doc->documentLayout();

    QSizeF docSize;

    if (QTextDocumentLayout *tlayout = qobject_cast<QTextDocumentLayout *>(layout)) 
	{
		// Bringt das wirklich etwas? Warum nicht nur documentSize()?
        docSize = tlayout->dynamicDocumentSize();
        qreal percentageDone = tlayout->layoutStatus();
        // extrapolate height
        if (percentageDone > 0)
            docSize.setHeight(docSize.height() * 100.0 / percentageDone);
    } else 
	{
        docSize = layout->documentSize();
    }

	bool change = docSize.height() != d_extent.height();
	d_extent.setHeight( docSize.height() );
	if( change )
		emit extentChanged();

}

void TextView::setClipboard(bool selection)
{
    QClipboard *clipboard = QApplication::clipboard();
	TextMimeData* mime = 0;
	if( selection )
	{
		if (!cursor.get().hasSelection() || !clipboard->supportsSelection() )
			return;
		mime = new TextMimeData( QTextDocumentFragment( cursor.get() ) );
	}else
	{
		if( cursor.get().hasSelection() )
 			mime = new TextMimeData( QTextDocumentFragment( cursor.get() ) );
		else
 			mime = new TextMimeData( QTextDocumentFragment( getDocument() ) );
	}
	assert( mime != 0 );
	clipboard->setMimeData( mime, (selection)?QClipboard::Selection:QClipboard::Clipboard);
}

void TextView::copy()
{
	setClipboard( false );
}

void TextView::ensureViewportLayouted( qreal len )
{
	if( len == 0.0 )
		return;
    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    if (!layout)
        return;
    if (QTextDocumentLayout *tlayout = qobject_cast<QTextDocumentLayout *>(layout))
        tlayout->ensureLayouted( len );
}

void TextView::emitCursorPosChanged(const QTextCursor &someCursor)
{
    
    if (someCursor.isCopyOf(cursor.get())) {
        emit cursorPositionChanged();
        emit updateMicroFocus();
    }
}

void TextView::setBlinkingCursorEnabled(bool enable)
{
    
    if (enable && QApplication::cursorFlashTime() > 0)
        cursorBlinkTimer.start(QApplication::cursorFlashTime() / 2, this );
    else
        cursorBlinkTimer.stop();
    repaintCursor();
}

void TextView::extendWordwiseSelection(int suggestedNewPosition, qreal mouseXPosition)
{
    

    // if inside the initial selected word keep that
    if (suggestedNewPosition >= selectedWordOnDoubleClick.selectionStart()
        && suggestedNewPosition <= selectedWordOnDoubleClick.selectionEnd()) 
	{
        setCursor(selectedWordOnDoubleClick);

        return;
    }

    QTextCursor curs = selectedWordOnDoubleClick;
    curs.setPosition(suggestedNewPosition, QTextCursor::KeepAnchor);

    if (!curs.movePosition(QTextCursor::StartOfWord))
        return;
    const int wordStartPos = curs.position();

    const int blockPos = curs.block().position();
    const QPointF blockCoordinates = doc->documentLayout()->blockBoundingRect(curs.block()).topLeft();

    QTextLine line = currentTextLine(curs);
    if (!line.isValid())
        return;

    const qreal wordStartX = line.cursorToX(curs.position() - blockPos) + blockCoordinates.x();

    if (!curs.movePosition(QTextCursor::EndOfWord))
        return;
    const int wordEndPos = curs.position();

    const QTextLine otherLine = currentTextLine(curs);
    if (otherLine.textStart() != line.textStart()
        || wordEndPos == wordStartPos)
        return;

    const qreal wordEndX = line.cursorToX(curs.position() - blockPos) + blockCoordinates.x();

    if (mouseXPosition < wordStartX || mouseXPosition > wordEndX)
        return;

    // keep the already selected word even when moving to the left
    // (#39164)
    if (suggestedNewPosition < selectedWordOnDoubleClick.position())
        cursor.setPosition(selectedWordOnDoubleClick.selectionEnd());
    else
        cursor.setPosition(selectedWordOnDoubleClick.selectionStart());

    const qreal differenceToStart = mouseXPosition - wordStartX;
    const qreal differenceToEnd = wordEndX - mouseXPosition;

    if (differenceToStart < differenceToEnd)
        setCursorPosition(wordStartPos, QTextCursor::KeepAnchor);
    else
        setCursorPosition(wordEndPos, QTextCursor::KeepAnchor);
}

void TextView::undo()
{
    
    (QObject::connect(doc, SIGNAL(contentsChange(int, int, int)),
                     this, SLOT(updateCursorAfterUndoRedo(int, int, int))));
    doc->undo();
    QObject::disconnect(doc, SIGNAL(contentsChange(int, int, int)),
                        this, SLOT(updateCursorAfterUndoRedo(int, int, int)));
	invalidate();
}

void TextView::redo()
{
    
    (QObject::connect(doc, SIGNAL(contentsChange(int, int, int)),
                     this, SLOT(updateCursorAfterUndoRedo(int, int, int))));
    doc->redo();
    QObject::disconnect(doc, SIGNAL(contentsChange(int, int, int)),
                        this, SLOT(updateCursorAfterUndoRedo(int, int, int)));
	invalidate();
}

void TextView::updateCursorAfterUndoRedo(int undoPosition, int /*charsRemoved*/, int charsAdded)
{
    if (cursor.get().isNull())
        return;

    cursor.setPosition(undoPosition + charsAdded);
}

qreal TextView::widhtToHeight( QTextDocument* doc, qreal width )
{
	// TODO: dieser Code ist Redundant zu relayoutDocument, ensureViewportLayouted
	// und emitExtentChanged
    doc->setPageSize( QSizeF( width, INT_MAX ) );
    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    if (!layout)
        return 0;
    if( QTextDocumentLayout *tlayout = qobject_cast<QTextDocumentLayout *>(layout) )
	{
        tlayout->ensureLayouted( INT_MAX );
        QSizeF docSize = tlayout->dynamicDocumentSize();
        qreal percentageDone = tlayout->layoutStatus();
        // extrapolate height
        if( percentageDone > 0 )
            docSize.setHeight(docSize.height() * 100.0 / percentageDone);
		return docSize.height();
    } else 
	{
        return layout->documentSize().width();
    }
}

void TextView::relayoutDocument( qreal len )
{

	/* Empirisch unnötig
    QAbstractTextDocumentLayout *layout = doc->documentLayout();
	QTextDocumentLayout *tlayout = qobject_cast<QTextDocumentLayout *>(layout);
    if( tlayout ) 
	{
		// TODO: Brauchen wir das?
        tlayout->setBlockTextFlags(tlayout->blockTextFlags() & ~Qt::TextSingleLine);
        tlayout->setFixedColumnWidth(-1);
    }
	*/

    // ignore calls to emitExtentChanged caused by an emission of the
    // usedSizeChanged() signal in the layout, as we're calling it
    // later on our own anyway (or deliberately not) .
    ignoreAutomaticScrollbarAdjustement = true;

    doc->setPageSize(QSizeF( d_extent.width(), INT_MAX) );
    ensureViewportLayouted( len );

    ignoreAutomaticScrollbarAdjustement = false;

    emitExtentChanged();
}

QRectF TextView::rectForPosition(int position) const
{
    const QTextBlock block = doc->findBlock(position);
    if (!block.isValid())
        return QRectF();
    const QAbstractTextDocumentLayout *docLayout = doc->documentLayout();
    const QTextLayout *layout = block.layout();
    const QPointF layoutPos = docLayout->blockBoundingRect(block).topLeft();
    int relativePos = position - block.position();
    QTextLine line = layout->lineForTextPosition(relativePos);

    if( line.isValid() )
        return QRectF( 
			( layoutPos.x() + line.cursorToX(relativePos) ) - 5.0, 
			layoutPos.y() + line.y(),
             10.0, line.ascent() + line.descent() + 1.0 );
    else
        return QRectF( 
			( layoutPos.x() - 5.0 ), 
			layoutPos.y(), 10.0, 10.0 ); // #### correct height

}

QRectF TextView::selectionRect() const
{
    QRectF r = rectForPosition(cursor.get().selectionStart());

    if( cursor.get().hasComplexSelection() && cursor.get().currentTable() ) 
	{
        QTextTable *table = cursor.get().currentTable();

        r = doc->documentLayout()->frameBoundingRect(table);
    } else if( cursor.get().hasSelection() ) 
	{
        const int position = cursor.get().selectionStart();
        const int anchor = cursor.get().selectionEnd();
        const QTextBlock posBlock = doc->findBlock(position);
        const QTextBlock anchorBlock = doc->findBlock(anchor);
        if (posBlock == anchorBlock && posBlock.layout()->lineCount()) {
            const QTextLine posLine = posBlock.layout()->lineForTextPosition(position - posBlock.position());
            const QTextLine anchorLine = anchorBlock.layout()->lineForTextPosition(anchor - anchorBlock.position());
            if (posLine.lineNumber() == anchorLine.lineNumber()) {
                r = posLine.rect();
            } else {
                r = posBlock.layout()->boundingRect();
            }
            r.translate( doc->documentLayout()->blockBoundingRect(posBlock).topLeft() );
        } else {
            QRectF anchorRect = rectForPosition( cursor.get().selectionEnd() );
            r |= anchorRect;
            QRectF frameRect( doc->documentLayout()->frameBoundingRect( cursor.get().currentFrame() ) );
            r.setLeft( frameRect.left() );
            r.setRight( frameRect.right() );
        }
    }

    return r;
}

void TextView::paint(QPainter *p, const QPalette& pal, const QRectF& clip )
{
   	p->save();

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette = pal;

    if( cursorOn ) 
	{
		ctx.cursorPosition = cursor.get().position();
    }

    if( cursor.get().hasSelection() ) 
	{
        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = cursor.get();
        selection.format.setBackground( pal.brush(QPalette::Highlight) );
        selection.format.setForeground( pal.brush(QPalette::HighlightedText) );
        ctx.selections.append( selection );
    }
    ctx.clip = clip;

	// TEST 
    doc->documentLayout()->draw(p, ctx);
	p->restore();
}

void TextView::wordDoubleClicked()
{
	QTextLine line = currentTextLine( cursor.get() );
	if (line.isValid() && line.textLength()) {
		cursor.selectWord();
		emitSelectionChanged();
		setClipboard(true);
		repaintSelection();
	}
	selectedWordOnDoubleClick = cursor.get();
}

bool TextView::moveCursor( CursorAction a, bool select, qreal pageHeight )
{
    if( cursor.isNull() )
        return false;
    
    QTextCursor::MoveMode mode = select ? QTextCursor::KeepAnchor
                                   : QTextCursor::MoveAnchor;

    QTextCursor::MoveOperation op = QTextCursor::NoMove;
	switch( a )
	{
	case MoveBackward:
		op = QTextCursor::Left;
		break;
	case MoveForward:
		op = QTextCursor::Right;
		break;
	case MoveWordBackward:
		op = QTextCursor::WordLeft;
		break;
	case MoveWordForward:
		op = QTextCursor::WordRight;
		break;
	case MoveUp:
        op = QTextCursor::Up;
		break;
	case MoveDown:
        op = QTextCursor::Down;
        if (mode == QTextCursor::KeepAnchor) 
		{
            QTextBlock block = cursor.get().block();
			QTextLine line = currentTextLine( cursor.get() );
            if (!block.next().isValid()
                && line.isValid()
                && line.lineNumber() == block.layout()->lineCount() - 1)
                op = QTextCursor::End;
        }
        break;
	case MoveLineStart:
		op = QTextCursor::StartOfLine;
		break;
	case MoveLineEnd:
		op = QTextCursor::EndOfLine;
		break;
	case MoveHome:
		op = QTextCursor::Start;
		break;
	case MoveEnd:
		op = QTextCursor::End;
		break;
	case MovePageUp:
		pageUp(mode, pageHeight );
		break;
	case MovePageDown:
		pageDown(mode, pageHeight );
		break;
	case MoveNextCell:
		cursor.movePosition(QTextCursor::NextBlock);
		op = QTextCursor::EndOfBlock;
		break;
	case MovePrevCell:
		cursor.movePosition(QTextCursor::PreviousBlock);
		op = QTextCursor::EndOfBlock;
		break;
	// default:
		// DontMove
	}
    repaintSelection(); // vorher und nachher zeichnen
    const bool moved = cursor.movePosition(op, mode);

    if( moved ) 
	{
        emit cursorPositionChanged();
        emit updateMicroFocus();
		repaintSelection();
		selectionChanged();
	}
	return moved;
}

bool TextView::find( const QString& pattern, bool forward, bool nocase, bool select, bool extend )
{
	QTextDocument::FindFlags f = 0;
	if( !forward )
		f |= QTextDocument::FindBackward;
	if( !nocase )
		f |= QTextDocument::FindCaseSensitively;
	int pos = 0;
	if( extend )
		pos = cursor.getSelStartPos();
	else
	{
		pos = (forward)?cursor.getSelEndPos():cursor.getSelStartPos()-1;
		if( pos == -1 )
			return false;
	}
	QTextCursor res = doc->find( pattern, pos, f );
	if( res.isNull() )
		return false;
    repaintSelection(); // vorher und nachher zeichnen
	cursor.setPosition( res.selectionStart() );
	if( res.hasSelection() )
		cursor.setPosition( res.selectionEnd(), QTextCursor::KeepAnchor );
	repaintSelection();
	return true;
}

void TextView::pageUp(QTextCursor::MoveMode moveMode, qreal pageHeight )
{
    bool moved = false;
    qreal y;
	const qreal start = cursorRect().bottom();
    // move to the targetY using movePosition to keep the cursor.get()'s x
    do {
        y = cursorRect().top();
        moved = cursor.movePosition(QTextCursor::Up, moveMode);
    } while (moved && y > ( start - pageHeight ) );

    if (moved) 
	{
        emit cursorPositionChanged();
        emit updateMicroFocus();
    }
}

void TextView::pageDown(QTextCursor::MoveMode moveMode, qreal pageHeight)
{
    bool moved = false;
    qreal y;
	const qreal start = cursorRect().bottom();
    // move to the targetY using movePosition to keep the cursor.get()'s x
    do {
        y = cursorRect().bottom();
        moved = cursor.movePosition(QTextCursor::Down, moveMode);
    } while (moved && y < ( start + pageHeight ) );

    if( moved ) 
	{
        emit cursorPositionChanged();
        emit updateMicroFocus();
    }
}

void TextView::setCursorVisible( bool yes )
{
    
    cursorOn = yes;
	repaintCursor();
}

void TextView::setWidth( qreal width, qreal visibleHeight )
{
	if( width != d_extent.width() )
	{
		d_extent.setWidth( width );
		relayoutDocument( visibleHeight );
	}else
		ensureViewportLayouted( visibleHeight );
}

void TextView::setCursorTo( const QPointF& pos, bool shift )
{
    int cursorPos = doc->documentLayout()->hitTest(pos, Qt::FuzzyHit);
    if( cursorPos == -1 )
        return;

    QTextLayout *layout = cursor.get().block().layout();
    if( shift ) 
	{
        if( isWordDoubleClicked() )
            extendWordwiseSelection( cursorPos, pos.x() );
        else
            setCursorPosition( cursorPos, QTextCursor::KeepAnchor );
    }else 
	{

        if( cursor.hasSelection()
            && cursorPos >= cursor.get().selectionStart()
            && cursorPos <= cursor.get().selectionEnd()
            && doc->documentLayout()->hitTest(pos, Qt::ExactHit) != -1) 
		{
            return;
        }

        setCursorPosition(cursorPos);
    }

    if (readOnly) 
	{
        emit cursorPositionChanged();
        emit selectionChanged();
    } else 
	{
        emit cursorPositionChanged();
        emit updateCurrentCharFormatAndSelection();
    }
    repaintSelection();
}

void TextView::selectTo( const QPointF& mousePos )
{
    const qreal mouseX = qreal(mousePos.x());


    QTextLayout *layout = cursor.get().block().layout();
    if (layout && !layout->preeditAreaText().isEmpty())
        return;

    int newCursorPos = doc->documentLayout()->hitTest(mousePos, Qt::FuzzyHit);
    if (newCursorPos == -1)
        return;

    if( isWordDoubleClicked() )
        extendWordwiseSelection( newCursorPos, mouseX );
    else
        setCursorPosition( newCursorPos, QTextCursor::KeepAnchor );

    if( readOnly ) 
	{
        emit cursorPositionChanged();
        emit selectionChanged();
    } else 
	{
        emit cursorPositionChanged();
        emit updateCurrentCharFormatAndSelection();
    }
    repaintSelection();
}

void TextView::selectWordAt( const QPointF& p )
{
    QTextLayout *layout = cursor.get().block().layout();
    if (layout && !layout->preeditAreaText().isEmpty())
        return;

    setCursorPosition( p );
	wordDoubleClicked();
    repaintSelection();
}

void TextView::clear()
{
    setDocument( 0 );
}

void TextView::timerEvent(QTimerEvent *e)
{ 
    if( e->timerId() == cursorBlinkTimer.timerId() ) 
	{
        cursorOn = !cursorOn;

        if ( cursor.hasSelection() )
            cursorOn = false;

        repaintCursor();
    }
}

QRectF TextView::cursorRect(const QTextCursor &cursor) const
{
    
    if (cursor.isNull())
        return QRectF();

    return rectForPosition( cursor.position() );
}


QRectF TextView::cursorRect() const
{
    
    return cursorRect( cursor.get() );
}

void TextView::selectAll()
{
	cursor.selectAll();
	selectionChanged();
	invalidate();
	setClipboard(true);
}

