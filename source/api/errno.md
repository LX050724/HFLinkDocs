# 错误码参考

HFLinkDriver 公开 API 以整数返回码报告结果：`HFLINK_OK`（0）表示成功，负数为错误码。
`HFLinkCLI` 的 flash 子命令失败时会在 stderr 打印 `result=`、`primary=`、`cleanup=` 三个驱动错误码，
可对照本表。

## 通用（-1 .. -14）

| 宏 | 值 | 含义 |
|----|-----|------|
| `HFLINK_OK` | 0 | 成功 |
| `HFLINK_ERR` | -1 | 通用错误 |
| `HFLINK_ERR_NOT_INITIALIZED` | -2 | 未初始化或参数为空 |
| `HFLINK_ERR_UNKNOWN_MODEL` | -3 | 未知设备型号 |
| `HFLINK_ERR_NO_TRANSFER` | -4 | 无可用传输 |
| `HFLINK_ERR_BUSY` | -5 | 设备正忙 |
| `HFLINK_ERR_COMMUNICATION` | -6 | 通信错误（USB 传输失败、JTAG 链访问失败等） |
| `HFLINK_ERR_NONEXECUTION` | -7 | 命令未执行（批量传输中该条目被前序错误抑制） |
| `HFLINK_ERR_ACK_FAULT` | -8 | DAP 响应 ACK 故障（非 OK/WAIT） |
| `HFLINK_ERR_ACK_WAIT` | -14 | DAP 在重试次数耗尽后仍返回 WAIT；驱动已尝试发送 DAPABORT |

## CMSIS-Pack（-9 .. -13）

| 宏 | 值 | 含义 |
|----|-----|------|
| `HFLINK_ERR_PACK_NOT_FOUND` | -9 | 已安装的 CMSIS-Pack 中未找到指定芯片 |
| `HFLINK_ERR_PACK_AMBIGUOUS` | -10 | 多个已安装 CMSIS-Pack 匹配指定芯片 |
| `HFLINK_ERR_PACK_FORMAT` | -11 | Pack 注册表或 Database 文件格式无效 |
| `HFLINK_ERR_PACK_VERSION` | -12 | Pack 注册表或 Database schema 版本不受支持 |
| `HFLINK_ERR_UNSUPPORTED` | -13 | 当前构建或探针不支持请求的规范能力 |

## Lua 脚本（-20 .. -23）

| 宏 | 值 | 含义 |
|----|-----|------|
| `HFLINK_ERR_LUA_SYNTAX` | -20 | Lua 代码存在语法错误 |
| `HFLINK_ERR_LUA_RUNTIME` | -21 | Lua 代码执行期间发生错误 |
| `HFLINK_ERR_LUA_MEMORY` | -22 | Lua 运行时内存分配失败 |
| `HFLINK_ERR_LUA_FILE` | -23 | Lua 脚本文件无法打开或读取 |

## 拓扑与 Flash（-30 .. -35）

| 宏 | 值 | 含义 |
|----|-----|------|
| `HFLINK_ERR_TOPOLOGY_NOT_FOUND` | -30 | 物理拓扑中没有满足 Pack 约束的目标 |
| `HFLINK_ERR_TOPOLOGY_AMBIGUOUS` | -31 | 物理拓扑中有多个目标满足 Pack 约束，无法唯一选择 |
| `HFLINK_ERR_IMAGE_FORMAT` | -32 | Flash 镜像格式或校验无效 |
| `HFLINK_ERR_PLAN_CONFLICT` | -33 | Flash bank、算法、地址空间或执行核心选择冲突 |
| `HFLINK_ERR_TIMEOUT` | -34 | 调试或 Flash 操作超时 |
| `HFLINK_ERR_TARGET_FAULT` | -35 | 目标在算法执行期间进入 Fault 或意外停止 |

## HSS 高速采样（-36 .. -39）

| 宏 | 值 | 含义 |
|----|-----|------|
| `HFLINK_ERR_HSS_NOT_STARTED` | -36 | HSS 会话未启动采样（需先 `HFLink_HSS_Start`） |
| `HFLINK_ERR_HSS_TOO_MANY_BLOCKS` | -37 | HSS 采样块数或总字节数超出单批能力上限 |
| `HFLINK_ERR_HSS_INVALID_PARAM` | -38 | HSS 参数无效（周期/块大小/地址对齐等） |
| `HFLINK_ERR_HSS_UNSUPPORTED` | -39 | 当前探针（如第三方 DAP）或固件能力不满足 HSS 请求 |

## 退出码映射

`HFLinkCLI` 进程退出码与驱动错误码相互独立：0 = 成功，1 = 运行失败（错误详情见 stderr 打印的
驱动错误码），2 = 命令行解析错误。详见 {doc}`../guide/cli_overview`。
