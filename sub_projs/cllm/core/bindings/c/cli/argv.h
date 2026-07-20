/* argv.h — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 cli.cpp 的解析段）。
 *
 * 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
 * 特判；反射旗標（連線／取樣，見 flags.h）吃下一個 argv；其餘當位置參數拼 prompt。 */
#ifndef CLLM_CLI_ARGV_H
#define CLLM_CLI_ARGV_H

/* 一個反射旗標的原始取值：欄位名／型別／原始字串（cli.c 續 parse＋覆寫進 client）。 */
typedef struct {
  const char *field;
  int type; /* flag_type_t */
  const char *flag;
  const char *value;
} raw_value_t;

/* argv 掃描結果。字串一律指向 argv（生命週期夠），只有幾個指標陣列動態配置。 */
typedef struct {
  raw_value_t *raw_values; int raw_n;
  const char **prompt_parts; int prompt_n;
  const char **media_specs; int media_n;
  const char **tool_specs; int tool_n;
  const char **modality_specs; int modality_n;
  const char *schema_text;
  const char *config_path;
  const char *media_out_dir;
  const char *system_text;
  int has_schema, has_config, has_system, stream;
} parsed_args_t;

/* 掃描 argv。回 -1＝解析成功、續用 *p（呼叫端用 parsed_free 收）；回 ≥0＝該退出碼直接返回
 * （--help→EXIT_OK；未知旗標／缺值→EXIT_USAGE）。 */
int parse_argv(int argc, char **argv, parsed_args_t *p);
void parsed_free(parsed_args_t *p);

#endif /* CLLM_CLI_ARGV_H */
