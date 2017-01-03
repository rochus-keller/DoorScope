#ifndef _Ds_DocTree
#define _Ds_DocTree

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

#include <QTreeView>

namespace Ds
{
	class DocTree : public QTreeView
	{
	public:
		DocTree( QWidget* );
		void setStepSize( quint16 );
		int stepSize() const { return d_stepSize; }
		void ensureVisibleInCurrent( int yOffset, int h ); // yOffset..Position innerhalb currentIndex 
		QRect getItemRect(const QModelIndex &index) const; // korrigierte Version von visualRect
	protected:
		int viewIndex(const QModelIndex &index) const;
		// Overrides
		void keyPressEvent(QKeyEvent *event);
		bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event);
		void drawBranches ( QPainter * painter, const QRect & rect, const QModelIndex & index ) const;
		int indexRowSizeHint(const QModelIndex &index) const;
		void mousePressEvent(QMouseEvent *event);
		void drawRow ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		void updateGeometries();
	private:
		mutable bool d_block;
		mutable int lastViewedItem;
		int d_stepSize;
		Q_DECLARE_PRIVATE(::QTreeView)
	};
}

#endif // _Ds_DocTree
