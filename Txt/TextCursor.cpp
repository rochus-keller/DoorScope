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

#include "TextCursor.h"
#include "ImageGlyph.h"
#include <QTextBlock>
#include <QTextList>
#include <QTextDocument>
#include <QTextTable>
#include <QUrl>
#include <QFileInfo>
#include <cassert>
using namespace Txt;

static void selectWholeBlock( QTextCursor& cur )
{
	const int anchor = (cur.anchor()<=cur.position())?cur.anchor():cur.position();
	const int position = (cur.anchor()<=cur.position())?cur.position():cur.anchor();
	
	const QTextBlock pb = cur.block();
	cur.setPosition( anchor );

	cur.movePosition(QTextCursor::StartOfBlock);
	cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	while( cur.block().position() != pb.position() )
	{
		cur.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	}
}

TextCursor::TextCursor( QTextCursor& cur, const Styles* s ):
	d_cur( cur ), d_styles( s )
{
	if( d_styles == 0 )
		d_styles = Styles::inst();
}

TextCursor::TextCursor( QTextDocument* doc, const Styles* s ):
	d_cur( doc ), d_styles( s )
{
	if( d_styles == 0 )
		d_styles = Styles::inst();
}

TextCursor::TextCursor( const TextCursor& rhs )
{
	d_styles = 0;
	*this = rhs;
}

TextCursor& TextCursor::operator=( const TextCursor& rhs )
{
	d_cur = rhs.d_cur;
	d_styles = rhs.d_styles;
	return *this;
}

QTextDocumentFragment TextCursor::getSelection() const 
{ 
	return QTextDocumentFragment( d_cur ); 
}

QString TextCursor::getSelectedText() const
{
	return d_cur.selectedText();
}

QTextCharFormat TextCursor::getCharFormat () const
{
	return d_cur.charFormat();
}

void TextCursor::removeSelectedText()
{
	d_cur.removeSelectedText();
}

bool TextCursor::isSuper() const
{
	return d_styles->isSuper( d_cur.charFormat() );
}

bool TextCursor::isSub() const
{
	return d_styles->isSub( d_cur.charFormat() );
}

bool TextCursor::isUnder() const
{
	return d_styles->isUnder( d_cur.charFormat() );
}

bool TextCursor::isStrike() const
{
	return d_styles->isStrike( d_cur.charFormat() );
}

bool TextCursor::isEm() const
{
	return d_styles->isEm( d_cur.charFormat() );
}

void TextCursor::setSuper( bool on )
{
	if( on )
	{
		removeCharFormat( d_cur, d_styles->getSub() );
		d_cur.mergeCharFormat( d_styles->getSuper() );
	}else
		removeCharFormat( d_cur, d_styles->getSuper() );
}

void TextCursor::setSub( bool on )
{
	if( on )
	{
		removeCharFormat( d_cur, d_styles->getSuper() );
		d_cur.mergeCharFormat( d_styles->getSub() );
	}else
		removeCharFormat( d_cur, d_styles->getSub() );
}

void TextCursor::setEm( bool on )
{
	if( on )
		d_cur.mergeCharFormat( d_styles->getEm() );
	else
		removeCharFormat( d_cur, d_styles->getEm() );
}

bool TextCursor::isStrong() const
{
	return d_styles->isStrong( d_cur.charFormat() );
}

void TextCursor::setStrong( bool on )
{
	if( on )
		d_cur.mergeCharFormat( d_styles->getStrong() );
	else
		removeCharFormat( d_cur, d_styles->getStrong() );
}

bool TextCursor::isFixed() const
{
	return d_styles->isFixed( d_cur.charFormat() );
}

void TextCursor::setFixed( bool on )
{
	if( on )
		d_cur.mergeCharFormat( d_styles->getFixed() );
	else
		removeCharFormat( d_cur, d_styles->getFixed() );
}

void TextCursor::setUnder( bool on )
{
	if( on )
		d_cur.mergeCharFormat( d_styles->getUnder() );
	else
		removeCharFormat( d_cur, d_styles->getUnder() );
}

void TextCursor::setStrike( bool on )
{
	if( on )
		d_cur.mergeCharFormat( d_styles->getStrike() );
	else
		removeCharFormat( d_cur, d_styles->getStrike() );
}

bool TextCursor::canSetCharFormat() const 
{
	return isSelection( d_cur, Styles::PAR ) || 
			isSelection( d_cur, Styles::CODE ) || 
			isSelection( d_cur, Styles::LST );
}

bool TextCursor::setParagraphStyle( Styles::ParagraphStyle s ) 
{
	if( s <= Styles::INVALID_PARAGRAPH_STYLE || s >= Styles::MAX_PARAGRAPH_STYLE )
		return false;
	return applyStyle( d_cur, d_styles, (Styles::ParagraphStyle)s );
}

void TextCursor::setDefaultCharFormat( Styles::ParagraphStyle s )
{
	if( s <= Styles::INVALID_PARAGRAPH_STYLE || s >= Styles::MAX_PARAGRAPH_STYLE )
		return;
	d_cur.setCharFormat( d_styles->getCharFormat( (Styles::ParagraphStyle)s ) );
}

bool TextCursor::canSetParagraphStyle( int s ) const
{
	if( s <= Styles::INVALID_PARAGRAPH_STYLE || s >= Styles::MAX_PARAGRAPH_STYLE )
		return false;
	return canApplyStyle( d_cur, (Styles::ParagraphStyle)s );
}

Styles::ParagraphStyle TextCursor::getParagraphStyle() const
{
	return (Styles::ParagraphStyle)d_cur.blockFormat().intProperty( Styles::PropBlockType );
}

bool TextCursor::canAddList() const
{
	return canAddListItem( d_cur );
}

bool TextCursor::canAddCodeBlock() const
{
	return canAddObject( d_cur );
}

bool TextCursor::addList( QTextListFormat::Style s )
{
	d_cur.movePosition( QTextCursor::EndOfBlock );
	return addList( d_cur, d_styles, s );
}

bool TextCursor::addItemLeft(bool jumpout )
{
	if( !inList() )
		return false;

	if( jumpout )
	{
		// Gehe zum Paragraph nach der Liste. Es sollte immer einen geben.
		QTextFrame* f = d_cur.currentFrame();
		assert( f );
		d_cur = f->lastCursorPosition();
		d_cur.movePosition( QTextCursor::NextBlock );
		return true;
	}

	QTextList* list = d_cur.currentList();
	QTextList* outer = 0;
	QTextBlock b = d_cur.block();
	if( list->format().hasProperty( Styles::PropListOwner ) )
	{
		outer = dynamic_cast<QTextList*>( b.document()->object( 
			list->format().intProperty( Styles::PropListOwner ) ) );
		assert( outer );
		assert( !outer->isEmpty() );
	}


	// Suche den letzten Block des aktuellen Levels
	QTextBlock last = b;
	while( b.isValid() && b.textList() == list )
	{
		last = b;
		b = b.next();
	}

	if( outer )
	{
		// Füge nach dem letzten Block des aktuellen Levels einen neuen Block ein
		d_cur.setPosition( last.position() );
		d_cur.movePosition( QTextCursor::EndOfBlock );
		d_cur.beginEditBlock();
		d_cur.insertBlock();		
		outer->add( d_cur.block() );
		d_cur.endEditBlock();
	}else
	{
		d_cur.setPosition( b.position() );
	}

	return true;
}

int TextCursor::getListStyle() const
{
	QTextList* list = d_cur.currentList();
	if( list == 0 )
		return 0;
	else
		return list->format().style();
}

void TextCursor::setListStyle( QTextListFormat::Style s )
{
	QTextList* list = d_cur.currentList();
	if( list == 0 )
		return;
	switch( s )
	{
	case QTextListFormat::ListDisc:
	case QTextListFormat::ListCircle:
	case QTextListFormat::ListSquare:
	case QTextListFormat::ListDecimal:
	case QTextListFormat::ListLowerAlpha:
	case QTextListFormat::ListUpperAlpha:
		break;
	default:
		return;
	}
	QTextListFormat lf = list->format();
	lf.setStyle( (QTextListFormat::Style)s );
	list->setFormat( lf );
}

bool TextCursor::indentList()
{
	if( !isSelection( d_cur, Styles::LST ) )
		return false;
	QTextBlockFormat f;
	f.setIndent( d_cur.blockFormat().indent() + 1 );
	d_cur.mergeBlockFormat( f );
	return true;
}

bool TextCursor::indentList( quint8 level )
{
	if( !isSelection( d_cur, Styles::LST ) )
		return false;
	if( level == 0 )
		return unindentList( true );
	const int indent = d_cur.blockFormat().indent(); // Qt: 0..n
	if( level == indent )
		return true;

	if( level > indent + 1 ) // Korrigiere grössere Einrückungen als 1
		level = indent + 1;

	QTextBlockFormat f;
	f.setIndent( level ); // Qt: 0..n
	d_cur.mergeBlockFormat( f );
	return true;
}

bool TextCursor::unindentList(bool all)
{
	if( !isSelection( d_cur, Styles::LST ) )
		return false;
	QTextBlockFormat f;
	const int indent = d_cur.blockFormat().indent();
	if( indent == 0 && !all )
		return false;
	if( all )
		f.setIndent( 0 );
	else
		f.setIndent( indent - 1 );
	d_cur.mergeBlockFormat( f );
	return true;
}

bool TextCursor::canAddTable() const
{
	return canAddObject( d_cur );
}

void TextCursor::addTable( quint16 rows, quint16 cols, bool fullWidth )
{
	const bool atStart = d_cur.atStart();
	QTextTableFormat tf;
	if( fullWidth )
	{
		QTextLength w( QTextLength::PercentageLength, 100.0 / cols );
		QVector<QTextLength> l;
		l.fill( w, cols );
		tf.setColumnWidthConstraints( l );
	}
	tf.setBorder( 1 );
	tf.setMargin( 0 );
	tf.setPadding( 0 );
	tf.setCellSpacing( 0 );
	tf.setCellPadding( 3 );
	tf.setProperty( Styles::PropFrameType, Styles::TBL );
	d_cur.insertTable( rows, cols, tf );
	if( atStart )
		d_cur.setCharFormat( d_styles->getCharFormat( Styles::PAR ) );
}

void TextCursor::addTableRow()
{
	QTextTable * tbl = d_cur.currentTable();
	if( tbl == 0 )
		return;

	tbl->insertRows( tbl->rows() - 1, 1 );
	d_cur = tbl->cellAt( tbl->rows() - 1, 0 ).firstCursorPosition();
}

void TextCursor::gotoRowCol( quint16 row, quint16 col )
{
	QTextTable * tbl = d_cur.currentTable();
	if( tbl == 0 )
		return;
	if( row >= tbl->rows() || col >= tbl->columns() )
		return;
	d_cur = tbl->cellAt( row, col ).firstCursorPosition();
}

QPair<quint16,quint16> TextCursor::getTableSize() const
{
	QTextTable * tbl = d_cur.currentTable();
	if( tbl == 0 )
		return qMakePair( quint16(0), quint16(0) );
	return qMakePair( quint16(tbl->rows()), quint16(tbl->columns()) );
}

void TextCursor::resizeTable( quint16 rows, quint16 cols, bool fullWidth )
{
	QTextTable * tbl = d_cur.currentTable();
	if( tbl == 0 )
		return;

	QTextTableFormat tf = tbl->format();
	if( fullWidth )
	{
		QTextLength w( QTextLength::PercentageLength, 100.0 / cols );
		QVector<QTextLength> l;
		l.fill( w, cols );
		tf.setColumnWidthConstraints( l );
	}
	tbl->resize( rows, cols );
	tbl->setFormat( tf );
}

void TextCursor::mergeTableCells()
{
	QTextTable * tbl = d_cur.currentTable();
	if( tbl == 0 )
		return;
	tbl->mergeCells( d_cur );
}

bool TextCursor::inCodeBlock() const
{
	return isSelection( d_cur, Styles::CODE );
}

bool TextCursor::atBeginOfCodeLine() const
{
	return inCodeBlock() && d_cur.atBlockStart();
}

bool TextCursor::indentCodeLines()
{
	if( !isSelection( d_cur, Styles::CODE ) )
		return false;
	QTextBlockFormat f;
	f.setIndent( d_cur.blockFormat().indent() + 1 );
	d_cur.mergeBlockFormat( f );
	return true;
}

bool TextCursor::unindentCodeLines()
{
	if( !isSelection( d_cur, Styles::CODE ) )
		return false;
	QTextBlockFormat f;
	const int indent = d_cur.blockFormat().indent();
	if( indent == 0 )
		return false;
	f.setIndent( indent - 1 );
	d_cur.mergeBlockFormat( f );
	return true;
}

bool TextCursor::isUrl() const
{
	QTextCharFormat f = d_cur.charFormat();
	return f.isAnchor() && !f.hasProperty( Styles::PropLink );
}

QUrl TextCursor::getUrl() const
{
	return QUrl::fromEncoded( d_cur.charFormat().anchorHref().toAscii() );
}

QByteArray TextCursor::getUrlEncoded() const
{
	return d_cur.charFormat().anchorHref().toAscii();
}

QString TextCursor::getUrlText() const
{
	if( isUrl() )
		return getSelectedText();
	else
		return QString();
}

void TextCursor::insertUrl( const QString& uri, bool encoded, const QString& text )
{
	QString s = uri.simplified();
	QTextCharFormat f = d_styles->getUrl();
	f.setAnchor( true );
	if( encoded )
		f.setAnchorHref( s );
	else
	{
		QUrl tmp;
		tmp.setUrl( s, QUrl::TolerantMode );
		f.setAnchorHref( tmp.toEncoded() );
	}
	if( text.isEmpty() )
        d_cur.insertText( createUrlText( QUrl::fromEncoded( f.anchorHref().toAscii() ) ), f );
	else
		d_cur.insertText( text, f );
	removeCharFormat( d_cur, f );
}

void TextCursor::insertUrl( const QUrl& url, const QString& text )
{
	QTextCharFormat f = d_styles->getUrl();
	f.setAnchor( true );
	f.setAnchorHref( url.toEncoded() );
	if( text.isEmpty() )
        d_cur.insertText( createUrlText( url ), f );
	else
		d_cur.insertText( text, f );
	removeCharFormat( d_cur, f );
}

void TextCursor::addCodeBlock()
{
	d_cur.movePosition( QTextCursor::EndOfBlock );
	addCodeBlock( d_cur, d_styles );
}

void TextCursor::deleteCharOrSelection( bool left )
{
	if( left )
	{
		if( d_cur.hasSelection() )
			d_cur.deletePreviousChar(); // lösche einfach Selektion
		else
		{
			if( !d_cur.movePosition( QTextCursor::PreviousCharacter ) )
				return;
			adjustSelection();
			// Wenn links ein Anchor war, ist er nun selektiert, und deleteChar löscht Selektion
			d_cur.deleteChar();
		}
	}else
	{
		if( d_cur.hasSelection() )
			d_cur.deleteChar(); // lösche einfach Selektion 
		else
		{
			if( !d_cur.movePosition( QTextCursor::NextCharacter ) )
				return;
			adjustSelection();
			// Wenn rechts ein Anchor war, ist er nun selektiert, und deletePrevious löscht ganze Selektion.
			d_cur.deletePreviousChar();
		}
	}
    // TODO emit updateCurrentCharFormat();
}

bool TextCursor::calcAnchorBounds( const QTextCursor& cur, int& left, int& right )
{
	// Annahmen:
	// 1) ein Anchor verläuft nie über einen Block oder einen Soft-Break hinweg.
	// 2) Curor:position() ist zwischen den Zeichen. pos=0 hat also nur rechts Zeichen.
	// 3) Jeder Block endet mit einem Zeilenumbruchzeichen. Die erste position() eines Blocks liegt unmittelbar
	//    rechts vom Zeilenumbruchzeichen (oder vom Dokumentenanfang).
	// 4) Ein Cursor kann vor oder nach dem Zeilenumbruchzeichen stehen. Die position() unterscheiden sich um 1.
	// 5) the "current block" is the block that contains the cursor position().
	// 6) cur.anchor() kann nie innerhalb einem Anchor-Fragment liegen.

	QTextFragment fragPos; // Das Fragment, in dem sich cur.position() befindet.
	for( QTextBlock::Iterator it = cur.block().begin(); !it.atEnd(); ++it )
	{
		QTextFragment f = it.fragment();
		if( f.contains( cur.position() ) )
		{
			fragPos = f;
			break;
		}
	}
	if( !fragPos.isValid() )
		return false;
	if( fragPos.position() == cur.position() )
		return false; // Wir stehen gleich am linken Ende des fragPosments, also nicht drin.
	QTextCharFormat f = fragPos.charFormat();
	if( f.isAnchor() )
	{
		left = fragPos.position();
		right = fragPos.position() + fragPos.length();
		return true;
	}else
		return false;
}

void TextCursor::adjustSelection()
{
	if( d_cur.hasComplexSelection() )
		return;


	int pos_left, pos_right;
	if( !calcAnchorBounds( d_cur, pos_left, pos_right ) )
		return; // Cursor ist nicht auf einem Anchor
	assert( pos_left < pos_right ); // Ein Fragment ist mindestens eine Stelle lang.
	// pos_right ist bereits der Anfang des Folgefragments

	const int anch = d_cur.anchor();
	const int pos = d_cur.position();
	if( anch == pos ) // Keine Selektion vorhanden
	{
		if( pos == ( pos_right - 1 ) )
		{
			// Wir kommen mit dem Cursor von rechts
			d_cur.setPosition( pos_right );
			d_cur.setPosition( pos_left, QTextCursor::KeepAnchor );
		}else
		{
			d_cur.setPosition( pos_left );
			d_cur.setPosition( pos_right, QTextCursor::KeepAnchor );
		}
	}else
	{
		if( anch < pos )
		{
			// Selektiere von Anchor in Richtung rechts
			if( anch > pos_left )
				d_cur.setPosition( pos_left );
			d_cur.setPosition( pos_right, QTextCursor::KeepAnchor );
		}else
		{
			// Selektiere von Anchor in Richtung links
			if( anch < pos_right )
				d_cur.setPosition( pos_right );
			d_cur.setPosition( pos_left, QTextCursor::KeepAnchor );
		}
	}
}

void TextCursor::insertText( const QString& text )
{
	if( isUrl() )
		d_cur.insertText( text, getDefault() );
	else
		d_cur.insertText( text );
	// TODO emit selectionChanged();
    // TODO emit updateCurrentCharFormat();
}

void TextCursor::insertImg( const QImage& img, int w, int h )
{
	ImageGlyph::regHandler( d_cur.block().document() );
	QTextCharFormat f = d_cur.charFormat();
	if( f.isAnchor() )
		f = getDefault();
	f.setObjectType( ImageGlyph::Type );
	f.setProperty( Styles::PropImage, QVariant::fromValue( img ) );
	if( w && h )
		f.setProperty( Styles::PropSize, QVariant::fromValue( QSize( w, h ) ) );
	d_cur.insertText( QString(QChar::ObjectReplacementCharacter), f );
}

QTextCharFormat TextCursor::getDefault() const
{
	if( d_cur.charFormat().isAnchor() || d_cur.atStart() ) // vorher atBlockStart
	{
		return d_styles->getDefaultFormat( d_cur );
	}else
	{
		QTextCharFormat fmt = d_cur.charFormat();
		fmt.clearProperty(QTextFormat::ObjectType); // wie in QTextCursor
		return fmt;
    }
}

QString TextCursor::createUrlText(const QUrl& url)
{
    if( url.scheme().toLower() == "file" )
    {
        QFileInfo info( url.toLocalFile() );
        return info.fileName();
    }else
    {
        const int pos = url.path().lastIndexOf( QChar('/') );
        if( pos != -1 )
            return url.host() + QLatin1String("/...") + url.path().mid( pos );
        else
            return url.host() + QChar('/') + url.path();
    }
}

void TextCursor::addParagraph( bool softBreak )
{
    QTextCharFormat fmt = getDefault();
	if (softBreak)
		d_cur.insertText(QString(QChar::LineSeparator), fmt);
	else
	{
		const Styles::ParagraphStyle s = getParagraphStyle();
		const Styles::ParagraphStyle ns = d_styles->getNextStyle( s );
		d_cur.insertBlock();
		d_cur.setCharFormat( fmt );
		if( s != ns )
			setParagraphStyle( ns );
	}
    // TODO emit updateCurrentCharFormat();
}

void TextCursor::insertFragment(const QTextDocumentFragment& f)
{
    QTextCharFormat fmt = getDefault();
	d_cur.setCharFormat( fmt ); // RISK
    d_cur.insertFragment( f );
	// TODO emit selectionChanged();
}

void TextCursor::gotoStart()
{
	d_cur.movePosition( QTextCursor::Start, QTextCursor::MoveAnchor );
}

void TextCursor::gotoEnd()
{
	// Funktioniert nicht. d_cur.movePosition( QTextCursor::End, QTextCursor::MoveAnchor );
	d_cur.setPosition( d_cur.block().document()->rootFrame()->lastPosition() );
	// Funktioniert beides nicht.
}

static QByteArray space( int indent )
{
	QByteArray s;
	for( int i = 0; i < indent; i++ )
		s += " ";
	return s;
}

static void _dump( const QTextFormat& tf, const char* title, int indent )
{
	QMap<int, QVariant> p = tf.properties();
	QMap<int, QVariant>::const_iterator i;
	for( i = p.begin(); i != p.end(); ++i )
		qDebug( "%s%s 0x%x=%s", space( indent ).data(), title,
			i.key(), i.value().toString().toLatin1().data() );
}

static void _dump( const QTextFragment& tf, int indent )
{
	qDebug( "%sFrag '%s'", space( indent ).data(), 
		tf.text().toLatin1().data() );
	_dump( tf.charFormat(), "CF", indent + 1 );
}

static void _dump( QTextBlock b, int indent )
{
	QTextBlockFormat bf = b.blockFormat();
	qDebug( "%sBlock pos=%d oid=%d", space( indent ).data(), 
		b.position(), bf.objectIndex() );

	_dump( bf, "BF", indent + 1 );

	QTextBlock::iterator it;
    for (it = b.begin(); !(it.atEnd()); ++it) 
	{
        QTextFragment currentFragment = it.fragment();
        if (currentFragment.isValid())
            _dump(currentFragment, indent + 1 );

    }
}

static void _dump( QTextFrame* f, int indent )
{
	qDebug( "%sFrame id=%d", space( indent ).data(), f->objectIndex() );
	QTextFrame::iterator i;
	for( i = f->begin(); i != f->end(); ++i )
	{
		if( i.currentFrame() )
			_dump( i.currentFrame(), indent + 1 );
		else
			_dump( i.currentBlock(), indent + 1 );
	}
}

void TextCursor::dump( const QTextDocument* doc)
{
	qDebug( "****** Dumping TextDocument *******" );
	_dump( doc->rootFrame(), 0 );
}

void TextCursor::dump() const
{
	dump( d_cur.block().document() );
}

bool TextCursor::canApplyStyle( const QTextCursor& cur, Styles::ParagraphStyle s ) 
{
	return isSelection( cur, Styles::PLAIN );
}

bool TextCursor::canAddObject( const QTextCursor& cur ) 
{
	return isSelection( cur, Styles::PLAIN );
}

bool TextCursor::applyStyle( QTextCursor& cur, const Styles* styles, Styles::ParagraphStyle s )
{
	if( !canApplyStyle( cur, s ) )
		return false;
	// Erweitere Selection auf Blockgrenze.
	const int anch = cur.anchor();
	const int pos = cur.position();

	bool res = cur.hasSelection();

	selectWholeBlock( cur );
	res = cur.hasSelection(); // Irgendwie bewirkt dieser Aufruf, dass der Font richtig ist.
	cur.setBlockFormat( styles->getBlockFormat(s) );
	cur.mergeCharFormat( styles->getCharFormat( s ) );
	// TODO: obiges setzt Font nur, wenn schon Text im Block ist.
	// cur.clearSelection();
	// cur.setCharFormat( f.d_char );
	cur.setPosition( anch );
	cur.setPosition( pos, QTextCursor::KeepAnchor );
	return true;
}

bool TextCursor::addList( QTextCursor& cur, const Styles* styles, QTextListFormat::Style s, bool create )
{
	cur.beginEditBlock();

	QTextList* owner =  cur.currentList();
	QTextListFormat f;
	f.setStyle( s ); // RISK
	if( owner )
	{
		f.setProperty( Styles::PropListOwner, owner->objectIndex() );
		f.setIndent( owner->format().indent() + 1 );
	}

	if( owner == 0 )
	{
		// Wir sind noch nicht in der Liste, eine neue wird erstellt.
		cur.insertFrame( styles->getListFrameFormat() ); // TODO: insertFrame bereinigen
		// insertFrame fügt zwei Blocks ein ohne irgendwelche Formate.
		QTextCursor next = cur;
		next.movePosition( QTextCursor::NextBlock );
		next.setBlockFormat( styles->getBlockFormat( Styles::PAR ) );
		next.setCharFormat( styles->getCharFormat( Styles::PAR ) );
		// qDebug( "*Frame inserted" );
		// dump( wnd()->document()->rootFrame(), 0 ); // TEST
		cur.createList( f );
	}else
	{
		// Es gibt bereits eine Liste, wir erzeugen eine neue Ebene.

		// insertList erzeugt einen neuen Block (im Gegensatz zu createList, 
		// das den aktuellen verwendet).
		if( create )
			cur.insertList( f );
		else
			cur.createList( f );
	}
	cur.setCharFormat( styles->getCharFormat( Styles::PAR ) );

	// RISK: sobald man setBlockCharFormat() macht, ist das Frame weg.
	cur.mergeBlockCharFormat( styles->getCharFormat( Styles::PAR ) );
	cur.endEditBlock();
	return true;
}

bool TextCursor::canAddListItem( const QTextCursor& cur )
{
	// RISK: 7.6.07: neu nur noch einstufige Listen unterstützt
	return canAddObject( cur ); // || cur.currentList();
}

bool TextCursor::addCodeBlock( QTextCursor& cur, const Styles* styles )
{
	cur.beginEditBlock();

	cur.insertFrame( styles->getCodeFrameFormat() ); // TODO: insertFrame bereinigen
	// insertFrame fügt zwei Blocks ein ohne irgendwelche Formate. Setze es auf PAR.
	QTextCursor next = cur;
	next.movePosition( QTextCursor::NextBlock );
	next.setBlockFormat( styles->getBlockFormat( Styles::PAR ) );
	next.setCharFormat( styles->getCharFormat( Styles::PAR ) );

	// RISK: sobald man setBlockCharFormat() macht, ist das Frame weg.
	cur.setCharFormat( styles->getCharFormat( Styles::_CODE ) );
	cur.setBlockFormat( styles->getBlockFormat( Styles::_CODE ) );
	cur.endEditBlock();
	return true;
}

bool TextCursor::isSelection( const QTextCursor& cur, Styles::ParagraphStyle s )
{
	if( !cur.hasSelection() )
		return cur.blockFormat().intProperty( Styles::PropBlockType ) == s;
	// else

	const int anchor = (cur.anchor()<=cur.position())?cur.anchor():cur.position();
	const int position = (cur.anchor()<=cur.position())?cur.position():cur.anchor();
	
	const QTextBlock pb = cur.block();
	QTextCursor tmp = cur;
	tmp.setPosition( anchor );
	if( tmp.blockFormat().intProperty( Styles::PropBlockType ) != s )
		return false;
	while( tmp.block().position() != pb.position() )
	{
		if( tmp.blockFormat().intProperty( Styles::PropBlockType ) != s )
			return false;
		tmp.movePosition(QTextCursor::NextBlock);
	}
	return true;
}

bool TextCursor::isSelection( const QTextCursor& cur, Styles::FrameType s )
{
	if( !cur.hasSelection() )
		return cur.currentFrame()->frameFormat().intProperty( Styles::PropFrameType ) == s;
	// else

	const int anchor = (cur.anchor()<=cur.position())?cur.anchor():cur.position();
	const int position = (cur.anchor()<=cur.position())?cur.position():cur.anchor();
	
	const QTextBlock pb = cur.block();
	QTextCursor tmp = cur;
	tmp.setPosition( anchor );
	if( tmp.currentFrame()->frameFormat().intProperty( Styles::PropFrameType ) != s )
		return false;
	while( tmp.block().position() != pb.position() )
	{
		if( tmp.currentFrame()->frameFormat().intProperty( Styles::PropFrameType ) != s )
			return false;
		tmp.movePosition(QTextCursor::NextBlock);
	}
	return true;
}

void TextCursor::removeCharFormat( QTextCursor& cur, const QTextCharFormat& f )
{
	QTextCharFormat tmp = cur.charFormat();
	QMap<int, QVariant> p = f.properties();
	QMap<int, QVariant>::const_iterator i;
	for( i = p.begin(); i != p.end(); ++i )
		tmp.clearProperty( i.key() );
	cur.setCharFormat( tmp );
}


