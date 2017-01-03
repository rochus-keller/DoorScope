#ifndef PROPSMDL_H
#define PROPSMDL_H

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

#include <QAbstractItemModel>
#include <Sdb/Obj.h>
#include <QList>
#include <QSet>

namespace Ds
{
	class PropsMdl : public QAbstractItemModel
	{
		Q_OBJECT

	public:
		enum IndexType { PairIndex = 1, FieldIndex = 2, ValueIndex = 3 };
		enum Role { 
			IndexTypeRole = Qt::UserRole + 1,
			TitleRole, 
			ValueRole, 
			SystemRole
		};

		PropsMdl(QObject *parent);
		~PropsMdl();

		void setObj( const Sdb::Obj&, bool autoFields = true );
		void setObj( const Sdb::Obj&, const QList<quint32>& );
		const Sdb::Obj& getObj() const { return d_obj; }
		quint32 getAtom( const QModelIndex& ) const;
		void setFields( const QList<quint32>& );
		void refill();

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
	private:
		Sdb::Obj d_obj;
		QList< QPair<quint32,int> > d_rows; // atom -> PairIndex oder FieldIndex
		QList<quint32> d_fields;
	};
}

#endif // PROPSMDL_H
