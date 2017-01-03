#ifndef MAINFRAME_H
#define MAINFRAME_H

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

#include <QMainWindow>

namespace Ds
{
	class DirViewer;

	class MainFrame : public QMainWindow
	{
		Q_OBJECT

	public:
		MainFrame(QWidget *parent = 0);
		~MainFrame();

	protected slots:
		void openDatabase();
		void newDatabase();
	protected:
		void setCaption();
		// Overrides
		void closeEvent ( QCloseEvent * event );
		void resizeEvent ( QResizeEvent * event );
	private:
		DirViewer* d_dir;
	};
}

#endif // MAINFRAME_H
