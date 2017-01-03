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

#include "DocDeleg.h"
#include "DocMdl.h"
#include "TypeDefs.h"
#include "DocManager.h"
#include "DocTree.h"
#include <Txt/Styles.h>
#include <QTextEdit>
#include <QPainter>
#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QAbstractItemView>
#include <QClipboard>
#include <QKeySequence>
#include <QtDebug>
#include <QToolTip>
#include <QHeaderView>
using namespace Ds;
using namespace Txt;

static const int s_wb = 1; // Change Bar Width
static const int s_ro = 10; // Right offset

static int _ro( const QModelIndex & index )
{
	return (index.column() == 0)?s_ro:0;
}

DocDeleg::DocDeleg(DocTree *parent, Styles* s)
	: QAbstractItemDelegate(parent), d_oldSize( 0 )
{
	parent->installEventFilter( this ); // wegen Focus und Resize während Edit
	connect( parent->header(), SIGNAL( sectionResized ( int, int, int ) ),
		this, SLOT( onSectionResized ( int, int, int ) ) );

	TextView* view = new TextView( this, s );
	d_ctrl = new TextCtrl( this, view );
	view->setReadOnly( true );
	onFontStyleChanged();

	// Das muss so sein. Ansonsten werden Cursortasten-Moves nicht aktualisiert
	connect( d_ctrl->view(), SIGNAL( invalidate( const QRectF& ) ), 
		this, SLOT( invalidate( const QRectF& ) ) );
	connect( Txt::Styles::inst(), SIGNAL( sigFontStyleChanged() ), this, SLOT( onFontStyleChanged() ) );
}

DocDeleg::~DocDeleg()
{

}

void DocDeleg::onFontStyleChanged()
{
	d_titleFont = Styles::inst()->getFont();
	d_titleFont.setBold( true );
	d_titleFont.setPointSize( d_titleFont.pointSize() * 1.2 );
	QFontMetrics m( d_ctrl->view()->getCursor().getStyles()->getFont() );
	view()->setStepSize( m.height() );
	view()->doItemsLayout();
}

DocTree* DocDeleg::view() const 
{ 
	return static_cast<DocTree*>( parent() ); 
}

void DocDeleg::paint ( QPainter * painter, 
			const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	//painter->save(); // führt zu Crash wenn print() in Lua Filter

	//painter->setClipRect( option.rect );
	if( d_edit == index )
	{
		d_ctrl->setPos( option.rect.left() + s_wb, option.rect.top() );
		d_ctrl->paint( painter, option.palette );
	}else
	{
		const QPoint off = QPoint( option.rect.left() + s_wb, option.rect.top() );
		painter->translate( off );
		const int level = index.data( DocMdl::LevelRole ).toInt();
		const QVariant v = index.data(Qt::DisplayRole);
		if( level > 0 && index.column() == 0 )
		{
			TextDocument doc;
			QTextCursor cur( &doc );
			QTextCharFormat f;
			f.setFont( d_titleFont );
			cur.insertText( v.toString(), f );
			doc.setTextWidth( option.rect.width() - s_wb - _ro(index) );
			doc.drawContents( painter );
		}else // level == 0 // für Text und alle weiteren Spalten
		{
			if( v.canConvert<QTextDocument*>() )
			{
				QTextDocument* doc = v.value<QTextDocument*>();
				doc->drawContents( painter );
			}else
			{
				TextDocument doc;
				QTextCursor cur( &doc );
				QTextCharFormat f;
				f.setFont( d_ctrl->view()->getCursor().getStyles()->getFont( 0 ) );
				cur.insertText( v.toString(), f );
				doc.setTextWidth( option.rect.width() - s_wb - _ro(index) );
				doc.drawContents( painter );
			}
		}
		painter->translate( -off.x(), -off.y() );
	}

	if( index.column() == 0 )
	{
		const int status = index.data( DocMdl::StatusRole ).toInt();
		QPixmap pix;
		bool hasStatus = false;
		if( status != 0 )
		{
			switch( status )
			{
			case ReviewStatus_Accepted:
				pix.load( ":/DoorScope/Images/accept.png" );
				hasStatus = true;
				break;
			case ReviewStatus_AcceptedWithChanges:
				pix.load( ":/DoorScope/Images/acceptwc.png" );
				hasStatus = true;
				break;
			case ReviewStatus_Recheck:
				pix.load( ":/DoorScope/Images/warning.png" );
				hasStatus = true;
				break;
			case ReviewStatus_Rejected:
				pix.load( ":/DoorScope/Images/reject.png" );
				hasStatus = true;
				break;
			case ReviewStatus_Pending:
				pix.load( ":/DoorScope/Images/watch.png" );
				hasStatus = true;
				break;
			}
			if( hasStatus )
				painter->drawPixmap( option.rect.right() - s_ro, option.rect.top() + 0, pix );
		}
		if( index.data( DocMdl::CommentRole ).toBool() )
		{
			pix.load( ":/DoorScope/Images/comment.png" );
			painter->drawPixmap( option.rect.right() - s_ro, 
				option.rect.top() + 10 /*(hasStatus)?10:0*/, pix );
		}

		const bool changed = index.data( DocMdl::ChangedRole ).toBool();
		painter->fillRect( option.rect.left() + s_wb - 1, option.rect.top() + 1, 
			1, option.rect.height() - 2, (changed)? Qt::red : Qt::green );
		/* das funktioniert:
		painter->fillRect( option.rect.left() + s_wb - 1, option.rect.top() + 3, 
			1, option.rect.height() - 6, (changed)? Qt::red : Qt::green );
			*/
	}else
	{
		painter->setPen( Qt::lightGray );
		painter->drawLine( option.rect.topLeft(), option.rect.bottomLeft() );
	}

	//painter->restore();
}

QSize DocDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	if( d_edit == index )
	{
		return d_ctrl->view()->getExtent().toSize();
	}

	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.
		// QSize() oder QSize(0,0) zeichnet gar nichts

	const int level = index.data( DocMdl::LevelRole ).toInt();
	const QVariant v = index.data(Qt::DisplayRole);
	if( level > 0 && index.column() == 0 )
	{
		TextDocument doc;
		QTextCursor cur( &doc );
		QTextCharFormat f;
		f.setFont( d_titleFont );
		cur.insertText( v.toString(), f );
		doc.setTextWidth( width - s_wb - _ro(index) );
		return doc.size().toSize();
	}else // level == 0 )
	{
		if( v.canConvert<QTextDocument*>() )
		{
			QTextDocument* doc = v.value<QTextDocument*>();
			doc->setTextWidth( width - s_wb - _ro(index) );
			return doc->size().toSize();
		}else
		{
			TextDocument doc;
			QTextCursor cur( &doc );
			QTextCharFormat f;
			f.setFont( d_ctrl->view()->getCursor().getStyles()->getFont( 0 ) );
			cur.insertText( v.toString(), f );
			doc.setTextWidth( width - s_wb - _ro(index) );
			return doc.size().toSize();
		}
	}
	return QSize();
}

QWidget * DocDeleg::createEditor ( QWidget * parent, 
		const QStyleOptionViewItem &, const QModelIndex & index ) const
{
	// Hierhin kommen wir immer, wenn ein anderes Item editiert werden soll.
	// Im Falle von QAbstractItemView::AllEditTriggers kommt man sofort hierher,
	// d.h. currentChanged() wird ausgelassen.
	closeEdit();

	disconnect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	d_edit = index;
	readData();
	// option.rect ist fehlerhaft
	QRect r = view()->getItemRect( d_edit );
	// Bei Indent wird diese Funktion irrtümlich von Qt ohne gültige Breite aufgerufen.
	d_ctrl->setPos( r.left() + s_wb, r.top() ); // das muss hier stehen, damit erster Click richtig übersetzt
	// option.rect.width() enthält bereits Indent
	d_ctrl->view()->setWidth( r.width() - s_wb - _ro(index), r.height() );
	
	if( r.height() != d_ctrl->view()->getExtent().height() )
	{
		QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
		mdl->setData( d_edit, 0, Qt::SizeHintRole );
		// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.
	}

	d_ctrl->view()->invalidate();
	d_ctrl->view()->setCursorVisible( false );
	d_ctrl->view()->setBlinkingCursorEnabled( false );

	connect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );

	return 0;
}

void DocDeleg::showConsolidatedStatus( const QPoint& pos, const QModelIndex & index ) const
{
	QString html = "<html>";
	QByteArray bml = index.data( DocMdl::ConsStatRole ).toByteArray();
	if( bml.isEmpty() )
	{
		html += "<p>no consolidated status information</p>";
	}else
	{
		Stream::DataReader r( bml );
		Stream::DataReader::Token t = r.nextToken();
		int i = 0;
		while( Stream::DataReader::isUseful( t ) )
		{
			switch( t )
			{
			case Stream::DataReader::BeginFrame:
				html += "<p>";
				i = 0;
				break;
			case Stream::DataReader::EndFrame:
				html += "</p>";
				break;
			case Stream::DataReader::Slot:
				switch( i )
				{
				case 0:
					// Status
					{
						QString img;
						switch( r.readValue().getUInt8() )
						{
						case ReviewStatus_Accepted:
							img = ":/DoorScope/Images/accept.png";
							break;
						case ReviewStatus_AcceptedWithChanges:
							img = ":/DoorScope/Images/acceptwc.png";
							break;
						case ReviewStatus_Recheck:
							img = ":/DoorScope/Images/warning.png";
							break;
						case ReviewStatus_Rejected:
							img = ":/DoorScope/Images/reject.png";
							break;
						case ReviewStatus_Pending:
							img = ":/DoorScope/Images/watch.png";
							break;
						}
						html += "<img src=\"" + img + "\"/>";
					}
					i++;
					break;
				case 1:
					html += "&nbsp;<strong>";
					html += r.readValue().getStr();
					html += "</strong>&nbsp;";
					i++;
					break;
				case 2:
					html += TypeDefs::formatDate( r.readValue().getDateTime() );
					i++;
					break;
				}
				break;
			default:
				break;
			}
			t = r.nextToken();
		}
	}
	html += "</html>";
	QToolTip::showText( pos, html, view() );
}

bool DocDeleg::editorEvent(QEvent *event,
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
			d_ctrl->mouseDoubleClickEvent( (QMouseEvent*) event );
            return true; // Dito
		case QEvent::MouseButtonPress:
			{
				QMouseEvent* e = (QMouseEvent*) event;
				QRect r( option.rect.right() - s_ro, option.rect.top(), s_ro, s_ro );
				if( index.column() == 0 && e->button() == Qt::LeftButton && r.contains( e->pos() ) )
				{
					showConsolidatedStatus( e->globalPos(), index );
				}else
					d_ctrl->mousePressEvent( e );
			}
            return true; // Event wird in jedem Fall konsumiert, damit Selektion nicht verändert wird durch Tree::edit
		case QEvent::MouseButtonRelease:
			d_ctrl->mouseReleaseEvent( (QMouseEvent*) event );
            return true; // Dito
		case QEvent::MouseMove:
			d_ctrl->mouseMoveEvent( (QMouseEvent*) event );
            return true; // Dito
		case QEvent::KeyPress:
			{
				// Lasse PageUp/Down durch an Tree
				// TODO: Tree hat mit PageUp/Down ein Problem, wenn Zeile höher als Bildschirmausschnitt.
				QKeyEvent* e = (QKeyEvent*) event;
				switch( e->key() ) 
				{
				case Qt::Key_Home:
				case Qt::Key_End:
				case Qt::Key_PageDown:
				case Qt::Key_PageUp:
					if( e->modifiers() == Qt::NoModifier )
						return false;
				}
				return d_ctrl->keyPressEvent( e );
			}
			break;
		// Focus, Paint und Resize kommen hier nicht an.
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

void DocDeleg::readData() const
{
	if( !d_edit.isValid() )
		return;

	const int level = d_edit.data( DocMdl::LevelRole ).toInt();
	const QVariant v = d_edit.data(Qt::DisplayRole);

	if( level > 0 && d_edit.column() == 0 ) // Titel
	{
		d_ctrl->view()->clear();
		QTextCursor cur( d_ctrl->view()->getDocument() );
		QTextCharFormat f;
		f.setFont( d_titleFont );
		cur.insertText( v.toString(), f );
		d_ctrl->view()->setPlainText( true );
	}else // level == 0
	{
		if( v.canConvert<QTextDocument*>() )
		{
			QTextDocument* doc = v.value<QTextDocument*>();
			d_ctrl->view()->setDocument( doc );
			d_ctrl->view()->setPlainText( false );
		}else
		{
			d_ctrl->view()->clear();
			d_ctrl->view()->getCursor().insertText( v.toString() );
			d_ctrl->view()->setPlainText( true );
		}
	} 
	d_ctrl->view()->getDocument()->setModified( false );
}

void DocDeleg::invalidate( const QRectF& r )
{
	QRect rr = r.toRect();
	rr.translate( d_ctrl->getDx(), d_ctrl->getDy() );
	view()->viewport()->update( rr );
}

void DocDeleg::extentChanged()
{
	disconnect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	
	QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
	mdl->setData( d_edit, 0, Qt::SizeHintRole );
	// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.

	connect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
}

TextView* DocDeleg::getEditor(bool force) const
{
	if( !force && !d_edit.isValid() )
		return 0;
	else
		return d_ctrl->view();
}

void DocDeleg::closeEdit() const
{
	if( !d_edit.isValid() )
		return;
	d_ctrl->view()->setCursorVisible( false );
	d_ctrl->view()->setBlinkingCursorEnabled(false);
	// writeData();
	d_edit = QModelIndex();
	disconnect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	d_ctrl->view()->setDocument();
	d_ctrl->view()->invalidate();
	connect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
}

void DocDeleg::relayout()
{
	view()->doItemsLayout();
	if( d_edit.isValid() )
	{
		QRect r = view()->getItemRect( d_edit ); // enthält bereits indent; visualRect hat Fehler drin
		//d_ctrl->view()->setWidth( r.width() - s_wb - s_ro - view()->indentationForItem( d_edit ), r.height() );
		d_ctrl->view()->setWidth( r.width() - s_wb - _ro(d_edit), r.height() );
		view()->scrollTo( d_edit );
	}
}

void DocDeleg::onSectionResized ( int logicalIndex, int oldSize, int newSize )
{
	if( logicalIndex == 0 && d_oldSize != newSize ) 
		// Event wird mehrfach gefeuert mit denselben oldSize und newSize, darum hier eigener d_oldSize
	{
		QMetaObject::invokeMethod( this, "relayout", Qt::QueuedConnection );
		d_oldSize = newSize;
	}
}

bool DocDeleg::eventFilter(QObject *watched, QEvent *event)
{
	bool res = QAbstractItemDelegate::eventFilter( watched, event );
	/* ersetzt durch onSectionResized
	if( event->type() == QEvent::Resize )
	{
		QResizeEvent* e = static_cast<QResizeEvent*>( event );
		if( e->size().width() != e->oldSize().width() )
			// Bei unmittelbarer Ausführung scheint die Breite noch nicht zu stimmen.
			QMetaObject::invokeMethod( this, "relayout", Qt::QueuedConnection );	
	}
	*/
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
				if( e == QKeySequence::Copy && d_ctrl->view()->getCursor().hasSelection() )
				{
					QApplication::clipboard()->setText(
						d_ctrl->view()->getCursor().getSelection().toPlainText() );
					return true;
				}else if( e == QKeySequence::SelectAll )
				{
					d_ctrl->view()->selectAll();
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

void DocDeleg::setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const
{
	// Mit diesem Trick kann man Editor schliessen, ohne Deleg zu spezialisieren.
	// Editor muss geschlossen werden, da QTextDocument ev. gelöscht wird, während Editor
	// noch angezeigt wird.
	if( d_edit.isValid() )
	{
		closeEdit();
	}
}
