# rtt — RTT 守护与终端

```
Usage: HFLinkCLI rtt [options]
```

建立 RTT 会话守护进程。**默认静默守护**：不在 API 侧消费主机缓冲、不写 stdout，全部数据以
telnet 收割全速（约 2.7 MB/s，零丢失）供给 telnet 客户端；`--terminal` 开启本地交互终端。

```text
Example:
  HFLinkCLI rtt --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --elf app.elf -c 0
```

## 选项

| 选项 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `-c, --channel` | `<n>` | `0` | RTT 通道（0..63）；启动后按目标实际 up 通道数校验 |
| `-t, --terminal` | flag | off | 开启本地交互终端（否则为静默守护 + telnet） |
| `--target` | `<pname:punit>` | 第一核 | 附着指定核 |
| `--elf` | `<path>` | 无 | 解析 `_SEGGER_RTT` 符号定位 RTT 控制块 |
| `--lua` | `<config.lua>` | 无 | 连接后执行 Lua 配置（SAFE 模式，如 `hf.rtt.set_control_block`） |

RTT 控制块定位优先级与 gdb 相同：`--rtt-addr` > ELF 符号 > Lua 配置 > 自动扫描。
telnet 服务端由全局选项 `--rtt-telnet` 控制（rtt 固定默认 `127.0.0.1:19021`，`off` 禁用）。

## 两种模式

**静默守护（默认）**：本地不打印任何数据，stdin 收到 EOF（管道）或交互 TTY 下 Ctrl-C 时退出；
telnet 客户端连接 `<host>:19021` 即可查看与回写。

**交互终端（`--terminal`）**：本地控制台进入 raw 模式（关闭行缓冲/回显/信号），同时保留 telnet
复制输出；控制台按键约定（screen 风格）：

| 按键 | 行为 |
|------|------|
| `Ctrl+A q` | 退出 |
| `Ctrl+A Ctrl+A` | 发送字面 `Ctrl+A`（0x01）到目标 |
| 其他按键（含 `Ctrl+C`） | 原样写入 RTT down 通道 |

stdin 非交互（管道/重定向）时自动退化为只显示模式。

## 示例

```bash
# 静默守护 + telnet 查看
HFLinkCLI rtt --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --elf app.elf
telnet 127.0.0.1 19021

# 交互终端，通道 1
HFLinkCLI rtt --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 -t -c 1
```
