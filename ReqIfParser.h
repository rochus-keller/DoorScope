#ifndef REQIFREADER_H
#define REQIFREADER_H

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
#include <QDomDocument>
#include <QMap>
#include <bitset>
#include <Stream/DataCell.h>

class QWidget;

namespace Ds
{
	class ReqIfParser : public QObject
    {
    public:
		static const char* s_placeHolderImage;
        enum SpecType { RelationGroupType, ObjectType, RelationType, SpecificationType, MaxType };
		explicit ReqIfParser(QObject *parent = 0);
		bool parseFile( const QString& path );
		bool editMapping(QWidget* parent);
        const QString& getError() const { return d_error; }
        static QDateTime parseDateTime( const QString& );
        static bool loadImage( QImage& img, const QString& src, const QString &w, const QString &h );
		static void addLocalAttr( quint32 id, const QString& name, const QString& defaultMapping = QString() );
    protected:
		struct Definition
        {
            QString d_id;
            QString d_longName;
            QString d_desc;
        };
        struct DataTypeDefinition : public Definition
        {
            enum Type {
                Boolean, Date, Enumeration, Integer, Real, String, Xhtml
            };
            Type d_type;
            QMap<QString,QString> d_enums; // Key->value
        };
        struct AttributeDef : public Definition
        {
            QString d_dataType;
            bool d_isEditable;
            Stream::DataCell d_default;
        };
        struct DefWithAtts : public Definition
        {
            SpecType d_type;
            QMap<QString,AttributeDef> d_atts;
        };
        struct ObjWithVals : public Definition
        {
            QMap<QString,Stream::DataCell> d_vals;
            QString d_ref;
            QDateTime d_lastChange;
        };
        struct SpecRelation : public ObjWithVals
        {
            QString d_source;
            QString d_target;
        };
        struct SpecHierarchy : public ObjWithVals
        {
            QList<SpecHierarchy> d_children;
            bool d_isTableInternal;
            SpecHierarchy():d_isTableInternal(false){}
        };
        struct RelationGroup : public ObjWithVals
        {
            QString d_source;
            QString d_target;
            QList<QString> d_relations;
        };

        bool readReqIfContent( const QDomNode& );
        bool readDataTypes( const QDomNode& );
        bool readSpecTypes( const QDomNode& );
        bool readSpecObjects( const QDomNode& );
        bool readSpecRelations( const QDomNode& );
        bool readSpecifications( const QDomNode& );
        bool readSpecHierarchy( const QDomNode&, SpecHierarchy& parent );
        bool readRelationGroups( const QDomNode& );
        bool readAttDefs( const QDomNode&, DefWithAtts& );
        bool readAttVals( const QDomNode&, ObjWithVals&, const DefWithAtts &def );
        Stream::DataCell readAttrValue( const QDomElement&, const AttributeDef& );
		void loadLocalMappings();
        static QString toHtml( const QDomNode & );
        void clearAll();
	protected:
        QMap<QString,DataTypeDefinition> d_dataTypes;
        QMap<QString,DefWithAtts> d_relationGroupTypes;
        QMap<QString,DefWithAtts> d_specObjectTypes;
        QMap<QString,DefWithAtts> d_specRelationTypes;
        QMap<QString,DefWithAtts> d_specTypes;
        QMap<QString,ObjWithVals> d_specObjects;
        QMap<QString,SpecRelation> d_specRelations;
        QMap<QString,SpecHierarchy> d_specifications;
        QMap<QString,RelationGroup> d_relationGroups;
		typedef QMap<QString,QPair<std::bitset<MaxType>,quint32> > AttrCache;
		AttrCache d_attrCache;
		QString d_error;
        QString d_path;
        QDomDocument d_doc;
	};
}

#endif // REQIFREADER_H
