# DoorScope
DoorScope is a software application which can be used in the specification review process. Specifications can be exported from IBM DOORS® and viewed with links, object attributes and change history (including tables, images and richtext). Objects can be annotated and marked with their review status. There is also a fulltext search and scripting facility. DoorScope is a single executable and runs natively on Windows without installation. DoorScope is open-source under GPL. 

## Download and Installation
DoorScope is deployed as a compressed single-file executable. See https://github.com/DoorScope#downloads. The executable is built from the source code accessible here. Of course you can build the executable yourself if you want (see below for instructions). Since DoorScope is a single executable, it can just be downloaded and unpacked. No installation is necessary. You therefore need no special privileges to run DoorScope on your machine. 

## How to Build DoorScope

### Preconditions
DoorScope requires Qt4.x. The single-file executables are static builds based on Qt 4.4.3. But it compiles also well with the Qt 4.8 series and should also be compatible with Qt 5.x. 

You can download the Qt 4.4.3 source tree from here: http://download.qt.io/archive/qt/4.4/qt-all-opensource-src-4.4.3.tar.gz

The source tree also includes documentation and build instructions.

If you intend to do static builds on Windows without dependency on C++ runtime libs and manifest complications, follow the recommendations in this post: http://www.archivum.info/qt-interest@trolltech.com/2007-02/00039/Fed-up-with-Windows-runtime-DLLs-and-manifest-files-Here's-a-solution.html (link is dead)

Here is the summary on how to do implement Qt Win32 static builds:

1. in Qt/mkspecs/win32-msvc2005/qmake.conf replace MD with MT and MDd with MTd
2. in Qt/mkspecs/features clear the content of the two embed_manifest_*.prf files (but don't delete the files)
3. run configure -release -static -platform win32-msvc2005

To use Qt with DoorScope you have to make the following modification: QTreeView::indexRowSizeHint has to be virtual; the correspondig line in qtreeview.h should look like:
    virtual int indexRowSizeHint(const QModelIndex &index) const;

### Build Steps
Follow these steps if you inted to build DoorScope yourself (don't forget to meet the preconditions before you start):

1. Create a directory; let's call it BUILD_DIR
2. Download the DoorScope source code from https://github.com/rochus-keller/DoorScope/archive/github.zip and unpack it to the BUILD_DIR.
3. Download the Stream source code from https://github.com/rochus-keller/Stream/archive/github.zip and unpack it to the BUILD_DIR; rename "Stream-github" to "Stream".
4. Download the Sdb source code from https://github.com/rochus-keller/Sdb/archive/github.zip and unpack it to the BUILD_DIR; rename "Sdb-github" to "Sdb".
5. Create the subdirectory "Lua" in BUILD_DIR; download the modified Lua source from http://cara.nmr-software.org/download/Lua_5.1.5_CARA_modified.tar.gz and unpack it to the subdirectory.
6. Create the subdirectory "Sqlite3" in BUILD_DIR; download the Sqlite source from http://software.rochus-keller.ch/DoorScope/Sqlite3.tar.gz and unpack it to the subdirectory.
7. Goto the BUILD_DIR/DoorScope subdirectory and execute `QTDIR/bin/qmake DoorScope.pro` (see the Qt documentation concerning QTDIR).
8. Run make; after a couple of minutes you will find the executable in the tmp subdirectory.

Alternatively you can open DoorScope.pro using QtCreator and build it there.

## Support
If you need support or would like to post issues or feature requests please post an issue on GitHub.

