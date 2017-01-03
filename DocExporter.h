#ifndef DOCEXPORTER_H
#define DOCEXPORTER_H

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
#include <QList>

namespace Ds
{
	class DocExporter : public QObject
	{
	public:
		DocExporter(QObject *parent = 0);
		~DocExporter();

		static bool saveStatusAnnotReportHtml( const Sdb::Obj& doc, const QString& path );
		static bool saveStatusAnnotReportHtml2( const Sdb::Obj& doc, const QString& path );
		static bool saveFullDocHtml( const Sdb::Obj& doc, const QString& path );
		static bool saveFullDocAnnotHtml( const Sdb::Obj& doc, const QString& path );
		static bool saveFullDocAnnotAttrHtml( const Sdb::Obj& doc, const QString& path );
		static bool saveAttrCsv( const Sdb::Obj& doc, const QList<quint32>& attrs, const QString& path, bool utf8 = false );
		static bool saveAnnot( const Sdb::Obj& doc, const QString& path );
		static bool saveStatusAnnotReportCsv( const Sdb::Obj& doc, const QString& path );
		static bool saveAnnotCsv( const Sdb::Obj& doc, const QString& path );
	private:
	    
	};
}

#endif // DOCEXPORTER_H
