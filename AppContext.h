#ifndef APPCONTEXT_H
#define APPCONTEXT_H

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
#include <QUuid>
#include <QSettings>
#include <Sdb/Database.h>
#include <Sdb/Transaction.h>
#include <Txt/Styles.h>
#include <Script/Engine2.h>

class QWidget;
class QSettings;

namespace Ds
{
	class AppContext : public QObject
	{
		Q_OBJECT

	public:
		static const char* s_company;
		static const char* s_domain;
		static const char* s_appName;
		static const char* s_rootUuid;
		static const char* s_version;
		static const char* s_date;
		static const char* s_about;
        static const QUuid s_reqIfMappings;

		AppContext(QObject *parent = 0);
		~AppContext();

		bool open( QString path );
		QString getUserName() const;
		QString askUserName( QWidget*, bool override = false );
		void setUserName( const QString& );

		static AppContext* inst();
		Sdb::Transaction* getTxn() const { return d_txn; }
		Sdb::Database* getDb() const;
		const Sdb::Obj& getRoot() const { return d_root; }
		QSettings* getSet() const { return d_set; }
		void setDocFont( const QFont& );
		QString getIndexPath() const;
	private:
		Sdb::Database* d_db;
		Sdb::Transaction* d_txn;
		Sdb::Obj d_root;
		mutable QString d_user;
		QSettings* d_set;
		Txt::Styles* d_styles;
		QString d_appPath;
	};
}

#endif // APPCONTEXT_H
