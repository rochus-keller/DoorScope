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

#include "PropsDeleg.h"
#include "PropsMdl.h"
#include <QAbstractItemView>
#include <Txt/TextInStream.h>
#include <Txt/TextCursor.h>
#include <QPainter>
#include <QTextDocument>
#include <QResizeEvent>
using namespace Ds;

static const int s_wb = 0; // Abstand von Links

PropsDeleg::PropsDeleg(QAbstractItemView *parent)
	: QAbstractItemDelegate(parent)
{
	parent->installEventFilter( this ); // wegen Resize 
	connect( Txt::Styles::inst(), SIGNAL( sigFontStyleChanged() ), parent, SLOT( doItemsLayout() ) );
}

PropsDeleg::~PropsDeleg()
{

}

QAbstractItemView* PropsDeleg::view() const 
{ 
	return static_cast<QAbstractItemView*>( parent() ); 
}

void PropsDeleg::paint ( QPainter * painter, 
			const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	painter->save();

	const QPoint off = QPoint( option.rect.left() + s_wb, option.rect.top() );
	int wLeft = option.rect.width() - s_wb;
	painter->translate( off );

	const int indexType = index.data( PropsMdl::IndexTypeRole ).toInt();
	if( indexType == PropsMdl::PairIndex || indexType == PropsMdl::FieldIndex )
	{
		const QString name = index.data( PropsMdl::TitleRole ).toString() + ":  ";
		QFont f = painter->font();
		f.setBold( true );
		painter->setFont( f );
		if( index.data( PropsMdl::SystemRole ).toBool() )
			painter->setPen( Qt::black );
		else
			painter->setPen( Qt::darkBlue );
		QRect bound;
		painter->drawText( 0, 0, wLeft, option.rect.height(), 
			Qt::AlignLeft | Qt::AlignVCenter, name, &bound );
		wLeft -= bound.width();
		f.setBold( false );
		painter->setFont( f );
		painter->setPen( Qt::black );
		if( indexType == PropsMdl::PairIndex )
		{
			const QString value = index.data( PropsMdl::ValueRole ).toString();
			painter->drawText( bound.width(), 0, wLeft, option.rect.height(),
				Qt::AlignLeft | Qt::AlignVCenter, 
				painter->fontMetrics().elidedText( value, Qt::ElideRight, wLeft ) );
		}else
			painter->drawText( bound.width(), 0, wLeft, option.rect.height(),
				Qt::AlignLeft | Qt::AlignVCenter, "..." );
	}else if( indexType == PropsMdl::ValueIndex )
	{
		painter->setPen( Qt::black );
		QVariant v = index.data( PropsMdl::ValueRole );
		if( v.type() == QVariant::ByteArray )
		{
			Stream::DataReader r( v.toByteArray() );
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc );
			Txt::TextInStream in;
			if( in.readFromTo( r, cur ) )
			{
				doc.setTextWidth( wLeft );
				doc.drawContents( painter );
			}
		}else
		{
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc );
			cur.insertText( v.toString() );
			doc.setTextWidth( wLeft );
			doc.drawContents( painter );
			//painter->drawText( 0, 0, wLeft, option.rect.height(),
			//	Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, v.toString() );
		}
	}


/*
	const bool changed = index.data( DocMdl::ChangedRole ).toBool();
	painter->fillRect( option.rect.left(), option.rect.top() + 3, 
		s_wb, option.rect.height() - 6, (changed)? Qt::red : Qt::green );
*/
	painter->translate( -off.x(), -off.y() );
	painter->restore();
}

QSize PropsDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.

	const int indexType = index.data( PropsMdl::IndexTypeRole ).toInt();
	if( indexType == PropsMdl::PairIndex || indexType == PropsMdl::FieldIndex )
		return QSize( width, option.fontMetrics.height() );
	else
	{
		QVariant v = index.data( PropsMdl::ValueRole );
		if( v.type() == QVariant::ByteArray )
		{
			Stream::DataReader r( v.toByteArray() );
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc );
			Txt::TextInStream in;
			if( in.readFromTo( r, cur ) )
			{
				doc.setTextWidth( width - s_wb );
				return doc.size().toSize();
			}
		}else
		{
			QTextDocument doc;
			doc.setDefaultFont( Txt::Styles::inst()->getFont() );
			Txt::TextCursor cur( &doc );
			cur.insertText( v.toString() );
			doc.setTextWidth( width );
			return doc.size().toSize();
			//return option.fontMetrics.boundingRect( 0, 0, width - s_wb, option.rect.height(),
			//	Qt::TextWordWrap, v.toString() ).size();
		}
	}

	return QSize();
}

bool PropsDeleg::eventFilter(QObject *watched, QEvent *event)
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

void PropsDeleg::relayout()
{
	view()->doItemsLayout();
}
