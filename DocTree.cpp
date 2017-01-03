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

#include "DocTree.h"
#include <QStack>
#include "private/qtreeview_p.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QtDebug>
using namespace Ds;

DocTree::DocTree( QWidget* parent ):
	QTreeView( parent), d_block( false ), d_stepSize(1)
{
	QPalette p = palette();
	p.setColor( QPalette::Base, QColor::fromRgb( 255, 254, 225 ) );
	p.setColor( QPalette::AlternateBase, Qt::white );
	//p.setColor( QPalette::Highlight, QColor::fromRgb( 248, 248, 248 ) );
	setPalette( p );
	setEditTriggers( AllEditTriggers );
	setUniformRowHeights( false );
	setAllColumnsShowFocus(true);
	setAlternatingRowColors( true );
	setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	setHorizontalScrollMode( QAbstractItemView::ScrollPerPixel );
	setWordWrap( true );
	setSelectionBehavior( QAbstractItemView::SelectRows );
	header()->hide();
}

void DocTree::drawBranches( QPainter * painter, 
							   const QRect & rect, const QModelIndex & index ) const
{
    Q_D(const QTreeView);
    const int indent = indentation();
	const bool expanded = isExpanded( index );
	const bool hasSubs = model()->hasChildren( index );

	if( alternatingRowColors() )
	{
		painter->fillRect( rect, palette().brush( 
			(d->current & 1)?QPalette::AlternateBase:QPalette::Base ) );
	}else
	{
		painter->fillRect( rect, palette().brush( QPalette::Base ) );
		painter->setPen( Qt::white );
		painter->drawLine( rect.bottomLeft(), rect.bottomRight() );
	}

	if( hasSubs )
	{
		QStyleOption opt;
		opt.initFrom(this);
		QStyle::State extraFlags = QStyle::State_None;
		if (isEnabled())
			extraFlags |= QStyle::State_Enabled;
		if (window()->isActiveWindow())
			extraFlags |= QStyle::State_Active;
		// +1: ansonsten gerät das Dreieck in den Selektionsrahmen
        opt.rect = QRect( rect.right() - indent + 1, rect.top() + 1, indent, indent );
        opt.state = QStyle::State_Item | extraFlags;
		if( expanded )
			style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, painter, this);
		else
			style()->drawPrimitive(QStyle::PE_IndicatorArrowRight, &opt, painter, this);
	}
}

void DocTree::drawRow( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const 
{
	QTreeView::drawRow( painter, option, index );
	if( selectionModel()->isSelected( index ) ) 
	{
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
		/* Nein, es soll künftig immer gut sichtbar sein; ansonsten auf Beamer nicht sichtbar
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;
		*/
		painter->setPen( option.palette.color( cg, QPalette::Highlight) );
		painter->drawRect( option.rect.adjusted( 0, 0, -1, -1 ) );
	}
}

bool DocTree::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
	// Im wesentlichen Kopie aus AbstractItemView, ausser dass wir hier ohne
	// Delay arbeiten und im Falle von SelectedClicked sofort editieren.
	// Zudem wird in jedem Fall der Event an den Editor weitergeleitet.

    Q_D(QTreeView);

    if (!d->isIndexValid(index))
        return false;

#if QT_VERSION >= 0x040600
    if (QWidget *w = (d->persistent.isEmpty() ? 0 : d->editorForIndex(index).editor)) {
#else
    if (QWidget *w = (d->persistent.isEmpty() ? 0 : d->editorForIndex(index))) {
#endif
        w->setFocus();
        return true;
    }

    if (d->sendDelegateEvent(index, event)) {
        d->viewport->update(visualRect(index));
        return true;
    }

	// Wir akzeptieren kein DoubleClick-Edit-Auslöser. Wenn SelectedClicked wird
	// Editor sofort angezeigt, also nicht erst via Timer verzögert.
    if (!d->shouldEdit(trigger, d->model->buddy(index)))
        return false;

    d->openEditor(index, event);
	// Dummerweise gibt AbstractView den Event an Budy oder Widget, also nicht an Delegate
    return true;
}

void DocTree::keyPressEvent(QKeyEvent *event)
{
    Q_D(QTreeView);
    switch (event->key()) 
	{
    case Qt::Key_Down:
    case Qt::Key_Up:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if( !d->sendDelegateEvent( currentIndex(), event) ) 
		{
			QTreeView::keyPressEvent( event );
			edit( currentIndex(), QAbstractItemView::SelectedClicked, event );
		}
        break;
	default:
		QTreeView::keyPressEvent( event );
		break;
    }
}

QRect DocTree::getItemRect(const QModelIndex &index) const
{
	// QTreeView::visualRect korrigiert
    Q_D(const QTreeView);

    if (!d->isIndexValid(index) || isIndexHidden(index))
        return QRect();

    d->executePostedLayout();

    int vi = d->viewIndex(index);
    if (vi < 0)
        return QRect();

    bool spanning = d->viewItems.at(vi).spanning;
	// bool firstColumnMoved = (d->header->logicalIndex(0) != 0);

    // if we have a spanning item, make the selection stretch from left to right
    int x = (spanning ? 0 : columnViewportPosition(index.column()));
    int w = (spanning ? d->header->length() : columnWidth(index.column()));
    // handle indentation
    //if (index.column() == 0 && !firstColumnMoved && !spanning) {
    if (index.column() == 0 ) {
        int i = d->indentationForItem(vi);
        x += i;
        w -= i;
    }

    int y = d->coordinateForItem(vi);
    int h = d->itemHeight(vi);

    return QRect(x, y, w, h);
}

#if QT_VERSION >= 0x040400
int DocTree::indexRowSizeHint(const QModelIndex &index) const
{
    Q_D(const QTreeView);
    if (!d->isIndexValid(index) || !d->itemDelegate)
        return 0;

    int start = -1;
    int end = -1;
    int count = d->header->count();
    bool emptyHeader = (count == 0);
    QModelIndex parent = index.parent();

    if (count && isVisible()) {
        // If the sections have moved, we end up checking too many or too few
        start = d->header->visualIndexAt(0);
    } else {
        // If the header has not been laid out yet, we use the model directly
        count = d->model->columnCount(parent);
    }

    if (isRightToLeft()) {
        start = (start == -1 ? count - 1 : start);
        end = 0;
    } else {
        start = (start == -1 ? 0 : start);
        end = count - 1;
    }

    if (end < start)
        qSwap(end, start);

    int height = -1;
    QStyleOptionViewItemV4 option = d->viewOptionsV4();

    // Hack to speed up the function
    option.rect.setWidth(-1);

    for (int column = start; column <= end; ++column) {
        int logicalColumn = emptyHeader ? column : d->header->logicalIndex(column);
        if (d->header->isSectionHidden(logicalColumn))
            continue;
        QModelIndex idx = d->model->index(index.row(), logicalColumn, parent);
        if (idx.isValid()) {
			int width = header()->sectionSize(logicalColumn);
			if( !d_block && logicalColumn == 0 )
			{
				// Block braucht es, da d->viewIndex intern seinerseits indexRowSizeHint aufruft
				//d_block = true;
				width -= d->indentationForItem( viewIndex(idx) );
				//d_block = false;
			}
			option.rect.setWidth( width );
#if QT_VERSION >= 0x040600
            QWidget *editor = d->editorForIndex(idx).editor;
#else
            QWidget *editor = d->editorForIndex(idx);
#endif
            if (editor && d->persistent.contains(editor)) {
                height = qMax(height, editor->sizeHint().height());
                int min = editor->minimumSize().height();
                int max = editor->maximumSize().height();
                height = qBound(min, height, max);
            }
            int hint = d->delegateForIndex(idx)->sizeHint(option, idx).height();
            height = qMax(height, hint);
        }
    }

    return height;
}
#else
int DocTree::indexRowSizeHint(const QModelIndex &index) const
{
    Q_D(const QTreeView);
    if (!d->isIndexValid(index) || !d->itemDelegate)
        return 0;

    int start = -1;
    int end = -1;
    int count = d->header->count();
    if (count && isVisible()) {
        // If the sections have moved, we end up checking too many or too few
        start = d->header->visualIndexAt(0);
        end = d->header->visualIndexAt(viewport()->width());
    } else {
        // If the header has not been laid out yet, we use the model directly
        count = d->model->columnCount(index.parent());
    }

    if (isRightToLeft()) {
        start = (start == -1 ? count - 1 : start);
        end = (end == -1 ? 0 : end);
    } else {
        start = (start == -1 ? 0 : start);
        end = (end == -1 ? count - 1 : end);
    }

    int tmp = start;
    start = qMin(start, end);
    end = qMax(tmp, end);

    int height = -1;
    QStyleOptionViewItemV3 option = d->viewOptionsV3();
    // ### If we want word wrapping in the items,
    // ### we need to go through all the columns
    // ### and set the width of the column

    // ### Temporary hack to speed up the function
    option.rect.setWidth(-1);
    QModelIndex parent = d->model->parent(index);
    bool emptyHeader = (d->header->count() == 0);
    for (int column = start; column <= end; ++column) {
        int logicalColumn = emptyHeader ? column : d->header->logicalIndex(column);
        if (d->header->isSectionHidden(logicalColumn))
            continue;
        QModelIndex idx = d->model->index(index.row(), logicalColumn, parent);
        if (idx.isValid()) {
			//* Start ROCHUS
			int width = header()->sectionSize(logicalColumn);
			if( !d_block && logicalColumn == 0 )
			{
				// Block braucht es, da d->viewIndex intern seinerseits indexRowSizeHint aufruft
				//d_block = true;
				width -= d->indentationForItem( viewIndex(idx) );
				//d_block = false;
			}
			option.rect.setWidth( width );
			//* End ROCHUS
            if (QWidget *editor = d->editorForIndex(idx))
                height = qMax(height, editor->size().height());
            int hint = d->delegateForIndex(idx)->sizeHint(option, idx).height();
            height = qMax(height, hint);
        }
    }
    return height;
}
#endif

int DocTree::viewIndex(const QModelIndex &index) const
{
	// TEST Kopie aus QTreeViewPrivate::viewIndex, jedoch ohne firstVisibleItem
	// Ergebnis: noch immer falsch gelayoutete Teile. Etwas langsamer als oben.
    Q_D(const QTreeView);
    if (!index.isValid() || d->viewItems.isEmpty())
        return -1;

    const int totalCount = d->viewItems.count();
    const QModelIndex parent = index.parent();

    // A quick check near the last item to see if we are just incrementing
    const int start = lastViewedItem > 2 ? lastViewedItem - 2 : 0;
    const int end = lastViewedItem < totalCount - 2 ? lastViewedItem + 2 : totalCount;
    for (int i = start; i < end; ++i) {
        const QModelIndex idx = d->viewItems.at(i).index;
        if (idx.row() == index.row()) {
            if (idx.internalId() == index.internalId() || idx.parent() == parent) {// ignore column
                lastViewedItem = i;
                return i;
            }
        }
    }

    // NOTE: this function is slow 
    for (int i = 0; i < totalCount; ++i) {
        const QModelIndex idx = d->viewItems.at(i).index;
        if (idx.row() == index.row()) {
            if (idx.internalId() == index.internalId() || idx.parent() == parent) {// ignore column
                lastViewedItem = i;
                return i;
            }
        }
    }
    // nothing found
    return -1;
}

/*
int QTreeViewPrivate::indentationForItem(int item) const
{
    if (item < 0 || item >= viewItems.count())
        return 0;
    int level = viewItems.at(item).level;
    if (rootDecoration)
        ++level;
    return level * indent;
}
*/

void DocTree::mousePressEvent(QMouseEvent *event)
{
    Q_D(QTreeView);
    // we want to handle mousePress in EditingState (persistent editors)
    if ((state() != NoState && state() != EditingState) || !d->viewport->rect().contains(event->pos())) {
        return;
    }
    const int row = d->itemAtCoordinate( event->pos().y() );
	if( row < 0 )
		return;
    const int col = d->columnAt( event->pos().x() );
	if( col < 0 )
		return;
	const int indent = (col == 0 )?d->indentationForItem( row ):0;
    QRect r( columnViewportPosition( col ) + indent, d->coordinateForItem(row),
		columnWidth( col ) - indent, d->itemHeight(row) );
	if( r.contains( event->pos() ) )
	{
		QAbstractItemView::mousePressEvent(event);
		QModelIndex i = d->modelIndex(row);
		i = i.sibling( i.row(), col );
		if( edit( i, QAbstractItemView::SelectedClicked, event ) )
			d->sendDelegateEvent( i, event);
	}else
	{
		QTreeView::mousePressEvent( event );
		//itemDelegate()->setModelData( 0, model(), i );
		//setCurrentIndex( i );
	}
}

void DocTree::updateGeometries()
{
	QTreeView::updateGeometries(); // setzt die Scrollbars
	QScrollBar* sb = verticalScrollBar();
	if( sb )
		sb->setSingleStep( d_stepSize );
}

void DocTree::setStepSize( quint16 s )
{
	d_stepSize = s;
	updateGeometries();
}

void DocTree::ensureVisibleInCurrent( int yOffset, int h )
{
    Q_D(const QTreeView);
	QScrollBar* sb = verticalScrollBar();
	if( sb == 0 )
		return;
    QRect area = d->viewport->rect();
	const int item = d->viewIndex( currentIndex() );
	const int y = d->coordinateForItem(item) + yOffset;
    const bool above = ( y < area.top() || area.height() < h );
    const bool below = ( y + h ) > area.bottom() && h < area.height();
    
	int verticalValue = sb->value();
    if ( above )
        verticalValue += y;
    else if ( below )
        verticalValue += ( y + h ) - area.height();
    verticalScrollBar()->setValue(verticalValue);
}
