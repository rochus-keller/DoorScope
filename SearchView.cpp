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

#include "SearchView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QShortcut>
#include <QLabel>
#include <QSettings>
#include <QResizeEvent>
#include <QHeaderView>
#include <Gui2/AutoMenu.h>
#include <Gui2/AutoShortcut.h>
#include "Indexer.h"
#include "AppContext.h"
#include "TypeDefs.h"
#include "DocViewer.h"
#include "DocManager.h"
using namespace Ds;

static SearchView* s_inst = 0;
static const int s_title = 0;
static const int s_doc = 1;
static const int s_score = 2;

struct _SearchViewItem : public QTreeWidgetItem
{
	_SearchViewItem( QTreeWidget* w ):QTreeWidgetItem( w ) {}

	bool operator<(const QTreeWidgetItem &other) const
	{
		const int col = treeWidget()->sortColumn();
		switch( col )
		{
		case s_score:
			return text(s_score) + text(s_doc) + text(s_title) < 
				other.text(s_score) + other.text(s_doc) + other.text(s_title);
		case s_doc:
			return text(s_doc) + text(s_score) < other.text(s_doc) + other.text(s_score );
		case s_title:
			return text(s_doc) + text(s_title) < other.text(s_doc) + other.text(s_title);
		}
		return text(col) < other.text(col);
	}
};

SearchView::SearchView()
{
	setAttribute( Qt::WA_DeleteOnClose );
	s_inst = this;

	setWindowTitle( tr("DoorScope Search") );

	QVBoxLayout* vbox = new QVBoxLayout( this );
	vbox->setMargin( 3 );
	QHBoxLayout* hbox = new QHBoxLayout();
	vbox->addLayout( hbox );

	hbox->addWidget( new QLabel( tr("Enter query:"), this ) );

	d_query = new QLineEdit( this );
	connect( d_query, SIGNAL( returnPressed() ), this, SLOT( onSearch() ) );
	hbox->addWidget( d_query );

	new QShortcut( tr("CTRL+F"), this, SLOT( onNew()) );

	QPushButton* doit = new QPushButton( tr("&Search"), this );
	doit->setDefault(true);
	connect( doit, SIGNAL( clicked() ), this, SLOT( onSearch() ) );
	hbox->addWidget( doit );

	d_result = new QTreeWidget( this );
	d_result->header()->setStretchLastSection( false );
	d_result->header()->setResizeMode( s_title, QHeaderView::Stretch );
	d_result->header()->setResizeMode( s_doc, QHeaderView::ResizeToContents );
	d_result->header()->setResizeMode( s_score, QHeaderView::ResizeToContents );
	d_result->setAllColumnsShowFocus( true );
	d_result->setRootIsDecorated( false );
	d_result->setHeaderLabels( QStringList() << tr("Title") << tr("Document") << tr("Score") ); // s_title, s_doc, s_score
	d_result->setSortingEnabled(true);
	d_result->setAlternatingRowColors( true );
	connect( d_result, SIGNAL( itemActivated ( QTreeWidgetItem *, int ) ), this, SLOT( onGoto() ) );
	connect( d_result, SIGNAL( itemDoubleClicked ( QTreeWidgetItem *, int ) ), this, SLOT( onGoto() ) );
	vbox->addWidget( d_result );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_result, true );
	pop->addAction( tr("Enter new query"), this, SLOT( onNew() ), tr("CTRL+F") );
	pop->addAction( tr("Execute search"), this, SLOT( onSearch() ) );
	pop->addCommand( tr("Show document"), this, SLOT( onGotoIf() ), tr("Return") );

	QSize s = AppContext::inst()->getSet()->value("SearchView/Size" ).toSize();
	if( s.isValid() )
		resize( s);
}

SearchView::~SearchView()
{
	s_inst = 0;
}

void SearchView::resizeEvent ( QResizeEvent * e )
{
	QWidget::resizeEvent( e );
	if( e->spontaneous() )
		AppContext::inst()->getSet()->setValue("SearchView/Size", e->size() );
}

SearchView* SearchView::inst()
{
	if( s_inst == 0 )
		s_inst = new SearchView();
	return s_inst;
}

void SearchView::onSearch()
{
	Indexer::ResultList res;
	Indexer idx;
	if( !idx.query( d_query->text(), res ) )
	{
		QMessageBox::critical( this, tr("DoorScope Search"), idx.getError() );
		return;
	}
	d_result->clear();
	for( int i = 0; i < res.size(); i++ )
	{
		QTreeWidgetItem* item = new _SearchViewItem( d_result );
		QString header = Indexer::fetchText( res[i].d_title );
		const QString nr = res[i].d_title.getValue( AttrObjNumber ).toString(true);
		if( !nr.isEmpty() )
			header = nr + QChar(' ') + header;
		if( header.isEmpty() )
			header = tr("  <document top level>");
		item->setText( s_title, header );
		item->setText( s_score, QString::number( res[i].d_score, 'f', 1 ) );
		item->setText( s_doc, TypeDefs::formatDocName( res[i].d_doc, false ) );
		item->setData( s_title, Qt::UserRole, res[i].d_title.getId() );
		item->setData( s_doc, Qt::UserRole, res[i].d_doc.getId() );
	}
	//d_result->resizeColumnToContents( s_title );
	d_result->resizeColumnToContents( s_doc );
	d_result->resizeColumnToContents( s_score );
	d_result->sortByColumn( s_score, Qt::DescendingOrder );
}

void SearchView::onNew()
{
	d_query->selectAll();
	d_query->setFocus();
}

void SearchView::onGoto()
{
	QTreeWidgetItem* cur = d_result->currentItem();
	if( cur == 0 )
		return;

	Sdb::Obj doc = AppContext::inst()->getTxn()->getObject( cur->data( s_doc,Qt::UserRole).toULongLong() );
	if( doc.isNull() )
		return;

	DocViewer* v = DocViewer::showDoc( doc );
	quint64 id = cur->data(s_title, Qt::UserRole).toULongLong();
	if( id == doc.getId() )
		id = doc.getFirstObj().getId();
	v->gotoObject( id );
}

void SearchView::onGotoIf()
{
	QTreeWidgetItem* cur = d_result->currentItem();
	ENABLED_IF( cur != 0 );
	onGoto();
}
