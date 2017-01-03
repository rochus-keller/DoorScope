#ifndef LINKSMDL_H
#define LINKSMDL_H

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
#include <QMap>

namespace Ds
{
	class LinksMdl : public QAbstractItemModel
	{
		Q_OBJECT

	public:
		enum Kind { InLinkKind, OutLinkKind, StubKind };

		enum Role { 
			OidRole = Qt::UserRole + 1,
			KindRole, 
			TitleRole,
			BodyRole,
			BodyTextRole,
			CommentRole
		};

		LinksMdl(QObject *parent);
		~LinksMdl();

		void setObj( const Sdb::Obj& );
		void refill();
		quint64 getOid( const QModelIndex & ) const;
		QModelIndex findIndex( quint64 oid ) const;
		void setPlainText( bool on ) { d_plainText = on; }
		bool getPlainText() const { return d_plainText; }

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
	private:
		Sdb::Obj d_obj;
		struct Slot
		{
			quint64 d_oid;
			QList<Slot*> d_subs;
			Slot* d_super;
			quint8 d_kind;
			Slot():d_super(0),d_oid(0),d_kind(0){}
			~Slot();
		};
		Slot* d_root;
		QMap<quint64,Slot*> d_map;
		bool d_plainText;
	protected:
		void fetchAll();
		void fetchStub( Slot* p, const Sdb::Obj& link );
		Slot* createSlot( Slot* p, quint64 oid, quint8 kind );
	    
	};
}

#endif // LINKSMDL_H
