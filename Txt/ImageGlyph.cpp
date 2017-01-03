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

#include "ImageGlyph.h"
#include "Styles.h"
#include <QFontMetricsF>
#include <QTextDocument>
#include <QPainter>
#include <QPicture>
#include <Stream/DataCell.h> // Picture Metatype
using namespace Txt;

ImageGlyph::ImageGlyph(QObject* p):
	QObject( p )
{

}

void ImageGlyph::regHandler(const QTextDocument* doc )
{	
	if( dynamic_cast<ImageGlyph*>(doc->documentLayout()->handlerForObject( Type )) == 0 )
		doc->documentLayout()->registerHandler( Type, 
			new ImageGlyph( const_cast<QTextDocument*>(doc) ) );
}

QSizeF ImageGlyph::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
	QTextCharFormat f = format.toCharFormat();

	/* TODO: funktioniert noch nicht
	if( !f.hasProperty( Styles::PropSize ) )
		return f.property( Styles::PropSize ).toSize();
	*/

	QVariant img = f.property( Styles::PropImage );
	QSize imgSize;
	if( img.type() == QVariant::Image )
	{
		QImage i = img.value<QImage>();
		imgSize = i.size();
	}else if( img.type() == QVariant::Pixmap )
	{
		QPixmap pix = img.value<QPixmap>();
		imgSize = pix.size();
	}else
		return imgSize;
	return imgSize;
}

void ImageGlyph::drawObject(QPainter *p, const QRectF &r, QTextDocument *doc, 
				int posInDocument, const QTextFormat &format)
{
	QTextCharFormat f = format.toCharFormat();

	QVariant img = f.property( Styles::PropImage );
	if( img.canConvert<QImage>() )
	{
		QImage i = img.value<QImage>();
		p->drawImage( r, i );
	}else if( img.canConvert<QPixmap>() )
	{
		QPixmap pix = img.value<QPixmap>();
		p->drawPixmap( r.toRect(), pix );
	}else if( img.canConvert<QPicture>() )
	{
		QPicture pic = img.value<QPicture>();
		p->drawPicture( r.topLeft(), pic );
	}
}
