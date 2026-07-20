/* argv.c — argv 掃描 → parsed_args_t（對齊 cli.cpp 解析段）。 */
#include "argv.h"
#include "flags.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 各動態陣列上限＝argc（旗標／位置參數再多不超過 argv 個數）。 */
static int alloc_arrays(parsed_args_t *p, int argc) {
  memset(p, 0, sizeof *p);
  p->raw_values = calloc(argc, sizeof *p->raw_values);
  p->prompt_parts = calloc(argc, sizeof *p->prompt_parts);
  p->media_specs = calloc(argc, sizeof *p->media_specs);
  p->tool_specs = calloc(argc, sizeof *p->tool_specs);
  p->modality_specs = calloc(argc, sizeof *p->modality_specs);
  return p->raw_values && p->prompt_parts && p->media_specs && p->tool_specs && p->modality_specs;
}

void parsed_free(parsed_args_t *p) {
  free(p->raw_values); free((void *)p->prompt_parts); free((void *)p->media_specs);
  free((void *)p->tool_specs); free((void *)p->modality_specs);
  memset(p, 0, sizeof *p);
}

int parse_argv(int argc, char **argv, parsed_args_t *p) {
  if (!alloc_arrays(p, argc)) { parsed_free(p); fputs("記憶體不足\n", stderr); return EXIT_USAGE; }
  int i = 1, no_more_flags = 0;
  while (i < argc) {
    char *a = argv[i];
    if (no_more_flags) { p->prompt_parts[p->prompt_n++] = a; i++; continue; }
    if (!strcmp(a, "--")) { no_more_flags = 1; i++; continue; }
    if (!strcmp(a, "--help") || !strcmp(a, "-h")) { print_usage(stderr); return EXIT_OK; }
    if (!strcmp(a, "--stream")) { p->stream = 1; i++; continue; }

    /* 需要吃下一個 argv 的旗標：缺值＝用法錯。 */
    const reflect_flag_t *rf = flag_lookup(a);
    int wants_value = rf || !strcmp(a, "--image") || !strcmp(a, "--media") ||
                      !strcmp(a, "--schema") || !strcmp(a, "--system") || !strcmp(a, "--config") ||
                      !strcmp(a, "--tool") || !strcmp(a, "--modality") || !strcmp(a, "--media-out");
    if (wants_value) {
      if (i + 1 >= argc) { fprintf(stderr, "%s 缺少值（llm-c --help 看用法）\n", a); return EXIT_USAGE; }
      char *v = argv[++i];
      if (rf) { p->raw_values[p->raw_n++] = (raw_value_t){rf->field, rf->type, rf->flag, v}; }
      else if (!strcmp(a, "--image") || !strcmp(a, "--media")) p->media_specs[p->media_n++] = v;
      else if (!strcmp(a, "--schema")) { p->schema_text = v; p->has_schema = 1; }
      else if (!strcmp(a, "--system")) { p->system_text = v; p->has_system = 1; }
      else if (!strcmp(a, "--config")) { p->config_path = v; p->has_config = 1; }
      else if (!strcmp(a, "--tool")) p->tool_specs[p->tool_n++] = v;
      else if (!strcmp(a, "--modality")) p->modality_specs[p->modality_n++] = v;
      else if (!strcmp(a, "--media-out")) p->media_out_dir = v;
      i++;
      continue;
    }
    if (a[0] == '-' && strcmp(a, "-")) { /* 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標 */
      fprintf(stderr, "未知旗標：%s（llm-c --help 看用法）\n", a);
      return EXIT_USAGE;
    }
    p->prompt_parts[p->prompt_n++] = a;
    i++;
  }
  return -1; /* 解析成功 */
}
