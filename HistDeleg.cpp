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

#include "HistDeleg.h"
#include "HistMdl.h"
#include <QAbstractItemView>
#include <Txt/TextInStream.h>
#include <Txt/TextCursor.h>
#include <QPainter>
#include <QTextDocument>
#include <QResizeEvent>
using namespace Ds;

Q_DECLARE_METATYPE( QTextDocument* ) 

static const int s_wb = 11; // Abstand von Links

HistDeleg::HistDeleg(QAbstractItemView *parent)
	: QAbstractItemDelegate(parent)
{
	parent->installEventFilter( this ); // wegen Resize 
	connect( Txt::Styles::inst(), SIGNAL( sigFontStyleChanged() ), parent, SLOT( doItemsLayout() ) );
}

HistDeleg::~HistDeleg()
{

}

QAbstractItemView* HistDeleg::view() const 
{ 
	return static_cast<QAbstractItemView*>( parent() ); 
}

void HistDeleg::paint ( QPainter * painter, 
			const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	painter->save();


	const int indexType = index.data( HistMdl::IndexTypeRole ).toInt();
	if( indexType == HistMdl::FieldIndex || indexType == HistMdl::OpIndex )
	{
		QPixmap pix;
		pix.load( (indexType == HistMdl::FieldIndex)?
			":/DoorScope/Images/stift.png":
			":/DoorScope/Images/zahnrad.png" );
		painter->drawPixmap( option.rect.left(), option.rect.top() + 1, pix );

		const QString name = index.data().toString();
		const int pos = name.indexOf( ':' );
		QFont f = painter->font();
		f.setBold( true );
		painter->setFont( f );
		QRect bound;
		if( index.data( HistMdl::SystemRole ).toBool() )
			painter->setPen( Qt::black );
		else
			painter->setPen( Qt::darkBlue );
		const QPoint off = QPoint( option.rect.left() + s_wb, option.rect.top() );
		int wLeft = option.rect.width() - s_wb;
		painter->translate( off );
		painter->drawText( 0, 0, wLeft, option.rect.height(), 
			Qt::AlignLeft | Qt::AlignVCenter, name.left( pos + 1 ), &bound );
		wLeft -= bound.width();
		f.setBold( false );
		painter->setPen( Qt::black );
		painter->setFont( f );
		painter->drawText( bound.width(), 0, wLeft, option.rect.height(),
			Qt::AlignLeft | Qt::AlignVCenter, 
			painter->fontMetrics().elidedText( name.mid( pos + 1 ), Qt::ElideRight, wLeft ) );
	}else if( indexType == HistMdl::ValueIndex )
	{
		QVariant v = index.data();
		const QPoint off = QPoint( option.rect.left(), option.rect.top() );
		painter->translate( off );
		if( v.canConvert<QTextDocument*>() )
		{
			QTextDocument* doc = v.value<QTextDocument*>();
			if( doc )
				doc->drawContents( painter );
		}
	}

	painter->restore();
}

QSize HistDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.

	const int indexType = index.data( HistMdl::IndexTypeRole ).toInt();
	if( indexType == HistMdl::FieldIndex || indexType == HistMdl::OpIndex )
		return QSize( width, option.fontMetrics.height() );
	else if( indexType == HistMdl::ValueIndex )
	{
		const QVariant v = index.data();
		if( v.canConvert<QTextDocument*>() )
		{
			QTextDocument* doc = v.value<QTextDocument*>();
			if( doc )
			{
				doc->setTextWidth( width );
				return doc->size().toSize();
			}
		}
	}
	return QSize();
}

bool HistDeleg::eventFilter(QObject *watched, QEvent *event)
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

void HistDeleg::relayout()
{
	view()->doItemsLayout();
}
