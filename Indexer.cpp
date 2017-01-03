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

#include "Indexer.h"
#include "TypeDefs.h"
#include "AppContext.h"
#include <QProgressDialog>
#include <QtDebug>
#include <QApplication>
#include <QProgressDialog>
#include <QDir>
#include <Txt/TextOutHtml.h>
#include <private/qindexwriter_p.h>
#include <private/qanalyzer_p.h>
#include <private/qindexreader_p.h>
#include <private/qquery_p.h>
#include <private/qsearchable_p.h>
#include <private/qhits_p.h>
#include <private/qqueryparser_p.h>
using namespace Ds;

typedef QVector< QPair<quint64,quint16> > ObjList;
typedef QHash<QString,ObjList> Dict;

// RISK
  class CLuceneError
  {
  public:
  	int error_number;
	char* _awhat;
	TCHAR* _twhat;
  };

static bool toIndex( quint32 t )
{
	switch( t )
	{
	case TypeTitle:
	case TypeSection:
	case TypeTable:
	case TypeTableRow:
	case TypeTableCell:
		return true;
	default:
		return false;
	}
}

static int count( const Sdb::Obj& super )
{
	int res = 0;
	Sdb::Obj sub = super.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( toIndex( sub.getType() ) )
		{
			res += 1;
			res += count( sub );
		}
	}while( sub.next() );
	return res;
}

static void indexWord( quint64 id, const QString& word, Dict& dict )
{
	ObjList& l = dict[ word ];
	if( !l.isEmpty() && l.last().first == id )
		l.last().second++;
	else
		l.append( qMakePair( id, quint16(1) ) );
}

static void indexString( quint64 id, const QString& text, Dict& dict )
{
    const QChar *buf = text.unicode();
    QChar str[64];
    QChar c = buf[0];
    int j = 0;
    int i = 0;
    while ( j < text.length() ) 
	{
        if ( ( c.isLetterOrNumber() || c == QLatin1Char('_') ) && i < 63 ) 
		{
            str[i] = c.toLower();
            ++i;
        } else 
		{
            if ( i > 1 )
                indexWord( id, QString(str,i), dict );
            i = 0;
        }
        c = buf[++j];
    }
    if ( i > 1 )
        indexWord( id, QString(str,i), dict );
}

static void indexBml( quint64 id, const QByteArray& bml, Dict& dict )
{
	Stream::DataReader r( bml );
	indexString( id, r.extractString(), dict );
}

static void indexHtml( quint64 id, const QString& text, Dict& dict )
{
	// Routine aus Qt Assistant index.cpp parseDocument

    if (text.isNull())
        return;

    bool valid = true;
    const QChar *buf = text.unicode();
    QChar str[64];
    QChar c = buf[0];
    int j = 0;
    int i = 0;
    while ( j < text.length() ) 
	{
        if ( c == QLatin1Char('<') || c == QLatin1Char('&') ) 
		{
            valid = false;
            if ( i > 1 )
                indexWord( id, QString(str,i), dict );
            i = 0;
            c = buf[++j];
            continue;
        }
        if ( ( c == QLatin1Char('>') || c == QLatin1Char(';') ) && !valid ) 
		{
            valid = true;
            c = buf[++j];
            continue;
        }
        if ( !valid ) 
		{
            c = buf[++j];
            continue;
        }
        if ( ( c.isLetterOrNumber() || c == QLatin1Char('_') ) && i < 63 ) 
		{
            str[i] = c.toLower();
            ++i;
        } else {
            if ( i > 1 )
                indexWord( id, QString(str,i), dict );
            i = 0;
        }
        c = buf[++j];
    }
    if ( i > 1 )
        indexWord( id, QString(str,i), dict );
}

static bool index( const Sdb::Obj& obj, Dict& dict, QProgressDialog& dlg )
{
	if( !toIndex( obj.getType() ) )
		return true;

	dlg.setValue( dlg.value() + 1 );
	if( dlg.wasCanceled() )
		return false;

	Stream::DataCell v = obj.getValue( AttrObjText );
	if( v.isBml() )
		indexBml( obj.getId(), v.getBml(), dict );
	else if( v.isHtml() )
		indexHtml( obj.getId(), v.getStr(), dict );
	else if( v.isString() )
		indexString( obj.getId(), v.getStr(), dict );
	else if( !v.isNull() )
		qWarning( "Indexer format not supported" );

	Sdb::Obj sub = obj.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( !index( sub, dict, dlg ) )
			return false;
	}while( sub.next() );
	return true;
}

static QString fetchHtml( const QString& html ) // not simplified
{
    return Txt::TextOutHtml::htmlToPlainText( html, false );
}

QString Indexer::fetchText( const Sdb::Obj& obj )
{
	if( obj.isNull() )
		return QString();
	QString res;
	Stream::DataCell v = obj.getValue( AttrObjText );
	if( v.isBml() )
	{
		Stream::DataReader r( v.getBml() );
		res = r.extractString();
	}else if( v.isHtml() )
		res = fetchHtml( v.getStr() );
	else if( v.isString() )
		res = v.getStr();
	else if( !v.isNull() )
		qWarning( "Indexer format not supported" );
	if( obj.getType() == TypeTitle )
	{
		QString nr = obj.getValue( AttrObjNumber ).getStr();
		if( !nr.isEmpty() )
			res += nr + " " + res;
	}
	return res;
}

static QString _aggregateTable( const Sdb::Obj& obj )
{
	QString res = Indexer::fetchText( obj );
	Sdb::Obj sub = obj.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( toIndex( sub.getType() ) )
		{
			res += _aggregateTable( sub );
			res += ' ';
		}
	}while( sub.next() );
	return res;
}

QString Indexer::fetchText( const Sdb::Obj& obj, bool aggregateTable )
{
	if( !aggregateTable )
		return fetchText( obj );
	QString res = fetchText( obj );
	if( obj.getType() == TypeTable )
	{
		res += _aggregateTable( obj );
		//qDebug() << res;
		return res;
	}else
		return res;
}

static int _findText( const QString& pattern, const QString& text )
{
    QRegExp expr(pattern);
    expr.setPatternSyntax(QRegExp::FixedString);
    expr.setCaseSensitivity(Qt::CaseInsensitive);
    return expr.indexIn(text.simplified());
}

Sdb::Obj Indexer::gotoNext( const Sdb::Obj& obj )
{
	if( obj.isNull() )
		return Sdb::Obj();
	if( obj.getType() != TypeTable )
	{
		Sdb::Obj next = obj.getFirstObj();
		if( !next.isNull() && toIndex( next.getType() ) )
			return next;
	}
	Sdb::Obj next = obj;
	while( !next.isNull() && next.getType() != TypeDocument )
	{
		if( next.next() ) // wenn false bleibt next auf ursprünglichem Objekt, bzw. wir nicht null.
		{
			// Es gibt einen nächsten
			if( toIndex( next.getType() ) )
				// Wenn es sich um den richtigen Typ handelt, ist es unser Objekt.
				return next;
		}else
			// Es gibt keinen nächsten. Gehe einen Stock nach oben. Der Owner wurde bereits behandelt, darum wieder next.
			next = next.getOwner();
	}
	return Sdb::Obj();
}

Sdb::Obj Indexer::gotoLast( const Sdb::Obj& obj )
{
	Sdb::Obj res = obj.getLastObj();
	if( res.isNull() || !toIndex( res.getType() ) )
		return obj;
	else
		return gotoLast( res );
}

Sdb::Obj Indexer::gotoPrev( const Sdb::Obj& obj )
{
	if( obj.isNull() )
		return Sdb::Obj();

	Sdb::Obj prev = obj;
	while( !prev.isNull() && prev.getType() != TypeDocument )
	{
		if( prev.prev() )
		{
			// Es gibt einen Vorgänger
			if( toIndex( prev.getType() ) && prev.getType() != TypeTable )
			{
				// Der Vorgänger hat den richtigen Type
				// Gehe zuunterst.
				return gotoLast( prev );
			}
		}else
		{
			// Es gibt keinen prev. Gehe also zum Owner. Dieser wurde noch nicht behandelt und ist unser Typ.
			return prev.getOwner();

		}
	}
	return Sdb::Obj(); 
}

Sdb::Obj Indexer::findText( const QString& pattern, const Sdb::Obj& cur, bool forward )
{
	if( cur.isNull() )
		return Sdb::Obj();
	Sdb::Obj obj = cur;
	while( !obj.isNull() )
	{
		//qDebug( "checking %d", obj.getValue( AttrObjRelId ).getInt32() );
		const QString text = fetchText( obj, true );
		const int hit = _findText( pattern, text );
		if( hit != -1 )
		{
			//qDebug( "hit!" );
			return obj;
		}
		if( forward )
			obj = gotoNext( obj );
		else
			obj = gotoPrev( obj );
	}
	return Sdb::Obj();
}

Indexer::Indexer( QObject* p ):QObject(p)
{
}

bool Indexer::exists()
{
	return QCLuceneIndexReader::indexExists( AppContext::inst()->getIndexPath() );
}

static QString collectText( const Sdb::Obj& title )
{
	QString str = Indexer::fetchText( title );
	Sdb::Obj o = title.getFirstObj();
	if( !o.isNull() ) do
	{
		switch( o.getType() )
		{
		case TypeTitle:
			break;
		case TypeSection:
			str += Indexer::fetchText( o );
			str += collectText( o );
			break;
		case TypeTableCell:
			str += Indexer::fetchText( o );
			break;
		case TypeTable:
		case TypeTableRow:
			str += collectText( o );
			break;
		}
	}while( o.next() );
	return str;
}

static void indexTitle( const Sdb::Obj& title, const Sdb::Obj& doc, QCLuceneIndexWriter& w, QCLuceneAnalyzer& a )
{
	QCLuceneDocument ld;
	const QString text = collectText( title );
	ld.add(new QCLuceneField(QLatin1String("id"),
		QString::number( title.getId(), 16 ), QCLuceneField::STORE_YES | QCLuceneField::INDEX_UNTOKENIZED ) );
	ld.add(new QCLuceneField(QLatin1String("content"), text, QCLuceneField::INDEX_TOKENIZED) );
	ld.add(new QCLuceneField(QLatin1String("doc"),
		QString::number( doc.getId(), 16 ), QCLuceneField::STORE_YES | QCLuceneField::INDEX_UNTOKENIZED ) );
	w.addDocument( ld, a );
}

static void iterateTitles( const Sdb::Obj& title, const Sdb::Obj& doc, 
						  QCLuceneIndexWriter& w, QCLuceneAnalyzer& a, QProgressDialog& progress )
{
	Sdb::Obj sec = title.getFirstObj();
	if( !sec.isNull() ) do
	{
		if( sec.getType() == TypeTitle )
		{
			indexTitle( sec, doc, w, a );
			progress.setValue( progress.value() + 1 );
			if( progress.wasCanceled() )
				return;
			iterateTitles( sec, doc, w, a, progress );
		}
	}while( sec.next() );
}

static int calcCount( const Sdb::Obj& super, QProgressDialog& progress )
{
	int res = 0;
	Sdb::Obj sub = super.getFirstObj();
	if( !sub.isNull() ) do
	{
		progress.setValue( progress.value() + 1 );
		if( progress.wasCanceled() )
			return -1;
		switch( sub.getType() )
		{
		case TypeDocument:
			res++; // für den Root
			res += calcCount( sub, progress ); // für die Titel
			break;
		case TypeFolder:
			res += calcCount( sub, progress );
			break;
		case TypeTitle:
			res++; // für den Titel und sein Body
			res += calcCount( sub, progress ); // für die Untertitel
			break;
		}
	}while( sub.next() );
	return res;
}

bool Indexer::indexDocument( const Sdb::Obj& doc )
{
	/* später
	d_error.clear();
	QString path = AppContext::inst()->getIndexPath();
	if( !QCLuceneIndexReader::indexExists( path ) )
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		QCLuceneStandardAnalyzer a;
		QCLuceneIndexWriter w( path, a, true );
		indexTitle( doc, doc, w, a ); // Index Root des Dokuments. id == doc
		iterateTitles( doc, doc, w, a );
		QApplication::restoreOverrideCursor();
		return true;
	}else
	{
		d_error = QLatin1String( "Lucene: " ) + tr("index does not exist!");
		return false;
	}
	*/
	return false;
}

static void iterateDocuments( const Sdb::Obj& o, QCLuceneIndexWriter& w, QCLuceneAnalyzer& a, QProgressDialog& progress )
{
	switch( o.getType() )
	{
	case TypeDocument:
		{
			indexTitle( o, o, w, a ); // Index Root des Dokuments. id == doc
			progress.setValue( progress.value() + 1 );
			if( progress.wasCanceled() )
				return;
			iterateTitles( o, o, w, a, progress );
		}
		break;
	case TypeFolder:
		{
			Sdb::Obj sub = o.getFirstObj();
			if( !sub.isNull() ) do
			{
				iterateDocuments( sub, w, a, progress );
			}while( sub.next() );
		}
		break;
	}
}

bool Indexer::indexRepository( QWidget* parent )
{
	d_error.clear();
	QString path = AppContext::inst()->getIndexPath();
	try
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		QApplication::processEvents();
		QCLuceneStandardAnalyzer a;
		QCLuceneIndexWriter w( path, a, true );
		w.setMinMergeDocs( 1000 );
		w.setMaxBufferedDocs( 100 );
		Sdb::Obj root = AppContext::inst()->getRoot();

		QApplication::processEvents();
		QProgressDialog progress( tr("Indexing repository..."), tr("Abort"), 0, 
			AppContext::inst()->getDb()->getMaxOid(), parent );
		progress.setAutoClose( false );
		// TODO setMaxFieldLength
		progress.setMinimumDuration( 0 );
		progress.setWindowTitle( tr( "DoorScope Search" ) );
		progress.setWindowModality(Qt::WindowModal);
		const int count = calcCount( root, progress );
		if( count == -1 )
		{
			QApplication::restoreOverrideCursor();
			return false;
		}
		progress.setRange( 0, count );
		progress.setAutoClose( true );

		Sdb::Obj o = root.getFirstObj();
		if( !o.isNull() ) do
		{
			iterateDocuments( o, w, a, progress );
			if( progress.wasCanceled() )
			{
				w.close();
				QDir dir( path );
				QStringList files = dir.entryList( QDir::Files );
				for( int i = 0; i < files.size(); i++ )
					dir.remove( files[i] );
				QApplication::restoreOverrideCursor();
				return false;
			}
		}while( o.next() );
		progress.setValue( count );
		QApplication::restoreOverrideCursor();
		return true;
	}catch( CLuceneError& e )
	{
		QApplication::restoreOverrideCursor();
		d_error = QString::fromLatin1( e._awhat );
		return false;
	}
}

bool Indexer::query( const QString& query, ResultList& result )
{
	d_error.clear();
	QString path = AppContext::inst()->getIndexPath();
	if( !QCLuceneIndexReader::indexExists( path ) )
	{
		d_error = QLatin1String( "Lucene: " ) + tr("index does not exist!");
		return false;
	}
	try
	{
		QCLuceneStandardAnalyzer a;
		QCLuceneQuery* q = QCLuceneQueryParser::parse( query, "content", a );
		if( q )
		{
			result.clear();
			QCLuceneIndexSearcher s( path );
			QCLuceneHits hits = s.search( *q );
			QApplication::setOverrideCursor( Qt::WaitCursor );
			for( int i = 0; i < hits.length(); i++ )
			{
				QCLuceneDocument doc = hits.document( i );
				Hit hit;
				hit.d_score = hits.score( i );
				hit.d_doc = AppContext::inst()->getTxn()->getObject( doc.get( "doc" ).toULongLong( 0, 16 ) );
				hit.d_title = AppContext::inst()->getTxn()->getObject( doc.get( "id" ).toULongLong( 0, 16 ) );
				if( hit.d_doc.isNull() || hit.d_title.isNull() || hit.d_doc.getType() != TypeDocument )
					continue;
				result.append( hit );
			}
			QApplication::restoreOverrideCursor();
			delete q;
			return true;
		}else
		{
			d_error = QLatin1String( "Lucene: " ) + tr("invalid query!");
			return false;
		}
	}catch( CLuceneError& e )
	{
		QApplication::restoreOverrideCursor();
		d_error = QLatin1String( "Lucene: " ) + QString::fromLatin1( e._awhat );
		return false;
	}
}
