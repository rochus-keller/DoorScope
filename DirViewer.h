#ifndef DIRVIEWER_H
#define DIRVIEWER_H

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

#include <QTreeWidget>
#include <Sdb/UpdateInfo.h>
#include <QMap>
#include <Sdb/Obj.h>

namespace Ds
{
	class DirViewer : public QTreeWidget
	{
		Q_OBJECT

	public:
		DirViewer(QWidget *parent);
		~DirViewer();
	signals:
		void sigExpandItem( const QTreeWidgetItem * );
	protected slots:
		void onDbUpdate( Sdb::UpdateInfo );
		void onItemChanged( QTreeWidgetItem * item, int column );
		void onItemExpanded( QTreeWidgetItem * item );
		void onItemCollapsed( QTreeWidgetItem * item );
		void onItemDoubleClicked( QTreeWidgetItem * item, int column );
		void onNewFolder();
		void onDeleteObject();
		void onImportDoc();
		void onOpenDoc();
		void onDumpDatabase();
		void onAbout();
		void onExpHtml();
		void onExpHtml2();
		void onExpHtml3();
		void onExpHtml4();
		void onExpHtml5();
		void onExpCsv();
		void onExpCsvUtf8();
		void onExpAnnot();
		void onExpAnnotCsv();
		void onExpAnnotCsv2();
		void onCreateHistory();
		void onSetDocFont();
		void onSetAppFont();
		void onImportAnnot();
        void onPasteHtmlDoc();
		void onSearch();
		void onReindex();
		void onDeleteAnnot();
		void adjustColumns();
		void onOpenIde();
        void onTest();
	protected:
        void importDocs( QTreeWidgetItem* parentItem, const QStringList& paths );
		void open( QTreeWidgetItem* );
	    void loadAll();
		void loadChildren( QTreeWidgetItem* i, const Sdb::Obj& );
		void loadFolder( QTreeWidgetItem* i, const Sdb::Obj& );
		void loadDoc( QTreeWidgetItem* i, const Sdb::Obj& );	
		void createChild( QTreeWidgetItem* parent, const Sdb::Obj& o );
		void expReport( bool (*report)(const Sdb::Obj& doc, const QString& path ) );
		// Overrides
		bool dropMimeData ( QTreeWidgetItem * parent, int index, const QMimeData * data, Qt::DropAction action );
		QMimeData * mimeData ( const QList<QTreeWidgetItem *> items ) const;
		QStringList mimeTypes () const;
		Qt::DropActions supportedDropActions () const;
		void dropEvent(QDropEvent *event);
        void startDrag ( Qt::DropActions supportedActions );
	private:
		QMap<quint64,QTreeWidgetItem*> d_map;
		QString d_lastPath;
		bool d_loadLock;
	};
}

#endif // DIRVIEWER_H
