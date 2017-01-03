#ifndef __Txt_TextOutStream__
#define __Txt_TextOutStream__

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

#include <Txt/TextInStream.h>
#include <Stream/DataWriter.h>

class QTextStream;

namespace Txt
{
	class Styles;

	class TextOutStream
	{
	public:
		TextOutStream() {}

		static bool hasStyle( const QTextDocument* );
		static void writeTo( Stream::DataWriter&, const QTextDocument* );
		static bool toHtml( Stream::DataReader& in, QTextStream& out, bool onlyFragment = true );
	protected:
		static void emitDocument(const QTextDocument* doc, Stream::DataWriter&);
		static void emitCode( const QTextFrame* f, Stream::DataWriter& );
		static void emitList( const QTextFrame* f, Stream::DataWriter& );
		static void emitBlock(const QTextBlock& block, Stream::DataWriter& );
		static void emitTable(const QTextTable* table, Stream::DataWriter& );
		static void emitTextFormat(const QTextCharFormat& tf, Stream::DataWriter& );
	};
}

#endif
