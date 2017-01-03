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

#include "AnnotDeleg.h"
#include "AnnotMdl.h"
#include "AppContext.h"
#include "TypeDefs.h"
#include <QAbstractItemView>
#include <Txt/TextInStream.h>
#include <Txt/TextCursor.h>
#include <Txt/Styles.h>
#include <QPainter>
#include <QtDebug>
#include <QTextDocument>
#include <QResizeEvent>
#include <QClipboard>
#include <QApplication>
using namespace Ds;
using namespace Txt;

static const int s_wb = 11; // Abstand von Links
static const int s_ro = 10; // Right offset

AnnotDeleg::AnnotDeleg(QAbstractItemView *parent, Styles* s)
	: QAbstractItemDelegate(parent)
{
	parent->installEventFilter( this ); // wegen Focus und Resize während Edit

	d_view = new TextView( this, s );
	d_ctrl = new TextCtrl( this, d_view );

	// Das muss so sein. Ansonsten werden Cursortasten-Moves nicht aktualisiert
	connect( d_view, SIGNAL( invalidate( const QRectF& ) ), 
		this, SLOT( invalidate( const QRectF& ) ) );
	connect( Txt::Styles::inst(), SIGNAL( sigFontStyleChanged() ), parent, SLOT( doItemsLayout() ) );
}

AnnotDeleg::~AnnotDeleg()
{

}

QAbstractItemView* AnnotDeleg::view() const 
{ 
	return static_cast<QAbstractItemView*>( parent() ); 
}

void AnnotDeleg::paint ( QPainter * painter, 
			const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	painter->save();

	const int indexType = index.data( AnnotMdl::IndexTypeRole ).toInt();
	if( indexType == AnnotMdl::TitleIndex )
	{
		const quint32 atom = index.data( AnnotMdl::AttrRole ).toUInt();
		QPixmap pix;
		pix.load( (atom!=0)?
			":/DoorScope/Images/attribute.png":
			":/DoorScope/Images/object.png" );
		painter->drawPixmap( option.rect.left(), option.rect.top() + 1, pix );

		const QString name = index.data().toString();
		const int pos = name.indexOf( ':' );
		QFont f = painter->font();
		f.setBold( true );
		painter->setFont( f );
		QRect bound;
		const QPoint off = QPoint( option.rect.left() + s_wb, option.rect.top() );
		int wLeft = option.rect.width() - s_wb - s_ro;
		painter->translate( off );
		painter->setPen( Qt::black );
		painter->drawText( 0, 0, wLeft, option.rect.height(), 
			Qt::AlignLeft | Qt::AlignVCenter, name.left( pos + 1 ), &bound );
		wLeft -= bound.width();
		f.setBold( false );
		painter->setFont( f );
		painter->drawText( bound.width(), 0, wLeft, option.rect.height(),
			Qt::AlignLeft | Qt::AlignVCenter, 
			painter->fontMetrics().elidedText( name.mid( pos + 1 ), Qt::ElideRight, wLeft ) );
		const int prio = index.data( AnnotMdl::PrioRole ).toInt();
		if( prio != 0 )
		{
			QPixmap pix;
			switch( prio )
			{
			case AnnPrio_NiceToHave:
				pix.load( ":/DoorScope/Images/nicetohave.png" );
				break;
			case AnnPrio_NeedToHave:
				pix.load( ":/DoorScope/Images/needtohave.png" );
				break;
			case AnnPrio_Urgent:
				pix.load( ":/DoorScope/Images/urgent.png" );
				break;
			}
			painter->translate( -off.x(), -off.y() );
			painter->drawPixmap( option.rect.right() - s_ro, 
				option.rect.top() + 1, pix );
		}
	}else if( indexType == AnnotMdl::BodyIndex )
	{
		if( d_edit == index )
		{
			d_ctrl->setPos( option.rect.left(), option.rect.top() );
			d_ctrl->paint( painter, option.palette );
		}else
		{
			QVariant v = index.data(AnnotMdl::BodyRole);
			const QPoint off = QPoint( option.rect.left(), option.rect.top() );
			painter->translate( off );
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc, d_view->getCursor().getStyles() );
			if( v.type() == QVariant::ByteArray )
			{
				Stream::DataReader r( v.toByteArray() );
				Txt::TextInStream in;
				in.readFromTo( r, cur );
			}else
				cur.insertText( v.toString() );
			doc.setTextWidth( option.rect.width() );
			doc.drawContents( painter );
		}
	}

	painter->restore();
}

QSize AnnotDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.

	const int indexType = index.data( AnnotMdl::IndexTypeRole ).toInt();
	if( indexType == AnnotMdl::TitleIndex )
		return QSize( width, option.fontMetrics.height() );
	else if( indexType == AnnotMdl::BodyIndex )
	{
		if( d_edit == index )
		{
			return d_view->getExtent().toSize();
		}else
		{
			QVariant v = index.data( AnnotMdl::BodyRole );
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc, d_view->getCursor().getStyles() );
			if( v.type() == QVariant::ByteArray )
			{
				Stream::DataReader r( v.toByteArray() );
				Txt::TextInStream in;
				in.readFromTo( r, cur );
			}else
			{
				cur.insertText( v.toString() );
			}
			doc.setTextWidth( width );
			return doc.size().toSize();
		}
	}
	return QSize();
}

bool AnnotDeleg::eventFilter(QObject *watched, QEvent *event)
{
	bool res = QAbstractItemDelegate::eventFilter( watched, event );
	if( event->type() == QEvent::Resize )
	{
		QResizeEvent* e = static_cast<QResizeEvent*>( event );
		if( e->size().width() != e->oldSize().width() )
			// Bei unmittelbarer Ausführung scheint die Breite noch nicht zu stimmen.
			QMetaObject::invokeMethod( this, "relayout", Qt::QueuedConnection );
	}
	if( d_edit.isValid() )
	{
		switch( event->type() )
		{
		case QEvent::FocusIn:
			d_ctrl->handleFocusIn();
			break;
		case QEvent::FocusOut:
			d_ctrl->handleFocusOut();
			break;
		case QEvent::KeyPress:
			{
				QKeyEvent* e = static_cast<QKeyEvent*>(event );
				if( e == QKeySequence::Copy && d_view->getCursor().hasSelection() )
				{
					QApplication::clipboard()->setText(
						d_view->getCursor().getSelection().toPlainText() );
					return true;
				}else if( e == QKeySequence::Cut && d_view->getCursor().hasSelection() )
				{
					QApplication::clipboard()->setText(
						d_view->getCursor().getSelection().toPlainText() );
					d_view->getCursor().removeSelectedText();
					d_view->invalidate();
					return true;
				}else if( e == QKeySequence::Paste )
				{
					QString str = QApplication::clipboard()->text();
					if( !str.isEmpty() )
					{
						d_view->getCursor().insertText( str );
						d_view->invalidate();
						return true;
					}
				}else if( e == QKeySequence::Undo )
				{
					d_view->undo();
					d_view->invalidate();
					return true;
				}else if( e == QKeySequence::Redo )
				{
					d_view->redo();
					d_view->invalidate();
					return true;
				}else if( e == QKeySequence::SelectAll )
				{
					d_view->selectAll();
					return true;
				}
			}
			break;
		default:
			break;
		}
	}
	return res;
}

void AnnotDeleg::relayout()
{
	view()->doItemsLayout();
	if( d_edit.isValid() )
	{
		QRect r = view()->visualRect( d_edit );
		d_view->setWidth( r.width(), r.height() );
		view()->scrollTo( d_edit );
	}
}

QWidget * AnnotDeleg::createEditor ( QWidget * parent, 
		const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	// Hierhin kommen wir immer, wenn ein anderes Item editiert werden soll.
	// Im Falle von QAbstractItemView::AllEditTriggers kommt man sofort hierher,
	// d.h. currentChanged() wird ausgelassen.
	closeEdit();

	const int indexType = index.data( AnnotMdl::IndexTypeRole ).toInt();
	if( indexType == AnnotMdl::BodyIndex )
	{
		AppContext::inst()->askUserName( parent );
		disconnect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
		d_edit = index;
		readData();
		// Bei Indent wird diese Funktion irrtümlich von Qt ohne gültige Breite aufgerufen.
		d_ctrl->setPos( option.rect.left(), option.rect.top() ); // das muss hier stehen, damit erster Click richtig übersetzt
		if( option.rect.width() != d_view->getExtent().width() )
			d_view->setWidth( option.rect.width(), option.rect.height() );
		
		if( option.rect.height() != d_view->getExtent().height() )
		{
			QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
			mdl->setData( d_edit, 0, Qt::SizeHintRole );
			// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.
		}

		d_view->invalidate();
		d_view->setCursorVisible( true );
		d_view->setBlinkingCursorEnabled( true );

		connect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	}
	return 0;
}

bool AnnotDeleg::editorEvent(QEvent *event,
                                QAbstractItemModel *model,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index)
{
	// option.rect bezieht sich immer auf das Item unter der Maus. event->pos() ist
	// daher immer innerhalb option.rect.

	if( d_edit == index  )
	{
		switch( event->type() )
		{
		case QEvent::MouseButtonDblClick:
			return d_ctrl->mouseDoubleClickEvent( (QMouseEvent*) event );
		case QEvent::MouseButtonPress:
			return d_ctrl->mousePressEvent( (QMouseEvent*) event );
		case QEvent::MouseButtonRelease:
			return d_ctrl->mouseReleaseEvent( (QMouseEvent*) event );
		case QEvent::MouseMove:
			return d_ctrl->mouseMoveEvent( (QMouseEvent*) event );
		case QEvent::KeyPress:
			return d_ctrl->keyPressEvent( (QKeyEvent*) event );
		// Focus, Paint und Resize kommen hier nicht an. Ebenfalls nicht Shortcuts.
		default:
			qDebug( "other editor event %d", event->type() );
		}
	}else
	{
		//closeEdit();
		return false;
	}
	return false;
}

void AnnotDeleg::readData() const
{
	if( !d_edit.isValid() )
		return;

	QVariant v = d_edit.data(AnnotMdl::BodyRole);
	d_view->clear();
	d_view->getDocument()->setUndoRedoEnabled( false );
	if( v.type() == QVariant::ByteArray )
	{
		Stream::DataReader r( v.toByteArray() );
		Txt::TextInStream in;
		in.readFromTo( r, d_view->getCursor() );
		d_view->setPlainText( false );
	}else
	{
		d_view->getCursor().insertText( v.toString() );
		d_view->setPlainText( true );
	}
	d_view->getDocument()->setModified( false );
	d_view->getDocument()->setUndoRedoEnabled( true );
}

void AnnotDeleg::writeData() const
{
	if( !d_edit.isValid() )
		return;
	if( d_view->getDocument()->isModified() )
	{
		QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
		mdl->setData( d_edit, d_view->getDocument()->toPlainText(), AnnotMdl::BodyRole );
	}
}

void AnnotDeleg::invalidate( const QRectF& r )
{
	QRect rr = r.toRect();
	rr.translate( d_ctrl->getDx(), d_ctrl->getDy() );
	view()->viewport()->update( rr );
}

void AnnotDeleg::extentChanged()
{
	disconnect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	
	QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
	mdl->setData( d_edit, 0, Qt::SizeHintRole );
	// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.

	connect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
}

void AnnotDeleg::closeEdit() const
{
	if( !d_edit.isValid() )
		return;
	d_view->setCursorVisible( false );
	d_view->setBlinkingCursorEnabled(false);
	writeData();
	d_edit = QModelIndex();
	disconnect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	d_view->setDocument();
	d_view->invalidate();
	connect( d_view, SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
}

void AnnotDeleg::setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const
{
	// Mit diesem Trick kann man Editor schliessen, ohne Deleg zu spezialisieren.
	if( d_edit.isValid() )
	{
		closeEdit();
	}
}
