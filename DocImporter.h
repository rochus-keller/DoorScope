#ifndef DOCIMPORTER_H
#define DOCIMPORTER_H

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
#include <QMap>
#include <QList>
#include <Sdb/Obj.h>
#include <Stream/DataReader.h>

namespace Ds
{
	class DocImporter : public QObject
	{
	public:
		DocImporter(QObject *parent = 0);
		~DocImporter();

		Sdb::Obj importHtmlFile( const QString& path ); // return: doc oder null bei fehler
        Sdb::Obj importHtmlString( const QString& html ); // return: doc oder null bei fehler
		bool checkCompatible( Sdb::Obj& doc, const QString& path );
		bool readAnnot( Sdb::Obj& doc, const QString& path, bool overwriteReviewStatus = false ); 
		const QString& getError() const { return d_error; }
		const QString& getInfo() const { return d_info; }
	protected:
		void readStat( Stream::DataReader&, Sdb::Obj& doc, bool overwriteReviewStatus );
		void readAnnot( Stream::DataReader&, Sdb::Obj& doc );
	private:
		QString d_error;
		QString d_info;
		QMap<quint32,quint64> d_nrToOid;
		QList<qint32> d_misses;
	};
}

#endif // DOCIMPORTER_H
