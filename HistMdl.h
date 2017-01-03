#ifndef HISTMDL_H
#define HISTMDL_H

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

class QTextDocument;
class QTextCursor;

namespace Ds
{
	class HistMdl : public QAbstractItemModel
	{
		Q_OBJECT

	public:
		enum IndexType { OpIndex = 1, FieldIndex = 2, ValueIndex = 3 };
		enum Role { 
			IndexTypeRole = Qt::UserRole + 1,
			FirstRole, 
			LastRole, 
			SystemRole,
		};
		HistMdl(QObject *parent);
		~HistMdl();

		void setObj( const Sdb::Obj& );
		void refill();
        static void printDiff( QTextCursor&, const QString& newText, const QString& oldText );

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
	protected:
		void calcDiff( int row ) const;
	private:
		Sdb::Obj d_obj;
		struct Slot
		{
			quint64 d_first;
			quint64 d_last;
			QTextDocument* d_doc;
			quint8 d_type;
			Slot():d_first(0),d_last(0),d_doc(0),d_type(0){}
			Slot(quint64 id, quint8 t):d_first(id),d_last(id),d_doc(0),d_type(t){}
		};
		mutable QList<Slot> d_rows; // First und Last Change per Attribute
	    
	};
}

#endif // HISTMDL_H
