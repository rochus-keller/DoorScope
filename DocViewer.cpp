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

#include "DocViewer.h"
#include "DocDeleg.h"
#include "DocMdl.h"
#include "TypeDefs.h"
#include "DocTree.h"
#include "PropsMdl.h"
#include "PropsDeleg.h"
#include "LinksMdl.h"
#include "LinksDeleg.h"
#include "HistMdl.h"
#include "HistDeleg.h"
#include "DocManager.h"
#include "AnnotMdl.h"
#include "AnnotDeleg.h"
#include "AppContext.h"
#include "DocExporter.h"
#include "Indexer.h"
#include <Sdb/Transaction.h>
#include <Sdb/Database.h>
#include <Sdb/Idx.h>
#include <QTreeView>
#include <QHeaderView>
#include <QResizeEvent>
#include <QApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <QPainter>
#include <QInputDialog>
#include <QSettings>
#include <Gui2/AutoMenu.h>
#include <Gui2/AutoShortcut.h>
#include <QtDebug>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <Txt/TextInStream.h>
#include "AttrSelectDlg.h"
#include "LuaFilterDlg.h"
#include "DocSelectorDlg.h"
#include "ScriptSelectDlg.h"
#include <memory>
using namespace Ds;
using namespace Stream;

static QList<DocViewer*> s_wins;

static inline QDockWidget* createDock( QMainWindow* parent, const QString& title, bool visi )
{
	QDockWidget* dock = new QDockWidget( title, parent );
	dock->setObjectName( title );
	dock->setAllowedAreas( Qt::AllDockWidgetAreas );
	dock->setFeatures( QDockWidget::AllDockWidgetFeatures );
	dock->setFloating( false );
	dock->setVisible( visi );
	return dock;
}

DocViewer::DocViewer(const Sdb::Obj& doc, QWidget *parent)
	: QMainWindow(parent), d_lockTocSelect( false ), d_expandLinks(false),
	  d_expandHist(false),d_expandAnnot(false),d_expandProps(false),d_fullScreen(false)
{
	setAttribute( Qt::WA_DeleteOnClose );
	s_wins.append( this );

	QWidget* central = new QWidget( this );
	QVBoxLayout* vbox = new QVBoxLayout( central );
	vbox->setMargin( 0 );
	vbox->setSpacing( 2 );
	
	setupFilter( central );

	d_tree = new DocTree( central );
	vbox->addWidget( d_tree );
	d_mdl = new DocMdl( this );
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/BodyOnly" ).toBool() )
		d_mdl->setFilter( DocMdl::BodyOnly );
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/OnlyHdrTxtCh" ).toBool() )
		d_mdl->setOnlyHdrTxtChanges( true );
	d_tree->setModel( d_mdl );
	d_deleg = new DocDeleg( d_tree );
	d_tree->setItemDelegate( d_deleg );
	d_tree->setIndentation( 10 ); // Standard ist 20

	setupSearch( central );

	setCentralWidget( central );

	connect( d_tree->selectionModel(), 
		SIGNAL(currentChanged( const QModelIndex &,const QModelIndex & ) ),
		this, SLOT( onElementClicked ( const QModelIndex &,const QModelIndex & ) ) );
	connect( d_tree, 
		SIGNAL(clicked ( const QModelIndex & )  ),
		this, SLOT( onElementClicked ( const QModelIndex & ) ) );

	setDockNestingEnabled(true);
	setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );
	setupTocTree();
	setupLinks();
	setupProps();
	setupHist();
	setupAnnot();
	setupMenus();
	// setupLuaFilter();
	updateCaption();

    new Gui2::AutoShortcut( tr("CTRL+G"), this, this, SLOT( onGotoAbsNr() ) );
	new Gui2::AutoShortcut( tr("F11"), this, this, SLOT( onFullScreen() ) );
	new Gui2::AutoShortcut( tr("CTRL+SHIFT+G"), this, this, SLOT( onGotoAnnot() ) );
    new Gui2::AutoShortcut( QKeySequence::Back, this, this, SLOT( onGoBack() ) );
    new Gui2::AutoShortcut( QKeySequence::Forward, this, this, SLOT( onGoForward() ) );
	new Gui2::AutoShortcut( tr("CTRL+SHIFT+F"), this, this, SLOT( onFilter() ) );
	new QShortcut( QKeySequence::Find, this, SLOT( onFind() ) );
	new QShortcut( QKeySequence::FindNext, this, SLOT( onFindNext() ) );
	new QShortcut( QKeySequence::FindPrevious, this, SLOT( onFindPrev() ) );

	QVariant state = AppContext::inst()->getSet()->value( "DocViewer/State" );
	if( !state.isNull() )
		restoreState( state.toByteArray() );
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/ExpLinks" ).toBool() )
		d_expandLinks = true;
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/ExpHist" ).toBool() )
		d_expandHist = true;
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/ExpAnnot" ).toBool() )
		d_expandAnnot = true;
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/ExpProps" ).toBool() )
		d_expandProps = true;
	d_annot->setSorted( AppContext::inst()->getSet()->value("DocViewer/Flags/SortAnnot").toBool() );

	setDoc( doc );
}

DocViewer::~DocViewer()
{
}

void DocViewer::setupSearch( QWidget* parent )
{
	QWidget* pane = new QWidget( parent );
	parent->layout()->addWidget( pane );
	/*
	pane->setFrameShape( QFrame::StyledPanel );
	pane->setFrameShadow( QFrame::Raised );
	pane->setLineWidth( 1 );
	pane->setMidLineWidth( 0 );
	*/

	pane->setVisible( false );
	QHBoxLayout* hbox = new QHBoxLayout( pane );
	hbox->setMargin( 0 );
	hbox->setSpacing( 2 );

	QToolButton* closer = new QToolButton( pane );
	hbox->addWidget( closer );
	closer->setIcon( QIcon( ":/DoorScope/Images/close.png" ) );
	closer->setAutoRaise( true );
	connect( closer, SIGNAL( clicked() ), this, SLOT( onCloseSearch() ) );

	hbox->addWidget( new QLabel( tr("Find:"), pane ) );

	d_search = new QLineEdit( pane );
	d_search->setFixedWidth( 250 );
	hbox->addWidget( d_search );
    connect( d_search, SIGNAL(textChanged ( const QString & ) ), this, SLOT(onSearchChanged(const QString &)) );
    connect( d_search, SIGNAL(returnPressed ()), this, SLOT(onFindNext()) );

	QToolButton* next = new QToolButton( pane );
	hbox->addWidget( next );
	next->setIcon( QIcon( ":/DoorScope/Images/next.png" ) );
	next->setText( tr( "Next" ) );
	next->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	next->setAutoRaise( true );
	//next->setEnabled( false );
	connect( next, SIGNAL( clicked() ), this, SLOT( onFindNext() ) );

	QToolButton* prev = new QToolButton( pane );
	hbox->addWidget( prev );
	prev->setIcon( QIcon( ":/DoorScope/Images/prev.png" ) );
	prev->setText( tr( "Previous" ) );
	prev->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	prev->setAutoRaise( true );
	//prev->setEnabled( false );
	connect( prev, SIGNAL( clicked() ), this, SLOT( onFindPrev() ) );

	d_searchInfo1 = new QLabel( pane );
	hbox->addWidget( d_searchInfo1 );
	d_searchInfo2 = new QLabel( pane );
	hbox->addWidget( d_searchInfo2 );

	hbox->addStretch();
}

void DocViewer::setupFilter(QWidget * parent)
{
	QWidget* pane = new QWidget( parent );
	parent->layout()->addWidget( pane );

	pane->setVisible( false );
	QHBoxLayout* hbox = new QHBoxLayout( pane );
	hbox->setMargin( 0 );
	hbox->setSpacing( 2 );

	QToolButton* closer = new QToolButton( pane );
	hbox->addWidget( closer );
	closer->setIcon( QIcon( ":/DoorScope/Images/close.png" ) );
	closer->setAutoRaise( true );
	connect( closer, SIGNAL( clicked() ), this, SLOT( onCloseFilter() ) );

	hbox->addWidget( new QLabel( tr("Select Filter:"), pane ) );

	d_filters = new QComboBox( pane );
	d_filters->setMinimumWidth( 300 );
	d_filters->setMaxVisibleItems( 50 );
	connect( d_filters, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterChanged(int)) );
	hbox->addWidget( d_filters );

	QToolButton* create = new QToolButton( pane );
	hbox->addWidget( create );
	create->setIcon( QIcon( ":/DoorScope/Images/funnel_plus.png" ) );
	create->setToolTip( tr( "Create Filter\nPress CTRL to duplicate selected filter" ) );
	create->setAutoRaise( true );
	connect( create, SIGNAL( clicked() ), this, SLOT( onFilterCreate() ) );

	QToolButton* edit = new QToolButton( pane );
	hbox->addWidget( edit );
	edit->setIcon( QIcon( ":/DoorScope/Images/funnel_pencil.png" ) );
	edit->setToolTip( tr( "Edit Filter" ) );
	edit->setAutoRaise( true );
	edit->setEnabled(false);
	connect( edit, SIGNAL( clicked() ), this, SLOT( onFilterEdit() ) );
	connect(this,SIGNAL(filterEditable(bool)),edit,SLOT(setEnabled(bool)));

	QToolButton* remove = new QToolButton( pane );
	hbox->addWidget( remove );
	remove->setIcon( QIcon( ":/DoorScope/Images/funnel_minus.png" ) );
	remove->setToolTip( tr( "Delete Filter" ) );
	remove->setAutoRaise( true );
	remove->setEnabled(false);
	connect( remove, SIGNAL( clicked() ), this, SLOT( onFilterRemove() ) );
	connect(this,SIGNAL(filterEditable(bool)),remove,SLOT(setEnabled(bool)));

	QToolButton* import = new QToolButton( pane );
	hbox->addWidget( import );
	import->setIcon( QIcon( ":/DoorScope/Images/funnel_import.png" ) );
	import->setToolTip( tr( "Import Filter\nPress CTRL to import from other repository" ) );
	import->setAutoRaise( true );
	connect( import, SIGNAL( clicked() ), this, SLOT( onFilterImport() ) );

	hbox->addStretch();
}

void DocViewer::fillFilterList()
{
	d_filters->clear();
	d_filters->addItem( tr("<no filter>"), 0 );
	QMultiMap<QString,Sdb::OID> sort;
	Sdb::Obj sub = d_doc.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == TypeLuaFilter )
			sort.insertMulti( sub.getValue(AttrScriptName).getStr(), sub.getOid() );
	}while( sub.next() );
	QMultiMap<QString,Sdb::OID>::const_iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
		d_filters->addItem( i.key(), i.value() );
	d_filters->setCurrentIndex(0);
}

DocViewer* DocViewer::showDoc( const Sdb::Obj& doc, bool clone )
{
	DocViewer* v = 0;
	if( clone )
	{
		v = new DocViewer(doc);
		v->showMaximized();
	}else
	{
		for( int i = 0; i < s_wins.size(); i++ )
			if( s_wins[i]->getDoc().getId() == doc.getId() )
			{
				if( s_wins[i]->isMinimized() )
					s_wins[i]->showMaximized();
				s_wins[i]->activateWindow();
				return s_wins[i];
			}
		v = new DocViewer(doc);
		v->showMaximized();
	}
	for( int i = 0; i < s_wins.size(); i++ )
		s_wins[i]->updateCaption();
	return v;
}

void DocViewer::closeAll( const Sdb::Obj& doc )
{
	for( int i = 0; i < s_wins.size(); i++ )
		if( s_wins[i]->getDoc().getId() == doc.getId() )
		{
			s_wins[i]->close();
		}
}

class _DocTree : public QTreeView
{
	Q_DECLARE_PRIVATE(QTreeView)
public:
	_DocTree( QWidget* p ):QTreeView(p)
	{
		setEditTriggers( QAbstractItemView::NoEditTriggers );
		//setUniformRowHeights( false );
		//setWordWrap( true );
		QPalette pal = palette();
		pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
		setPalette( pal );
		setAlternatingRowColors( true );
		setIndentation( 5 );
		setRootIsDecorated( false );
		setAllColumnsShowFocus(true);
		setItemsExpandable( false );
		header()->hide();
	}
	void drawBranches( QPainter * painter, const QRect & rect, const QModelIndex & index ) const
	{
		// NOP
	}	
	void drawRow( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const 
	{
		// NOTE: zeichne einen schwarzen Rahmen, damit auch auf Beamer noch gut sichtbar.
		QTreeView::drawRow( painter, option, index );
		if( selectionModel()->isSelected( index ) ) 
		{
			QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
									  ? QPalette::Normal : QPalette::Disabled;
			painter->setPen( option.palette.color( cg, QPalette::Highlight) );
			painter->drawRect( option.rect.adjusted( 0, 0, -1, -1 ) );
		}
	}
};

void DocViewer::setupTocTree()
{
	d_toc = new DocMdl( this );
	d_toc->setFilter( DocMdl::TitleOnly );

	d_tocTree = new _DocTree( this );
	d_tocTree->setModel( d_toc );
	d_tocTree->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
	connect( d_tocTree->selectionModel(), 
		SIGNAL(currentChanged( const QModelIndex &,const QModelIndex & ) ),
		this, SLOT( onTitleClicked ( const QModelIndex &,const QModelIndex & ) ) );

	QDockWidget* dock = createDock( this, tr("Table of Contents" ), true );
	dock->setWidget( d_tocTree );
	addDockWidget( Qt::LeftDockWidgetArea, dock );
}

void DocViewer::setupProps()
{
	d_props = new PropsMdl( this );

	d_propsTree = new DocTree( this );
	d_propsTree->setItemDelegate( new PropsDeleg( d_propsTree ) );
	d_propsTree->setModel( d_props );
	d_propsTree->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
	d_propsTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
	d_propsTree->setUniformRowHeights( false );
	d_propsTree->setWordWrap( true );
	QPalette pal = palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	d_propsTree->setPalette( pal );
	d_propsTree->setAlternatingRowColors( true );
	d_propsTree->setRootIsDecorated( true );
	d_propsTree->setAllColumnsShowFocus(true);
	d_propsTree->setIndentation( 10 ); // RISK
	d_propsTree->header()->hide();

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_propsTree, true );
	pop->addCommand( tr("Annotate Attribute"), this, SLOT(onAttrAnnot()) );
	pop->addSeparator();
	pop->addCommand( tr("Show in Window"), this, SLOT( onAttrView() ) );
	pop->addCommand( tr("Add Viewer Column"), this, SLOT( onAttrAddCol() ) );
	pop->addSeparator();
	pop->addCommand( tr("Show Document Attributes" ), this, SLOT(onShowDocAttr()) );
	pop->addCommand( tr("Expand Attributes"), this, SLOT(onExpProps()) )->setCheckable(true);

	QDockWidget* dock = createDock( this, tr("Attributes" ), true );
	dock->setWidget( d_propsTree );
	addDockWidget( Qt::RightDockWidgetArea, dock );
}

void DocViewer::selectAttrs( Sdb::Obj& o )
{
	QMap<quint32,QTextEdit*>::const_iterator i;
	for( i = d_attrViews.begin(); i != d_attrViews.end(); ++i )
		setAttrView( o, i.key(), false );
}

void DocViewer::setAttrView( Sdb::Obj& o, quint32 attr, bool create )
{
	QTextEdit* e = d_attrViews.value( attr );
	if( e == 0 )
	{
		if( !create )
			return;

		QDockWidget* dock = createDock( this, TypeDefs::getPrettyName( attr, d_doc.getDb() ), true );
		e = new QTextEdit( dock );
		e->setReadOnly( true );
		dock->setWidget( e );
		d_attrViews[ attr ] = e;
		addDockWidget( Qt::RightDockWidgetArea, dock );

		Gui2::AutoMenu* pop = new Gui2::AutoMenu( e, true );
		pop->addCommand( tr("Annotate Attribute"), this, SLOT(onAttrAnnot2()) )->setData( attr );
	}
	Q_ASSERT( e != 0 );

	e->clear();
	e->setTextColor( Qt::black );
	Stream::DataCell v = o.getValue( attr );
	if( v.isBml() )
	{
		Txt::TextInStream in;
		Stream::DataReader r( v );
		Txt::TextCursor cur( e->document(), Txt::Styles::inst() );
		in.readFromTo( r, cur, false );
	}else if( v.isHtml() )
	{
		e->document()->setDefaultFont( Txt::Styles::inst()->getFont( 0 ) );
		e->setHtml( v.getStr() );
	}else if( v.isImg() )
	{
		Txt::TextCursor cur( e->document() );
		QImage img;
		v.getImage( img );
		cur.insertImg( img );
	}else if( v.isNull() )
	{
		e->setTextColor( Qt::lightGray );
		e->setPlainText( tr("<empty>") ); // RISK
	}else
		e->setPlainText( TypeDefs::prettyValue( v ).toString() );
}

void DocViewer::setupLinks()
{
	d_links = new LinksMdl( this );
	if( AppContext::inst()->getSet()->value("DocViewer/Flags/PlainTextLinks" ).toBool() )
		d_links->setPlainText( true );

	d_linksTree = new DocTree( this );
	d_linksTree->setItemDelegate( new LinksDeleg( d_linksTree ) );
	d_linksTree->setModel( d_links );
	d_linksTree->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
	d_linksTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
	d_linksTree->setUniformRowHeights( false );
	d_linksTree->setWordWrap( true );
	QPalette pal = palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	d_linksTree->setPalette( pal );
	d_linksTree->setAlternatingRowColors( true );
	d_linksTree->setRootIsDecorated( true );
	d_linksTree->setAllColumnsShowFocus(true);
	d_linksTree->setIndentation( 10 ); // RISK
	d_linksTree->header()->hide();
	connect( d_linksTree->selectionModel(), 
		SIGNAL(currentChanged( const QModelIndex &,const QModelIndex & ) ),
		this, SLOT( onLinkClicked ( const QModelIndex &,const QModelIndex & ) ) );
	connect( d_linksTree, 
		SIGNAL(clicked( const QModelIndex &) ),
		this, SLOT( onLinkClicked ( const QModelIndex & ) ) );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_linksTree, true );
	pop->addCommand( tr("Follow Link"), this, SLOT(onFollowLink()) );
	pop->addSeparator();
	pop->addCommand( tr("Annotate Link"), this, SLOT(onLinkAnnot()) );
	pop->addSeparator();
	pop->addCommand( tr("Expand Links"), this, SLOT(onExpLinks()) )->setCheckable(true);
	pop->addCommand( tr("Plain Text"), this, SLOT(onPlainBodyLinks()) )->setCheckable(true);

	QDockWidget* dock = createDock( this, tr("Links" ), true );
	dock->setWidget( d_linksTree );
	addDockWidget( Qt::RightDockWidgetArea, dock );
}

void DocViewer::setupHist()
{
	d_hist = new HistMdl( this );

	d_histTree = new DocTree( this );
	d_histTree->setItemDelegate( new HistDeleg( d_histTree ) );
	d_histTree->setModel( d_hist );
	d_histTree->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
	d_histTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
	d_histTree->setUniformRowHeights( false );
	d_histTree->setWordWrap( true );
	QPalette pal = palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	d_histTree->setPalette( pal );
	d_histTree->setAlternatingRowColors( true );
	d_histTree->setRootIsDecorated( true );
	d_histTree->setAllColumnsShowFocus(true);
	d_histTree->setIndentation( 10 ); // RISK
	d_histTree->header()->hide();

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_histTree, true );
	pop->addCommand( tr("Show Document Changes" ), this, SLOT(onShowDocAttr()) );
	pop->addSeparator();
	pop->addCommand( tr("Expand Changes"), this, SLOT(onExpHist()) )->setCheckable(true);

	QDockWidget* dock = createDock( this, tr("Change History" ), true );
	dock->setWidget( d_histTree );
	addDockWidget( Qt::RightDockWidgetArea, dock );
}

void DocViewer::setupAnnot()
{
	d_annot = new AnnotMdl( this );

	d_annotTree = new DocTree( this );
	d_annotTree->setItemDelegate( new AnnotDeleg( d_annotTree ) );
	d_annotTree->setModel( d_annot );
	d_annotTree->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
	d_annotTree->setEditTriggers( QAbstractItemView::SelectedClicked | 
		QAbstractItemView::AnyKeyPressed );
	d_annotTree->setUniformRowHeights( false );
	d_annotTree->setWordWrap( true );
	QPalette pal = palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	d_annotTree->setPalette( pal );
	d_annotTree->setAlternatingRowColors( true );
	d_annotTree->setRootIsDecorated( true );
	d_annotTree->setAllColumnsShowFocus(true);
	d_annotTree->setIndentation( 10 ); // RISK
	d_annotTree->header()->hide();

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_annotTree, true );
	pop->addCommand( tr("Urgent" ), this, SLOT(onUrgent()) )->setCheckable(true);
	pop->addCommand( tr("Need to have" ), this, SLOT(onNeedToHave()) )->setCheckable(true);
	pop->addCommand( tr("Nice to have" ), this, SLOT(onNiceToHave()) )->setCheckable(true);
	pop->addSeparator();
	pop->addCommand( tr("Goto Annotation..." ), this, SLOT(onGotoAnnot()), tr("CTRL+SHIFT+G") );
	pop->addCommand( tr("Expand Annotations"), this, SLOT(onExpAnnot()) )->setCheckable(true);
	pop->addCommand( tr("Sort Annotations"), this, SLOT(onSortAnnot()) )->setCheckable(true);
	pop->addSeparator();
	pop->addCommand( tr("Delete Annotation..." ), this, SLOT(onDeleteAnnot()) );
	pop->addSeparator();
	pop->addCommand( tr("Change Selected Reviewer..." ), this, SLOT(onAnnotUserName()) );
	pop->addCommand( tr("Set Default Reviewer..." ), this, SLOT(onDefUserName()) );
	pop->addSeparator();
	pop->addCommand( tr("Annotate Document" ), this, SLOT(onDocAnnot()) );
	pop->addCommand( tr("Show Document Annotations" ), this, SLOT(onShowDocAttr()) );
	//pop->addSeparator();
	//pop->addCommand( tr("Export HTML..." ), this, SLOT(onExpHtml()) );

	QDockWidget* dock = createDock( this, tr("Annotations" ), true );
	dock->setWidget( d_annotTree );
	addDockWidget( Qt::BottomDockWidgetArea, dock );
}

void DocViewer::createMainPop( QWidget* w, QWidget* t )
{
	Gui2::AutoMenu* pop = new Gui2::AutoMenu( w, true );
	pop->addCommand( tr("Approved" ), t, SLOT(onAccept()) )->setCheckable(true);
	pop->addCommand( tr("Approved w. Changes" ), t, SLOT(onAcceptWc()) )->setCheckable(true);
	pop->addCommand( tr("Disapproved" ), t, SLOT(onReject()) )->setCheckable(true);
	pop->addCommand( tr("Clarification pend." ), t, SLOT(onRecheck()))->setCheckable(true);
	pop->addSeparator();
	pop->addCommand( tr("Annotate Heading/Text"), t, SLOT(onTextAnnot()) );
	pop->addCommand( tr("Annotate Object"), t, SLOT(onObjAnnot()) );
	pop->addSeparator();
	pop->addCommand( tr("Expand current" ), t, SLOT(onExpandSubs()) );
	pop->addCommand( tr("Expand all" ), t, SLOT(onExpandAll()) );
	pop->addCommand( tr("Collapse all" ), t, SLOT(onCollapseAll()) );
	pop->addSeparator();
	pop->addCommand( tr("Go Back" ), t, SLOT(onGoBack()), QKeySequence::Back );
	pop->addCommand( tr("Go Forward" ), t, SLOT(onGoForward()), QKeySequence::Forward );
	pop->addCommand( tr("Goto Object..." ), t, SLOT(onGotoAbsNr()), tr("CTRL+G") );
	pop->addAction( tr("Find..." ), t, SLOT(onFind()), QKeySequence::Find );
	pop->addAction( tr("Filter..." ), t, SLOT(onFilter()), tr("CTRL+SHIFT+F") );
	pop->addSeparator();
    pop->addCommand( tr("Copy"), t, SLOT(onCopy()), tr("CTRL+C"));
    pop->addSeparator();
	pop->addCommand( tr("Add Columns..." ), t, SLOT(onAddCol()) );
	pop->addCommand( tr("Remove Columns" ), t, SLOT(onAttrColClear()) );
	pop->addCommand( tr("Show Text only"), t, SLOT(onFilterBody()) )->setCheckable(true);
	pop->addCommand( tr("Heading/Text Change Bars only"), t, SLOT(onOnlyHdrTxtChanges()) )->setCheckable(true);
	pop->addSeparator();
	QMenu* sub = createPopupMenu();
	sub->setTitle( tr("Show Window") );
	pop->addCommand( tr("Fullscreen" ), t, SLOT(onFullScreen()), tr("F11") );
	pop->addCommand( tr("New Window" ), t, SLOT(onCloneWindow()) );
	pop->addMenu( sub );
}

void DocViewer::setupMenus()
{
	/*
	Gui2::AutoMenu* m = new Gui2::AutoMenu( tr( "View" ), this, true );
	m->addCommand( tr("Adjust Layout"), this, SLOT(onLayout()) );
	*/

	createMainPop( d_tree, this );
	createMainPop( d_tocTree, this );
}

void DocViewer::onLayout()
{
	ENABLED_IF( true );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	d_tree->doItemsLayout();
	QApplication::restoreOverrideCursor();
}

void DocViewer::setDoc( const Sdb::Obj& o )
{
	QApplication::setOverrideCursor( Qt::WaitCursor );
	d_doc = o;
	d_mdl->setDoc( o );
	d_toc->setDoc( o );
	d_toc->fetchAll();
	d_tocTree->expandAll();
	d_tree->doItemsLayout();

	d_fields.clear();
	if( !o.isNull() )
	{
		d_fields.append( AttrCreatedBy );
		d_fields.append( AttrCreatedOn );
		d_fields.append( AttrModifiedBy );
		d_fields.append( AttrModifiedOn );
		d_fields.append( AttrCreatedThru );
		d_fields.append( AttrDocId );
		d_fields.append( AttrDocPath );
		d_fields.append( AttrDocVer );
		d_fields.append( AttrDocDesc );
		d_fields.append( AttrDocName );
		d_fields.append( AttrDocType );
		d_fields.append( AttrDocVerAnnot );
        d_fields.append( AttrDocVerDate );
		d_fields.append( AttrDocPrefix );
        d_fields.append( AttrDocImported );
		d_fields.append( AttrObjIdent );
		d_fields.append( AttrObjNumber );
		d_fields.append( AttrObjShort );
		d_fields.append( AttrObjDeleted ); 
        d_fields.append( AttrDocStream );
		Stream::DataReader r( o.getValue( AttrDocObjAttrs ) );
		Stream::DataReader::Token t = r.nextToken();
		while( t == Stream::DataReader::Slot )
		{
			const quint32 a = r.readValue().getAtom();
			if( a )
				d_fields.append( a );
			t = r.nextToken();
		}
		Stream::DataReader r2( o.getValue( AttrDocAttrs ) );
		t = r2.nextToken();
		while( t == Stream::DataReader::Slot )
		{
			const quint32 a = r2.readValue().getAtom();
			if( a )
				d_fields.append( a );
			t = r2.nextToken();
		}
	}
	d_props->setObj( o, d_fields );

	updateCaption();
	QApplication::restoreOverrideCursor();
}

void DocViewer::updateCaption()
{
	if( d_doc.isNull() )
		setWindowTitle( tr("DoorScope") );
	else
	{
		int n = 0;
		bool clones = false;
		for( int i = 0; i < s_wins.size(); i++ )
		{
			if( s_wins[i]->getDoc().getId() == d_doc.getId() )
			{
				n++;
				if( s_wins[i] != this )
				{
					clones = true;
					break;
				}
			}
		}
		QString nr;
		if( clones )
			nr = QString("%1:").arg(n);
		setWindowTitle( tr( "%1%2 - DoorScope" ).
			arg( nr ).
			arg( TypeDefs::formatDocName( d_doc ) ) );
	}
	if( d_mdl->columnCount() > 1 )
		d_tree->header()->show();
	else
		d_tree->header()->hide();
}

static void _expand( QTreeView* tv, DocMdl* mdl, const QModelIndex& index, bool f )
{
	// Kopie aus TreeView
	if( f )
		mdl->fetchLevel( index, true );
	const int count = mdl->rowCount( index );
	if( count == 0 )
		return;
	tv->setExpanded( index, f );
	for( int i = 0; i < count; i++ )
		_expand( tv, mdl, mdl->index( i, 0, index ), f );
}

void DocViewer::onExpandAll()
{
	ENABLED_IF( true );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	d_mdl->fetchLevel( QModelIndex(), true );
	const int count = d_mdl->rowCount();
	for( int i = 0; i < count; i++ )
		_expand( d_tree, d_mdl, d_mdl->index( i, 0 ), true );
	QApplication::restoreOverrideCursor();
}

void DocViewer::onExpandSubs()
{
	QModelIndex i = d_tree->currentIndex();
	ENABLED_IF( i.isValid() );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	_expand( d_tree, d_mdl, i, true );
	QApplication::restoreOverrideCursor();
}

void DocViewer::onCollapseAll()
{
	ENABLED_IF( true );

	const int count = d_mdl->rowCount();
	for( int i = 0; i < count; i++ )
		_expand( d_tree, d_mdl, d_mdl->index( i, 0 ), false );
}

void DocViewer::selectAnnot( Sdb::Obj& o )
{
	d_annotTree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
	d_annot->setObj( o );
	if( d_expandAnnot )
		d_annotTree->expandAll();
	else
	{
		QModelIndex i = d_annot->index( 0, 0 );
		if( i.isValid() )
		{
			d_annotTree->expand( i );
		}
	}
}

void DocViewer::onShowDocAttr()
{
	ENABLED_IF( true );

	selectDoc();
}

void DocViewer::selectDoc()
{
	if( d_mdl->getFilter() == DocMdl::BodyOnly )
		d_mdl->setDoc( d_doc );
	d_tree->setCurrentIndex( QModelIndex() );
	d_props->setObj( d_doc, d_fields );
	if( d_expandProps )
		d_propsTree->expandAll();
	d_hist->setObj( d_doc );
	d_links->setObj( Sdb::Obj() );
	selectAnnot( d_doc );
}

void DocViewer::onTitleClicked(const QModelIndex & index,const QModelIndex &)
{
	if( d_lockTocSelect )
		return;
	quint64 oid = d_toc->getOid( index );
	if( oid != 0 )
	{
		if( d_mdl->getFilter() == DocMdl::BodyOnly )
		{
			d_tree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
			Sdb::Obj o = d_doc.getTxn()->getObject( 
				d_toc->getOid( d_tocTree->currentIndex() ) );
			d_mdl->setDoc( o );
			d_mdl->fetchAll();
			QModelIndex i = d_mdl->getFirstIndex();
			if( i.isValid() )
			{
				d_tree->setCurrentIndex( i );
				d_tree->doItemsLayout();
				d_tree->expandAll();
			}else
				onElementClicked( i, i );
		}else
		{
			QModelIndex i = d_mdl->findIndex( oid );
			if( i.isValid() )
			{ 
				d_tree->expand( i );
				d_tree->setCurrentIndex( i );
				d_tree->scrollTo( i, QAbstractItemView::PositionAtTop );
			}
		}
	}
}

void DocViewer::onLinkClicked(const QModelIndex & index,const QModelIndex &)
{
	if( d_lockTocSelect ) // Das ist nötig, damit bei Follow Link nicht getriggert wird.
		return;
	quint64 oid = d_links->getOid( index );
	Sdb::Obj l = d_doc.getTxn()->getObject( oid );
	d_props->setObj( l, true );
	if( d_expandProps )
		d_propsTree->expandAll();
	d_hist->setObj( Sdb::Obj() );
	selectAnnot( l );
}

void DocViewer::onLinkClicked(const QModelIndex & index)
{
	onLinkClicked( index, index );
}

void DocViewer::onElementClicked(const QModelIndex & index,const QModelIndex &)
{
	quint64 oid = d_mdl->getOid( index );
	Sdb::Obj o = d_doc.getTxn()->getObject( oid );
	d_lockTocSelect = true;
	d_props->setObj( o, d_fields );
	if( d_expandProps )
		d_propsTree->expandAll();
	d_links->setObj( o );
	if( d_expandLinks )
		d_linksTree->expandAll();
	d_hist->setObj( o );
	if( d_expandHist )
		d_histTree->expandAll();
	selectAnnot( o );
	selectAttrs( o );
	d_tocTree->setCurrentIndex( d_toc->findIndex( oid, true ) );
	d_lockTocSelect = false;
	d_tocTree->scrollTo( d_tocTree->currentIndex() ); 
	if( d_backHisto.empty() || d_backHisto.top() != oid )
		d_backHisto.push( oid );
}

void DocViewer::onGoBack()
{
	ENABLED_IF( d_backHisto.size() > 1 );

	d_forwardHisto.push( d_backHisto.top() );
	// d_backHisto.top() zeigt auf das gerade selektierte Objekt. Darum Pop zuerst
	d_backHisto.pop();
	gotoObject( d_backHisto.top() );
}

void DocViewer::onGoForward()
{
	ENABLED_IF( d_forwardHisto.size() > 0 );

	gotoObject( d_forwardHisto.top() );
	d_forwardHisto.pop();
}

void DocViewer::onElementClicked(const QModelIndex & index )
{
	if( !d_tree->currentIndex().isValid() )
		d_tree->setCurrentIndex( index );
	else
		onElementClicked( index, index );
}

void DocViewer::onFilterBody()
{
	CHECKED_IF( true, d_mdl->getFilter() == DocMdl::BodyOnly );

	d_tree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
	if( d_mdl->getFilter() == DocMdl::BodyOnly )
	{
		d_mdl->setDoc( d_doc, DocMdl::TitleAndBody );
		AppContext::inst()->getSet()->setValue("DocViewer/Flags/BodyOnly", false );
	}else
	{
		Sdb::Obj o = d_doc.getTxn()->getObject( 
			d_toc->getOid( d_tocTree->currentIndex() ) );
		d_mdl->setDoc( o, DocMdl::BodyOnly );
		AppContext::inst()->getSet()->setValue("DocViewer/Flags/BodyOnly", true );
	}
	d_tree->doItemsLayout();
}

void DocViewer::onOnlyHdrTxtChanges()
{
	CHECKED_IF( true, d_mdl->getOnlyHdrTxtChanges() );

	d_mdl->setOnlyHdrTxtChanges( !d_mdl->getOnlyHdrTxtChanges() );
	AppContext::inst()->getSet()->setValue( "DocViewer/Flags/OnlyHdrTxtCh", 
		d_mdl->getOnlyHdrTxtChanges() );
	d_tree->viewport()->update();
}

void DocViewer::onUrgent()
{
	handlePrio( AnnPrio_Urgent );
}

void DocViewer::onNeedToHave()
{
	handlePrio( AnnPrio_NeedToHave );
}

void DocViewer::onNiceToHave()
{
	handlePrio( AnnPrio_NiceToHave );
}

void DocViewer::onAccept()
{
	handleReview( ReviewStatus_Accepted );
}

void DocViewer::onAcceptWc()
{
	handleReview( ReviewStatus_AcceptedWithChanges );
}

void DocViewer::onReject()
{
	handleReview( ReviewStatus_Rejected );
}

void DocViewer::onRecheck()
{
	handleReview( ReviewStatus_Recheck );
}

void DocViewer::handleReview( quint8 status )
{
	Sdb::Obj o = d_doc.getTxn()->getObject( d_mdl->getOid( d_tree->currentIndex() ) );
	CHECKED_IF( true, o.getValue(AttrReviewStatus).getUInt8() == status );

	if( o.getValue(AttrReviewStatus).getUInt8() == status )
	{
		if( o.getValue(AttrReviewNeeded).getBool() )
			o.setValue(AttrReviewStatus, Stream::DataCell().setUInt8(ReviewStatus_Pending) );
		else
			o.setValue(AttrReviewStatus, Stream::DataCell().setNull() );
		o.setValue( AttrReviewer, DataCell().setNull() );
		o.setValue( AttrStatusDate, DataCell().setNull() );
	}else
	{
		o.setValue(AttrReviewStatus, Stream::DataCell().setUInt8(status) );
		o.setValue( AttrReviewer, DataCell().setString( AppContext::inst()->askUserName( this ) ) );
		o.setValue( AttrStatusDate, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	}
	d_tree->update(d_tree->currentIndex());
	d_doc.getTxn()->commit();
}

void DocViewer::handlePrio( quint8 status )
{
	Sdb::Obj o = d_doc.getTxn()->getObject( d_annot->getOid( d_annotTree->currentIndex() ) );
	CHECKED_IF( !o.isNull(), !o.isNull() && o.getValue(AttrAnnPrio).getUInt8() == status );

	if( o.getValue(AttrAnnPrio).getUInt8() == status )
	{
		o.setValue(AttrAnnPrio, Stream::DataCell().setNull() );
	}else
		o.setValue(AttrAnnPrio, Stream::DataCell().setUInt8(status) );
	d_doc.getTxn()->commit();
	d_annotTree->update(d_annotTree->currentIndex());
}

Sdb::Obj DocViewer::createAnnot( Sdb::Obj& p, quint32 attr )
{
	d_annotTree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
	Sdb::Obj a = d_doc.getTxn()->createObject( TypeAnnotation );
	if( attr )
		a.setValue( AttrAnnAttr, Stream::DataCell().setAtom( attr ) );
	a.setValue( AttrAnnHomeDoc, d_doc );
	const quint32 nr = d_doc.getValue( AttrDocMaxAnnot ).getUInt32() + 1;
	a.setValue( AttrAnnNr, DataCell().setUInt32( nr ) );
	d_doc.setValue( AttrDocMaxAnnot, DataCell().setUInt32( nr ) );
	const QString user = AppContext::inst()->askUserName( this );
	a.setValue( AttrCreatedBy, DataCell().setString( user ) );
	a.setValue( AttrModifiedBy, DataCell().setString( user ) );
	DataCell d;
	d.setDateTime( QDateTime::currentDateTime() );
	a.setValue( AttrCreatedOn, d );
	a.setValue( AttrModifiedOn, d );
	a.aggregateTo( p );
	p.setValue( AttrAnnotated, DataCell().setInt32( p.getValue( AttrAnnotated ).getInt32() + 1 ) );
	if( !d_doc.getId() == p.getId() )
		d_doc.setValue( AttrAnnotated, DataCell().setInt32( d_doc.getValue( AttrAnnotated ).getInt32() + 1 ) );
	d_doc.getTxn()->commit();
	QModelIndex i = d_annot->prependAnnot( a.getId() );
	d_annotTree->setFocus();
	d_annotTree->scrollTo( i, QAbstractItemView::PositionAtTop );
	d_annotTree->setCurrentIndex( i );
	d_annotTree->edit( i );
	return a;
}

void DocViewer::onObjAnnot()
{
	QModelIndex i = d_tree->currentIndex();
	ENABLED_IF( i.isValid() );

	Sdb::Obj p = d_doc.getTxn()->getObject( d_mdl->getOid( i ) );
	Sdb::Obj a = createAnnot( p, 0 );
}

void DocViewer::onTextAnnot()
{
	QModelIndex i = d_tree->currentIndex();
	ENABLED_IF( i.isValid() );

	Sdb::Obj p = d_doc.getTxn()->getObject( d_mdl->getOid( i ) );
    createAnnot( p, ( p.getType() == TypeTitle )? quint32(AttrStubTitle): quint32(AttrObjText) );
}

void DocViewer::onAttrAnnot()
{
	QModelIndex i = d_propsTree->currentIndex();
	const quint32 t = d_props->getObj().getType();
	ENABLED_IF( i.isValid() && !d_props->getObj().isNull() && 
		( t == TypeTitle || t == TypeSection || t == TypeOutLink || 
			t == TypeInLink || t == TypeDocument ) );

	Sdb::Obj p = d_props->getObj();
	createAnnot( p, d_props->getAtom( i ) );
}

void DocViewer::onAttrAnnot2()
{
	Gui2::UiFunction* f = Gui2::UiFunction::me();
	if( f == 0 )
		return;
	const quint32 a = f->data().toUInt();
	ENABLED_IF( a );

	Sdb::Obj p = d_props->getObj();
	createAnnot( p, a );
}

void DocViewer::onAttrView()
{
	QModelIndex i = d_propsTree->currentIndex();
	const quint32 t = d_props->getObj().getType();
	ENABLED_IF( i.isValid() && !d_props->getObj().isNull() && 
		( t == TypeTitle || t == TypeSection || t == TypeOutLink || 
			t == TypeInLink || t == TypeDocument || t == TypeTable || t == TypePicture ) );

	Sdb::Obj p = d_props->getObj();
	setAttrView( p, d_props->getAtom( i ), true );
}

void DocViewer::onAttrAddCol()
{
	QModelIndex i = d_propsTree->currentIndex();
	const quint32 t = d_props->getObj().getType();
	ENABLED_IF( i.isValid() && !d_props->getObj().isNull() && 
		( t == TypeTitle || t == TypeSection || t == TypeTable || t == TypePicture ) );

	addCol( d_props->getAtom( i ) );
}

void DocViewer::onAddCol()
{
	ENABLED_IF( true );
	AttrSelectDlg dlg( this );
	AttrSelectDlg::Atoms a = d_fields;
	if( dlg.select( d_doc, a ) )
	{
		for( int i = 0; i < a.size(); i++ )
			addCol( a[i] );
	}
}

void DocViewer::addCol(quint32 attr)
{
	d_deleg->closeEdit();
	const int colCount = d_mdl->columnCount();
	QByteArray name;
	if( colCount == 1 )
	{
		d_tree->setHeaderHidden( false );
		const int w = d_tree->header()->sectionSize( 0 );
		d_tree->header()->setStretchLastSection( false );
		d_mdl->addCol( TypeDefs::getPrettyName( attr, d_doc.getDb() ), attr );
		d_tree->header()->resizeSection( 0, w - d_tree->header()->sectionSize( colCount ) );
	}else
		d_mdl->addCol( TypeDefs::getPrettyName( attr, d_doc.getDb() ), attr );
}

void DocViewer::onAttrColClear()
{
	ENABLED_IF( d_mdl->columnCount() > 1 );

	d_deleg->closeEdit();
	d_tree->setHeaderHidden( true );
	d_mdl->clearCols();
	d_tree->header()->setStretchLastSection( true );
}

void DocViewer::onLinkAnnot()
{
	QModelIndex i = d_linksTree->currentIndex();
	Sdb::Obj l = d_doc.getTxn()->getObject( d_links->getOid( i ) );
	const quint32 t = l.getType();
	ENABLED_IF( i.isValid() && !l.isNull() && 
		( t == TypeOutLink || t == TypeInLink ) );

	createAnnot( l, 0 );
}

void DocViewer::onDeleteAnnot()
{
	QModelIndex i = d_annotTree->currentIndex();
	ENABLED_IF( i.isValid() ); 

	if( i.parent().isValid() )
		i = i.parent();
	if( QMessageBox::warning( this, tr("Delete Annotation"),
		tr("Do you really want to delete '%1'? Cannot be undone." ).
		arg( i.data().toString() ), QMessageBox::Ok | QMessageBox::Cancel,
                   QMessageBox::Ok) == QMessageBox::Ok )
	{
		d_annot->deleteAnnot( i );
	}
}

void DocViewer::onDocAnnot()
{
	ENABLED_IF( true );

	selectDoc();
	createAnnot( d_doc, 0 );
}

void DocViewer::onGotoAbsNr()
{
	ENABLED_IF( true );

	bool ok;
	const QString nr = QInputDialog::getText( this, tr("Goto Object"),
		tr("Enter the ID of the Object:"), QLineEdit::Normal, QString(), &ok );
	if( !ok )
		return;
    Stream::DataCell v;
    v.setInt32( (qint32)nr.toInt( &ok ) );
    if( !ok )
        v.setString( nr );

	if( !gotoAbsNr( v ) )
		QMessageBox::information( this, tr("Goto Object"), tr("Unknown object!") );
}

bool DocViewer::gotoAbsNr( const DataCell &nr )
{
	Sdb::Idx i( d_doc.getTxn(), d_doc.getDb()->findIndex( IndexDefs::IdxDocObjId ) );
	if( i.seek( QList<Stream::DataCell>() << d_doc.getValue( AttrDocId ) << nr ) ) do
	{
		if( gotoObject( i.getId() ) )
			return true;
	}while( i.nextKey() );
	return false;
}

bool DocViewer::gotoObject( quint64 oid )
{
	Sdb::Obj o = d_doc.getTxn()->getObject( oid );
	if( o.getValue( AttrObjHomeDoc ).getOid() == d_doc.getId() && 
		o.getType() != TypeStub ) 
		// NOTE: Stubs werden über denselben Index angesprochen wie richtige Objekte
	{
		if( d_mdl->getFilter() == DocMdl::BodyOnly )
		{
			d_tree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
			d_mdl->setDoc( o.getOwner() );
			d_mdl->fetchAll();
			d_tree->doItemsLayout();
			d_tree->expandAll(); 
		}
		QModelIndex idx = d_mdl->findIndex( oid );
		if( idx.isValid() )
		{ 
			d_tree->expand( idx );
			d_tree->setCurrentIndex( idx );
			d_tree->scrollTo( idx, QAbstractItemView::PositionAtTop );
		}
		return true;
	}
	return false;
}

void DocViewer::onDefUserName()
{
	ENABLED_IF( true );

	AppContext::inst()->askUserName( this, true );
}

void DocViewer::onAnnotUserName()
{
	QModelIndex i = d_annotTree->currentIndex();
	ENABLED_IF( i.isValid() ); 

	if( i.parent().isValid() )
		i = i.parent();
	bool ok;
	QString user = QInputDialog::getText( this, tr("Reviewer Name"),
		tr("Please enter a name:"), QLineEdit::Normal,
		i.data( AnnotMdl::NameRole ).toString(), &ok );
	if( !ok )
		return;
	d_annot->setUserName( i, user );
}

void DocViewer::closeEvent ( QCloseEvent * event )
{
	int i;
	for( i = 0; i < s_wins.size(); i++ )
		if( s_wins[i] == this )
		{
			s_wins.removeAt(i);
			break;
		}
	for( i = 0; i < s_wins.size(); i++ )
		s_wins[i]->updateCaption();
	d_annotTree->itemDelegate()->setModelData( 0, 0, QModelIndex() );
	AppContext::inst()->getSet()->setValue("DocViewer/State", saveState() );
	QMainWindow::closeEvent( event );
}

void DocViewer::onExpHist()
{
	CHECKED_IF( true, d_expandHist );
	d_expandHist = !d_expandHist;
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/ExpHist", d_expandHist );
	if( d_expandHist )
		d_histTree->expandAll();
}

void DocViewer::onExpProps()
{
	CHECKED_IF( true, d_expandProps );
	d_expandProps = !d_expandProps;
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/ExpProps", d_expandProps );
	if( d_expandProps )
		d_propsTree->expandAll();
}

void DocViewer::onExpLinks()
{
	CHECKED_IF( true, d_expandLinks );
	d_expandLinks = !d_expandLinks;
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/ExpLinks", d_expandLinks );
	if( d_expandLinks )
		d_linksTree->expandAll();
}

void DocViewer::onPlainBodyLinks()
{
	CHECKED_IF( true, d_links->getPlainText() );
	d_links->setPlainText( !d_links->getPlainText() );
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/PlainTextLinks", d_links->getPlainText() );
	d_linksTree->doItemsLayout();
}

void DocViewer::onExpAnnot()
{
	CHECKED_IF( true, d_expandAnnot );
	d_expandAnnot = !d_expandAnnot;
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/ExpAnnot", d_expandAnnot );
	if( d_expandAnnot )
		d_annotTree->expandAll();
}

void DocViewer::onGotoAnnot()
{
	ENABLED_IF( true );

	bool ok;
	const int nr = QInputDialog::getInteger( this, tr("Goto Annotation"),
		tr("Enter the Number of the Annotation:"), 0, 1, 0xffffff, 1, &ok );
	if( !ok )
		return;

	Sdb::Idx i( d_doc.getTxn(), d_doc.getDb()->findIndex( IndexDefs::IdxAnnDocNr ) );
	if( i.seek( QList<Stream::DataCell>() << Stream::DataCell().setOid( d_doc.getOid() )
		<< Stream::DataCell().setUInt32( nr ) ) ) do
	{
		QModelIndex idx = d_mdl->findIndex( i.getId() );
		if( idx.isValid() )
		{ 
			d_tree->expand( idx );
			d_tree->setCurrentIndex( idx );
			d_tree->scrollTo( idx, QAbstractItemView::PositionAtTop );
			Sdb::Obj annot = d_doc.getTxn()->getObject( i.getId() );
			Q_ASSERT( !annot.isNull() );
			Sdb::Obj owner = annot.getOwner();
			Q_ASSERT( !owner.isNull() );
			if( owner.getType() == TypeOutLink || owner.getType() == TypeInLink )
			{
				QModelIndex idx3 = d_links->findIndex( owner.getId() );
				if( idx3.isValid() )
				{
					d_linksTree->setCurrentIndex( idx3 );
					d_linksTree->scrollTo( idx3, QAbstractItemView::PositionAtTop );
				}
			}
			QModelIndex idx2 = d_annot->findIndex( i.getId() );
			if( idx2.isValid() )
			{
				d_annotTree->expand( idx2 );
				d_annotTree->setCurrentIndex( idx2 );
				d_annotTree->setFocus();
				d_annotTree->scrollTo( idx2, QAbstractItemView::PositionAtTop );
			}
			return;
		}
	}while( i.nextKey() );
	QMessageBox::information( this, tr("Goto Annotation"), tr("Unknown annotation!") );
}

void DocViewer::onExpHtml()
{
	ENABLED_IF( true );

	/*
	QString path = QFileDialog::getSaveFileName( this, tr("Export Annotations"), 
		QString(), "*.html", 0 );
	if( path.isNull() )
		return;
	if( !DocExporter::saveHtml( d_doc, path ) )
		QMessageBox::critical( this, tr("Export Annotations"),
			tr("Error saving file" ) );
			*/
}

void DocViewer::onCloneWindow()
{
	ENABLED_IF( true );

	DocViewer::showDoc( d_doc, true );
}

void DocViewer::onSortAnnot()
{
	CHECKED_IF( true, d_annot->getSorted() );

	d_annot->setSorted( !d_annot->getSorted() );
	AppContext::inst()->getSet()->setValue("DocViewer/Flags/SortAnnot", d_annot->getSorted() );
	if( d_expandAnnot )
		d_annotTree->expandAll();
}

void DocViewer::onFollowLink()
{
	QModelIndex i = d_linksTree->currentIndex();
	ENABLED_IF( i.isValid() );

	if( i.parent().isValid() )
		i = i.parent();

	Sdb::Obj l = d_doc.getTxn()->getObject( d_links->getOid( i ) );
	Stream::DataCell docId;
	Stream::DataCell nr;
	QDateTime dt;
	if( l.getType() == TypeInLink )
	{
		docId = l.getValue( AttrSrcDocId );
		nr = l.getValue( AttrSrcObjId );
		dt = l.getValue( AttrSrcDocModDate ).getDateTime();
	}else if( l.getType() == TypeOutLink )
	{
		docId = l.getValue( AttrTrgDocId );
		nr = l.getValue( AttrTrgObjId );
		dt = l.getValue( AttrTrgDocModDate ).getDateTime();
	}else
		Q_ASSERT( false ); 

	if( docId.equals( d_doc.getValue( AttrDocId ) ) && 
		dt == d_doc.getValue( AttrModifiedOn ).getDateTime() )
	{
		// Dasselbe Dokument ist gemeint
		if( gotoAbsNr( nr ) ) // RISK
			return;
	}

	QMap<quint64, quint64 > docs; // es kann mehrere Dokumente mit docId geben, die nr enthalten
	Sdb::Idx idx( d_doc.getTxn(), d_doc.getDb()->findIndex( IndexDefs::IdxDocObjId ) );
	if( idx.seek( QList<Stream::DataCell>() << docId << nr ) ) do
	{
		Sdb::Obj obj = d_doc.getTxn()->getObject( idx.getId() );
		Q_ASSERT( !obj.isNull() );
		const quint64 doc = obj.getValue( AttrObjHomeDoc ).getOid();
		if( obj.getType() != TypeStub && !docs.contains( doc ) )
			docs[ doc ] = idx.getId();
	}while( idx.nextKey() );

	quint64 doc = 0;
	quint64 obj = 0;
	if( docs.size() == 1 )
	{
		doc = docs.begin().key();
		obj = docs.begin().value();
	}else
	{
		QMenu m;
		if( !docs.isEmpty() )
		{
			QMap<quint64, quint64 >::const_iterator j;
			for( j = docs.begin(); j != docs.end(); ++j )
			{
				Sdb::Obj obj = d_doc.getTxn()->getObject( j.key() );
				m.addAction( TypeDefs::formatDocName( obj ) )->setData( j.key() );
			}
		}else
			m.addAction( tr("<unknown document>") )->setEnabled( false );
		QAction* a = m.exec(QCursor::pos()); 
		if( a == 0 )
			return;
		doc = a->data().toULongLong();
		obj = docs[doc];
		Q_ASSERT( obj != 0 );
	}
	DocViewer* v = DocViewer::showDoc( d_doc.getTxn()->getObject( doc ) );
	v->gotoObject( obj );
}

void DocViewer::onFind()
{
	d_search->parentWidget()->setVisible( true );
	d_search->setFocus();
	d_search->selectAll();
	d_searchInfo1->clear();
	d_searchInfo2->clear();
	// Txt::TextView* v = d_deleg->getEditor();
	/*
	if( v && v->getCursor().hasSelection() )
	{
		d_search->setText( v->getCursor().getSelectedText() );
	}
	*/
}

void DocViewer::onCloseSearch()
{
	d_search->parentWidget()->setVisible( false );
}

void DocViewer::onSearchChanged(const QString & str)
{
	search( str, true, false );
}

void DocViewer::onFindNext()
{
	if( !d_search->parentWidget()->isVisible() )
		return;
	search( d_search->text(), true, true );
}

void DocViewer::onFindPrev()
{
	if( !d_search->parentWidget()->isVisible() )
		return;
    search( d_search->text(), false, true );
}

void DocViewer::onCopy()
{
    ENABLED_IF( d_deleg->getEditor() );
	d_deleg->getEditor()->copy();
}

void DocViewer::onFullScreen()
{
	CHECKED_IF( true, d_fullScreen );
	if( d_fullScreen )
	{
		showMaximized();
		d_fullScreen = false;
	}else
	{
		showFullScreen();
		d_fullScreen = true;
	}
}

void DocViewer::onFilter()
{
	fillFilterList();
	d_filters->parentWidget()->setVisible( true );
}

void DocViewer::onCloseFilter()
{
	d_filters->parentWidget()->setVisible( false );
	d_mdl->setLuaFilter();
}

void DocViewer::onFilterChanged(int i)
{
	if( i < 0 )
		return;
	Sdb::Obj filter = d_doc.getTxn()->getObject(d_filters->itemData( i ).toULongLong());
	if( filter.isNull() )
		d_mdl->setLuaFilter();
	else
	{
		Q_ASSERT( d_doc.getType() == TypeDocument );
		QString id = d_doc.getValue(AttrDocId).getStr();
		if( id.isEmpty() )
			id = QString::number( d_doc.getOid(), 16 );
		const QString name = tr("[%1] %2").arg(id).arg( filter.getValue( AttrScriptName ).getStr() );
		if( !d_mdl->setLuaFilter( filter.getValue( AttrScriptSource ).getStr().toLatin1(), name.toLatin1() ) )
		{
			QMessageBox::critical( this, tr("Error in Filter Function"), Lua::Engine2::getInst()->getLastError() );
		}
	}
	emit filterEditable( i > 0 );
}

void DocViewer::onFilterCreate()
{
	LuaFilterDlg dlg(this);
	dlg.setWindowTitle( tr("Create Lua Filter - DoorScope"));

	QString title;
	QString code = tr("obj, top = ...\n\nreturn true");

	if( QApplication::keyboardModifiers() == Qt::ControlModifier && d_filters->currentIndex() > 0 )
	{
		Sdb::Obj filter = d_doc.getTxn()->getObject(d_filters->itemData( d_filters->currentIndex() ).toULongLong());
		if( !filter.isNull() )
			code = filter.getValue( AttrScriptSource ).getStr();
	}
	if( dlg.edit( title, code ) )
	{
		Sdb::Obj filter = d_doc.createAggregate( TypeLuaFilter );
		filter.setValue( AttrScriptName, Stream::DataCell().setString( title ) );
		filter.setValue( AttrScriptSource, Stream::DataCell().setString( code ) );
		filter.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
		AppContext::inst()->getTxn()->commit();
		d_filters->addItem( title, filter.getOid() );
		d_filters->setCurrentIndex( d_filters->count() - 1 );
	}
}

void DocViewer::onFilterEdit()
{
	Sdb::Obj filter = d_doc.getTxn()->getObject(d_filters->itemData( d_filters->currentIndex() ).toULongLong());
	Q_ASSERT( !filter.isNull() );

	LuaFilterDlg dlg(this);
	dlg.setWindowTitle( tr("Edit Lua Filter - DoorScope"));

	QString title = filter.getValue( AttrScriptName ).getStr();
	QString code = filter.getValue( AttrScriptSource ).getStr();
	if( dlg.edit( title, code ) )
	{
		filter.setValue( AttrScriptName, Stream::DataCell().setString( title ) );
		filter.setValue( AttrScriptSource, Stream::DataCell().setString( code ) );
		filter.setValue( AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
		AppContext::inst()->getTxn()->commit();
		d_filters->setItemText( d_filters->currentIndex(), title );
		onFilterChanged( d_filters->currentIndex() );
	}
}

void DocViewer::onFilterRemove()
{
	Sdb::Obj filter = d_doc.getTxn()->getObject(d_filters->itemData( d_filters->currentIndex() ).toULongLong());
	Q_ASSERT( !filter.isNull() );

	if( QMessageBox::information( this, tr("Delete Lua Filter"),
			  tr("Do you really want to delete this filter (cannot be undone)?"),
			  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
		return;

	filter.erase();
	AppContext::inst()->getTxn()->commit();
	d_filters->removeItem( d_filters->currentIndex() );
}

void DocViewer::onFilterImport()
{
	DocSelectorDlg dlg1( this );
	dlg1.setWindowTitle( tr("Select Source Document - DoorScope" ) );
	dlg1.resize( 400, 400 );
	Sdb::Obj doc;
	std::auto_ptr<Sdb::Database> db;
	std::auto_ptr<Sdb::Transaction> txn;
	if( QApplication::keyboardModifiers() == Qt::ControlModifier )
	{
		const QString path = QFileDialog::getOpenFileName( this, tr("Select Source Repository"), QString(), tr("*.dsdb") );
		if( path.isEmpty() )
			return;
		try
		{
			db.reset( new Sdb::Database() );
			db->open( path );
			txn.reset( new Sdb::Transaction( db.get() ) );
			Sdb::Obj root = txn->getObject( QUuid( AppContext::s_rootUuid ) );
			if( root.isNull() )
			{
				QMessageBox::critical( this, tr("Import from other Repository"),
									   tr("This seems to be an invalid DoorScope repository!") );
				return;
			}
			doc = txn->getObject( dlg1.select( root ) );
		}catch( ... )
		{
			QMessageBox::critical( this, tr("Import from other Repository"), tr("Cannot import due to internal error!") );
			return;
		}
	}else
		doc = AppContext::inst()->getTxn()->getObject( dlg1.select( AppContext::inst()->getRoot() ) );
	if( doc.isNull() )
		return;
	ScriptSelectDlg dlg2( this );
	ScriptSelectDlg::Result res;
	if( dlg2.select( doc, res ) )
	{
		foreach( Sdb::OID id, res )
		{
			Sdb::Obj source = doc.getTxn()->getObject( id );
			Q_ASSERT( !source.isNull() );
			Sdb::Obj filter = d_doc.createAggregate( TypeLuaFilter );
			filter.setValue( AttrScriptName, source.getValue(AttrScriptName) );
			filter.setValue( AttrScriptSource, source.getValue(AttrScriptSource) );
			filter.setValue( AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
			AppContext::inst()->getTxn()->commit();
			d_filters->addItem( source.getValue(AttrScriptName).getStr(), filter.getOid() );
		}
	}
	doc = Sdb::Obj();
}

void DocViewer::search( const QString& pattern, bool forward, bool again )
{
	struct Waiter
	{
		Waiter() { QApplication::setOverrideCursor(QCursor(Qt::WaitCursor)); }
		~Waiter() { QApplication::restoreOverrideCursor(); }
	};
	Waiter w;

	if( !d_oldNotFound.isEmpty() && pattern.contains( d_oldNotFound ) )
		return; // stelle sicher, dass wenn ein Begriff nicht gefunden, nicht weiter Rechenzeit verschwendet wird.
	d_searchInfo1->clear();
	d_searchInfo2->clear();
	if( pattern.isEmpty() )
		return;
	Txt::TextView* v = d_deleg->getEditor();
	if( v )
	{
		// Suche zuerst im aktuellen Editor
		if( v->find( pattern, forward, true, true, !again ) )
		{
			QRectF r = v->cursorRect();
			d_tree->ensureVisibleInCurrent( r.top(), r.height() );
			d_oldNotFound.clear();
			return;
		}
	}
	Sdb::Obj cur = d_doc.getTxn()->getObject( d_mdl->getOid( d_tree->currentIndex() ) );
	bool wrap = false;
	if( forward )
		cur = Indexer::gotoNext( cur );
	else
		cur = Indexer::gotoPrev( cur );
	if( cur.isNull() )
		wrap = true;
	else
		cur = Indexer::findText( pattern, cur, forward );
	if( cur.isNull() )
	{
		if( forward )
			cur = d_doc.getFirstObj(); 
		else
			cur = Indexer::gotoLast( d_doc.getLastObj() );
		wrap = true;
		Q_UNUSED(wrap);
		cur = Indexer::findText( pattern, cur, forward );
		d_searchInfo2->setText( tr( "Passed the end of the document" ) );
		d_searchInfo1->setPixmap( QPixmap( ":/DoorScope/Images/wrap.png" ) );
	}
	if( cur.isNull() )
	{
		// nicht gefunden
		d_oldNotFound = pattern;
		d_searchInfo2->setText( tr( "The text was not found" ) );
		d_searchInfo1->setPixmap( QPixmap( ":/DoorScope/Images/notfound.png" ) );
		return;
	}
	gotoObject( cur.getId() );
	d_oldNotFound.clear();
	v = d_deleg->getEditor();
	if( v == 0 )
	{
		// Das kann vorkommen im Text-Only-Mode, wenn es keinen Inhalt gibt.
		return;
	}
	// Hier sollte es in v keine bestehende Selektion geben.
	if( forward )
		v->getCursor().gotoStart();
	else
		v->getCursor().gotoEnd();
	if( v->find( pattern, forward, true, true, !again ) )
	{
		QRectF r = v->cursorRect();
		d_tree->ensureVisibleInCurrent( r.top(), r.height() );
	}
}
