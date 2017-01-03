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

#include "ReqIfParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextDocument>
#include <QBuffer>
#include <QApplication>
#include <QTreeView>
#include <QItemDelegate>
#include <QComboBox>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QtDebug>
using namespace Ds;
using namespace Stream;

typedef QMap<quint32,QPair<QString,QString> > LocalAttrs; // Attr -> Name, DefaultMapping
static LocalAttrs s_localAttrs;

ReqIfParser::ReqIfParser(QObject *parent) :
	QObject(parent)
{
}

bool ReqIfParser::parseFile(const QString &path)
{
    clearAll();

    QFile file( path );
    if( !file.open(QIODevice::ReadOnly) )
    {
        d_error = tr("Cannot open file for reading: %1").arg(path);
		return false;
    }
    d_path = path;
	QDir::setCurrent( QFileInfo(path).path() ); // damit relative Pfadangaben der Bilder funktionieren
    QString error;
    int line;
    if( !d_doc.setContent( &file, true, &error, &line ) )
    {
        d_error = tr("Line %1: %2").arg(line).arg(error);
		return false;
    }
    QDomElement e = d_doc.firstChildElement("REQ-IF");
    if( e.isNull() )
    {
        d_error = "Invalid schema: REQ-IF expected";
		return false;
    }
    e = e.firstChildElement("CORE-CONTENT");
    if( e.isNull() )
    {
        d_error = "Invalid schema: CORE-CONTENT expected";
		return false;
    }
    e = e.firstChildElement("REQ-IF-CONTENT");
    if( e.isNull() )
    {
        d_error = "Invalid schema: REQ-IF-CONTENT expected";
		return false;
    }
    if( !readReqIfContent( e ) )
    {
        d_doc.clear();
		return false;
    }
    d_doc.clear();
    if( d_specifications.isEmpty() )
    {
		d_error = tr("File '%1' contains no specifications!").arg(
					QFileInfo(d_path).baseName() );
		return false;
    }
	loadLocalMappings();
	return true;
}

QDateTime ReqIfParser::parseDateTime(const QString & str)
{
    const int tPos = str.indexOf( QChar('T') );
    if( tPos == -1 )
        return QDateTime();
    QString dateStr = str.left( tPos );
    QString timeStr = str.mid( tPos + 1 );
    ;
    // NOTE: Jahr 0 ist in XSD nicht zulssig
    bool plus = true;
    if( dateStr.startsWith( QChar('-') ) )
    {
        plus = false;
        dateStr = dateStr.mid(1);
    }
    // QDate::fromString kommt nicht zurecht mit dreistelligen Jahren
    QStringList dateParts = dateStr.split(QChar('-') );
    if( dateParts.size() != 3 )
    {
        qWarning() << "invalid date: " << dateStr;
        return QDateTime();
    }
    QDate d( ( (plus)?1:-1 ) * dateParts[0].toInt(), dateParts[1].toInt(), dateParts[2].toInt() );

	const int zPos = timeStr.indexOf( QRegExp("Z|[\\+\\-]") );
    QString zoneStr;
    if( zPos != -1 )
    {
        zoneStr = timeStr.mid(zPos);
        timeStr.truncate(zPos);
    }
    QTime t;
    if( timeStr.contains(QChar('.') ) )
        t = QTime::fromString( timeStr, "h:m:s.z" );
    else
        t = QTime::fromString( timeStr, "h:m:s" );
    if( zoneStr.isEmpty() )
        return QDateTime( d, t );
    else if( zoneStr == "Z")
        return QDateTime( d, t, Qt::UTC );
    else
    {
        plus = zoneStr.startsWith( QChar('+') );
        QTime z = QTime::fromString( zoneStr.mid(1), "h:m" );
        const int offset = ( (plus)?1:-1 ) * ( z.hour() * 60 + z.minute() );
        QDateTime dt( d, t, Qt::UTC );
        dt = dt.addSecs( - offset * 60 );
        return dt;
    }
    return QDateTime(); // dummy
}

const char* ReqIfParser::s_placeHolderImage = "";

bool ReqIfParser::loadImage(QImage &img, const QString &src, const QString & w, const QString & h)
{
    QFileInfo path;
    const int width = (w.contains(QChar('%')) )?0:w.toInt();
    const int height = (h.contains(QChar('%')) )?0:h.toInt();
	if( src.startsWith( QLatin1String( "file:///" ), Qt::CaseInsensitive ) ) // Windows-Spezialitt
		// siehe http://en.wikipedia.org/wiki/File_URI_scheme
		path = src.mid( 8 );
    else if( src.startsWith( "data:" ) )
    {
        // Daten sind direkt in das Attribut eingebettet
        // image/png;base64, siehe http://de.wikipedia.org/wiki/Data-URL
        const int pos1 = src.indexOf(QLatin1String("image/"),4, Qt::CaseInsensitive);
        if( pos1 != -1 )
        {
            const int pos2 = src.indexOf( QChar(';'), pos1 + 6 );
            if( pos2 != -1 )
            {
                const QString imgType = src.mid( pos1 + 6, pos2 - pos1 -6 ).toUpper();
                const int pos3 = src.indexOf(QLatin1String("base64,"),pos2 + 1, Qt::CaseInsensitive);
                if( pos3 != -1 )
                {
                    const QByteArray base64 = src.mid( pos3 + 7 ).toAscii();
                    if( img.loadFromData( QByteArray::fromBase64( base64 ), imgType.toAscii() ) )
                    {
                        if( width > 0 && height > 0 )
                            img = img.scaled( QSize( width, height ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
                        return true;
                    }
                }
            }
        }
        // else
		img.load( s_placeHolderImage );
        return false;
    }else
	{
		const QUrl url = QUrl::fromEncoded( src.toAscii(), QUrl::TolerantMode );
		path = url.toLocalFile();
	}
    if( !img.load( path.absoluteFilePath() ) )
	{
		img.load( s_placeHolderImage );
        return false;
	}else
    {
        if( width > 0 && height > 0 )
            img = img.scaled( QSize( width, height ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
        return true;
	}
}

void ReqIfParser::addLocalAttr(quint32 id, const QString &name, const QString &defaultMapping)
{
	if( id == 0 )
		return; // 0 hat Spezialbedeutung
	s_localAttrs[id] = qMakePair(name,defaultMapping);
}

bool ReqIfParser::readReqIfContent(const QDomNode & core)
{
    QDomNodeList l = core.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        if( l.at(i).localName() == "DATATYPES" )
        {
            if( !readDataTypes( l.at(i) ) )
                return false;
        }else if( l.at(i).localName() == "SPEC-TYPES" )
        {
            if( !readSpecTypes( l.at(i) ) )
                return false;
        }else if( l.at(i).localName() == "SPEC-OBJECTS" )
        {
            if( !readSpecObjects( l.at(i) ) )
                return false;
        }else if( l.at(i).localName() == "SPEC-RELATIONS" )
        {
            if( !readSpecRelations( l.at(i) ) )
                return false;
        }else if( l.at(i).localName() == "SPECIFICATIONS" )
        {
            if( !readSpecifications( l.at(i) ) )
                return false;
        }else if( l.at(i).localName() == "SPEC-RELATION-GROUPS" )
        {
            if( !readRelationGroups( l.at(i) ) )
                return false;
        }
    }
    return true;
}

bool ReqIfParser::readDataTypes(const QDomNode & super)
{
    QDomNodeList l = super.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        DataTypeDefinition def;
        if( l.at(i).localName() == "DATATYPE-DEFINITION-BOOLEAN" )
            def.d_type = DataTypeDefinition::Boolean;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-DATE" )
            def.d_type = DataTypeDefinition::Date;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-ENUMERATION" )
            def.d_type = DataTypeDefinition::Enumeration;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-INTEGER" )
            def.d_type = DataTypeDefinition::Integer;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-REAL" )
            def.d_type = DataTypeDefinition::Real;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-STRING" )
            def.d_type = DataTypeDefinition::String;
        else if( l.at(i).localName() == "DATATYPE-DEFINITION-XHTML" )
            def.d_type = DataTypeDefinition::Xhtml;
        else
        {
            d_error = tr("Unknown DATATYPE-DEFINITION '%1' on line %2").arg(
                        l.at(i).localName() ).arg( l.at(i).lineNumber() );
            return false;
        }
        QDomNamedNodeMap atts = l.at(i).attributes();
        QDomAttr id = atts.namedItem( "IDENTIFIER" ).toAttr();
        if( id.isNull() || d_dataTypes.contains( id.value() ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id.value()).arg(id.lineNumber());
            return false;
        }
        def.d_id = id.value();
        def.d_longName = atts.namedItem( "LONG-NAME" ).toAttr().value();
        def.d_desc = atts.namedItem( "DESC" ).toAttr().value();
        if( def.d_type == DataTypeDefinition::Enumeration )
        {
            QDomNodeList l2 = l.at(i).firstChildElement("SPECIFIED-VALUES").childNodes();
            for( int j = 0; j < l2.size(); j++ )
            {
                QDomElement e = l2.at(j).toElement();
                def.d_enums[ e.attribute( "IDENTIFIER" ) ] = e.attribute("LONG-NAME");
                // RISK: auf Doubletten prfen
            }
        }
        d_dataTypes[def.d_id] = def;
        QApplication::processEvents();
    }
	return true;
}

bool ReqIfParser::readSpecTypes(const QDomNode & super)
{
    QDomNodeList l = super.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        DefWithAtts def;
        QMap<QString,DefWithAtts>* map = 0;
        if( l.at(i).localName() == "RELATION-GROUP-TYPE" )
        {
            def.d_type = RelationGroupType;
            map = &d_relationGroupTypes;
        }else if( l.at(i).localName() == "SPEC-OBJECT-TYPE" )
        {
            def.d_type = ObjectType;
            map = &d_specObjectTypes;
        }else if( l.at(i).localName() == "SPEC-RELATION-TYPE" )
        {
            def.d_type = RelationType;
            map = &d_specRelationTypes;
        }else if( l.at(i).localName() == "SPECIFICATION-TYPE" )
        {
            def.d_type = SpecificationType;
            map = &d_specTypes;
        }else
        {
            d_error = tr("Unknown SPEC-TYPES '%1' on line %2").arg(
                        l.at(i).localName() ).arg( l.at(i).lineNumber() );
            return false;
        }
        Q_ASSERT( map != 0 );
        if( !readAttDefs( l.at(i).firstChildElement("SPEC-ATTRIBUTES"), def ) )
            return false;
        QDomNamedNodeMap atts = l.at(i).attributes();
        QDomAttr id = atts.namedItem( "IDENTIFIER" ).toAttr();
        if( id.isNull() || map->contains( id.value() ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id.value()).arg(id.lineNumber());
            return false;
        }
        def.d_id = id.value();
        def.d_longName = atts.namedItem( "LONG-NAME" ).toAttr().value();
        def.d_desc = atts.namedItem( "DESC" ).toAttr().value();
        map->insert( def.d_id, def );
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readSpecObjects(const QDomNode & root)
{
    QDomNodeList l = root.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        QDomElement e = l.at(i).toElement();
        ObjWithVals obj;
        QString id = e.attribute( "IDENTIFIER" );
        if( id.isEmpty() || d_specObjects.contains( id ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id).arg(root.lineNumber());
            return false;
        }
        obj.d_id = id;
        obj.d_longName = e.attribute( "LONG-NAME" );
        obj.d_desc = e.attribute( "DESC" );
        obj.d_lastChange = parseDateTime( e.attribute( "LAST-CHANGE" ) );
        obj.d_ref = e.firstChildElement( "TYPE" ).firstChildElement("SPEC-OBJECT-TYPE-REF").text();
        if( !d_specObjectTypes.contains( obj.d_ref ) )
        {
            d_error = tr("Unknown object type '%1' on line %2" ).arg( obj.d_ref ).arg( e.lineNumber() );
            return false;
        }
        if( !readAttVals( e.firstChildElement("VALUES"), obj, d_specObjectTypes.value( obj.d_ref ) ) )
            return false;

        d_specObjects[obj.d_id] = obj;
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readSpecRelations(const QDomNode & root)
{
    QDomNodeList l = root.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        QDomElement e = l.at(i).toElement(); // e ist SPEC-RELATION
        SpecRelation obj;
        QString id = e.attribute( "IDENTIFIER" );
        if( id.isEmpty() || d_specRelations.contains( id ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id).arg(root.lineNumber());
            return false;
        }
        obj.d_id = id;
        obj.d_longName = e.attribute( "LONG-NAME" );
        obj.d_desc = e.attribute( "DESC" );
        obj.d_lastChange = parseDateTime( e.attribute( "LAST-CHANGE" ) );
        obj.d_ref = e.firstChildElement( "TYPE" ).firstChildElement("SPEC-RELATION-TYPE-REF").text();
        if( !d_specRelationTypes.contains( obj.d_ref ) )
        {
            d_error = tr("Unknown object type '%1' on line %2" ).arg( obj.d_ref ).arg( e.lineNumber() );
            return false;
        }
        if( !readAttVals( e.firstChildElement("VALUES"), obj, d_specRelationTypes.value( obj.d_ref ) ) )
            return false;
        obj.d_source = e.firstChildElement( "SOURCE" ).firstChildElement("SPEC-OBJECT-REF").text();
        if( !d_specObjects.contains( obj.d_source ) )
        {
            d_error = tr("Unknown source object '%1' on line %2" ).arg( obj.d_source ).arg( e.lineNumber() );
            return false;
        }
        obj.d_target = e.firstChildElement( "TARGET" ).firstChildElement("SPEC-OBJECT-REF").text();
        if( !d_specObjects.contains( obj.d_target ) )
        {
            d_error = tr("Unknown target object '%1' on line %2" ).arg( obj.d_target ).arg( e.lineNumber() );
            return false;
        }

        d_specRelations[obj.d_id] = obj;
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readSpecifications(const QDomNode & root)
{
    QDomNodeList l = root.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        QDomElement e = l.at(i).toElement(); // e ist SPECIFICATION
        SpecHierarchy obj;
        QString id = e.attribute( "IDENTIFIER" );
        if( id.isEmpty() || d_specifications.contains( id ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id).arg(root.lineNumber());
            return false;
        }
        obj.d_id = id;
        obj.d_longName = e.attribute( "LONG-NAME" );
        obj.d_desc = e.attribute( "DESC" );
        obj.d_lastChange = parseDateTime( e.attribute( "LAST-CHANGE" ) );
        obj.d_ref = e.firstChildElement( "TYPE" ).firstChildElement("SPECIFICATION-TYPE-REF").text();
        if( !d_specTypes.contains( obj.d_ref ) )
        {
            d_error = tr("Unknown specification type '%1' on line %2" ).arg( obj.d_ref ).arg( e.lineNumber() );
            return false;
        }
        if( !readAttVals( e.firstChildElement("VALUES"), obj, d_specTypes.value( obj.d_ref ) ) )
            return false;
        if( !readSpecHierarchy( e.firstChildElement( "CHILDREN" ), obj ) )
            return false;
        d_specifications[obj.d_id] = obj;
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readSpecHierarchy(const QDomNode & root, ReqIfParser::SpecHierarchy &parent)
{
    QDomNodeList l = root.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        QDomElement e = l.at(i).toElement(); // e ist SPEC-HIERARCHY
        SpecHierarchy obj;
        // Es fehlt IS-EDITABLE
        obj.d_id = e.attribute( "IDENTIFIER" );
        obj.d_longName = e.attribute( "LONG-NAME" );
        obj.d_desc = e.attribute( "DESC" );
        obj.d_lastChange = parseDateTime( e.attribute( "LAST-CHANGE" ) );
        obj.d_ref = e.firstChildElement( "OBJECT" ).firstChildElement("SPEC-OBJECT-REF").text();
        obj.d_isTableInternal = e.attribute("IS-TABLE-INTERNAL") == "true" || e.attribute("IS-TABLE-INTERNAL") == "1";
        if( !d_specObjects.contains( obj.d_ref ) )
        {
            d_error = tr("Unknown specification object '%1' on line %2" ).arg(
                        obj.d_ref ).arg( e.lineNumber() );
            return false;
        }
        if( !readAttVals( e.firstChildElement("EDITABLE-ATTS"), obj, DefWithAtts() ) )
            return false;
        if( !readSpecHierarchy( e.firstChildElement( "CHILDREN" ), obj ) )
            return false;
        parent.d_children.append( obj );
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readRelationGroups(const QDomNode & root)
{
    QDomNodeList l = root.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        QDomElement e = l.at(i).toElement(); // e ist RELATION-GROUP
        RelationGroup obj;
        QString id = e.attribute( "IDENTIFIER" );
        if( id.isEmpty() || d_relationGroups.contains( id ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id).arg(root.lineNumber());
            return false;
        }
        obj.d_id = id;
        obj.d_longName = e.attribute( "LONG-NAME" );
        obj.d_desc = e.attribute( "DESC" );
        obj.d_lastChange = parseDateTime( e.attribute( "LAST-CHANGE" ) );
        obj.d_ref = e.firstChildElement( "TYPE" ).firstChildElement("RELATION-GROUP-TYPE-REF").text();
        if( !d_relationGroupTypes.contains( obj.d_ref ) )
        {
            d_error = tr("Unknown relation group type '%1' on line %2" ).arg( obj.d_ref ).arg( e.lineNumber() );
            return false;
        }
        obj.d_source = e.firstChildElement( "SOURCE-SPECIFICATION" ).firstChildElement("SPECIFICATION-REF").text();
        if( !d_specifications.contains( obj.d_source ) )
        {
            d_error = tr("Unknown source specification '%1' on line %2" ).arg( obj.d_source ).arg( e.lineNumber() );
            return false;
        }
        obj.d_target = e.firstChildElement( "TARGET-SPECIFICATION" ).firstChildElement("SPECIFICATION-REF").text();
        if( !d_specifications.contains( obj.d_target ) )
        {
            d_error = tr("Unknown target specification '%1' on line %2" ).arg( obj.d_target ).arg( e.lineNumber() );
            return false;
        }
        if( !readAttVals( e.firstChildElement("VALUES"), obj, d_relationGroupTypes.value( obj.d_ref ) ) )
            return false;
        QDomNodeList l2 = e.firstChildElement("SPEC-RELATIONS").childNodes();
        for( int j = 0; j < l2.size(); j++ )
        {
            const QString ref = l2.at(j).toElement().text();
            if( !d_specRelations.contains( ref ) )
            {
                d_error = tr("Unknown relation '%1' on line %2" ).arg( ref ).arg( l2.at(j).lineNumber() );
                return false;
            }
            obj.d_relations.append( ref );
        }
        d_relationGroups[obj.d_id] = obj;
        QApplication::processEvents();
    }
    return true;
}

bool ReqIfParser::readAttDefs(const QDomNode & super, ReqIfParser::DefWithAtts & def)
{
    QDomNodeList l = super.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        AttributeDef ad;
        DataTypeDefinition::Type t;
        if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-BOOLEAN" )
        {
            t = DataTypeDefinition::Boolean;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-DATE" )
        {
            t = DataTypeDefinition::Date;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-ENUMERATION" )
        {
            t = DataTypeDefinition::Enumeration;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-INTEGER" )
        {
            t = DataTypeDefinition::Integer;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-REAL" )
        {
            t = DataTypeDefinition::Real;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-STRING" )
        {
            t = DataTypeDefinition::String;
        }else if( l.at(i).localName() == "ATTRIBUTE-DEFINITION-XHTML" )
        {
            t = DataTypeDefinition::Xhtml;
        }
        else
        {
            d_error = tr("Unknown ATTRIBUTE-DEFINITION '%1' on line %2").arg(
                        l.at(i).localName() ).arg( l.at(i).lineNumber() );
            return false;
        }
        QDomNamedNodeMap atts = l.at(i).attributes();
        QDomAttr id = atts.namedItem( "IDENTIFIER" ).toAttr();
        if( id.isNull() || def.d_atts.contains( id.value() ) )
        {
            d_error = tr("Invalid IDENTIFIER '%1'' on line %2").arg(id.value()).arg(id.lineNumber());
            return false;
        }
        ad.d_id = id.value();
        ad.d_longName = atts.namedItem( "LONG-NAME" ).toAttr().value();
        ad.d_desc = atts.namedItem( "DESC" ).toAttr().value();
        const QString v = atts.namedItem( "IS-EDITABLE" ).toAttr().value();
        ad.d_isEditable = v == "true" || v == "1";

        const QDomElement type = l.at(i).firstChildElement( "TYPE" ).firstChildElement(); // DATATYPE-DEFINITION-*-REF
        if( !d_dataTypes.contains(type.text()) || d_dataTypes[type.text()].d_type != t )
        {
            d_error = tr("Invalid Data Type Definition Ref '%1'' on line %2").arg(
                        type.text()).arg(l.at(i).lineNumber());
            return false;
        }
        ad.d_dataType = type.text();
        const QDomElement value = l.at(i).firstChildElement( "DEFAULT-VALUE" ).firstChildElement(); // ATTRIBUTE-VALUE-*
        ad.d_default = readAttrValue( value, ad );
        // NOTE: MULTI-VALUED fr ATTRIBUTE-DEFINITION-ENUMERATION
        def.d_atts[ad.d_id] = ad;
		d_attrCache[ad.d_longName].first.set(def.d_type);
    }
    return true;
}

bool ReqIfParser::readAttVals(const QDomNode & values, ObjWithVals & obj, const DefWithAtts& def)
{
    QDomNodeList l = values.childNodes();
    for( int i = 0; i < l.size(); i++ )
    {
        const QString id = l.at(i).firstChildElement("DEFINITION").firstChildElement().text();
        if( obj.d_vals.contains( id ) )
        {
            d_error = tr("ATTRIBUTE-VALUE '%1' on line %2 already set").arg(
                        id ).arg( l.at(i).lineNumber() );
            return false;
        }
        QMap<QString,AttributeDef>::const_iterator ai = def.d_atts.find( id );
        if( ai == def.d_atts.end() )
        {
            d_error = tr("Unknown ATTRIBUTE-VALUE definition '%1' on line %2").arg(
                        id ).arg( l.at(i).lineNumber() );
            return false;
        }
        Stream::DataCell v = readAttrValue( l.at(i).toElement(), ai.value() );
        if( v.isNull() || !v.isValid() )
            v = ai.value().d_default;
        obj.d_vals[ id ] = v;
    }
    return true;
}

Stream::DataCell ReqIfParser::readAttrValue(const QDomElement & value, const AttributeDef& def)
{
    Stream::DataCell res;
    bool ok = true;
    if( value.isNull() )
    {
        res.setNull();
    }else if( value.localName() == "ATTRIBUTE-VALUE-BOOLEAN" )
    {
        QString v = value.attribute( "THE-VALUE" );
        res.setBool( v == "true" || v == "1" );
    }else if( value.localName() == "ATTRIBUTE-VALUE-DATE" )
    {
        QDateTime dt = parseDateTime( value.attribute( "THE-VALUE" ) );
        if( dt.isValid() )
            res.setDateTime( dt );
        else
            ok = false;
    }else if( value.localName() == "ATTRIBUTE-VALUE-ENUMERATION" )
    {
        QStringList values;
        QDomNodeList l = value.firstChildElement("VALUES").childNodes();
        for( int i = 0; i < l.size(); i++ )
        {
            // ENUM-VALUE-REF
            const QString id = l.at(i).toElement().text();
            const QMap<QString,QString>& enums = d_dataTypes.value( def.d_dataType ).d_enums;
            QMap<QString,QString>::const_iterator it = enums.find(id);
            if( it != enums.end() )
            {
                if( !it.value().isEmpty() )
                    values.append( it.value() );
                else
                    values.append( id );
            }else
                qWarning() << tr("Undefined ENUM-VALUE-REF on line %2").arg( value.lineNumber() );
        }
        res.setString( values.join(QChar('|') ) );
    }else if( value.localName() == "ATTRIBUTE-VALUE-INTEGER" )
    {
        res.setInt64( value.attribute( "THE-VALUE" ).toLongLong(&ok) );
    }else if( value.localName() == "ATTRIBUTE-VALUE-REAL" )
    {
        res.setDouble( value.attribute( "THE-VALUE" ).toDouble(&ok) );
    }else if( value.localName() == "ATTRIBUTE-VALUE-STRING" )
    {
        res.setString( value.attribute( "THE-VALUE" ) );
    }else if( value.localName() == "ATTRIBUTE-VALUE-XHTML" )
    {
        // TODO: falls html eine Titelstruktur enthlt, sollte diese als Objektstruktur importiert werden
        // Es gibt ein Element "THE-VALUE", unter dem dann die XHTML-Elemente sind, alle prefixed mit xhtml:
        QDomElement xhtml = value.firstChildElement("THE-VALUE");
        QDomNode root = xhtml.firstChild();
        // Die xhtml.BlkStruct.class lsst als Root genau einen <p> oder <div> zu.
        if( root.localName() != "p" && root.localName() != "div" )
			qWarning() << "ReqIfParser read xhtml value: root neither <p> nor <div>";

        // Wenn der Root nur ein object oder img enthlt, generiere ein TypePicture
        if( root.childNodes().size() == 1 && ( root.firstChild().localName() == "object" ||
                                          root.firstChild().localName() == "img" ) )
        {
            QDomElement e = root.firstChild().toElement();
            QImage img;
            if( e.localName() == "img" )
                loadImage( img, e.attribute( "src" ),e.attribute( "width" ), e.attribute( "height" ) );
            else
                loadImage( img, e.attribute( "data" ),e.attribute( "width" ), e.attribute( "height" ) );
            res.setImage( img );
       }else
            res.setHtml( toHtml( root ) );
    }
    else
    {
        qWarning() << tr("Unknown ATTRIBUTE-VALUE '%1' on line %2").arg( value.localName() ).arg(
                          value.lineNumber() );
    }
    if( !ok )
        qWarning() << tr("Invalid ATTRIBUTE-VALUE '%1' on line %2").arg( value.text() ).arg(
                          value.lineNumber() );
	return res;
}

void ReqIfParser::loadLocalMappings()
{
	LocalAttrs::const_iterator i;
	for( i = s_localAttrs.begin(); i != s_localAttrs.end(); ++i )
	{
		if( d_attrCache.contains( i.value().second ) )
			d_attrCache[ i.value().second ].second = i.key();
	}
}

static void _descent( QTextStream& out, const QDomNode& node )
{
    if( node.isElement() )
    {
        QString name = node.localName();
        if( name == "object" )
            name = "img"; // wir wollen nur simples html speichern, ohne object Tag.
        out << "<" << name;
        if( node.hasAttributes() )
        {
            if( node.localName() == "img" || node.localName() == "object" )
            {
                QDomElement e = node.toElement();
                QImage img;
                QString src;
                if( e.localName() == "img" )
                    src = e.attribute( "src" );
                else
                    src = e.attribute( "data" );
                if( src.startsWith( "data:") )
                {
                    // Image liegt bereits embedded vor
                    out << " src=\"" << src << "\"";
                }else
                {
                    // TODO: Bilder ev. besser als separate Objekte speichern und dann von Ressource-Maschine
                    // von QTextDocument laden lassen.
					ReqIfParser::loadImage( img, src, e.attribute( "width" ), e.attribute( "height" ) );
                    QByteArray byteArray;
                    QBuffer buffer(&byteArray);
                    buffer.open(QIODevice::WriteOnly);
                    img.save( &buffer, "PNG" );
                    buffer.close();
                    out << " src=\"data:image/png;base64," << byteArray.toBase64() << "\"";
                }
            }else
            {
                QDomNamedNodeMap a = node.attributes();
                for( int i = 0; i < a.size(); i++ )
                {
                    QDomAttr at = a.item(i).toAttr();
                    out << " " << at.localName() << "=\"" << Qt::escape( at.value() ) << "\"";
                }
            }
        }
        if( !node.firstChild().isNull() )
        {
            out << ">";
            QDomNodeList l = node.childNodes();
            for( int i = 0; i < l.size(); i++ )
                _descent( out, l.at(i) );
            out << "</" << name << ">";

        }else
            out << "/>";
    }else if( node.isCDATASection() || node.isText() )
    {
        out << Qt::escape( node.nodeValue() );
    }
}

QString ReqIfParser::toHtml(const QDomNode & root)
{
    QString html;
    QTextStream out( &html, QIODevice::WriteOnly );
    out << "<html><body>";
    _descent( out, root );
    out << "</body></html>";
    return html;
}

void ReqIfParser::clearAll()
{
    d_error.clear();
    d_path.clear();
    d_doc.clear();
    d_dataTypes.clear();
    d_relationGroupTypes.clear();
    d_specObjectTypes.clear();
    d_specRelationTypes.clear();
    d_specTypes.clear();
    d_specObjects.clear();
    d_specRelations.clear();
    d_specifications.clear();
    d_relationGroups.clear();
    d_attrCache.clear();
}

class ReqIFMappingDelegate : public QItemDelegate
{
public:
	ReqIFMappingDelegate(QObject *parent=0): QItemDelegate(parent){}
	QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &/* option */, const QModelIndex & index ) const
	{
		if( index.column() != 1 )
			return 0;
		QComboBox* cb = new QComboBox(parent);
		// cb->setInsertPolicy(QComboBox::InsertAlphabetically);
		fillCombo( cb );
		cb->setMaxVisibleItems( cb->count() );
		return cb;
	}
	static void fillCombo( QComboBox* cb )
	{
		QMap<QString,quint32> sorter;
		sorter[ ReqIfParser::tr("<custom attribute>") ] = 0;
		LocalAttrs::const_iterator i;
		for( i = s_localAttrs.begin(); i != s_localAttrs.end(); ++i )
			sorter[ i.value().first ] = i.key();
		QMap<QString,quint32>::const_iterator j;
		for( j = sorter.begin(); j != sorter.end(); ++j )
			cb->addItem( j.key(), j.value() );
	}
	void setEditorData(QWidget *editor, const QModelIndex &index) const
	{
		QComboBox * box = static_cast<QComboBox*>(editor);
		const int pos = box->findData( index.model()->data(index, Qt::EditRole ) );
		box->setCurrentIndex( (pos==-1)?0:pos );
	}
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
	{
		QComboBox * box = static_cast<QComboBox*>(editor);
		model->setData(index, box->itemData( box->currentIndex() ), Qt::EditRole);
	}
	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
	{
		editor->setGeometry(option.rect);
	}
};

class ReqIFMappingMdl : public QAbstractItemModel
{
public:
	struct Slot
	{
		QString d_name;
		std::bitset<ReqIfParser::MaxType> d_where;
		quint32 d_to;
		Slot( const QString& name, const std::bitset<ReqIfParser::MaxType>& where, quint32 to ):
			d_name(name),d_where(where),d_to(to){}
	};

	QList<Slot> d_rows;
	ReqIFMappingMdl(QObject* p = 0):QAbstractItemModel(p){}
	int columnCount( const QModelIndex & ) const { return 3; }
	static QString toString( quint32 id )
	{
		return s_localAttrs.value( id, qMakePair(ReqIfParser::tr("<custom attribute>"),QString()) ).first;
	}
	QVariant data ( const QModelIndex & index, int role ) const
	{
		if( index.column() == 0 && role == Qt::DisplayRole )
		{
			return d_rows[ index.row() ].d_name;
		}else if( index.column() == 1 )
		{
			if( role == Qt::EditRole )
				return d_rows[ index.row() ].d_to;
			else if( role == Qt::DisplayRole)
				return toString( d_rows[ index.row() ].d_to );
		}else if( index.column() == 2 && ( role == Qt::DisplayRole ||  role == Qt::ToolTipRole ) )
		{
			QStringList where;
			if( d_rows[ index.row() ].d_where.test( ReqIfParser::ObjectType) )
				where.append( ReqIfParser::tr("SpecObject") );
			if( d_rows[ index.row() ].d_where.test( ReqIfParser::RelationType) )
				where.append( ReqIfParser::tr("SpecRelation") );
			if( d_rows[ index.row() ].d_where.test( ReqIfParser::SpecificationType) )
				where.append( ReqIfParser::tr("Specification") );
			if( d_rows[ index.row() ].d_where.test( ReqIfParser::RelationGroupType) )
				where.append( ReqIfParser::tr("RelationGroup") );
			return where.join( ", " );
		}
		return QVariant();
	}
	Qt::ItemFlags flags ( const QModelIndex & index ) const
	{
		if( index.column() == 1 )
			return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
		else
			return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	}
	QVariant headerData ( int section, Qt::Orientation orientation, int role ) const
	{
		if( orientation == Qt::Horizontal && role == Qt::DisplayRole )
		{
			switch( section )
			{
			case 0:
				return ReqIfParser::tr("ReqIF Attr.");
			case 1:
				return ReqIfParser::tr("Repository Attr." );
			case 2:
				return ReqIfParser::tr("Used in");
			}
		}
		return QVariant();
	}
	QModelIndex index ( int row, int column, const QModelIndex & parent ) const
	{
		if( !parent.isValid() )
			return createIndex( row, column, row );
		return QModelIndex();
	}
	QModelIndex parent ( const QModelIndex & index ) const { return QModelIndex(); }
	int rowCount ( const QModelIndex & parent ) const
	{
		if( !parent.isValid() )
			return d_rows.size();
		return 0;
	}
	bool setData ( const QModelIndex & index, const QVariant & value, int role )
	{
		if( index.column() == 1 && role == Qt::EditRole )
		{
			d_rows[ index.row() ].d_to = value.toUInt();
			dataChanged( index, index );
			return true;
		}
		return false;
	}
};

bool ReqIfParser::editMapping(QWidget* parent)
{
	QApplication::restoreOverrideCursor();
	QDialog dlg( parent );
	dlg.setWindowTitle( tr("Edit Attribute Mapping") );

	QVBoxLayout vbox( &dlg );

	QLabel l1( tr("For '%1'").arg( QFileInfo(d_path).fileName() ), &dlg );
	vbox.addWidget( &l1 );

	QTreeView tree( &dlg );
	tree.setEditTriggers( QAbstractItemView::AllEditTriggers );
	tree.setAlternatingRowColors( true );
	tree.setRootIsDecorated( false );
	tree.setAllColumnsShowFocus(true);
	vbox.addWidget( &tree );

	QDialogButtonBox bb( QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, &dlg );
	vbox.addWidget( &bb );

	connect(&bb, SIGNAL(accepted()), &dlg, SLOT(accept()));
	connect(&bb, SIGNAL(rejected()), &dlg, SLOT(reject()));

	ReqIFMappingMdl mdl;
	for( AttrCache::const_iterator i = d_attrCache.begin(); i != d_attrCache.end(); ++i )
		mdl.d_rows.append( ReqIFMappingMdl::Slot( i.key(), i.value().first, i.value().second ) );

	tree.setModel( &mdl );

	ReqIFMappingDelegate del;
	tree.setItemDelegate( &del );

	tree.resizeColumnToContents(0);
	tree.resizeColumnToContents(1);
	tree.setMinimumSize( 400, 300 );

	while( dlg.exec() == QDialog::Accepted )
	{
		QSet<quint32> test;
		bool ok = true;
		for( int i = 0; i < mdl.d_rows.size(); i++ )
		{
			if( mdl.d_rows[i].d_to != 0 )
			{
				if( test.contains( mdl.d_rows[i].d_to ) )
				{
					QMessageBox::critical( parent, dlg.windowTitle(), tr("The mapping is not unique!") );
					ok = false;
				}else
					test.insert( mdl.d_rows[i].d_to );
			}
		}
		if( ok )
		{
			for( int i = 0; i < mdl.d_rows.size(); i++ )
			{
				d_attrCache[mdl.d_rows[i].d_name].second = mdl.d_rows[i].d_to;
			}
			QApplication::setOverrideCursor( Qt::WaitCursor );
			return true;
		}
	}

	QApplication::setOverrideCursor( Qt::WaitCursor );
	return false;
}
