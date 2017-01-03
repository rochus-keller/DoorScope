#ifndef _Ds_DocIndex_
#define _Ds_DocIndex_

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

#include <Sdb/Obj.h>
#include <QList>
#include <QMutex>

class QWidget;

namespace Ds
{
	class Indexer : public QObject
	{
	public:
		struct Hit
		{
			Sdb::Obj d_doc;
			Sdb::Obj d_title;
			qreal d_score;
		};
		typedef QList<Hit> ResultList;

		static QString fetchText( const Sdb::Obj& ); // not simplified, original case
		static QString fetchText( const Sdb::Obj&, bool aggregateTable ); // not simplified, original case
		static Sdb::Obj findText( const QString& pattern, const Sdb::Obj& start, bool forward = true ); 
		static Sdb::Obj gotoNext( const Sdb::Obj& obj );
		static Sdb::Obj gotoPrev( const Sdb::Obj& obj );
		static Sdb::Obj gotoLast( const Sdb::Obj& obj ); // zuunterst

		Indexer( QObject* p = 0 );
		static bool exists();
		bool indexRepository( QWidget* ); // Blocking
		const QString& getError() const { return d_error; }
		bool query( const QString& query, ResultList& result );
	private:
		bool indexDocument( const Sdb::Obj& doc );
		QString d_error;
	};
}

#endif
