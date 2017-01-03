#ifndef ANNOTMDL_H
#define ANNOTMDL_H

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
#include <Sdb/UpdateInfo.h>
#include <QList>

namespace Ds
{
	class AnnotMdl : public QAbstractItemModel
	{
		Q_OBJECT

	public:
		enum IndexType { TitleIndex = 2, BodyIndex = 3 };
		enum Role { 
			IndexTypeRole = Qt::UserRole + 1,
			OidRole,
			AttrRole,
			BodyRole,
			NameRole,
			PrioRole,
			NrRole
		};
		
		AnnotMdl(QObject *parent);
		~AnnotMdl();

		void setObj( const Sdb::Obj& );
		void refill();
		QModelIndex prependAnnot( quint64 ); // returns Edit Index
		void deleteAnnot( const QModelIndex& );
		void setUserName( const QModelIndex&, const QString& name );
		quint64 getOid( const QModelIndex & ) const;
		QModelIndex findIndex( quint64 oid ) const;
		bool getSorted() const { return d_sorted; }
		void setSorted( bool );

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		bool setData ( const QModelIndex & index, const QVariant & value, int role );
	protected slots:
		void onDbUpdate( Sdb::UpdateInfo );
	private:
		void clearAll();
		int find( quint64 ) const; // row oder -1
		Sdb::Obj d_obj;
		struct Slot
		{
			Sdb::Obj d_obj;
			Slot(){}
			Slot( const Sdb::Obj& o ):d_obj(o){}
		};
		QList<Slot*> d_rows;
		bool d_sorted;
	};
}

#endif // ANNOTMDL_H
