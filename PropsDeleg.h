#ifndef PROPSDELEG_H
#define PROPSDELEG_H

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

namespace Ds
{
	class PropsDeleg : public QAbstractItemDelegate
	{
		Q_OBJECT

	public:
		PropsDeleg(QAbstractItemView *parent);
		~PropsDeleg();

		QAbstractItemView* view() const;

		//* Overrides von QAbstractItemDelegate
		void paint(QPainter *painter, const QStyleOptionViewItem &option, 
			const QModelIndex &index) const;
		QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		bool eventFilter(QObject *watched, QEvent *event);
	protected slots:
		void relayout();
	};
}

#endif // PROPSDELEG_H
