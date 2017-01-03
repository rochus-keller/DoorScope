#ifndef DOCVIEWER_H
#define DOCVIEWER_H

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
#include <Sdb/Obj.h>
#include <QList>
#include <QStack>

class QTreeView;
class QModelIndex;
class QLineEdit;
class QLabel;
class QTextEdit;
class QComboBox;

namespace Ds
{
	class DocMdl;
	class DocDeleg;
	class DocTree;
	class PropsMdl;
	class LinksMdl;
	class HistMdl;
	class AnnotMdl;
	class LuaFilterDlg;

	class DocViewer : public QMainWindow
	{
		Q_OBJECT
	public:
		~DocViewer();

		static DocViewer* showDoc( const Sdb::Obj& doc, bool clone = false );
		static void closeAll( const Sdb::Obj& doc );
		const Sdb::Obj& getDoc() const { return d_doc; }
		bool gotoObject( quint64 oid );
		bool gotoAbsNr( const Stream::DataCell& nr );
	signals:
		void filterEditable(bool);
	protected slots:
		void onLayout();
		void onExpandAll();
		void onCollapseAll();
		void onExpandSubs();
		void onTitleClicked(const QModelIndex &,const QModelIndex &);
		void onElementClicked(const QModelIndex &,const QModelIndex &);
		void onElementClicked(const QModelIndex & );
		void onLinkClicked(const QModelIndex &,const QModelIndex &);
		void onLinkClicked(const QModelIndex &);
		void onFilterBody();
		void onShowDocAttr();
		void onOnlyHdrTxtChanges();
		void onAccept();
		void onAcceptWc();
		void onReject();
		void onRecheck();
		void onObjAnnot();
		void onTextAnnot();
		void onAttrAnnot();
		void onAttrAnnot2();
		void onAttrView();
		void onAttrAddCol();
		void onAttrColClear();
		void onAddCol();
		void onLinkAnnot();
		void onDeleteAnnot();
		void onDocAnnot();
		void onGotoAbsNr();
		void onDefUserName();
		void onAnnotUserName();
		void onUrgent();
		void onNeedToHave();
		void onNiceToHave();
		void onExpHist();
		void onExpProps();
		void onExpLinks();
		void onPlainBodyLinks();
		void onExpAnnot();
		void onGotoAnnot();
		void onExpHtml();
		void onCloneWindow();
		void onFollowLink();
		void onGoBack();
		void onGoForward();
		void onSortAnnot();
		void onFind();
		void onSearchChanged(const QString &);
		void onCloseSearch();
		void onFindNext();
		void onFindPrev();
        void onCopy();
		void onFullScreen();
		void onFilter();
		void onCloseFilter();
		void onFilterChanged(int);
		void onFilterCreate();
		void onFilterEdit();
		void onFilterRemove();
		void onFilterImport();
	protected:
		DocViewer(const Sdb::Obj& doc, QWidget *parent = 0);
		void setDoc( const Sdb::Obj& );
		void updateCaption();
		void setupMenus();
		void setupTocTree();
		void setupProps();
		void setupLinks();
		void setupHist();
		void setupAnnot();
		void setupSearch( QWidget* );
		void setupFilter( QWidget* );
		void fillFilterList();
		void handleReview( quint8 status );
		void handlePrio( quint8 status );
		Sdb::Obj createAnnot( Sdb::Obj& p, quint32 attr );
		void selectAnnot( Sdb::Obj& o );
		void setAttrView( Sdb::Obj& o, quint32 attr, bool create );
		void selectAttrs( Sdb::Obj& o );
		void selectDoc();
		void createMainPop( QWidget* w, QWidget* t );
		void search( const QString& pattern, bool forward, bool again );
		void addCol( quint32 );
		// Overrides
		void closeEvent ( QCloseEvent * event );
	private:
	    DocMdl* d_mdl;
		DocTree* d_tree;
		DocDeleg* d_deleg;
		QTreeView* d_tocTree;
	    DocMdl* d_toc;
		PropsMdl* d_props;
		QTreeView* d_propsTree;
		LinksMdl* d_links;
		QTreeView* d_linksTree;
		HistMdl* d_hist;
		QTreeView* d_histTree;
		AnnotMdl* d_annot;
		QTreeView* d_annotTree;
		QLineEdit* d_search;
		QLabel* d_searchInfo1;
		QLabel* d_searchInfo2;
		QComboBox* d_filters;
		QString d_oldNotFound;
		QMap<quint32,QTextEdit*> d_attrViews;
		Sdb::Obj d_doc;
		QList<quint32> d_fields;
		QStack<quint64> d_backHisto;
		QStack<quint64> d_forwardHisto;
		bool d_lockTocSelect;
		bool d_expandLinks;
		bool d_expandHist;
		bool d_expandAnnot;
		bool d_expandProps;
		bool d_fullScreen;
	};
}

#endif // DOCVIEWER_H
