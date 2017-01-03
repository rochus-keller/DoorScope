TEMPLATE = app
TARGET = DoorScope
INCLUDEPATH += ../NAF . ./.. ../../Libraries
DEFINES += _Not_Use_Oln_Link_
QT += xml

	DESTDIR = ./tmp
	OBJECTS_DIR = ./tmp
	RCC_DIR = ./tmp
	MOC_DIR = ./tmp
	CONFIG(debug, debug|release) {
		DESTDIR = ./tmp-dbg
		OBJECTS_DIR = ./tmp-dbg
		RCC_DIR = ./tmp-dbg
		MOC_DIR = ./tmp-dbg
		DEFINES += _DEBUG
	}

win32 {
	INCLUDEPATH += $$[QT_INSTALL_PREFIX]/include/Qt
	RC_FILE = DoorScope.rc
	DEFINES -= UNICODE
	CONFIG(debug, debug|release) {
		LIBS += -lQtCLucened -lqjpegd -lqgifd
	 } else {
		LIBS += -lQtCLucene -lqjpeg -lqgif
	 }
 }else {
	INCLUDEPATH += $$(HOME)/Programme/Qt-4.4.3/include/Qt
	LIBS += -lQtCLucene -lqjpeg -lqgif
	QMAKE_CXXFLAGS += -Wno-reorder -Wno-unused-parameter
 }

#Resource file(s)
RESOURCES += ./DoorScope.qrc


#Header files
HEADERS += ./AnnotDeleg.h \
    ./AnnotMdl.h \
    ./AppContext.h \
    ./AttrSelectDlg.h \
    ./DirViewer.h \
    ./DocDeleg.h \
    ./DocExporter.h \
    ./DocImporter.h \
    ./DocManager.h \
    ./DocMdl.h \
    ./DocSelectorDlg.h \
    ./DocTree.h \
    ./DocViewer.h \
    ./HistDeleg.h \
    ./HistMdl.h \
    ./Indexer.h \
    ./LinksDeleg.h \
    ./LinksMdl.h \
    ./LuaBinding.h \
    ./MainFrame.h \
    ./PropsDeleg.h \
    ./PropsMdl.h \
    ./SearchView.h \
    ./StringDiff.h \
    ./TypeDefs.h \
	./Txt/ImageGlyph.h \
	./Txt/Styles.h \
	./Txt/TextCtrl.h \
	./Txt/TextCursor.h \
	./Txt/TextInStream.h \
	./Txt/TextMimeData.h \
	./Txt/TextOutHtml.h \
	./Txt/TextOutStream.h \
	./Txt/TextView.h \
	ReqIfParser.h \
    LuaIde.h \
    ../NAF/Gui/ListView.h \
    LuaFilterDlg.h \
    ScriptSelectDlg.h \
    ReqIfImport.h

#Source files
SOURCES += ./AnnotDeleg.cpp \
    ./AnnotMdl.cpp \
    ./AppContext.cpp \
    ./AttrSelectDlg.cpp \
    ./DirViewer.cpp \
    ./DocDeleg.cpp \
    ./DocExporter.cpp \
    ./DocImporter.cpp \
    ./DocManager.cpp \
    ./DocMdl.cpp \
    ./DocSelectorDlg.cpp \
    ./DocTree.cpp \
    ./DocViewer.cpp \
    ./HistDeleg.cpp \
    ./HistMdl.cpp \
    ./Indexer.cpp \
    ./LinksDeleg.cpp \
    ./LinksMdl.cpp \
    ./LuaBinding.cpp\
    ./main.cpp \
    ./MainFrame.cpp \
    ./PropsDeleg.cpp \
    ./PropsMdl.cpp \
    ./SearchView.cpp \
    ./StringDiff.cpp \
    ./TypeDefs.cpp \
	./Txt/ImageGlyph.cpp \
	./Txt/Styles.cpp \
	./Txt/TextCtrl.cpp \
	./Txt/TextCursor.cpp \
	./Txt/TextInStream.cpp \
	./Txt/TextMimeData.cpp \
	./Txt/TextOutHtml.cpp \
	./Txt/TextOutStream.cpp \
	./Txt/TextView.cpp \
    LuaIde.cpp \
    ../NAF/Gui/ListView.cpp \
    LuaFilterDlg.cpp \
    ScriptSelectDlg.cpp \
	ReqIfParser.cpp \
    ReqIfImport.cpp

include(../Sqlite3/Sqlite3.pri)
include(../Stream/Stream.pri)
include(../Sdb/Sdb.pri)
include(../Lua/Lua.pri)
include(../NAF/Gui2/Gui2.pri)
include(../NAF/Script/Script.pri)
include(../NAF/Script2/Script2.pri)
include(../NAF/Qtl2/Qtl2.pri)
