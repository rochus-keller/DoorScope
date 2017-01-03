#ifndef __TXT_MIME_DATA
#define __TXT_MIME_DATA

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

#include <QMimeData>
#include <QTextDocumentFragment>

namespace Txt
{
	class TextMimeData : public QMimeData
	{
	public:
		TextMimeData(const QTextDocumentFragment&f, bool isHtml = false ): 
			d_fragment( f ), d_isHtml( isHtml ) {}

		virtual QStringList formats() const;
		static QString toHtml( const QTextDocumentFragment& frag );
	protected:
		virtual QVariant retrieveData(const QString &mimeType, QVariant::Type type) const;
	private:
		QTextDocumentFragment d_fragment;
		bool d_isHtml;
	};
}

#endif // __TXT_MIME_DATA
