#ifndef LUAFILTERDLG_H
#define LUAFILTERDLG_H

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

#include <QDialog>

class QLineEdit;

namespace Lua
{
	class CodeEditor;
}
namespace Ds
{
	class LuaFilterDlg : public QDialog
	{
		Q_OBJECT
	public:
		explicit LuaFilterDlg(QWidget *parent);
		bool edit( QString& title, QString& code );
	protected slots:
		void handleCheck();
	protected:
		bool checkSyntax();
	private:
		QLineEdit* d_title;
		Lua::CodeEditor* d_code;
	};
}

#endif // LUAFILTERDLG_H
