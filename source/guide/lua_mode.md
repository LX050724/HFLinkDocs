# Lua 脚本模式

未指定 `flash` / `gdb` / `rtt` 子命令时，HFLinkCLI 进入内嵌 **Lua 5.4** 脚本模式，适合自动化
脚本、产线工具与快速实验。

```
Usage: HFLinkCLI [options] [-c <chunk>] [<file> ...]
```

## 运行方式

| 输入 | 行为 |
|------|------|
| `-c <chunk>` | 执行一段 Lua 代码，可重复（全部 `-c` 先于脚本文件执行） |
| 位置参数 `<file> ...` | 依序执行脚本文件 |
| 无参数 + 交互终端 | 进入 REPL（提示符 `>`，未完成语句续行 `>>`，自动打印表达式结果） |
| 无参数 + 管道 stdin | 整个 stdin 作为一个 chunk 执行 |

```bash
# REPL
HFLinkCLI
> =hf.version()

# 单行脚本 + 脚本文件
HFLinkCLI -c "print(hf.version())" flash_once.lua

# 管道
echo "print(hf.version())" | HFLinkCLI
```

## 选项

| 选项 | 说明 |
|------|------|
| `--privileged` | 开放全部 Lua 标准库（默认只开放安全子集） |
| `-c <chunk>` | 执行一段 Lua 代码（最多 64 个） |
| `--version` | 显示 Driver 与 Lua 运行时版本 |
| `--log` / `--atomic` | 同全局选项（Lua 模式下有效的两个全局选项） |

## safe 与 privileged 模式

默认 **safe** 模式只开放 `_G`、`coroutine`、`table`、`string`、`math`、`utf8`，`io`、`os`、
`package`、`require`、`debug`、`dofile`、`loadfile` 均不可用；`--privileged` 开放全部标准库。

```{warning}
safe 模式不是完整沙箱：仅隔离 OS 文件/进程接口，不限制指令数、时间与内存。不要对
不受信任的来源运行脚本。
```

## 全局函数

| 函数 | 说明 |
|------|------|
| `print(...)` | 输出到 CLI 控制台（重定向到宿主输出回调） |
| `exit(code)` | 终止 CLI，进程退出码为 `code`（nil 视为 0） |
| `atomic_behavior('error'\|'ignore')` | 设置/查询 atomic 策略（初始值来自 `--atomic`） |
| `hf.version()` | Driver DLL 版本整数 |
| `hf.OK` / `hf.ERR` | 常量 0 / -1 |

## 设备与芯片

### `hf.devices(maximum_count?)`

枚举探针（默认 16 个，上限 256），返回设备信息表数组：

```lua
for _, dev in ipairs(hf.devices()) do
    print(dev.device_name, dev.serial_number, dev.speed, dev.is_thirdparty)
end
```

### `hf.open(device_info)`

打开一个探针，返回 Device 对象（冒号调用方法）。

### `hf.find_chip(selector)`

离线查询 Pack 芯片信息（不接触探针），返回含 `pack_vendor`、`memories`、`algorithms` 等字段的表：

```lua
local chip = hf.find_chip("STM32H723ZGTx@STM32H7xx_DFP")
for _, m in ipairs(chip.memories) do print(m.name, m.start, m.size) end
```

### Device 方法

| 方法 | 说明 |
|------|------|
| `close()` | 关闭设备 |
| `serial_number()` / `model_name()` | 读取序列号/型号 |
| `start_dap()` | 启动 DAP 发送线程 |
| `set_5v_supply(on)` | 控制 5V 输出；`get_vtrg()` / `get_usb_voltage()` / `get_5v_current()` 读取电源与电压电流 |
| `pack(selector)` / `attach_pack(selector, options?)` | 查询/绑定 CMSIS-Pack 芯片 |
| `set_interface("swd"\|"jtag"\|"cjtag")` / `set_speed(hz)` | 设置传输接口与时钟 |
| `set_reset(type, under?, pre?)` | 设置连接复位策略 |
| `set_jtag_auto()` / `set_jtag_chain(taps, mappings)` / `set_swd_targets(targets)` | JTAG 链 / SWD Multidrop 配置 |
| `set_atomic_policy("error"\|"ignore")` / `set_clock(hz)` | atomic 策略与时钟 |
| `connect(mode?)` / `disconnect()` | 连接/断开目标 |
| `reset(Pname?, Punit?, scope?)` | 复位 |
| `pack_context(Pname?, Punit?)` / `pack_target(Pname?, Punit?)` | 获取 Pack CoreSight 上下文/目标 |
| `coresight()` | 获取原始 CoreSight 上下文对象 |

### CoreSight Context 方法

`initialize(swd_mode?)`、`set_ap`/`get_ap`、`read8/16/32/64`、`write8/16/32/64`、
`read_ap`/`write_ap`、`read_access_ap`/`write_access_ap`、`read_dp`/`write_dp`、`write_abort`、
`swj_sequence`/`swj_pins`/`jtag_sequence`、`delay`、`set_clock`、`time_us`、
`atomic_begin`/`atomic_end`、`set_sequences`、`load_debugvars`、`load_sequences`、`sequence(name)`、
`target(apsel?, type_name?)`、`truthy`/`udiv`/`umod`/`ucompare`、`unsupported`、`message`、`query`、
`get_variable`/`set_variable`、`destroy`。

### CoreSight Target 方法

`examine`、`poll`、`halt`、`resume(addr?)`、`step(addr?)`、`is_halted`、`get_state()`
（`unknown/running/halted/reset/debug_running/unavailable`）、`wait_event(timeout_ms?)`、
`clear_events`、`assert_reset`/`deassert_reset`、`read_reg`/`write_reg`、
`read_memory(addr, len)`/`write_memory(addr, data)`、`destroy`。

## 功能模块

### `hf.flash.*`

| 函数 | 说明 |
|------|------|
| `hf.flash.probe(...)` / `hf.flash.banks(...)` | 已连接 Device 或 `(selector, offline_options)` 离线探测 Flash bank |
| `hf.flash.dry_run(device, image_path, opts?)` | 生成下载计划 |
| `hf.flash.erase(device, image_path, opts?)` | 擦除 |
| `hf.flash.write_image(device, image_path, opts?)` | 仅编程 |
| `hf.flash.verify_image(device, image_path, opts?)` | 校验 |
| `hf.flash.program(device, image_path, opts?)` | 完整下载；`opts` 支持 `format`/`base`/`Pname`/`punit`/`erase` 等 |

### `hf.rtt.*`

全局配置（供宿主 C 侧消费，用于 gdb/rtt 子命令 `--lua` 方式）：
`hf.rtt.create(target)`、`hf.rtt.set_control_block(addr)`、`hf.rtt.set_scan_range(base, size)`、
`hf.rtt.clear_config()`。

RTT 会话对象（`hf.rtt.create` 返回）方法：`start(cb_addr?)`、`stop`、`read(ch, max_len?)`
（默认 4096 / 上限 65536）、`write(ch, data)`、`get_num_bytes(ch)`、`get_num_buffers(dir)`、
`get_buffer_desc(idx, dir)`、`set_poll_interval(ms)`（1..1000，默认 2）、`set_telnet`、
`get_control_block_address`、`get_last_error`。SAFE 模式可用。

### `hf.semihosting.*`

`register_handler(opcode, fn)`、`unregister_handler(opcode)`、`clear_handlers`。
处理函数接收 Request 对象：`get_opcode`、`get_argument`、`read_u32`/`write_u32`、
`read_memory`/`write_memory`、`set_result`、`set_errno`。

### `hf.expr.*`（表达式引擎）

`hf.expr.open(...)` 打开 ExprSession；方法：`attach`、`set_reader`、`eval`、`typeof`、`members`、
`member_offset`、`sizeof`、`sym`、`close`。

### `hf.hss.*`（高速内存采样）

`hf.hss.create(target)` 创建会话；`hf.hss.caps(target)` 查询能力
（`{max_blocks, max_freq_hz}`）。会话方法：`add_block(addr, size)`、`start(freq_hz)`、`stop`、
`read(n)`、`stats`、`set_paused`。SAFE 模式可用。

## 退出码

- 脚本执行成功：退出码 `0`
- 脚本/Chunk 出错：退出码 `1`（错误信息经 `hf` 宿主打印到 stderr）
- `exit(N)`：退出码 `N`
