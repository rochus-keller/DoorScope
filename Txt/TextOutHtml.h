#ifndef __Txt_HtmlExporter__
#define __Txt_HtmlExporter__

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

#include <QTextDocument>
#include <QTextFrame>

class QTextTable;

namespace Txt
{
	// Wie QTextHtmlExporter, aber ohne typographische Formatierung
	class TextOutHtml
	{
	public:
		typedef QString (*TransformAnchor)( const QString& ref );
		static TransformAnchor transformAnchor;

		TextOutHtml(const QTextDocument *_doc, bool emitP = true );

        QString toHtml(bool fragmentOnly = false, const QByteArray &charset = QByteArray());
		static QString htmlToPlainText( const QString& html, bool simlified = true );
	private:
		enum FrameType { TextFrame, TableFrame, RootFrame };

		void emitFrame(QTextFrame::Iterator frameIt);
		void emitTextFrame(const QTextFrame *frame);
		void emitBlock(const QTextBlock &block);
		void emitTable(const QTextTable *table);
		void emitFragment(const QTextFragment &fragment);
		void emitBlockAttributes(const QTextBlock &block);
		bool emitCharFormatStyle(const QTextCharFormat &format);
		void emitTextLength(const char *attribute, const QTextLength &length);
		void emitAlignment(Qt::Alignment alignment);
		void emitFloatStyle(QTextFrameFormat::Position pos );
		void emitMargins(const QString &top, const QString &bottom, const QString &left, const QString &right);
		void emitAttribute(const char *attribute, const QString &value);
		void emitFrameStyle(const QTextFrameFormat &format, FrameType frameType);
		void emitBorderStyle(QTextFrameFormat::BorderStyle style);
		void emitPageBreakPolicy(QTextFormat::PageBreakFlags policy);

		void emitFontFamily(const QString &family);

		void emitBackgroundAttribute(const QTextFormat &format);

		QString html;
		const QTextDocument *doc;
		bool d_emitP;
		bool d_firstPar;
	};
}

#endif
