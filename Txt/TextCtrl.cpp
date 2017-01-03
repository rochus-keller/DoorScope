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

#include "TextCtrl.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QPainter>
#include <QAbstractSlider>
using namespace Txt;

TextCtrl::TextCtrl(QObject* owner, TextView* view ):
	QObject( owner ), d_mousePressed( false ), d_view( view ), d_yOff( 0 )
{
	connect( view, SIGNAL( cursorPositionChanged() ),
		this, SLOT( handleCursorChanged() ) );
	connect( view, SIGNAL( invalidate( const QRectF& ) ), 
		this, SLOT( handleInvalidate( const QRectF& ) ) );
	connect( view, SIGNAL( extentChanged() ),
		this, SLOT( handleExtentChanged() ) );
}

void TextCtrl::handleExtentChanged()
{
	emit extentChanged();
}

void TextCtrl::setPos( qint16 dx, qint16 dy )
{
	d_vr.setX( dx );
	d_vr.setY( dy );
}

void TextCtrl::handleInvalidate( const QRectF& r )
{
	QRect rr = r.toRect();
	rr.translate( d_vr.x(), d_vr.y() );
    rr = rr.intersect( d_vr );
    if ( rr.isEmpty())
        return;
	emit invalidate( rr );
}

bool TextCtrl::cursorMoveKeyEvent(QKeyEvent *e)
{
    bool select = e->modifiers() & Qt::ShiftModifier
                                   ? true
                                   : false;

    TextView::CursorAction op = TextView::DontMove;
    if( e->modifiers() & ( Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier ) ) 
	{
        e->ignore();
        return false;
    }
    switch (e->key()) 
	{
        case Qt::Key_Up:
            op = TextView::MoveUp;
            break;
        case Qt::Key_Down:
            op = TextView::MoveDown;
            break;
        case Qt::Key_Left:
            op = e->modifiers() & Qt::ControlModifier
                 ? TextView::MoveWordBackward
                 : TextView::MoveBackward;
            break;
        case Qt::Key_Right:
            op = e->modifiers() & Qt::ControlModifier
                 ? TextView::MoveWordForward
                 : TextView::MoveForward;
            break;
        case Qt::Key_Home:
            op = e->modifiers() & Qt::ControlModifier
                 ? TextView::MoveHome
                 : TextView::MoveLineStart;
            break;
        case Qt::Key_End:
            op = e->modifiers() & Qt::ControlModifier
                 ? TextView::MoveEnd
                 : TextView::MoveLineEnd;
            break;
        case Qt::Key_PageDown:
			op = TextView::MovePageDown;
            break;
        case Qt::Key_PageUp:
			op = TextView::MovePageUp;
            break;
    default:
        return false;
    }
	return view()->moveCursor( op, select, visibleHeight() );
}

bool TextCtrl::keyPressEvent(QKeyEvent *e)
{
    // schedule a repaint of the region of the cursor, as when we move it we
    // want to make sure the old cursor disappears (not noticable when moving
    // only a few pixels but noticable when jumping between cells in tables for
    // example)
	TextView* me = view();
    me->repaintSelection();

    if ( cursorMoveKeyEvent(e))
        goto accept;

	if( e == QKeySequence::Copy )
	{
		view()->copy();
        goto accept;
	}

    if( me->isReadOnly() ) 
		return false; // TODO downcall

    switch( e->key() ) 
	{
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageDown:
    case Qt::Key_PageUp:
		return false;
    case Qt::Key_Tab: 
		if( me->getCursor().inCodeBlock() )
		{
			me->getCursor().indentCodeLines();
		}else
		{
			me->setCursorVisible( false );
			me->moveCursor( TextView::MoveNextCell, false );
		}
		break;
	case Qt::Key_Backtab:
		if( me->getCursor().inCodeBlock() )
		{
			me->getCursor().unindentCodeLines();
		}else
		{
			me->setCursorVisible( false );
			me->moveCursor( TextView::MovePrevCell, false );
		}
		break;
	case Qt::Key_Backspace:
		if( me->getCursor().atBeginOfCodeLine() )
		{
			me->getCursor().unindentCodeLines();
			break;
		}else
			me->getCursor().deleteCharOrSelection( true );
        break;
    case Qt::Key_Delete:
        me->getCursor().deleteCharOrSelection( false );
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
		if( !d_view->isSingleBlock() )
			me->getCursor().addParagraph(e->modifiers() & Qt::ControlModifier);
        break;
    default:
        {
            QString text = e->text();

			if (!text.isEmpty() && 
				(text.at(0).isPrint() || text.at(0) == QLatin1Char('\t'))) 
			{
				// no need to call deleteChar() if we have a selection, cursor.insertText
				// does it already
				if( text.at(0) == QLatin1Char(' ') && 
					( e->modifiers() & ( Qt::ShiftModifier | Qt::ControlModifier ) ) )
					me->getCursor().insertText( QString( QChar::Nbsp ) );
				else
					me->getCursor().insertText( text );
			} else {
				return false; // TODO downcall
			}
			break;
        }
    }
accept:
	e->accept();
    me->setCursorVisible( true );

	return true;
}

bool TextCtrl::mousePressEvent( QMouseEvent *e)
{
    if ( e->button() != Qt::LeftButton )
        return false; 

	TextView* me = view();

    me->ensureViewportLayouted( d_yOff + visibleHeight() );

    d_mousePressed = true;
    me->repaintSelection();

	QPoint pos = e->pos();
	pos.setX( pos.x() - d_vr.x() );
	pos.setY( pos.y() - d_vr.y() );
	me->setCursorTo( pos, e->modifiers() == Qt::ShiftModifier );
	if( e->modifiers() == Qt::ControlModifier && me->getCursor().isUrl() )
		emit anchorActivated( me->getCursor().getUrlEncoded() );

	return true;
}

bool TextCtrl::mouseMoveEvent(QMouseEvent *e)
{
    if ( e->buttons() != Qt::LeftButton )
        return false; 

	TextView* me = view();

	if( !d_mousePressed )
		return false;

    me->ensureViewportLayouted( d_yOff + visibleHeight() );
	me->repaintSelection();

	QPoint pos = e->pos();
	pos.setX( pos.x() - d_vr.x() );
	pos.setY( pos.y() - d_vr.y() );

	me->selectTo( pos );
	return true;
}

bool TextCtrl::mouseReleaseEvent(QMouseEvent * e)
{
    if ( e->button() != Qt::LeftButton )
        return false; 

	TextView* me = view();

	me->ensureViewportLayouted( d_yOff + visibleHeight() );
    me->repaintSelection();

    d_mousePressed = false;
	return true;
}

bool TextCtrl::mouseDoubleClickEvent(QMouseEvent *e)
{
    if( e->button() != Qt::LeftButton ) 
	{
        e->ignore();
        return false;
    }
	TextView* me = view();

	me->ensureViewportLayouted( d_yOff + visibleHeight() );

	QPoint pos = e->pos();
	pos.setX( pos.x() - d_vr.x() );
	pos.setY( pos.y() - d_vr.y() );

	me->selectWordAt( pos );

	return true;
}

void TextCtrl::handleResize(const QSize& s)
{
	d_vr.setWidth( s.width() );
	d_vr.setHeight( s.height() );
	d_view->setWidth( s.width(), d_yOff + visibleHeight() );
}

void TextCtrl::paint(QPainter * p, const QPalette& t)
{
	p->translate( d_vr.x(), d_vr.y() );
	d_view->paint( p, t );
	p->translate( -d_vr.x(), -d_vr.y() );
}

void TextCtrl::handleFocusIn()
{
	if (!d_view->isReadOnly() ) 
	{
		d_view->setCursorVisible( true );
        d_view->setBlinkingCursorEnabled(true);
    }
}

void TextCtrl::handleFocusOut()
{
    d_view->setBlinkingCursorEnabled(false);
    d_view->setCursorVisible( false );
}

void TextCtrl::setScrollOff( quint16 off )
{
	d_yOff = off;
}

int TextCtrl::visibleHeight()
{
	if( d_vr.height() )
		return d_vr.height();
	else
		return d_view->getExtent().height(); 
}

void TextCtrl::ensureVisible(const QRect &rect)
{
    if (rect.y() < d_yOff )
        setScrollOff( rect.y() - rect.height());
    else if (rect.y() + rect.height() > d_yOff + visibleHeight() )
        setScrollOff( rect.y() + rect.height() - visibleHeight() );
	emit scrollOffChanged( d_yOff );
}

void TextCtrl::handleCursorChanged()
{
	ensureVisible( d_view->cursorRect().toRect() );
}
