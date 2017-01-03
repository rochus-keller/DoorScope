#ifndef DocManager_H
#define DocManager_H

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

#include <QObject>
#include <Sdb/Obj.h>
#include <Stream/DataReader.h>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QList>

namespace Ds
{
	class DocManager : public QObject
	{
	public:
		DocManager();
		~DocManager();

		bool deleteObj( Sdb::Obj );
		bool deleteHisto( Sdb::Obj doc );
		bool createHisto( Sdb::Obj prev, Sdb::Obj doc, const QList<quint32>& attrs );
		bool deleteAnnots( Sdb::Obj doc, bool resetReviewStatus = true );
		Sdb::Obj importStream( const QString& path ); // return: doc oder null bei fehler
		const QString& getError() const { return d_error; }
	protected:
		bool createDiff( Sdb::Obj lhs, Sdb::Obj rhs, Sdb::Obj own, const QList<quint32>& attrs );
		bool createHistoOfObj( Sdb::Obj super, Sdb::Obj doc, Sdb::Obj own, const QList<quint32>& attrs );
		bool deleteHistoOfObj( Sdb::Obj obj );
		bool deleteDoc( Sdb::Obj doc );
		bool deleteFolder( Sdb::Obj folder );
		bool readAttr( Stream::DataReader&, Sdb::Obj&, quint32 );
		bool readModAttr( Stream::DataReader&, Sdb::Obj& );
		bool readObjAttr( Stream::DataReader&, Sdb::Obj& );
		bool readPic( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
		bool readTbl( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
		bool readObj( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
		bool readStub( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
		bool readLout( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
		bool readLin( Stream::DataReader&, Sdb::Obj&, const Sdb::Obj& doc );
        Sdb::Obj splitTitleBody( Sdb::Obj& title, const Sdb::Obj &doc );
		bool readHistAttr( Stream::DataReader& in, Sdb::Obj& o );
		bool readHist( Stream::DataReader& in, Sdb::Obj& doc, Sdb::Obj& own );
		static QByteArray transformBml( const QByteArray& );
		quint32 getHistAttr( const QByteArray& ) const;
	private:
		QString d_error;
		QMap<quint32,quint64> d_nrToOid;
		QSet<quint32> d_customObjAttr;
		QSet<quint32> d_customModAttr;
		QMap<QByteArray,quint32> d_cache; // QHash::value funktioniert nicht. Er findet nichts
		QMap<QByteArray,quint8> d_cache2; // auf HistoryType
	};
}

#endif // DocManager_H
