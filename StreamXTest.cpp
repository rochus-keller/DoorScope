#include "StreamXTest.h"
#include <Stream/DataReader.h>
#include <Txt/TextCtrl.h>
#include <Txt/TextCursor.h>
#include <Txt/TextView.h>
#include <QFile>
#include <QMessageBox>
#include <QtDebug>
#include <QImage>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextDocument>
using namespace Ds;
using namespace Stream;
using namespace Txt;

static const char* s_type[] =
{
	"Attribute",
	"Object",
	"Picture",
	"Table",
	"Row",
	"Cell",
	"Link",
	"Inbound",
};

StreamXTest::StreamXTest(QWidget *parent)
	: QTreeWidget(parent)
{
	setWindowTitle( tr("DoorScope") );
	setColumnCount( 3 );
	setHeaderLabels( QStringList() << "Value" << "Name" << "Type" );
	setUniformRowHeights( false );
	setWordWrap( true );
}

StreamXTest::~StreamXTest()
{
}

static QString _decode( QStringList& d_names, const DataCell& name )
{
	if( name.isAtom() )
	{
		if( name.getAtom() >= d_names.size() )
			return name.toPrettyString();
		else
			return d_names[ name.getAtom() ];
	}else if( name.isNull() )
		return "";
	else
	{
		QString str = name.toPrettyString();
		d_names.append( str );
		return str;
	}
}

QTreeWidgetItem* StreamXTest::readAttr( Stream::DataReader& in )
{
	DataCell v = in.readValue();
	QString val = v.toPrettyString();
	if( !val.isEmpty() )
	{
		QTreeWidgetItem* item = new QTreeWidgetItem( Attribute );
		item->setText( 0, val );
		item->setText( 1, _decode( d_names, in.getName() ) );
		item->setText( 2, s_type[Attribute] );
		if( v.isBml() )
			item->setData( 0, Qt::UserRole, v.toVariant() );
		return item;
	}else
		return 0;
}

void StreamXTest::load( const QString& path )
{
	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		QMessageBox::critical( this, tr("Load StreamX"),
			tr("Cannot open '%1' for reading").arg(path) );
		return;
	}
	clear();
	d_names.clear();

	DataReader in( &f );

	enum State { ExpectingModule, ReadingModule };
	State state = ExpectingModule;
	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		switch( state )
		{
		case ExpectingModule:
			if( t == DataReader::Slot )
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
					addTopLevelItem( item );
			}else if( t == DataReader::BeginFrame )
			{
				if( _decode( d_names, in.getName() ) != "mod" )
					goto _err;
				state = ReadingModule;
			}
			break;
		case ReadingModule:
			if( t == DataReader::Slot )
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
					addTopLevelItem( item );
			}else if( t == DataReader::EndFrame )
			{
				enrich();
				return;
			}else if( t == DataReader::BeginFrame )
			{
				QString name = _decode( d_names, in.getName() );
				if( name == "obj" )
				{
					QTreeWidgetItem* item = readObj( in );
					if( item )
						addTopLevelItem( item );
				}else if( name == "pic" )
				{
					QTreeWidgetItem* item = readPic( in );
					if( item )
						addTopLevelItem( item );
				}else if( name == "tbl" )
				{
					QTreeWidgetItem* item = readTbl( in );
					if( item )
						addTopLevelItem( item );
				}
				else
					qWarning() << "Unknown element type " << name;
			}
			break;
		}
		t = in.nextToken();
	}
	return;
_err:
	QMessageBox::critical( this, tr("Load StreamX"),
		tr("Error in stream file format" ) );
	return;
}

QTreeWidgetItem* StreamXTest::readObj( DataReader& in )
{
	DataReader::Token t = in.nextToken();
	QTreeWidgetItem* obj = new QTreeWidgetItem( Object );
	obj->setText( 2, s_type[Object] );

	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			QTreeWidgetItem* item = readAttr( in );
			if( item )
			{
				obj->addChild( item );
				if( item->text( 1 ) == "Absolute Number" )
				{
					obj->setText( 1, item->text(0) );
					//qDebug() << item->text(0);
				}else if( item->text( 1 ) == "Object Heading" &&
					!item->text(0).isEmpty() )
					obj->setText( 0, item->text(0) );
				else if( item->text( 1 ) == "Object Text" &&
					!item->text(0).isEmpty() )
					obj->setText( 0, item->text(0) );
			}
		}else if( t == DataReader::EndFrame )
		{
			return obj;
		}else if( t == DataReader::BeginFrame )
		{
			QString name = _decode( d_names, in.getName() );
			if( name == "obj" )
			{
				QTreeWidgetItem* item = readObj( in );
				if( item )
					obj->addChild( item );
			}else if( name == "pic" )
			{
				QTreeWidgetItem* item = readPic( in );
				if( item )
					obj->addChild( item );
			}else if( name == "tbl" )
			{
				QTreeWidgetItem* item = readTbl( in );
				if( item )
					obj->addChild( item );
			}else if( name == "lnk" )
			{
				QTreeWidgetItem* item = readLnk( in );
				if( item )
					obj->addChild( item );
			}else if( name == "lin" )
			{
				QTreeWidgetItem* item = readLin( in );
				if( item )
					obj->addChild( item );
			}else
				qWarning() << "Unknown element type " << name;
		}
		t = in.nextToken();
	}
	return obj;
}

QTreeWidgetItem* StreamXTest::readPic( DataReader& in )
{
	DataReader::Token t = in.nextToken();
	QTreeWidgetItem* obj = new QTreeWidgetItem( Picture );
	obj->setText( 2, s_type[Picture] );
	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			if( in.getName().isNull() )
			{
				QImage img;
				in.readValue().getImage( img );
				obj->setData( 0, Qt::UserRole, img );
			}else
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
				{
					obj->addChild( item );
					if( item->text( 1 ) == "Absolute Number" )
					{
						obj->setText( 1, item->text(0) );
						//qDebug() << item->text(0);
					}else if( item->text( 1 ) == "Object Text" &&
						!item->text(0).isEmpty() )
						obj->setText( 0, item->text(0) );
				}
			}
		}else if( t == DataReader::EndFrame )
		{
			return obj;
		}else 
			qWarning() << "Illegal frame in Pic";

		t = in.nextToken();
	}
	return obj;
}

QTreeWidgetItem* StreamXTest::readTbl( DataReader& in )
{
	DataReader::Token t = in.nextToken();
	QTreeWidgetItem* tbl = new QTreeWidgetItem( Table );
	tbl->setText( 2, s_type[Table] );
	enum State { ReadingHead, ReadingRow, ReadingCell };
	State state = ReadingHead;
	QTreeWidgetItem* row; 
	QTreeWidgetItem* cell;
	while( DataReader::isUseful( t ) )
	{
		switch( state )
		{
		case ReadingHead:
			if( t == DataReader::Slot )
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
					tbl->addChild( item );
			}else if( t == DataReader::BeginFrame )
			{
				QString name = _decode( d_names, in.getName() );
				if( name == "row" )
				{
					row = new QTreeWidgetItem( Row );
					row->setText( 2, s_type[Row] );
					tbl->addChild( row );
					state = ReadingRow;
				}else
					qWarning() << "Invalid frame " << name;
			}else if( t == DataReader::EndFrame )
			{
				goto _end;
			}
			break;
		case ReadingRow:
			if( t == DataReader::Slot )
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
				{
					row->addChild( item );
					if( item->text( 1 ) == "Absolute Number" )
					{
						row->setText( 1, item->text(0) );
						//qDebug() << item->text(0);
					}
				}
			}else if( t == DataReader::BeginFrame )
			{
				QString name = _decode( d_names, in.getName() );
				if( name == "cell" )
				{
					cell = new QTreeWidgetItem( Cell );
					cell->setText( 2, s_type[Cell] );
					row->addChild( cell );
					state = ReadingCell;
				}else
					qWarning() << "Invalid frame " << name;
			}else if( t == DataReader::EndFrame )
			{
				state = ReadingHead;
			}
			break;
		case ReadingCell:
			if( t == DataReader::Slot )
			{
				QTreeWidgetItem* item = readAttr( in );
				if( item )
				{
					cell->addChild( item );
					if( item->text( 1 ) == "Absolute Number" )
					{
						cell->setText( 1, item->text(0) );
						//qDebug() << item->text(0);
					}else if( item->text( 1 ) == "Object Text" &&
						!item->text(0).isEmpty() )
						cell->setText( 0, item->text(0) );
				}
			}else if( t == DataReader::EndFrame )
			{
				state = ReadingRow;
			}else
				qWarning() << "Invalid frame token ";
			break;
		}
		t = in.nextToken();
	}
_end:
	return tbl;
}

void StreamXTest::enrich()
{
	QTreeWidgetItemIterator it( this );
    while(*it) 
	{
		switch( (*it)->type() )
		{
		case Attribute:
			if( !(*it)->data( 0, Qt::UserRole ).isNull() )
			{
				// BML
				QTextDocument* doc = readBml( (*it)->data( 0, Qt::UserRole ).toByteArray() );
				if( doc )
				{
					/* TODO
					TextCtrl* t = new TextCtrl( this );
					t->getView()->setDocument( doc );
					t->show();
					QTreeWidgetItem* item = new QTreeWidgetItem( (*it) );
					setItemWidget( item, 0, t );
					*/
				}
			}
			break;
		case Picture:
			{
				QImage img = (*it)->data( 0, Qt::UserRole ).value<QImage>();
				QLabel* l = new QLabel( this );
				l->setFixedSize( img.size() );
				l->setPixmap( QPixmap::fromImage( img ) );
				//l->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
				QTreeWidgetItem* item = new QTreeWidgetItem( (*it) );
				setItemWidget( item, 0, l );
				// setFirstItemColumnSpanned( item, true );
			}
			break;
		case Table:
			{
				QTableWidget* tw = new QTableWidget( 0, 0, this );
				tw->horizontalHeader()->hide();
				tw->verticalHeader()->hide();
				int row = 0;
				for( int i = 0; i < (*it)->childCount(); i++ )
				{
					QTreeWidgetItem* ri = (*it)->child( i );
					if( ri->type() == Row )
					{
						tw->insertRow( row );
						int col = 0;
						for( int j = 0; j < ri->childCount(); j++ )
						{
							QTreeWidgetItem* ci = ri->child( j );
							if( ci->type() == Cell )
							{
								if( col >= tw->columnCount() )
									tw->insertColumn( col );
								tw->setItem( row, col, new QTableWidgetItem( ci->text( 0 ) ) );
								col++;
							}
						}
						row++;
					}
				}
				tw->setFixedSize( tw->sizeHint() );
				//tw->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
				QTreeWidgetItem* item = new QTreeWidgetItem( (*it) );
				setItemWidget( item, 0, tw );
				// setFirstItemColumnSpanned( item, true );

			}
			break;
		}
        ++it;
    } 
}

QTextDocument* StreamXTest::readBml( const QByteArray& bml )
{
	QTextDocument* doc = new QTextDocument( this );
	TextCursor cur( doc );

	DataReader in( bml );

	DataReader::Token t = in.nextToken();
	enum State { WaitPar, ReadPar, ReadRt };
	State state = WaitPar;
	int indent;
	bool bullet = false;
	bool oldBullet = false;
	bool firstRtInPar = false;
	while( DataReader::isUseful( t ) )
	{
		switch( state )
		{
		case WaitPar:
			if( t == DataReader::BeginFrame && _decode( d_names, in.getName() ) == "par" )
			{
				state = ReadPar;
				bullet = false;
				firstRtInPar = true;
				indent = 0;
			}
			break;
		case ReadPar:
			if( t == DataReader::Slot )
			{
				QString name = _decode( d_names, in.getName() );
				if( name == "il" )
					indent = in.readValue().getInt32() / 360;
				else if( name == "bu" )
					bullet = true;
			}else if( t == DataReader::BeginFrame && _decode( d_names, in.getName() ) == "rt" )
			{
				if( firstRtInPar )
				{
					firstRtInPar = false;
					if( oldBullet )
					{
						// wir sind bereits in einer Liste
						if( bullet )
							cur.addParagraph(); // weiterhin in Liste
						else
						{
							// Liste abschliessen
							oldBullet = false;
							cur.addItemLeft(true);
						}
					}else
					{
						// wir sind noch nicht in der Liste
						if( bullet )
						{
							// Liste beginnen
							oldBullet = true;
							cur.addList( QTextListFormat::ListDisc );
						}else
							cur.addParagraph(); // weiterhin keine Liste
					}
				}
				cur.setEm( false );
				cur.setUnder( false );
				cur.setStrike( false );
				cur.setSub( false );
				cur.setSuper( false );
				cur.setEm( false );
				cur.setStrong( false );
				cur.setFixed( false );
				if( bullet )
				{
					cur.unindentList(true);
					for( int i = 0; i < indent; i++ )
						cur.indentList();
				}
				state = ReadRt;
			}else if( t == DataReader::EndFrame )
			{
				state = WaitPar;
			}
			break;
		case ReadRt:
			if( t == DataReader::Slot )
			{
				QString name = _decode( d_names, in.getName() );
				DataCell v;
				in.readValue( v );
				if( ( name.isEmpty() && v.getType() == DataCell::TypeImg ) || name == "ole" )
				{
					if( v.getType() == DataCell::TypeImg )
					{
						QImage img;
						v.getImage(img);
						cur.insertImg( img );
					}
				}else if( name == "url" )
				{
					cur.insertUrl( v.getStr(), v.getStr() );
				}else if( name.isEmpty() && v.getType() == DataCell::TypeUInt8 )
				{
					if( v.getUInt8() == 'i' )
						cur.setEm( true );
					else if( v.getUInt8() == 'b' )
						cur.setStrong( true );
					else if( v.getUInt8() == 'u' )
						cur.setUnder( true );
					else if( v.getUInt8() == 'k' )
						cur.setStrike( true );
					else if( v.getUInt8() == 'p' )
						cur.setSuper( true );
					else if( v.getUInt8() == 's' )
						cur.setSub( true );
				}else if( name.isEmpty() && v.getType() == DataCell::TypeString )
				{
					cur.insertText( v.getStr() );
				}
			}else if( t == DataReader::EndFrame )
			{
				state = ReadPar;
			}
			break;
		}
		t = in.nextToken();
	}
	// Der erste Block ist überschüssig
	cur.gotoStart();
	cur.deleteCharOrSelection();
	return doc;
}

QTreeWidgetItem* StreamXTest::readLnk( DataReader& in )
{
	DataReader::Token t = in.nextToken();
	QTreeWidgetItem* obj = new QTreeWidgetItem( Link );
	obj->setText( 2, s_type[Link] );

	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			QTreeWidgetItem* item = readAttr( in );
			if( item )
			{
				obj->addChild( item );
			}
		}else if( t == DataReader::EndFrame )
		{
			return obj;
		}else if( t == DataReader::BeginFrame )
		{
			QString name = _decode( d_names, in.getName() );
			if( name == "tobj" )
			{
				QTreeWidgetItem* item = readObj( in );
				if( item )
					obj->addChild( item );
			}
		}
		t = in.nextToken();
	}
	return obj;
}

QTreeWidgetItem* StreamXTest::readLin( DataReader& in )
{
	DataReader::Token t = in.nextToken();
	QTreeWidgetItem* obj = new QTreeWidgetItem( Inbound );
	obj->setText( 2, s_type[Inbound] );

	while( DataReader::isUseful( t ) )
	{
		if( t == DataReader::Slot )
		{
			QTreeWidgetItem* item = readAttr( in );
			if( item )
			{
				obj->addChild( item );
			}
		}else if( t == DataReader::EndFrame )
		{
			return obj;
		}else if( t == DataReader::BeginFrame )
		{
			QString name = _decode( d_names, in.getName() );
			if( name == "sobj" )
			{
				QTreeWidgetItem* item = readObj( in );
				if( item )
					obj->addChild( item );
			}
		}
		t = in.nextToken();
	}
	return obj;
}
