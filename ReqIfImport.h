#ifndef REQIFIMPORT_H
#define REQIFIMPORT_H

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

#include <DoorScope/ReqIfParser.h>
#include <Sdb/Obj.h>

namespace Ds
{
	class ReqIfImport : public ReqIfParser
	{
	public:
		ReqIfImport(QObject* parent = 0);
		QList<Sdb::Obj> importFile( const QString& path, QWidget* = 0 ); // return: doc oder null bei fehler
	protected:
		Sdb::Obj splitTitleBody( Sdb::Obj& title, const Sdb::Obj &doc );
		Sdb::Obj generateSpecification( const SpecHierarchy& );
		void generateAttrs( Sdb::Obj& obj, const ObjWithVals& vals, const DefWithAtts &defs );
		void generateStructure( Sdb::Obj& parent, Sdb::Obj& home, const SpecHierarchy& );
		void generateStub( Sdb::Obj& rel, const Sdb::Obj& obj );
		void clearAll();
		void loadMapping();
		void generateRelation( const SpecRelation& );
		void applyRelationGroups();
	private:
		QMap<QString,QList<Sdb::Obj> > d_objCache;
		QMap<QString,QList<Sdb::Obj> > d_relCache;
		QSet<quint32> d_customObjAttr;
		QSet<quint32> d_customModAttr;
		quint32 d_nextId;
	};
}

#endif // REQIFIMPORT_H
