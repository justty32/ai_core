/*
** lua_linit_clean.c — galtxt try_4：Lua 標準庫初始化的「乾淨覆蓋版」。
**
** ★ 為什麼有這一檔：try_4 的 Lua 是**借編 try_2 的 vendored 5.5 原始碼**（見 CMakeLists，
**   `../try_2/vendor/lua/*.c`）——但 try_2 把它的 `linit.c` 改過，在 `luaL_openselectedlibs`
**   尾端硬塞了 `luaopen_cjson`／`luaopen_http` 兩個 native 模組的 preload。
**
**   那兩個是 try_2 的分工（Lua 端自己做 JSON／HTTP）。**try_4 的分工相反**：JSON／HTTP／schema
**   全歸 C++ 核心，Lua 只當薄層吃 C++ function API。若照編 try_2 的 linit.c，連結器會索求
**   `luaopen_cjson`／`luaopen_http` 符號 → 逼著把 try_2 的 native/*.c 也拉進來（死重量、且模糊
**   「Lua 是薄層」的定位）。
**
**   解法＝**借全部、只覆蓋這一檔**：CMake 編 `../try_2/vendor/lua/*.c` 時排除它家的 `linit.c`，
**   改編這份原味版（就是上游 Lua 5.5 的 linit.c，未加 cjson/http）。這正呼應 try_4 的主張——
**   「借、不分叉」：核心與 vendor 都不複製，只在真正要分道的那一個檔上做最小覆蓋。
**
** 內容＝Lua 5.5 上游 linit.c 原樣（僅此註解為 try_4 所加）。定義 `luaL_openselectedlibs`，
** `luaL_openlibs`（見 lualib.h 的 inline 包裝）即靠它載入全部標準庫。
*/


#define linit_c
#define LUA_LIB


#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants LUA_<libname>K.)
*/
static const luaL_Reg stdlibs[] = {
  {LUA_GNAME, luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
LUALIB_API void luaL_openselectedlibs (lua_State *L, int load, int preload) {
  int mask;
  const luaL_Reg *lib;
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      luaL_requiref(L, lib->name, lib->func, 1);  /* require library */
      lua_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      lua_pushcfunction(L, lib->func);
      lua_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  lua_assert((mask >> 1) == LUA_UTF8LIBK);
  lua_pop(L, 1);  /* remove PRELOAD table */
}
