#ifndef __TypeDefs__
#define __TypeDefs__

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

#include <QString>
#include <Stream/DataCell.h>

namespace Sdb
{
	class Database;
    class Obj;
}

namespace Ds
{
	enum ThreadKeeper_Numbers
	{
		DsStart = 0,
		DsMax = 65535 // ffff
	};

	struct IndexDefs
	{
		static const char* IdxDocId; // AttrDocId: Zeigt auf alle vorhandenen Versionen eines Dokuments
		static const char* IdxDocObjId; // AttrDocId, AttrObjIdent: Index auf UniqueID des Doc und Absolute Number des Obj
		static const char* IdxAnnDocNr;	// Index auf OID des Home Doc und Annotationslaufnummer
	};

	enum TypeDef_ContentObject // abstract
	{
		// TypeContentObject = DsStart + 0x00,
		AttrCreatedBy = DsStart + 2, // Created By, String
		AttrCreatedOn = DsStart + 3, // Created On, DateTime
		AttrModifiedBy = DsStart + 4, // Last Modified By, String
		AttrModifiedOn = DsStart + 5, // Last Modified On, DateTime
		AttrCreatedThru = DsStart + 6, // Created Thru
		AttrReviewStatus = DsStart + 7, // UInt8, markiert das Objekt als Done, etc.
		AttrReviewNeeded = DsStart + 8, // Bool, markiert zu revidierende Objekte
		AttrAnnotated = DsStart + 9, // Int32, Anzahl Annotations
		AttrReviewer = DsStart + 10, // String, Name der Person, die den ReviewStatus gesetzt hat
		AttrStatusDate = DsStart + 11, // DateTime, Datum des ReviewStatus
		AttrConsRevStat = DsStart + 12 // BML=[ frame stat [who when] end ]*, konsolidierter Review-Status
	};

	enum ReviewStatus
	{
		// Persistent
		ReviewStatus_None = 0,
		ReviewStatus_Pending = 1,
		ReviewStatus_Recheck = 2,
		ReviewStatus_Accepted = 3,
		ReviewStatus_AcceptedWithChanges = 4,
		ReviewStatus_Rejected = 5
	};

	enum TypeDef_Document // inherits ContentObject
	{
		TypeDocument = DsStart + 80,
		AttrDocId = DsStart + 1, // String, UniqueId(Module), ~moduleID
		AttrDocPath = DsStart + 91, // String, ~modulePath
		AttrDocVer = DsStart + 82, // String, ~moduleVersion
		AttrDocDesc = DsStart + 83, // String, ~moduleDescription, Description
		AttrDocName = DsStart + 84, // String, ~moduleName, Name
		// FullName = Path + / + Name
		AttrDocType = DsStart + 85, // String, ~moduleType
		AttrDocBaseline = DsStart + 86, // Bool, ~isBaseline
		AttrDocVerMaj = DsStart + 87, // Int, ~baselineMajor
		AttrDocVerMin = DsStart + 88, // Int, ~baselineMinor
		AttrDocVerSuff = DsStart + 89, // String, ~baselineSuffix
		AttrDocVerAnnot = DsStart + 90, // String, ~baselineAnnotation
		AttrDocVerDate = DsStart + 91, // Date, ~baselineDate
		AttrDocPrefix = DsStart + 92, // String, Prefix
		AttrDocExported = DsStart + 93, // Script Export Date
		AttrDocImported = DsStart + 94, // DoorScope Import Date
		AttrDocStream = DsStart + 95, // DoorScope Import Stream File
		AttrDocAttrs = DsStart + 96, // Referenz auf Obj/Liste von Custom Attributes
		AttrDocObjAttrs = DsStart + 97, // Referenz auf Obj/Liste von Custom Attributes
		AttrDocOwning = DsStart + 98, // Queue der nicht-aggregierten Objekte in Ownership von Doc
		AttrDocMaxAnnot = DsStart + 99, // UInt32, max. Annotationsnummer
		AttrDocDiffSource = DsStart + 100, // OID, Referenz auf Dokument, zu dem die Histo erzeugt wurde
		AttrDocAltName = DsStart + 101, // String oder leer, optionaler Override von AttrDocName
		AttrDocIndex = DsStart + 102 // BML mit Volltextindex oder Null
	};

	enum TypeDef_Object // abstract, inherits ContentObject
	{
		// Objekte sind Aggregate von Documents und sich selber
		TypeObject = DsStart + 256,
		AttrObjIdent = DsStart + 257,	// Type Int32|String (in DOORS Int32), Absolute Number, unique innerhalb Document
		AttrObjNumber = DsStart + 258,	// String,  ~number Object Number, Randziffer
		AttrObjAttrChanged = DsStart + 259, // Bool, berechnet: true wenn Hist mit Attrnamen
		AttrObjTextChanged = DsStart + 260, // Bool, berechnet: true wenn Hist mit Header/Text
		// AttrObjLevel ,	// ~level
		// AttrObjOutl ,	// ~outline
		// AttrObjTitle = DsStart + 0x105,	// Object Heading
		AttrObjShort = DsStart + 262,	// String, Object Short Text
		AttrObjText = DsStart + 263,	// String|BML|HTML, Object Heading oder Object Text
		AttrObjHomeDoc = DsStart + 264,	// OID, Referenz auf Document, welches Obj besitzt
		AttrObjDeleted = DsStart + 265,	// Bool, ~deleted, optional
		AttrObjDocId = DsStart + 272	// redundant UniqueId(Home Doc)
	};

	// NOTE: Doors-Objekte können gleichzeitig Titel und Body sein. Das geht hier nicht.
	// Ansonsten wäre Titel in anderem Level als Body; wenn aber Body ohne Titel wird
	// dieses Konzept durchbrochen.
	// Wenn Titel und Body, werden die Custom-Attribute dupliziert, Links dem Body zugeordnet.
	// ObjRelId ist damit nicht mehr unbedingt eindeutig, sondern kann von Titel und Section
	// geteilt werden. Es gibt aber pro RelId max. einen Titel und eine Section, nichts anderes.

	enum TypeDef_Title // inherits Object
	{
		TypeTitle = DsStart + 277,
		AttrTitleSplit = DsStart + 278 // OID, zeigt auf Body wenn Original TypeStubTitle hatte
	};

	enum TypeDef_Section // inherits Object
	{
		TypeSection = DsStart + 288,
		AttrSecSplit = DsStart + 289 // OID, zeigt auf Title wenn Original TypeStubTitle hatte
	};

	enum TypeDef_Link // inherits ContentObject
	{
		// TypeLink = DsStart + 0x150,
		// OutLink und InLink haben das referenzierte Objekt redundant als Aggregat
		AttrLnkModId = DsStart + 337, // ~linkModuleID, String
		AttrLnkModName = DsStart + 338 // ~linkModuleName, name
	};

	enum TypeDef_OutLink // inherits Link
	{
		TypeOutLink = DsStart + 368,
		AttrTrgObjId = DsStart + 369, // Int32|String (Int32 in DOORS) ~targetObjAbsNo
		AttrTrgDocId = DsStart + 370, // ~targetModID, String
		AttrTrgDocName = DsStart + 371, // ~targetModName, fullName
		AttrTrgDocVer = DsStart + 372, // ~targetModVersion
		AttrTrgDocModDate = DsStart + 373, // ~targetModLastModified
		AttrTrgDocIsBl = DsStart + 374 // ~isTargetModBaseline
	};

	enum TypeDef_InLink // inherits Link
	{
		TypeInLink = DsStart + 384,
		AttrSrcObjId = DsStart + 385, // Int32|String (Int32 in DOORS) ~sourceObjAbsNo
		AttrSrcDocId = DsStart + 386, // String, ~sourceModID
		AttrSrcDocName = DsStart + 387, // ~sourceModName, fullName
		AttrSrcDocVer = DsStart + 388, // ~sourceModVersion
		AttrSrcDocModDate = DsStart + 389, // ~sourceModLastModified
		AttrSrcDocIsBl = DsStart + 390 // ~isSourceModBaseline
	};

	enum TypeDef_Table // inherits Object
	{
		TypeTable = DsStart + 512
	};

	enum TypeDef_TableRow // inherits Object
	{
		TypeTableRow = DsStart + 544
	};

	enum TypeDef_TableCell // inherits Object
	{
		TypeTableCell = DsStart + 576
	};

	enum TypeDef_Picture // inherits Object
	{
		TypePicture = DsStart + 608,
		AttrPicImage = DsStart + 609, // PNG
		AttrPicWidth = DsStart + 610, // ~width
		AttrPicHeight = DsStart + 611 // ~height
	};

	enum TypeDef_History 
	{
		// Es werden primär Object- und Link-Meldungen gespeichert, inkl. Baseline
		// Insb. Type und AttrDef, Ole etc. interessieren wenig
		// History-Records werden in Documents und Objects in Queue-Slots gespeichert.
		TypeHistory = DsStart + 640,
		AttrHistAuthor = DsStart + 641, // ~author
		AttrHistDate = DsStart + 642, // ~date
		AttrHistType = DsStart + 643, // ~type übersetzt in quint8
		AttrHistObjId = DsStart + 644, // ~absNo
		AttrHistAttr = DsStart + 645, // ~attrName übersetzt in Atom
		AttrHistOld = DsStart + 646, // ~oldValue
		AttrHistNew = DsStart + 647, // ~newValue
		AttrHistInfo = DsStart + 648 // Intern, Optionale Information zu Operationen, QString
	};

	enum HistoryType
	{
		// Persistent Code
		HistoryType_unknown = 0,
		HistoryType_createType,
		HistoryType_modifyType, 
		HistoryType_deleteType, 
		HistoryType_createAttr, 
		HistoryType_modifyAttr, 
		HistoryType_deleteAttr, 
		HistoryType_createObject, 
		HistoryType_copyObject, 
		HistoryType_modifyObject, 
		HistoryType_deleteObject, 
		HistoryType_unDeleteObject, 
		HistoryType_purgeObject, 
		HistoryType_clipCutObject, 
		HistoryType_clipMoveObject, 
		HistoryType_clipCopyObject, 
		HistoryType_createModule, 
		HistoryType_baselineModule, 
		HistoryType_partitionModule, 
		HistoryType_acceptModule, 
		HistoryType_returnModule, 
		HistoryType_rejoinModule, 
		HistoryType_createLink, 
		HistoryType_modifyLink, 
		HistoryType_deleteLink, 
		HistoryType_insertOLE, 
		HistoryType_removeOLE, 
		HistoryType_changeOLE, 
		HistoryType_pasteOLE, 
		HistoryType_cutOLE, 
		HistoryType_readLocked
	};

	enum TypeDef_Folder // inherits ContentObject
	{
		// Folder enthalten Unterordner oder Dokumente als aggregierte Objekte
		// Root ist selber ein Folder in dem Sinne, dass auch er Unterordner oder Dokumente 
		// als aggregierte Objekte enthält.

		TypeFolder = DsStart + 768,
		AttrFldrName = DsStart + 769,
		AttrFldrExpanded = DsStart + 770
	};

	enum TypeDef_Annotation // inherits ContentObject
	{
		TypeAnnotation = DsStart + 784,
		//AttrAnnObj =  // Referenz auf Document, Link oder Object. TODO: unnötig, da Annotation als Subobject
		AttrAnnNr = DsStart + 785, // UInt32, Laufnummer der Annotation
		AttrAnnAttr = DsStart + 786, // Optional: Attribut, auf das sich Annotation bezieht (Atom)
		AttrAnnPos = DsStart + 787, // Optional: Zeichenposition in einem Textfeld
		AttrAnnLen = DsStart + 788, // Optional: Länge des annotierten Bereichs in einem Textfeld
		AttrAnnText = DsStart + 789, // Text der Annotation
		AttrAnnHomeDoc = DsStart + 790, // OID, Referenz auf übergeordnetes Dokument
		AttrAnnPrio = DsStart + 791, // UInt8, Enum AnnPrio
		AttrAnnOldNr = DsStart + 792 // String, Referenz auf Nummer in der Quelldatenbank
		// TODO: Streichen oder WaveUnderline, ev. QSyntaxHighlighter
	};

	enum AnnPrio
	{
		// Persistent
		AnnPrio_None = 0,
		AnnPrio_NiceToHave = 1,
		AnnPrio_NeedToHave = 2,
		AnnPrio_Urgent = 3
	};

	enum TypeDef_Stub // inherits Object
	{
		// Dies ist die redundante Kopie des vom einem Link auf der jeweils anderen 
		// Seite berührten Objects.
		TypeStub = DsStart + 800,
		AttrStubTitle = DsStart + 801 // Object Heading
	};

	enum TypeDef_LuaScript // inherits ContentObject
	{
		TypeLuaScript = DsStart + 820,
		TypeLuaFilter = DsStart + 823,
		AttrScriptName = DsStart + 821,
		AttrScriptSource = DsStart + 822
	};

	enum TypeDef_Root
	{
	};

	struct TypeDefs
	{
		static const char* historyTypeString[];
		static const char* historyTypePrettyString[];
		static void init( Sdb::Database& db );
        static quint32 findAtom( const char* name, quint32 type = 0 );
        static quint32 findAtom( Sdb::Database*, const char* name, quint32 type = 0 );
        static const char* getPrettyName( quint32 atom );
		static QByteArray getPrettyName( quint32 atom, Sdb::Database* ); // alles
        static QByteArray getSimpleName( quint32 atom, Sdb::Database* ); // alles
		static QString elided( const Stream::DataCell&, quint32 len = 50 );
		static QString formatDate( const QDateTime& );
		static QString extractName( const QString& fullName );
		static QString formatDocName( const Sdb::Obj& doc, bool full = true );
        static QVariant prettyValue( const Stream::DataCell& v );
    };
}
#endif // __TypeDefs__
