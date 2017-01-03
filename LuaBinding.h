#ifndef LUABINDING_H
#define LUABINDING_H

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

#include <Sdb/Obj.h>

typedef struct lua_State lua_State;

class LuaBinding
{
public:
    static void install(lua_State * L);
    static void setRoot(lua_State * L, const Sdb::Obj& root );
	static int pushObject(lua_State *L, const Sdb::Obj& obj );
private:
    LuaBinding(){}
};



#endif // LUABINDING_H
