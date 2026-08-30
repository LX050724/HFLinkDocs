# HFLinkCLI 使用指南

HFLinkCLI 是 SDK 自带的命令行工具，提供四种工作模式：

```text
HFLinkCLI <command> [options]     运行子命令（flash / gdb / rtt）
HFLinkCLI [options] [-c <chunk>] [<file> ...]   Lua 脚本模式（未指定子命令时默认）
HFLinkCLI help [command]          查看指定子命令的帮助
```

| 子命令 | 用途 |
|--------|------|
| `flash` | Flash 操作：probe / dry-run / erase / write / verify / program |
| `gdb` | GDB 服务器（RSP over TCP），支持 RTT 与 semihosting |
| `rtt` | RTT 会话守护：默认静默 + telnet 服务，`--terminal` 开启交互终端 |
| （无子命令） | 内嵌 Lua 脚本模式，见 {doc}`lua_mode` |

## 全局选项

所有子命令共享以下连接选项：

| 选项 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `--probe` | `<serial>` | 无 | 按序列号选择物理探针；未指定时要求系统恰好枚举到 1 个探针，否则报错 `HFLINK_ERR_TOPOLOGY_AMBIGUOUS` |
| `--pack` | `<selector>` | 无 | CMSIS-Pack 设备选择器，`flash` / `gdb` / `rtt` 必填，如 `STM32H723ZGTx@STM32H7xx_DFP` |
| `--interface` | `swd\|jtag` | `gdb`/`rtt` 走 swd | 调试传输接口，`flash` 必填 |
| `--speed` | `<hz>` | 无 | 调试时钟频率（Hz），必须非零 |
| `--log` | `error\|warn\|info\|debug` | 无 | 驱动日志等级，同时把驱动日志重定向到 stderr |
| `--atomic` | `error\|ignore` | `ignore` | 探针不支持缓冲时对 atomic 块的处理策略 |
| `--rtt-telnet` | `<[host:]port>\|off` | `127.0.0.1:19021` | RTT telnet 服务端绑定地址；`off` / `disable` 禁用 |
| `--rtt-addr` | `<0x...>` | 无 | RTT 控制块显式地址（最高优先级，覆盖 `--elf` 与扫描） |
| `--rtt-scan` | `<base>,<size>` | Pack 首个可写 RAM 区 | RTT 控制块扫描范围 |

设备选择器语法：`设备名[@Pack名[/版本]][/处理器名]`，省略版本时自动取已安装的最高版本。

## 退出码

| 退出码 | 含义 |
|--------|------|
| `0` | 成功（含 `--help` / `help` / `--version`） |
| `1` | 运行失败：Lua 脚本执行出错、Driver 初始化/清理失败、子命令执行失败（flash 失败时 stderr 会打印 `result=%d primary=%d cleanup=%d` 三个驱动错误码） |
| `2` | 命令行解析错误：未知命令、缺必选参数、选项取值非法等 |
| `N` | Lua 脚本调用 `exit(N)` 时的自定义退出码 |

## 帮助

```bash
HFLinkCLI help            # 顶层帮助：子命令列表 + 全局选项 + Lua 模式选项
HFLinkCLI help flash      # flash 子命令帮助
HFLinkCLI flash --help    # 同上
HFLinkCLI --version       # 显示 Driver 与 Lua 运行时版本
```
