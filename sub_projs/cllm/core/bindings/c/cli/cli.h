/* cli.h — 薄 CLI 外殼的進入點與取消掛勾（對齊 cli.cpp）。 */
#ifndef CLLM_CLI_CLI_H
#define CLLM_CLI_CLI_H

/* CLI 進入點；回退出碼。 */
int cli_main(int argc, char **argv);
/* SIGINT handler 呼叫：對在途請求戳取消（llm_cancel）。任一 thread／signal 上下文可呼叫。 */
void cli_request_cancel(void);

#endif /* CLLM_CLI_CLI_H */
