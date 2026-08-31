# Lua 脚本模式

未指定 `flash` / `gdb` / `rtt` 子命令时，HFLinkCLI 进入内嵌 **Lua 5.4** 脚本模式，适合自动化
脚本、产线工具与快速实验。脚本无需 `require` 加载模块，启动时自动注册全局表 `hf`。

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

REPL 中 `Ctrl+C` 清除当前输入行，`Ctrl+D` 退出；输入表达式会自动打印结果，例如输入 `1 + 2`
输出 `3`。

## 选项

| 选项 | 说明 |
|------|------|
| `--privileged` | 开放全部 Lua 标准库（默认只开放安全子集） |
| `-c <chunk>` | 执行一段 Lua 代码（最多 64 个） |
| `--version` | 显示 Driver 与 Lua 运行时版本 |
| `--log` / `--atomic` | 同全局选项（Lua 模式下有效的两个全局选项） |
| `--` | 停止选项解析，其后参数一律视为脚本文件 |

`-c` 与文件参数可混合，按出现顺序执行；任一步失败后后续不再执行，进程退出码为 1。
参数错误时退出码为 2。

## safe 与 privileged 模式

默认 **safe** 模式只开放 `_G`、`coroutine`、`table`、`string`、`math`、`utf8`，`io`、`os`、
`package`、`require`、`debug`、`dofile`、`loadfile` 均不可用；`--privileged` 开放全部标准库。

```{warning}
safe 模式不是完整沙箱：仅隔离 OS 文件/进程接口，不限制指令数、时间与内存。不要对
不受信任的来源运行脚本。
```

## 通用约定

**错误返回**。只返回数据的接口成功时直接返回数据；执行操作的接口成功时返回结果码或 `true`。
原生调用失败统一返回三个值（本文档简称“错误三元组”）：

```lua
nil, message, code
```

```lua
local result, message, code = device:connect()
if result == nil then
    error(string.format("连接失败：%s (%d)", message, code))
end
```

参数类型、取值范围或调用顺序错误则直接抛出 Lua 异常，应用 `pcall` / `xpcall` 捕获。

**常量**。`hf.OK` = `0`（成功），`hf.ERR` = `-1`（通用错误）。

**对象生命周期**。`hf.open()`、`device:coresight()`、`context:target()` 返回 Lua userdata，
父对象会被子对象自动引用，即使忘记显式释放也能安全回收；仍建议按创建逆序显式释放：

```lua
target:destroy()      -- 1. Target
context:destroy()     -- 2. CoreSight Context
device:disconnect()   -- 3. 调试会话
device:close()        -- 4. 设备
```

`close()` / `destroy()` 可重复调用；对象关闭后再调用其业务方法会报参数错误。
`device:disconnect()` 会使 Pack 会话、`pack_context` 与 `pack_target` 失效，重新连接后需重新获取。

## 全局函数

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `print(...)` | 任意 | 无 | 输出到 CLI 控制台（多参数以制表符分隔） |
| `exit(code)` | `code`：整数 | 无（终止进程） | 结束 CLI，进程退出码为 `code`（`nil` 视为 0，非整数报错） |
| `atomic_behavior(policy)` | `'error'` / `'ignore'` | 更新的策略字符串 | 设置并查询 atomic 策略，同时刷新全局布尔 `__HFLINK_ATOMIC_IGNORE`；初始值来自 `--atomic` |
| `hf.version()` | 无 | integer | Driver DLL 版本整数（如 `0x01020300` 形） |
| `hf.OK` / `hf.ERR` | — | integer | 常量 `0` / `-1` |

## 设备与芯片

### `hf.devices(maximum_count?)`

枚举探针，成功返回设备信息表数组，失败返回错误三元组。

- `maximum_count`：可选整数，1..256，默认 16。

```lua
local devices, message, code = hf.devices()
assert(devices, message)
for _, dev in ipairs(devices) do
    print(dev.device_name, dev.serial_number, dev.speed, dev.is_thirdparty)
end
```

设备信息表字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `device_name` | string | 设备名称（第三方 DAP 带 `(Thirdparty)` 后缀） |
| `serial_number` | string | 序列号（第三方 DAP 为 CRC32 生成的统一 8 位十六进制） |
| `speed` | integer | 设备速度信息 |
| `interface` | integer | 接口类型（USB / IP） |
| `is_thirdparty` | integer | 非零表示第三方 DAP：不支持配置/升级/传感器/SWO，限速 10 MHz，FIFO 深度 1 |

### `hf.open(device_info)`

按 `hf.devices()` 返回的设备信息打开探针，返回 Device 对象（冒号调用方法），失败返回错误三元组。

```lua
local device, message, code = hf.open(devices[1])
assert(device, message)
```

### `hf.find_chip(selector)`

离线查询 Pack 芯片信息（不接触探针，无需连接设备），失败返回错误三元组。

- `selector` 支持三种形式：`DeviceName`、`DeviceName@PackName[/Version]`、
  `DeviceName/ProcessorName@PackName[/Version]`；省略版本时自动取最高版本。

```lua
local chip = hf.find_chip("STM32H723ZGTx@STM32H7xx_DFP")
for _, m in ipairs(chip.memories) do print(m.name, m.start, m.size) end
```

返回表字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `pack_vendor` / `pack_name` / `pack_version` | string | Pack 数据库中的厂商、名称、版本 |
| `package_vendor` | string | PDSC package vendor |
| `family` / `sub_family` | string（`sub_family` 可为 nil） | 芯片 Family / SubFamily |
| `device` | string | 芯片名称 |
| `Pname` | string 或 nil | 显式选择的处理器名称 |
| `algorithms` | table | FLM 下载算法列表（见下） |
| `memories` | table | 设备内存区域列表（见下） |
| `debugvars_paths` | table | debugvars 文件路径列表 |
| `svd_path` / `debug_description_path` | string 或 nil | SVD / 官方调试描述文件路径 |
| `sequence_paths` | table | Sequence 路径列表 |

- `algorithms[]` 元素：`{path, Pname?, start, size, ram_start, ram_size, default(bool)}`。
- `memories[]` 元素：`{name, access, start, size, default(bool), startup(bool), init(bool)}`。
- 地址、大小等 64 位字段在 Lua 整数无法安全表示时返回十六进制字符串，如 `"0x80000000"`。

### Device 方法

Device 方法用冒号语法调用（`device:xxx(...)`）。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `close()` | — | `true` | 停止本设备启动的 DAP 线程并关闭设备，可重复 |
| `serial_number()` | — | string | 读取序列号 |
| `model_name()` | — | string | 读取型号 |
| `start_dap()` | — | `true` 或错误三元组 | 启动全局 DAP 发送线程；`connect()` 等函数的前置条件 |
| `set_5v_supply(on)` | `on`：boolean | `true` 或错误三元组 | 控制 5V SUPPLY 管脚输出（给目标重新上电） |
| `get_vtrg()` | — | number（伏特） | 读取目标电压 |
| `get_usb_voltage()` | — | number（伏特） | 读取 USB 输入电压 |
| `get_5v_current()` | — | integer（毫安） | 读取 5V 输出电流 |
| `pack(selector)` | `selector`：string | Device 自身 | 仅保存 Pack 选择器，`connect()` 时打开 Pack 会话 |
| `attach_pack(selector, options?)` | 见下 | `true` 或错误三元组 | 用 `options` 立即打开 Pack 会话（需先 `start_dap()`） |
| `set_interface(mode)` | `'swd'` / `'jtag'` / `'cjtag'` | Device 自身 | 选择传输接口 |
| `set_speed(hz)` | `hz`：1..2³²−1 | Device 自身 | 设置 Pack 会话调试速度 |
| `set_reset(type, under?, pre?)` | 见下 | Device 自身 | 设置复位策略 |
| `set_jtag_auto()` | — | Device 自身 | 开启 JTAG 自动链探测（先 `set_interface("jtag")`） |
| `set_jtag_chain(taps, mappings)` | 见下 | Device 自身 | 设置 JTAG TAP / DP 映射（先 `set_interface("jtag")`） |
| `set_swd_targets(targets)` | 见下 | Device 自身 | 设置 SWD multidrop target 数组（先 `set_interface("swd")`） |
| `set_atomic_policy(policy)` | `'error'` / `'ignore'` | Device 自身 | 设置 atomic 策略 |
| `connect(mode?)` | `'swd'`（默认）/ `'jtag'` | 结果码或错误三元组 | 连接调试端口；已配置 Pack 时打开 Pack 会话并忽略 `mode`（需先 `start_dap()`） |
| `disconnect()` | — | 结果码或错误三元组 | 断开调试端口 / Pack 会话 |
| `reset(Pname, Punit?, scope?)` | 见下 | integer 或错误三元组 | 执行 Pack 默认复位，返回受影响 route 数 |
| `pack_context(Pname, Punit?)` | `Punit` 默认 0 | Pack Context | 获取指定处理器核心的 CoreSight 上下文 |
| `pack_target(Pname, Punit?)` | `Punit` 默认 0 | Pack Target | 获取指定处理器核心的 Target |
| `set_clock(hz)` | `hz`：1..2³²−1 | 结果码或错误三元组 | 设置原始 CoreSight 连接时钟；配置 Pack 后请用 `pack_context(...):set_clock(hz)` |
| `coresight()` | — | CoreSight Context | 创建原始 CoreSight 上下文（需 `start_dap()` 且未配置 Pack） |

说明：

- 配置类方法（`pack` / `set_interface` / `set_speed` / `set_reset` / `set_jtag_*` /
  `set_swd_targets` / `set_atomic_policy`）返回 **Device 自身**，可链式调用：
  `device:set_interface("swd"):set_speed(4000000):set_reset("system")`。
- 这些配置必须在 Pack 会话打开前完成（`connect()` 或 `attach_pack()` 之后配置即冻结）。
- `set_reset(type, under?, pre?)`：`type` 为 `'none'` / `'hardware'` / `'system'` /
  `'processor'`；`under` 表示硬件复位期间保持连接，`pre` 表示连接前复位。
- `reset(Pname, Punit?, scope?)`：`Pname` 必填；`scope` 默认 `'chip'`，可选 `'core'` /
  `'chip'` / `'physical-chain'`（也接受 `'physical_chain'`）。多核设备无法唯一确定核心时会报错。
- `pack_context` / `pack_target` / `reset` 的 `Pname` 以 Pack 定义为准：多核设备用 PDSC 处理器名
  （如 `'CM7'` / `'CM4'`）；未定义 `<processor>` 的单核设备用 `'P0'`（如 STM32H723 的 Keil DFP）。
- `set_jtag_chain(taps, mappings)`：`taps` 为整数（IR 长度）或 `{ir_length=, idcode=?}` 表数组；
  `mappings` 为 `{dp=, tap=, absolute=}` 数组，`dp` 可为 u64 字符串。
- `set_swd_targets(targets)`：`targets` 为非空 `{dp=, targetsel=}` 表数组。

`attach_pack` 的 `options` 表支持字段：

| 字段 | 值 | 说明 |
|------|-----|------|
| `interface` | `'swd'` / `'jtag'` / `'cjtag'` | 传输接口（默认未指定） |
| `speed` | 整数 Hz | 调试速度 |
| `jtag` | `'auto'` / `'explicit'` | JTAG 链模式 |
| `swd_targets` | `{dp, targetsel}` 数组 | SWD multidrop 目标 |
| `atomic` | `'error'` / `'ignore'` | atomic 策略（默认 `'ignore'`） |
| `hook_script` | string | Hook 脚本路径 |
| `connection` | `'debug'` / `'download'` | 连接类型（默认 `'download'`） |
| `reset` | `'none'` / `'hardware'` / `'system'` / `'processor'` | 复位策略 |
| `under_hardware_reset` | boolean | 硬件复位期间保持连接 |
| `pre_connection_reset` | boolean | 连接前复位 |

```lua
assert(device:attach_pack("STM32H743@STM32H7_DFP/1.0.0", {
    interface = "jtag",
    speed = 10000000,
    jtag = "auto",
}))
```

### CoreSight Context 方法

原始上下文由 `device:coresight()` 创建，Pack 上下文由 `device:pack_context(Pname, Punit)` 获取。
均应先执行 `context:initialize()` 再创建 Target。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `initialize(swd_mode)` | `true` = SWD，`false` = JTAG | 结果码或错误三元组 | 初始化 CoreSight DP |
| `set_ap(apsel)` / `get_ap()` | — | integer 或错误三元组 | 设置 / 获取默认 AP |
| `read8/16/32/64(address)` | `address` ≥ 0 | integer 或错误三元组 | 按宽度读内存，小端数值（64 位超范围返回十六进制字符串） |
| `write8/16/32/64(address, value)` | `value`：对应宽度 | 结果码或错误三元组 | 按宽度写内存 |
| `read_ap(reg)` / `write_ap(reg, value)` | 寄存器号 | integer 或错误三元组 | 读写当前 AP 寄存器 |
| `read_access_ap(address)` / `write_access_ap(address, value)` | — | integer 或错误三元组 | 读写 ADIv6 顶层 AP |
| `read_dp(reg)` / `write_dp(reg, value)` | 寄存器号 | integer 或错误三元组 | 读写 DP 寄存器 |
| `write_abort(value)` | — | 结果码或错误三元组 | 写 ABORT 寄存器 |
| `swj_sequence(...)` / `swj_pins(...)` / `jtag_sequence(...)` | 底层序列参数 | 结果码或错误三元组 | SWJ / JTAG 序列与引脚操作 |
| `delay(us)` | 微秒 | 结果码 | 延时 |
| `time_us()` | — | integer | 微秒时间戳 |
| `set_clock(hz)` | 1..2³²−1 | 结果码或错误三元组 | 设置当前 Context 时钟 |
| `truthy(cond)` / `udiv(a, b)` / `umod(a, b)` / `ucompare(a, b)` | 无符号值 | 结果码或 integer | Sequence 常用无符号辅助函数 |
| `unsupported(message)` | string | 结果码或错误三元组 | 标记 / 返回 unsupported |
| `message(text)` / `query(...)` | — | 结果码或错误三元组 | CMSIS 调试消息与查询回调 |
| `atomic_begin()` / `atomic_end()` | — | 结果码或错误三元组 | atomic 块边界 |
| `set_sequences()` / `load_debugvars()` / `load_sequences()` | — | 结果码或错误三元组 | 加载 / 设置 Pack 脚本资源（多用于 Hook 或测试环境） |
| `sequence(name)` | string | 结果码或错误三元组 | 执行 Pack 或用户 Sequence |
| `target(apsel?, type_name?)` | `apsel` 默认 0，`type_name` 默认 `'cortex_m'` | Target | 创建目标 |
| `destroy()` | — | `true` | 销毁上下文 |

### CoreSight Target 方法

Target 由 `context:target(...)` 或 `device:pack_target(Pname, Punit)` 获取。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `examine()` | — | 结果码或错误三元组 | 探测目标 |
| `poll()` | — | 结果码或错误三元组 | 更新目标状态缓存 |
| `halt()` | — | 结果码或错误三元组 | 请求停止（返回时已停靠，并产生停靠事件） |
| `resume(addr?)` | 可选 PC 地址 | 结果码或错误三元组 | 恢复运行；带地址时重定向 PC，断点处指令不会被自动跳过 |
| `step(addr?)` | 可选 PC 地址 | 结果码或错误三元组 | 单步一条指令（返回时已停靠） |
| `is_halted()` | — | boolean 或错误三元组 | 是否已停靠（实时读 DHCSR） |
| `get_state()` | — | string 或错误三元组 | 上次 poll 缓存的状态：`unknown` / `running` / `halted` / `reset` / `debug_running` / `unavailable` |
| `wait_event(timeout_ms?)` | 默认 0 = 非阻塞 | `true, reason` 或错误三元组 | 等待停靠事件；**仅 Pack 会话目标（`pack_target` 创建）可用**，`context:target()` 的原始目标返回 `-2`（未初始化） |
| `clear_events()` | — | 结果码或错误三元组 | 丢弃该核全部挂起停靠事件（同样仅 Pack 会话目标可用） |
| `assert_reset()` / `deassert_reset()` | — | 结果码或错误三元组 | 置位 / 释放复位 |
| `read_reg(number)` | 0..2³²−1 | integer 或错误三元组 | 读取寄存器 |
| `write_reg(number, value)` | 均限 2³²−1 | 结果码或错误三元组 | 写入寄存器 |
| `read_memory(address, len)` | `len` ≥ 1 字节 | string 或错误三元组 | 按字节读内存，返回二进制安全的 Lua string |
| `write_memory(address, data)` | `data` 为二进制 string | 结果码或错误三元组 | 按字节写内存 |
| `destroy()` | — | `true` | 销毁目标 |

```lua
local data = target:read_memory(0x20000000, 16)
for index = 1, #data do
    io.write(string.format("%02X ", data:byte(index)))
end
-- 二进制数据可包含 \0，长度以 # 为准
```

上例的 `io.write` 仅在特权模式可用；安全模式用 `print` 或自行拼接文本。

## 功能模块

### `hf.flash.*`

统一 Flash 操作接口：`probe`（探测 bank）、`dry_run`（只生成计划，不操作硬件）、`erase`、
`write_image`（仅编程）、`verify_image`（仅校验）、`program`（完整下载）。
`probe` 与 `dry_run` 支持两种调用形式：**已连接 Pack 的 Device**，或 **离线**
（Pack 选择器 + 选项，不访问硬件）。

| 函数 | 参数 | 成功返回 |
|------|------|----------|
| `hf.flash.probe(device)` | 已连接 Pack 的 Device | Flash bank 数组 |
| `hf.flash.probe(selector, options)` | 选择器 + 离线选项 | Flash bank 数组 |
| `hf.flash.banks(...)` | 同 `probe` | 同 `probe`（兼容别名） |
| `hf.flash.dry_run(device, path, opts?)` | Device、镜像路径、选项 | 计划表 `{segments, banks, operations}` |
| `hf.flash.dry_run(selector, path, opts)` | 选择器、路径、离线选项 | 离线计划表 |
| `hf.flash.erase(device, path, opts?)` | Device、镜像路径、选项 | `0`（HFLINK_OK） |
| `hf.flash.write_image(device, path, opts?)` | 同上 | `0` |
| `hf.flash.verify_image(device, path, opts?)` | 同上 | `0` |
| `hf.flash.program(device, path, opts?)` | 同上 | `0` |

执行类接口失败返回**四元组**（比通用约定多一个清理错误码）：

```lua
nil, message, primary_error, cleanup_error
```

`primary_error` 是主操作错误，`cleanup_error` 是 FLM / Target / Hook / 设备清理阶段的错误，
排查时应同时记录两者。

离线形式的 `options` 必须包含 `interface`（`'swd'` / `'jtag'`）与 `speed`（Hz）。

镜像选项 `opts` 支持字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `format` | string | `'auto'`（默认）/ `'elf'` / `'bin'` / `'hex'`（`ihex` 同义）。BIN 必须提供 `base` |
| `base` | 整数 / u64 字符串 | BIN 镜像基地址 |
| `Pname` / `processor` | string | 处理器名（两个键等价） |
| `Punit` | 整数 | 处理器实例 |
| `bank` | 整数 | 指定 bank 索引 |
| `algorithm` | string | 自定义 FLM 算法路径 |
| `address_space` | 整数 / u64 字符串 | 地址空间 |
| `erase` | `'sectors'` / `'chip'` | 擦除模式 |
| `skip_verify` | boolean | 跳过校验 |
| `restore_after_erase` | boolean | 擦除前回读保存镜像未覆盖区域，编程后写回（默认 `false`，即擦除 = 擦空；由 planner 裁决，所有 Flash 方法共用） |
| `turbo` | boolean | 执行类接口：Turbo 加速（默认由上下文决定） |
| `verify_mode` | `'crc'` / `'readback'` | 执行类接口：校验方式（默认 CRC） |

`dry_run()` 返回的计划表：

- `segments`：镜像段 `{address, length, Pname?, address_space?}`；
- `banks`：Flash bank（字段见下）；
- `operations`：`preserve_read` / `erase_sector` / `erase_chip` / `program_page` / `verify`
  等操作，各含 `{type, bank, address, length, preserve, segment?}`；
  `preserve_read` 仅在 `restore_after_erase = true` 时生成。

Flash bank 字段：`{index, algorithm_index, algorithm, algorithm_Pname, executor_Pname,
executor_Punit, memory_Pname, memory_Punit, start, size, ram_start, ram_size, dp, ap|apid,
device, page_size, erased_value}`（`ap` / `apid` 视是否使用 ADIv6 顶层 AP 而定）。

`program()` 额外支持完成后行为：

```lua
local options = {
    reset_after = true,
    reset_scope = "chip",   -- 'core' / 'chip' / 'chain'（同 'physical_chain'）；要求 reset_after=true
    run_after = true,
}
```

```lua
local result, message, primary, cleanup = hf.flash.program(device, "firmware.bin", {
    format = "bin",
    base = 0x08000000,
    Pname = "CM7",
    erase = "sectors",
})
assert(result, string.format("%s (primary=%d, cleanup=%d)", message, primary, cleanup))
```

### `hf.rtt.*`

RTT 会话对象（对照 `JLINK_RTTERMINAL_*`）：`hf.rtt.create(target)` 返回 RTT 会话，
`target` 为 `device:pack_target` / `context:target` 创建的 CoreSight target。启动后 SDK 在
CoreSight 轮询调度器上注册收割任务，把 up 通道数据搬进主机侧缓冲；`read` 为纯内存拷贝、
非阻塞、不触碰调试总线。SAFE 模式即可使用（只读 up 通道 + 写 down 通道）。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `hf.rtt.create(target)` | CoreSight Target | userdata 或错误三元组 | 创建 RTT 会话 |
| `rtt:start(cb_addr?)` | 可选控制块地址 | `true` 或错误三元组 | 定位控制块并启动收割；带地址为显式控制块，否则走配置 / 自动扫描 |
| `rtt:stop()` | — | `true` 或错误三元组 | 停止收割 |
| `rtt:read(ch, max_len?)` | 通道号；`max_len` 默认 4096、上限 65536 | string 或错误三元组 | 读 up 通道（读走即失） |
| `rtt:write(ch, data)` | 通道号、二进制 string | integer 或错误三元组 | 写 down 通道，返回实际写入字节数（空间不足部分写） |
| `rtt:get_num_bytes(ch)` | 通道号 | integer 或错误三元组 | 主机缓冲可读字节数（水位，纯内存） |
| `rtt:get_num_buffers(dir)` | up = 0 / down = 1 | integer 或错误三元组 | 查询缓冲数量 |
| `rtt:get_buffer_desc(idx, dir)` | 索引、方向 | `{name, size, flags}` 或错误三元组 | 缓冲描述 |
| `rtt:set_scan_range(base, size)` | 地址、长度 | `true` 或错误三元组 | 设置自动扫描范围 |
| `rtt:set_poll_interval(ms)` | 1..1000，默认 2 | `true` 或错误三元组 | 收割周期，运行中修改即时生效 |
| `rtt:set_telnet(...)` | — | — | 绑定 telnet 服务（见 {doc}`cli_rtt`） |
| `rtt:get_control_block_address()` | — | integer 或错误三元组 | 已定位的控制块地址 |
| `rtt:get_last_error()` | — | string | 最近一次错误描述 |

```lua
local rtt = assert(hf.rtt.create(target))
rtt:set_scan_range(0x24000000, 0x00050000)
rtt:start()                    -- 或 rtt:start(0x24000100) 显式控制块地址
local data = rtt:read(0, 4096) -- up 通道 0
rtt:write(0, "cmd\n")          -- down 通道 0
rtt:stop()
```

全局配置函数（宿主 C 侧在 gdb / rtt 子命令 `--lua` 方式下消费）仍可用：
`hf.rtt.set_control_block(addr)` / `hf.rtt.set_scan_range(base, size)` / `hf.rtt.clear_config()`。

### `hf.semihosting.*`

注册 Lua 处理函数拦截目标发出的 semihosting 请求：

| 函数 | 参数 | 说明 |
|------|------|------|
| `hf.semihosting.register_handler(opcode, fn)` | opcode：0..2³²−1；`fn`：函数 | 注册处理函数 |
| `hf.semihosting.unregister_handler(opcode)` | opcode | 注销处理函数 |
| `hf.semihosting.clear_handlers()` | — | 清除全部处理函数 |

处理函数接收一个 Request 对象（userdata）：

- 字段：`req.opcode`（integer）、`req.argument`（integer，超 Lua 整数范围时为
  `"0x%016X"` 字符串）。
- 方法：`read_u32(index)` / `write_u32(index, value)`、`read_memory(address, size)`（返回
  二进制 string）/ `write_memory(address, data)`、`set_result(value)`、`set_errno(value)`。
- 处理函数须返回字符串：`"handled"` 表示已处理，`"fallback"` 表示交给默认处理；其他返回
  值视为错误（`errno = EINVAL`）。

```lua
hf.semihosting.register_handler(0x01, function(req)
    local arg = req.argument           -- 参数字段
    local data = req:read_memory(arg, 16)
    req:set_result(#data)
    return "handled"
end)
```

### `hf.expr.*`（表达式引擎）

对 ELF/DWARF（`.elf` / `.axf`）提供 GDB 语法 C 表达式**只读**求值，类型与符号信息来自
调试信息，内存访问经可替换后端注入。safe 与 privileged 模式均可用。

```lua
local sess = hf.expr.open("app.elf")   -- 打开 ELF/DWARF，失败返回 nil, message, code
sess:close()                            -- 显式关闭；也可交给 __gc
```

内存后端二选一，后设者生效：

```lua
-- 方式一：绑定 CoreSight context（真机内存）
sess:attach(context)

-- 方式二：Lua 回调后端（离线测试 / RTOS 插件读任务私有栈）
-- fn(addr, size) 返回正好 size 字节的 Lua string，失败返回 nil
sess:set_reader(function(addr, size)
    return read_task_memory(addr, size)
end)
```

| 方法 | 参数 | 返回值 |
|------|------|--------|
| `eval(expr)` | string | 标量 number（超范围时 hex 字符串）、`char *` → string、复合类型 → 元数据表 |
| `typeof(expr_or_type)` | string | `{kind, name, size}` |
| `members(expr_or_type)` | string | `{{name, type, offset, size, bit_size?, bit_offset?}...}`（匿名 struct/union 自动平铺） |
| `member_offset(type, path)` | 点链路径 | integer（匿名成员自动穿透） |
| `sizeof(expr_or_type)` | string | integer（字节数） |
| `sym(name)` | string | 变量 / 函数地址 |
| `attach(context)` / `set_reader(fn)` | 见上 | `attach` 成功不返回值；`set_reader` 返回 nil | 绑定内存后端（后设者生效） |
| `close()` | — | 关闭会话 |

支持的语法：标识符（全局/静态变量、`&func`）、整型与字符字面量、`a[i]`（含多维）、
`s.member`、`p->member`、一元 `* & + - ~ !`、强制转型 `(uint8_t)x` / `(struct Foo *)p`、
全套算术/位/逻辑/比较运算（短路求值）、三目 `?:`。函数调用、赋值、复合字面量、字符串
字面量、浮点算术等不支持的特性返回 `-67`。

```lua
local count = sess:eval("g_sysStats.rxCount")            -- 标量
local name  = sess:eval("g_device->name")                -- char* 字符串
local meta  = sess:eval("g_sysStats")                    -- {kind="struct", ...}
local item  = sess:eval("g_sysStats.history[3].ticks")   -- 成员链 + 下标
```

求值错误码：`-60` 语法错误、`-61` 未找到符号、`-62` 类型错误、`-63` 结果非标量、
`-64` 无内存后端、`-65` 内存读取失败、`-66` 运算域错误（NULL 解引用、除零）、
`-67` 不支持的特性。

### `hf.hss.*`（高速内存采样）

对目标内存按固定周期批量采样（HSS，仅读目标内存）。SAFE 模式即可使用。

```lua
local s = hf.hss.create(target)      -- target 为 CoreSight Target
s:add_block(0x20000000, 4)
s:add_block(0x20000100, 8)
s:start(1000)                        -- 采样周期 1000 µs（≈ 1 kHz）
```

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `hf.hss.create(target)` | CoreSight Target | HssSession 或错误三元组 | 创建采样会话 |
| `hf.hss.caps(target?)` | 可省略 | `{max_blocks, max_freq_hz}` | 查询能力 |
| `s:add_block(addr, size)` | `addr` ≥ 0，`size` > 0 | `true` 或错误三元组 | 添加采样块（总数不超过 `caps` 的 `max_blocks`） |
| `s:start(period_us)` | 周期微秒，≥ 1 | `true` 或错误三元组 | 启动采样（需先 `add_block`）；周期为尽力满足，实际受 SWD 速度与块布局限制 |
| `s:stop()` | — | `true` 或错误三元组 | 停止采样 |
| `s:read(n?)` | 帧数，默认 64，1..4096 | 帧数组或错误三元组 | 读取样本帧 |
| `s:stats()` | — | 统计表（见下） | 采样统计 |
| `s:set_paused(paused)` | boolean | `true` 或错误三元组 | 暂停 / 恢复采样 |

`read()` 返回帧数组，每帧为 `{ts = 纳秒, blocks = {v0, v1, ...}}`；块值 ≤ 8 字节按小端
解码为无符号整数（超出 Lua 整数范围时为十六进制字符串），超过 8 字节返回二进制字符串。

`stats()` 返回：`{total_samples, dropped_samples, dropped_batches, mean_period_ns, paused,
halted, ts_from_probe, delay_mode}`。

```lua
local frames = s:read(64)
for _, frame in ipairs(frames) do
    print(frame.ts, frame.blocks[1])
end
```

## 完整示例

```lua
local function check(value, message, code)
    if value == nil then
        error(string.format("%s (%d)", message, code))
    end
    return value
end

local device = check(hf.open(check(hf.devices())[1]))
print("设备：", device:model_name(), device:serial_number())

check(device:start_dap())
check(device:set_interface("swd"):set_speed(4000000))
check(device:connect())                    -- 无 Pack：原始 DAP 连接

local context = check(device:coresight())
check(context:initialize(true))
local target = check(context:target(0, "cortex_m"))
check(target:examine())
check(target:halt())

print(string.format("PC = 0x%08X", check(target:read_reg(15))))

-- RTT：读 up 通道 0 / 写 down 通道 0
local rtt = assert(hf.rtt.create(target))
rtt:set_scan_range(0x24000000, 0x00050000)
rtt:start()
print("RTT:", rtt:read(0, 256))
rtt:stop()

target:destroy()
context:destroy()
device:disconnect()
device:close()
```

生产脚本应通过 `pcall` 或统一清理函数确保异常路径也能释放设备资源。

## 退出码

- 脚本执行成功：退出码 `0`
- 脚本 / Chunk 出错：退出码 `1`（错误信息经宿主打印到 stderr）
- 参数错误：退出码 `2`
- `exit(N)`：退出码 `N`