# gdb — GDB 服务器

```
Usage: HFLinkCLI gdb [options]
```

启动 GDB 远程串行协议（RSP）服务器：多核目标默认每核一个 TCP 端口（基准端口 + 核序号，
最多 4 核），启动后打印端口映射表 `coreN(Pname.Punit) -> host:port`。

```text
Example:
  HFLinkCLI gdb --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 \
      --rtt --elf app.elf --semihosting
```

## 选项

| 选项 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `-p, --port` | `<port>` | `3333` | 基准 GDB 端口 [1, 65535]；多核每核 `base + 核序号` |
| `--host` | `<ip>` | `127.0.0.1` | 监听地址；`0.0.0.0` 允许远程连接 |
| `--target` | `<pname:punit>` | 全部核 | 只服务指定核（单核模式），如 `cm7:0` |
| `--elf` | `<path>` | 无 | ELF 文件；解析 `_SEGGER_RTT` 符号定位 RTT 控制块 |
| `--rtt` | flag | off | 启用 RTT；无 `--elf` 时依次回退 Lua 配置 / 自动扫描 |
| `--semihosting` | flag | off | 启用 ARM semihosting（SYS_WRITEC/WRITE0 输出经 RTT 通道或 stdout） |
| `--semihosting-cwd` | `<dir>` | 进程 cwd | semihosting 文件操作的工作目录 |
| `--lua` | `<config.lua>` | 无 | 连接后执行的 Lua 配置脚本（如 `hf.rtt.set_control_block`） |
| `--log-rsp` | flag | off | 记录每个 RSP 包（`--log debug` 时隐含开启） |

RTT 控制块定位优先级：`--rtt-addr` > ELF 符号 > Lua 配置 > 自动扫描。
RTT 数据经全局选项 `--rtt-telnet` 的 telnet 服务端输出（gdb 下多核默认端口从 19021 起按核递增）。

## GDB 侧行为

- **内存映射**：启动时按 Pack 设备信息自动生成 memory-map（RAM/Flash 区段）并报告给 GDB；
  这是 `load` 命令走 `vFlashErase/vFlashWrite` 流程的前提，Flash 区段直写会被硬件静默丢弃。
- **qSupported**：协商 `PacketSize=4000`、`swbreak+`、`vContSupported+`、`QStartNoAckMode+` 等。
- **断点/观察点**：`Z0/Z1` 软/硬件断点，`Z2/Z3/Z4` 写/读/访问观察点。
- **SIGINT**：Ctrl-C 触发目标 halt。

## monitor 命令

在 GDB 中使用 `monitor <command>`：

| 命令 | 说明 |
|------|------|
| `monitor reset` | reset-halt：halt → 复位 → 重新 examine → PC/SP 指向复位向量 |
| `monitor rtt start` / `rtt stop` | 启停 RTT |
| `monitor arm semihosting enable` / `disable` | 启停 semihosting |
| `monitor md <addr>[,<count>]` | 读内存显示（count ≤ 256） |
| `monitor mw <addr>,<value>` | 写内存 |
| `monitor swd_clock` | 查询当前 SWD 时钟 |

未识别的 `monitor` 命令回落到会话内置表（`halt` / `resume` / `reg` 等）。

## 连接示例

```bash
# 单核
HFLinkCLI gdb --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000

# 双核：GDB 端口 3333/3334，RTT telnet 端口 19021/19022
HFLinkCLI gdb --pack STM32H745ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 -p 3333 --rtt

# GDB 侧
(gdb) target remote :3333
(gdb) load
(gdb) monitor reset
(gdb) continue
```
