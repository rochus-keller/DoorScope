#ifndef DOCMDL_H
#define DOCMDL_H

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
#include <QTextDocument>
#include <QMap>

namespace Ds
{
    class TextDocument : public QTextDocument
    {
    public:
        TextDocument( QObject * parent = 0 ):QTextDocument(parent) {}
    protected:
        QVariant loadResource ( int type, const QUrl & name );
    };

	class DocMdl : public QAbstractItemModel
	{
		Q_OBJECT
	public:
		enum Filter { TitleAndBody, TitleOnly, BodyOnly };

		enum Role { 
			OidRole = Qt::UserRole + 1,
			LevelRole, // 0..Body, 1..255 Header
			ChangedRole, // bool
			StatusRole,
			CommentRole,
			ConsStatRole
		};

		DocMdl(QObject *parent);
		~DocMdl();

		void setDoc( const Sdb::Obj& doc );
		void setDoc( const Sdb::Obj& doc, Filter f );
		void setFilter( Filter );
		Filter getFilter() const { return d_filter; }
		bool setLuaFilter( const QByteArray &code = QByteArray(), const QByteArray &name = QByteArray());
		bool hasLuaFilter() const;
		void addCol( const QString&,quint32 );
		void clearCols();
		const Sdb::Obj& getDoc() const { return d_doc; }
		void refill();
		void fetchLevel( const QModelIndex & parent, bool all = false );
		void fetchAll();
		quint64 getOid( const QModelIndex & ) const;
		QModelIndex findIndex( quint64 oid, bool title = false );
		QModelIndex findInLevel( const QModelIndex & parent, quint64 oid );
		QModelIndex getFirstIndex() const;
		bool getOnlyHdrTxtChanges() const { return d_onlyHdrTxtChanges; }
		void setOnlyHdrTxtChanges( bool b ) { d_onlyHdrTxtChanges = b; }

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1 + d_cols.size(); }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		bool canFetchMore ( const QModelIndex & parent ) const;
		void fetchMore ( const QModelIndex & parent );
		bool hasChildren( const QModelIndex & parent = QModelIndex() ) const;	
		QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
		bool setData ( const QModelIndex & index, const QVariant & value, int role );
	protected slots:
		void onDbUpdate( Sdb::UpdateInfo );
	private:
		Sdb::Obj d_doc;
		struct Slot
		{
			QTextDocument* d_text;
			quint64 d_oid;
			QList<Slot*> d_subs;
			Slot* d_super;
			quint8 d_level;
			bool d_empty;
			Slot():d_super(0),d_text(0),d_oid(0),d_level(0),d_empty(false){}
			~Slot();
		};
		Slot* d_root;
		QList< QPair<QString,quint32> > d_cols;
		QMap<quint64,Slot*> d_map;
		Filter d_filter;
		int d_luaFilter;
		QByteArray d_luaFilterName;
		bool d_onlyHdrTxtChanges;
	protected:
		int fetch( Slot*, bool all = false );
		bool callLuaFilter( const Sdb::Obj& ) const;
		Slot* createSlot( Slot* p, quint64 oid, quint32 type );
		void fetchTable( Slot* s, const Sdb::Obj& o );
		void fetchText( Slot* s, const Sdb::Obj& o );
		void fetchPic( Slot* s, const Sdb::Obj& o );
	};
}
Q_DECLARE_METATYPE( QTextDocument* ) 

#endif // DOCMDL_H
