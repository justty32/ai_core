/* mingw shim：s7.c 在 !MS_Windows（即非 MSVC，含 MinGW）分支會
 * unconditionally `#include <sys/utsname.h>`（見 s7.c 的 g_uname，
 * "-------------------------------- uname --------------------------------" 段），
 * 但 MinGW 沒帶這個 POSIX 標頭。這裡補一個最小可編譯版本，
 * 只求編譯過、uname() 回傳堪用的假資料（s7 的 (uname) 內建函式本來就是玩具用途）。
 */
#ifndef MINGW_SHIM_SYS_UTSNAME_H
#define MINGW_SHIM_SYS_UTSNAME_H

#include <string.h>

struct utsname
{
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
};

static inline int uname(struct utsname *buf)
{
  if (!buf) return -1;
  strcpy(buf->sysname, "Windows");
  strcpy(buf->nodename, "mingw");
  strcpy(buf->release, "");
  strcpy(buf->version, "");
#if defined(_WIN64)
  strcpy(buf->machine, "x86_64");
#else
  strcpy(buf->machine, "x86");
#endif
  return 0;
}

#endif /* MINGW_SHIM_SYS_UTSNAME_H */
