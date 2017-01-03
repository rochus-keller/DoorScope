#ifndef ANNOTDELEG_H
#define ANNOTDELEG_H

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

#include <QAbstractItemDelegate>
#include <Txt/TextCtrl.h>
#include <QPersistentModelIndex>

namespace Ds
{
	class AnnotDeleg : public QAbstractItemDelegate
	{
		Q_OBJECT

	public:
		AnnotDeleg(QAbstractItemView *parent, Txt::Styles* = 0);
		~AnnotDeleg();

		void writeData() const;
		void closeEdit() const;
		void readData() const;
		bool isEditing() const { return d_edit.isValid(); }

		QAbstractItemView* view() const;

		//* Overrides von QAbstractItemDelegate
		void paint(QPainter *painter, const QStyleOptionViewItem &option, 
			const QModelIndex &index) const;
		QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		bool eventFilter(QObject *watched, QEvent *event);
		QWidget * createEditor ( QWidget * parent, const QStyleOptionViewItem & option, 
			const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model,
			const QStyleOptionViewItem &option, const QModelIndex &index);
		void setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const;
	protected slots:
		void extentChanged();
		void invalidate( const QRectF& );
		void relayout();
	private:
		Txt::TextView* d_view;
		Txt::TextCtrl* d_ctrl;
		mutable QPersistentModelIndex d_edit;
	};
}

#endif // ANNOTDELEG_H
