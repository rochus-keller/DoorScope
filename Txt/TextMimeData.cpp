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

#include "TextMimeData.h"
#include "TextOutStream.h"
#include "TextOutHtml.h"
#include <QStringList>
#include <QTextDocument>
using namespace Txt;

QStringList TextMimeData::formats() const
{
    if (!d_fragment.isEmpty())
	{
		QStringList l;
		l << "text/plain" << "text/html";
		if( !d_isHtml )
			l << TextInStream::s_mimeRichText;
		return l;
	}else
        return QMimeData::formats();
}

QString TextMimeData::toHtml( const QTextDocumentFragment& frag )
{
	QTextDocument doc;
	QTextCursor cur( &doc );
	cur.insertFragment( frag );
	TextOutHtml exp( &doc );
	return exp.toHtml();
}

QVariant TextMimeData::retrieveData(const QString &mt, QVariant::Type type) const
{
	if ( d_fragment.isEmpty() )
		return QVariant();

	if( mt == "text/plain" )
	{
		return d_fragment.toPlainText();
	}else if( mt == "text/html" )
	{
		return toHtml( d_fragment );
	}else if( mt == TextInStream::s_mimeRichText )
	{
		QTextDocument doc;
		QTextCursor cur( &doc );
		cur.insertFragment( d_fragment );

		TextOutStream s;
		Stream::DataWriter w;
		s.writeTo( w, &doc );
		return w.getStream();
	}else
		return QMimeData::retrieveData(mt, type);
	return QVariant();
}

