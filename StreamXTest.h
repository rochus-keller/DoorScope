#ifndef STREAMXTEST_H
#define STREAMXTEST_H

#include <QTreeWidget>
#include <Stream/DataReader.h>

class QTextDocument;

namespace Ds
{
	class StreamXTest : public QTreeWidget
	{
	public:
		StreamXTest(QWidget *parent);
		~StreamXTest();

		void load( const QString& path );
	private:
		QTreeWidgetItem* readObj( Stream::DataReader& );
		QTreeWidgetItem* readTbl( Stream::DataReader& );
		QTreeWidgetItem* readPic( Stream::DataReader& );
		QTreeWidgetItem* readLnk( Stream::DataReader& );
		QTreeWidgetItem* readLin( Stream::DataReader& );
		QTreeWidgetItem* readAttr( Stream::DataReader& );
		QTextDocument* readBml( const QByteArray& bml );
		void enrich();
		enum Item { Attribute, Object, Picture, Table, Row, Cell, Link, Inbound };
		QStringList d_names;
	};
}

#endif // STREAMXTEST_H
