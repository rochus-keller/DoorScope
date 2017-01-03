#ifndef DOCSELECTORDLG_H
#define DOCSELECTORDLG_H

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
#include <Sdb/Obj.h>

class QTreeWidget;
class QDialogButtonBox;
class QTreeWidgetItem;

namespace Ds
{
	class DocSelectorDlg : public QDialog
	{
		Q_OBJECT

	public:
		DocSelectorDlg(QWidget *parent);
		~DocSelectorDlg();

		quint64 select( Sdb::Obj root, quint64 preselectedItem = 0 );
	protected slots:
		void onCurrentItemChanged(  QTreeWidgetItem * current, QTreeWidgetItem * previous );
		void onItemDoubleClicked ( QTreeWidgetItem *, int );
	protected:
		void loadChildren( QTreeWidgetItem* i, const Sdb::Obj&, quint64 sel );
		void loadFolder( QTreeWidgetItem* i, const Sdb::Obj& );
		void loadDoc( QTreeWidgetItem* i, const Sdb::Obj& );	
		void createChild( QTreeWidgetItem* parent, const Sdb::Obj& o, quint64 sel );
	private:
		QTreeWidget* d_tree;
		QDialogButtonBox* d_bb;
	};
}

#endif // DOCSELECTORDLG_H
