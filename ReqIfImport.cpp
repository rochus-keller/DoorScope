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

#include "ReqIfImport.h"
#include <Sdb/Database.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTreeView>
#include <QItemDelegate>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QFileInfo>
#include "TypeDefs.h"
#include "AppContext.h"
using namespace Ds;
using namespace Stream;

static const struct StdMapping
{
	const char* name;
	quint32 id;
	const char* reqIf;
} s_mappingPairs[] = {
	{ "Object ID", AttrObjIdent, "ReqIF.ForeignID" }, // Absolute Number
	{ "Object Summary", AttrObjShort, "ReqIF.Name" }, // Object Short Text
	{ "Object Text", AttrObjText, "ReqIF.Text" },     // Object Text
	// Objekt wird als Body interpretiert, wenn AttrObjText gesetzt ist
	{ "Heading Text", AttrStubTitle, "ReqIF.ChapterName" }, // Object Heading
	// Objekt wird als Titel interpretiert, wenn AttrStubTitle gesetzt ist
	{ "Heading Number", AttrObjNumber, "ReqIF.ChapterNumber" }, // Object Number
	{ "Document ID", AttrDocId, "" },
	{ "Document Version", AttrDocVer, "ReqIF.Revision" },
	{ "Document Prefix", AttrDocPrefix, "ReqIF.Prefix" }, // Prefix
	{ "Document Description", AttrDocDesc, "ReqIF.Description" }, // Description
	{ "Document Name", AttrDocName, "" }, // Doors.Name, ReqIF.Name
	{ "Created By", AttrCreatedBy, "ReqIF.ForeignCreatedBy" }, // Created By
	{ "Created On", AttrCreatedOn, "ReqIF.ForeignCreatedOn" }, // Created On
	{ "Created Thru", AttrCreatedThru, "ReqIF.ForeignCreatedThru" }, // Created Thru
	{ "Modified By", AttrModifiedBy, "ReqIF.ForeignModifiedBy" }, // Last Modified By
	// { "Modified On", AttrModifiedOn, "SpecObject::lastChange" }, // Last Modified On
	{ 0, 0, 0 }
};

ReqIfImport::ReqIfImport(QObject* parent):ReqIfParser(parent)
{
	s_placeHolderImage = ":/DoorScope/Images/img_placeholder.png";
	const StdMapping* s = &s_mappingPairs[0];
	while( s->id )
	{
		addLocalAttr( s->id, QString::fromLatin1(s->name), QString::fromLatin1(s->reqIf) );
		s++;
	}
}

QList<Sdb::Obj> ReqIfImport::importFile(const QString &path, QWidget * w)
{
	clearAll();
	if( !parseFile( path ) )
		return QList<Sdb::Obj>();

	loadMapping();
	if( w )
	{
		if( editMapping( w ) )
		{
			AttrCache::const_iterator i;
			Sdb::Obj db = AppContext::inst()->getTxn()->getOrCreateObject( AppContext::s_reqIfMappings );
			for( i = d_attrCache.begin(); i != d_attrCache.end(); ++i )
				db.setCell( Sdb::Obj::KeyList() << Stream::DataCell().setString(i.key()),
								Stream::DataCell().setUInt32( i.value().second ) );
		}else
			return QList<Sdb::Obj>();
	}

	QList<Sdb::Obj> res;
	for( QMap<QString,SpecHierarchy>::const_iterator i = d_specifications.begin();
		 i != d_specifications.end(); ++i )
	{
		Sdb::Obj s = generateSpecification( i.value() );
		if( s.isNull() )
			return QList<Sdb::Obj>();
		else
			res.append( s );
	}
	for( QMap<QString,SpecRelation>::const_iterator j = d_specRelations.begin();
		 j != d_specRelations.end(); ++j )
	{
		generateRelation( j.value() );
	}
	applyRelationGroups();
	return res;
}

void ReqIfImport::clearAll()
{
	ReqIfParser::clearAll();
	d_objCache.clear();
	d_relCache.clear();
	d_customObjAttr.clear();
	d_customModAttr.clear();
	d_nextId = 1;
}

void ReqIfImport::loadMapping()
{
	Sdb::Obj db = AppContext::inst()->getTxn()->getOrCreateObject( AppContext::s_reqIfMappings );
	Sdb::Mit m = db.findCells( Sdb::Obj::KeyList() );
	if( !m.isNull() ) do
	{
		Sdb::Obj::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isString() )
		{
			if( d_attrCache.contains( l.first().getStr() ) )
				d_attrCache[ l.first().getStr() ].second = m.getValue().getUInt32();
		}
	}while( m.nextKey() );
}

void ReqIfImport::generateAttrs(Sdb::Obj &obj, const ReqIfParser::ObjWithVals &vals, const DefWithAtts& defs )
{
	for( QMap<QString,Stream::DataCell>::const_iterator i = vals.d_vals.begin();
		 i != vals.d_vals.end(); ++i )
	{
		QString name = defs.d_atts.value( i.key() ).d_longName;
		if( name.isEmpty())
			name = i.key(); // RISK: Warning?
		quint32 id = d_attrCache.value( name ).second;
		if( id == 0 )
		{
			// Es handelt sich um eine Custom ID, keine eingebaute.
			id = AppContext::inst()->getTxn()->getAtom( name.toLatin1() );
			if( defs.d_type == SpecificationType )
				d_customModAttr.insert( id );
			else
				d_customObjAttr.insert( id );

		}else if( defs.d_type == SpecificationType )
		{
			// Diese Attribute haben semantisch in Document und Object dieselbe Bedeutung
			// Im Mapping sind sie jedoch allenfalls nur einmal vorhanden.
			switch( id )
			{
			case AttrObjIdent:
				id = AttrDocId;
				break;
			case AttrObjShort:
				id = AttrDocName;
				break;
			}
		}else
		{
			switch( id )
			{
			case AttrDocId:
				id = AttrObjIdent;
				break;
			case AttrDocName:
				id = AttrObjShort;
				break;
			}
		}
		obj.setValue( id, i.value() );
	}
}

Sdb::Obj ReqIfImport::splitTitleBody( Sdb::Obj& title, const Sdb::Obj& doc )
{
	if( title.getType() != TypeTitle )
		return title;

	// 1:1 aus DocManager
	const bool hasTitle = !title.getValue( AttrStubTitle ).isNull();
	const bool hasBody = !title.getValue( AttrObjText ).isNull();
	Sdb::Obj body = title;
	if( !hasBody )
	{
		title.setValue( AttrObjText, title.getValue( AttrStubTitle ) );
		title.setValue( AttrStubTitle, Stream::DataCell().setNull() );
	}else if( !hasTitle && hasBody )
		title.setType( TypeSection );
	else // if hasTitle && hasBody
	{
		// Hier wird Titel und Body in zwei Objekte aufgespalten, so dass Body
		// das erste Subobject von Titel ist.
		body = AppContext::inst()->getTxn()->createObject( TypeSection );
		//d_nrToOid[ title.getValue( AttrObjRelId ).getInt32() ] = body.getId();
		body.aggregateTo( title );
		body.setValue( AttrObjDocId, doc.getValue( AttrDocId ) );
		body.setValue( AttrObjText, title.getValue( AttrObjText ) );
		title.setValue( AttrObjText, title.getValue( AttrStubTitle ) );
		title.setValue( AttrStubTitle, Stream::DataCell().setNull() );
		title.setValue( AttrTitleSplit, body );

		body.setValue( AttrSecSplit, title );
		body.setValue( AttrObjIdent, title.getValue( AttrObjIdent ) ); // dupliziert
		body.setValue( AttrObjShort, title.getValue( AttrObjShort ) );
		body.setValue( AttrObjDeleted, title.getValue( AttrObjDeleted ) );
		body.setValue( AttrObjHomeDoc, title.getValue( AttrObjHomeDoc ) );
		body.setValue( AttrObjNumber, title.getValue( AttrObjNumber ) );
		body.setValue( AttrObjDocId, title.getValue( AttrObjDocId ) );

		body.setValue( AttrCreatedBy, title.getValue( AttrCreatedBy ) );
		body.setValue( AttrCreatedOn, title.getValue( AttrCreatedOn ) );
		body.setValue( AttrModifiedBy, title.getValue( AttrModifiedBy ) );
		body.setValue( AttrModifiedOn, title.getValue( AttrModifiedOn ) );
		body.setValue( AttrCreatedThru, title.getValue( AttrCreatedThru ) );

		Sdb::Obj::Names n = title.getNames();
		Sdb::Obj::Names::const_iterator i;
		for( i = n.begin(); i != n.end(); ++i )
		{
			if( *i > DsMax )
				body.setValue( *i, title.getValue( *i ) );
		}
	}
	return body;
}

void ReqIfImport::generateRelation(const ReqIfParser::SpecRelation & rel )
{
	//const ObjWithVals& source = d_specObjects.value( rel.d_source );
	QList<Sdb::Obj> sourceObs = d_objCache.value( rel.d_source );
	//const ObjWithVals& target = d_specObjects.value( rel.d_target );
	QList<Sdb::Obj> targetObs = d_objCache.value( rel.d_target );

	foreach( Sdb::Obj sob, sourceObs )
	{
		Sdb::Obj sdoc = sob.getObject( AttrObjHomeDoc );
		foreach( Sdb::Obj tob, targetObs )
		{
			Sdb::Obj tdoc = tob.getObject( AttrObjHomeDoc );

			// Es wird in sob OutLink und in tob ein InLink erzeugt
			Sdb::Obj olnk = AppContext::inst()->getTxn()->createObject( TypeOutLink );
			olnk.aggregateTo( sob );
			olnk.setValue( AttrTrgObjId, tob.getValue( AttrObjIdent ) );
			olnk.setValue( AttrTrgDocId, tdoc.getValue( AttrDocId ) );
			olnk.setValue( AttrTrgDocName, tdoc.getValue( AttrDocName ) );
			olnk.setValue( AttrTrgDocVer, tdoc.getValue( AttrDocVer ) );
			olnk.setValue( AttrTrgDocModDate, tdoc.getValue( AttrModifiedOn ) );
			olnk.setValue( AttrTrgDocIsBl, tdoc.getValue( AttrDocBaseline ) );
			olnk.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
			olnk.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
			olnk.setValue( AttrCreatedThru, DataCell().setString( "ReqIF Import" ) );
			if( rel.d_lastChange.isValid() )
				olnk.setValue( AttrModifiedOn, DataCell().setDateTime( rel.d_lastChange ) );
			generateAttrs( olnk, rel, d_specRelationTypes.value( rel.d_ref ) );
			generateStub( olnk, tob );
			d_relCache[rel.d_id].append( olnk );


			Sdb::Obj ilnk = AppContext::inst()->getTxn()->createObject( TypeInLink );
			ilnk.aggregateTo( tob );
			ilnk.setValue( AttrSrcObjId, sob.getValue( AttrObjIdent ) );
			ilnk.setValue( AttrSrcDocId, sdoc.getValue( AttrDocId ) );
			ilnk.setValue( AttrSrcDocName, sdoc.getValue( AttrDocName ) );
			ilnk.setValue( AttrSrcDocVer, sdoc.getValue( AttrDocVer ) );
			ilnk.setValue( AttrSrcDocModDate, sdoc.getValue( AttrModifiedOn ) );
			ilnk.setValue( AttrSrcDocIsBl, sdoc.getValue( AttrDocBaseline ) );
			ilnk.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
			ilnk.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
			ilnk.setValue( AttrCreatedThru, DataCell().setString( "ReqIF Import" ) );
			if( rel.d_lastChange.isValid() )
				ilnk.setValue( AttrModifiedOn, DataCell().setDateTime( rel.d_lastChange ) );
			generateAttrs( ilnk, rel, d_specRelationTypes.value( rel.d_ref ) );
			generateStub( ilnk, sob );
			d_relCache[rel.d_id].append( ilnk );
		}
	}
}

void ReqIfImport::generateStub( Sdb::Obj &rel, const Sdb::Obj &obj)
{
	Sdb::Obj stub = AppContext::inst()->getTxn()->createObject( TypeStub );
	stub.aggregateTo( rel );
	stub.setValue( AttrObjIdent, obj.getValue(AttrObjIdent) );
	stub.setValue( AttrObjText, obj.getValue(AttrObjText) );
	stub.setValue( AttrObjShort, obj.getValue(AttrObjShort) );
	stub.setValue( AttrCreatedBy, obj.getValue(AttrCreatedBy) );
	stub.setValue( AttrCreatedOn, obj.getValue(AttrCreatedOn) );
	stub.setValue( AttrCreatedThru, obj.getValue(AttrCreatedThru) );
	stub.setValue( AttrModifiedOn, obj.getValue(AttrModifiedOn) );
	stub.setValue( AttrModifiedBy, obj.getValue(AttrModifiedBy) );
	stub.setValue( AttrObjHomeDoc, obj.getValue(AttrObjHomeDoc) );
	stub.setValue( AttrObjDocId, obj.getValue(AttrObjDocId) );
	stub.setValue( AttrObjNumber, obj.getValue(AttrObjNumber) );
	foreach( quint32 attr, d_customObjAttr )
		stub.setValue( attr, obj.getValue( attr ) );
}

Sdb::Obj ReqIfImport::generateSpecification(const ReqIfParser::SpecHierarchy & spec )
{
	Sdb::Database::Lock lock( AppContext::inst()->getDb(), true );
	try
	{
		d_customObjAttr.clear();
		d_customModAttr.clear();
		Sdb::Obj doc = AppContext::inst()->getTxn()->createObject( TypeDocument );
		doc.setValue( AttrDocImported, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		doc.setValue( AttrDocStream, DataCell().setString( d_path ) );
		doc.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
		doc.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		doc.setValue( AttrCreatedThru, DataCell().setString( "ReqIF Import" ) );
		if( spec.d_lastChange.isValid() )
			doc.setValue( AttrModifiedOn, DataCell().setDateTime( spec.d_lastChange ) );
		doc.setValue( AttrDocType, DataCell().setString( d_specTypes.value(spec.d_ref).d_longName ) );

		generateAttrs( doc, spec, d_specTypes.value(spec.d_ref) );

		if( doc.getValue(AttrDocId).isNull() )
			doc.setValue( AttrDocId, DataCell().setString( spec.d_id ) );
		if( doc.getValue(AttrDocName).isNull() )
		{
			QString name = spec.d_longName;
			if( name.isEmpty() )
				name = QFileInfo( d_path ).baseName();
			doc.setValue( AttrDocName, DataCell().setString( name ) );
		}
		if( doc.getValue(AttrDocDesc).isNull() )
			doc.setValue( AttrDocDesc, DataCell().setString( spec.d_desc ) );
		if( !doc.getValue(AttrDocVer).isNull() && spec.d_lastChange.isValid() )
			doc.setValue( AttrDocVerDate, DataCell().setDateTime( spec.d_lastChange ) );

		for( int i = 0; i < spec.d_children.size(); i++ )
			generateStructure( doc, doc, spec.d_children[i] );

		DataWriter w;
		QSet<quint32>::const_iterator i;
		for( i = d_customObjAttr.begin(); i != d_customObjAttr.end(); ++i )
			w.writeSlot( DataCell().setAtom( *i ) );
		doc.setValue( AttrDocObjAttrs, w.getBml() );

		w.setDevice();
		for( i = d_customModAttr.begin(); i != d_customModAttr.end(); ++i )
			w.writeSlot( DataCell().setAtom( *i ) );
		doc.setValue( AttrDocAttrs, w.getBml() );

		AppContext::inst()->getTxn()->commit();
		lock.commit();
		return doc;
	}catch( Sdb::DatabaseException& e )
	{
		d_error += e.getCodeString() + " " + e.getMsg();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return Sdb::Obj();
	}catch( std::exception& e )
	{
		d_error += e.what();
		AppContext::inst()->getTxn()->rollback();
		lock.rollback();
		return Sdb::Obj();
	}
	return Sdb::Obj();
}

void ReqIfImport::generateStructure(Sdb::Obj &parent, Sdb::Obj &home, const SpecHierarchy & me )
{
	Sdb::Obj obj = AppContext::inst()->getTxn()->createObject( TypeTitle );
	obj.aggregateTo( parent );
	if( me.d_isTableInternal )
	{
		switch( parent.getType())
		{
		case TypeTable:
			obj.setType( TypeTableRow );
			break;
		case TypeTableRow:
			obj.setType( TypeTableCell );
			break;
		case TypeTableCell:
			obj.setType( TypeTable ); // Tabelle in der Tabelle; nicht ausgeschlossen
			break;
		default:
			obj.setType( TypeTable );
			break;
		}
	}
	// me selber scheint keine relevanten Daten zu enthalten ausser die Struktur
	const ObjWithVals & data = d_specObjects.value( me.d_ref );
	obj.setValue( AttrObjText, DataCell().setString( data.d_longName ) );
	obj.setValue( AttrObjShort, DataCell().setString( data.d_desc ) );
	obj.setValue( AttrCreatedBy, DataCell().setString( "DoorScope" ) );
	obj.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
	obj.setValue( AttrCreatedThru, DataCell().setString( "ReqIF Import" ) );
	if( data.d_lastChange.isValid() )
		obj.setValue( AttrModifiedOn, DataCell().setDateTime( data.d_lastChange ) );
	obj.setValue( AttrObjHomeDoc, home );
	obj.setValue( AttrObjDocId, home.getValue( AttrDocId ) );

	generateAttrs( obj, data, d_specObjectTypes.value( data.d_ref ) );

	if( obj.getValue( AttrObjIdent ).isNull() )
		obj.setValue( AttrObjIdent, DataCell().setInt32( d_nextId++ ) );

	Sdb::Obj body = splitTitleBody( obj, home );
	d_objCache[ me.d_ref ].append( body );

	for( int i = 0; i < me.d_children.size(); i++ )
		generateStructure( obj, home, me.d_children[i] );
	QApplication::processEvents();
}

void ReqIfImport::applyRelationGroups()
{
	for( QMap<QString,RelationGroup>::const_iterator i = d_relationGroups.begin();
		 i != d_relationGroups.end(); ++i )
	{
		const RelationGroup& group = i.value();
		foreach( QString id, group.d_relations )
		{
			QList<Sdb::Obj> rels = d_relCache.value(id);
			foreach( Sdb::Obj rel, rels )
			{
				rel.setValue( AttrLnkModId, DataCell().setString( group.d_id ) );
				rel.setValue( AttrLnkModName, DataCell().setString( group.d_longName ) );
			}
		}
	}
}
