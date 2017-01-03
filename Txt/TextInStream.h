#ifndef __Txt_TextInStream__
#define __Txt_TextInStream__

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

#include <Stream/DataReader.h>
#include <Txt/TextCursor.h>

namespace Txt
{
	class Styles;

	class TextInStream
	{
	public:
		static const char* s_mimeRichText;

		/* Format (names as Tags):

			Frame 'rtxt'
				Slot 'ver' = "0.1" // Ascii
				Frame 'par' or 'lst' or 'code'

				Falls par or lst, darunter
					Frame 'frag' or 'img' or 'anch'

			frag:
				[ Slot <format bitset> ]	// UInt8, Format wird mit jedem frag zurückgesetzt
				Slot <text>					// String, Text kann \n und \r enthalten
			lst:
				Slot 'il' = 0..255 <indentation level>	// UInt8
				Slot 'ls' = <list style>				// UInt8
			img:
				[ Slot 'w' = UInt16 ]
				[ Slot 'h' = UInt16 ]
				Slot <png>
			anch:
				Slot 'url'					// TypeUrl (vorher encoded String)
				[ Slot 'text' ]				// String, dargestellter String
			code:
				Slot <text>					// String, kann \n und \r enthalten

		*/
		enum ListStyle { NoList = 0, Disc, Circle, Square, Decimal, LowAlpha, UpAlpha };
		enum CharFormat { Italic, Bold, Underline, Strikeout, Super, Sub, Fixed }; // Index in bitset
		
		TextInStream( const Styles* = 0 );

		QTextDocument* readFrom( Stream::DataReader&, QObject* owner = 0 );
		bool readFromTo( Stream::DataReader& in, TextCursor& cur, bool insert = false );
        bool readFromTo( const Stream::DataCell& bml, QTextDocument* doc, bool insert = false );
		const QString& getError() const { return d_error; }
	private:
		const Styles* d_styles;
		QString d_error;
	};
}

#endif //__Txt_TextInStream__
