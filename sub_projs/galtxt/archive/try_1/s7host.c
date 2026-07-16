/* s7host.c —— argv-aware 的 s7 host。
 *
 * s7 內建的 main()（s7.c ~103968 行，WITH_MAIN 分支）只吃「剛好一個檔名參數」、
 * s7_load 它，完全不把 argv 傳進 Scheme 層。這支 host 補上這一段：
 *
 *   s7host.exe script.scm arg1 arg2 --flag value
 *
 * 會把 argv[2..]（腳本名之後的所有參數）綁成 Scheme 變數 *argv*（一個字串 list，
 * 順序保持），再 s7_load 腳本檔（argv[1]）。找不到腳本就印錯誤、回傳非零。
 *
 * 編譯注意：把 s7.c 當庫一起編，*不要*定義 WITH_MAIN（否則撞到 s7 自己的 main）。
 * MinGW 上 s7.c 缺 <sys/utsname.h>，需要 shim_include/ 底下的 shim 標頭
 * （見同目錄 shim_include/sys/utsname.h 的說明），本檔不需要另外處理。
 *
 * Windows 上用 wmain + -municode 拿到 UTF-16 argv 再轉 UTF-8，讓中文參數
 * 不因為 ANSI codepage 轉換而先損壞一手（終端機能否正確「顯示」中文，
 * 還要看終端機自己的輸出編碼，這是另一回事，此處只保證「host 收到的
 * *argv* 內容」是正確的 UTF-8）。
 */
#include <stdio.h>
#include <stdlib.h>
#include "s7.h"

#if defined(_WIN32)
  #include <windows.h>
#endif

/* 共用邏輯：拿到 argc/argv（皆已是可直接餵給 s7_make_string 的 C 字串）之後，
 * 建 *argv*、s7_load 腳本、回傳 process exit code。
 */
static int run_host(int argc, char **argv)
{
  s7_scheme *sc;
  s7_pointer lst;
  int i;

  if (argc < 2)
    {
      fprintf(stderr, "用法: %s <script.scm> [arg1 arg2 ...]\n", argv[0]);
      return 1;
    }

  sc = s7_init();
  if (!sc)
    {
      fprintf(stderr, "s7_init 失敗\n");
      return 3;
    }

  /* 把 argv[2..] 由尾往前串成 *argv*（s7_cons 要從尾往前疊才能保持原順序） */
  lst = s7_nil(sc);
  for (i = argc - 1; i >= 2; i--)
    lst = s7_cons(sc, s7_make_string(sc, argv[i]), lst);
  s7_define_variable(sc, "*argv*", lst);

  if (!s7_load(sc, argv[1]))
    {
      fprintf(stderr, "找不到或無法載入腳本: %s\n", argv[1]);
      return 2;
    }

  return 0;
}

#if defined(_WIN32)

/* Windows：用 wmain 拿到 UTF-16 命令列，逐一轉成 UTF-8 narrow string，
 * 避免先被系統 ANSI codepage 轉換損壞中文參數。需搭配連結旗標 -municode。
 */
int wmain(int argc, wchar_t **wargv)
{
  char **argv_utf8;
  int i;
  int rc;

  /* 讓輸出到 console 的 UTF-8 位元組盡量被正確顯示（是否成功仍看終端機本身） */
  SetConsoleOutputCP(CP_UTF8);

  argv_utf8 = (char **)malloc(sizeof(char *) * (size_t)argc);
  if (!argv_utf8)
    {
      fprintf(stderr, "記憶體不足\n");
      return 3;
    }

  for (i = 0; i < argc; i++)
    {
      int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
      argv_utf8[i] = (char *)malloc((size_t)len);
      if (!argv_utf8[i])
        {
          fprintf(stderr, "記憶體不足\n");
          return 3;
        }
      WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv_utf8[i], len, NULL, NULL);
    }

  rc = run_host(argc, argv_utf8);

  for (i = 0; i < argc; i++)
    free(argv_utf8[i]);
  free(argv_utf8);

  return rc;
}

#else

/* 非 Windows：多數 POSIX locale 下 argv 本來就是 UTF-8，直接用一般 main。 */
int main(int argc, char **argv)
{
  return run_host(argc, argv);
}

#endif
