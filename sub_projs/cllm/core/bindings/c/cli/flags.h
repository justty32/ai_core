/* flags.h — 反射旗標表與 --help 用法文字（對齊 core/src/cli_flags.cpp）。
 *
 * 反射旗標＝連線／取樣選項，逐項對應 llm_client_t 欄位；固定旗標（--stream/--image/…）
 * 的解析在 argv.c。C 無反射，故手列一張表對齊 cabi_client.h 的欄位。 */
#ifndef CLLM_CLI_FLAGS_H
#define CLLM_CLI_FLAGS_H
#include <stdio.h>

/* 旗標值型別（決定 parse 方式與 --help 提示）。 */
typedef enum { F_STR, F_LONG, F_INT, F_FLOAT } flag_type_t;

typedef struct {
  const char *flag;  /* 命令列旗標，如 "--endpoint" */
  const char *field; /* 對應 llm_client_t 欄位名（底線版），如 "endpoint" */
  flag_type_t type;
} reflect_flag_t;

extern const reflect_flag_t REFLECT_FLAGS[];
extern const int REFLECT_FLAGS_COUNT;

/* flag 字串 → 反射旗標表項；非反射旗標回 NULL。 */
const reflect_flag_t *flag_lookup(const char *flag);
/* 印 --help（對齊 cli_flags.cpp 的 print_usage；輸出到 out，慣例為 stderr）。 */
void print_usage(FILE *out);

#endif /* CLLM_CLI_FLAGS_H */
