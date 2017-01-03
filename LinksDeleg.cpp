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

#include "LinksDeleg.h"
#include "LinksMdl.h"
#include <QAbstractItemView>
#include <Txt/TextInStream.h>
#include <Txt/TextCursor.h>
#include <QPainter>
#include <QTextDocument>
#include <QResizeEvent>
using namespace Ds;

static const int s_wb = 15; // Abstand von Links, Iconbreite
static const int s_ro = 10; // Right offset

LinksDeleg::LinksDeleg(QAbstractItemView *parent)
	: QAbstractItemDelegate(parent)
{
	parent->installEventFilter( this ); // wegen Resize 
	connect( Txt::Styles::inst(), SIGNAL( sigFontStyleChanged() ), parent, SLOT( doItemsLayout() ) );
}

LinksDeleg::~LinksDeleg()
{

}

QAbstractItemView* LinksDeleg::view() const 
{ 
	return static_cast<QAbstractItemView*>( parent() ); 
}

void LinksDeleg::paint ( QPainter * painter, 
			const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	painter->save();

	const int kind = index.data( LinksMdl::KindRole ).toInt();
	if( kind == LinksMdl::InLinkKind || kind == LinksMdl::OutLinkKind )
	{
		QPixmap pix;
		pix.load( (kind == LinksMdl::InLinkKind)?
			":/DoorScope/Images/inlink.png":
			":/DoorScope/Images/outlink.png" );
		painter->drawPixmap( option.rect.left(), option.rect.top() + 1, pix );
		const int wLeft = option.rect.width() - s_wb - s_ro;
		painter->setPen( Qt::black );
		painter->drawText( option.rect.left() + s_wb, option.rect.top(), 
			wLeft, option.rect.height(),
			Qt::AlignLeft | Qt::AlignVCenter, 
			painter->fontMetrics().elidedText( index.data().toString(), 
			Qt::ElideRight, wLeft ) );
		if( index.data( LinksMdl::CommentRole ).toBool() )
		{
			QPixmap pix;
			pix.load( ":/DoorScope/Images/comment.png" );
			painter->drawPixmap( option.rect.right() - s_ro, 
				option.rect.top() + 1, pix );
		}
	}else if( kind == LinksMdl::StubKind )
	{
		const QPoint off = QPoint( option.rect.left(), option.rect.top() );
		painter->translate( off );
		QTextDocument doc;
		doc.setDefaultFont( Txt::Styles::inst()->getFont() );
		renderDoc( doc, index );
		doc.setTextWidth( option.rect.width() );
		doc.drawContents( painter );
		painter->translate( -off.x(), -off.y() );
	}
	painter->restore();
}

void LinksDeleg::renderDoc( QTextDocument& doc, const QModelIndex & index ) const
{
	const QVariant title = index.data( LinksMdl::TitleRole );
	const QVariant body = index.data( LinksMdl::BodyRole );
	bool hasBody = false;
	if( body.type() == QVariant::ByteArray )
	{
		Stream::DataReader r( body.toByteArray() );
		Txt::TextCursor cur( &doc );
		Txt::TextInStream in;
		if( in.readFromTo( r, cur ) )
			hasBody = true;
	}else
	{
		QString txt = body.toString();
		if( !txt.isEmpty() )
		{
			QTextCursor cur( &doc );
			cur.insertText( txt );
			hasBody = true;
		}
	}
	if( !title.isNull() )
	{
		QTextCursor cur( &doc );
		cur.movePosition( QTextCursor::Start, QTextCursor::MoveAnchor );
		cur.insertText( title.toString(), Txt::Styles::inst()->getCharFormat( 
			Txt::Styles::H4 ) );
		if( hasBody )
			cur.insertBlock();
	}
} 

QSize LinksDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.

	const int kind = index.data( LinksMdl::KindRole ).toInt();
	if( kind == LinksMdl::InLinkKind || kind == LinksMdl::OutLinkKind )
	{
		return QSize( width, option.fontMetrics.height() );
	}else if( kind == LinksMdl::StubKind )
	{
		QTextDocument doc;
		doc.setDefaultFont( Txt::Styles::inst()->getFont() );
		renderDoc( doc, index );
		doc.setTextWidth( width );
		return doc.size().toSize();
	}
	return QSize();
}

bool LinksDeleg::eventFilter(QObject *watched, QEvent *event)
{
	bool res = QAbstractItemDelegate::eventFilter( watched, event );
	if( event->type() == QEvent::Resize )
	{
		QResizeEvent* e = static_cast<QResizeEvent*>( event );
		if( e->size().width() != e->oldSize().width() )
			// Bei unmittelbarer Ausführung scheint die Breite noch nicht zu stimmen.
			QMetaObject::invokeMethod( this, "relayout", Qt::QueuedConnection );
	}
	return res;
}

void LinksDeleg::relayout()
{
	view()->doItemsLayout();
}
