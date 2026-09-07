# AI 自动化调试指南

HFLinkCLI 内嵌的 Lua 脚本模式（见 {doc}`lua_mode`）天然适合与 AI 编程助手结合：它把调试器的全部能力——
连接探针、Flash 下载、内存/寄存器访问、RTT、高速采样、表达式求值——收敛为一套**文本进、文本出**的
机器接口。AI 擅长读文档、写脚本、解读输出，调试器擅长确定性地执行硬件操作，两者恰好互补。
本页说明这一设计思路，并给出几个可直接改造的案例。

## 设计思路：AI 出主意，调试器动手

传统调试流程中，人是唯一的"执行器"：看寄存器、点烧录、抄日志，再把现象转述给工具或同事。
Lua 脚本模式把"执行"交给程序，AI 则接管"判断"：

| 角色 | 职责 |
|------|------|
| AI | 阅读 API 文档与现象描述 → 生成 Lua 脚本 → 解读执行输出 → 迭代修正 |
| HFLinkCLI | 确定性地执行脚本：连探针、烧镜像、读内存、采数据，结构化报告成败 |

这套接口有几个刻意为之的特点，正好是 AI 协作需要的：

- **文本进、文本出**。脚本是纯文本，执行结果是控制台输出与退出码，都能原样贴进对话。
- **确定性与可复现**。同一脚本对同一目标行为一致；脚本可以先审查、再执行，可进版本库、可进 CI，
  也能反复复跑。
- **结构化错误**。接口失败统一返回 `nil, message, code`（错误三元组），进程以退出码 `0/1/2`
  区分成败，AI 能读懂失败原因并自我修正。
- **默认最小权限**。safe 模式裁剪了 `io` / `os` 等标准库；Flash 操作可用 `hf.flash.dry_run()`
  离线生成计划先行确认，再真正接触硬件。
- **全链路覆盖**。从枚举探针到烧录、读变量、采波形，一个脚本即可编排完整流程，
  无需在多个工具间搬运数据。

除 Lua 模式外，`gdb` / `rtt` 子命令兼容标准工具链（GDB/MI、telnet），AI 生成的通用工具同样可以驱动；
但"把完整流程编排进一个脚本"的能力是 Lua 模式独有的，也是下文案例的基础。

## 推荐工作流

```text
  人：描述意图、审查脚本、授权执行
   │
   ▼
AI 生成 Lua 脚本 ──→ 人工审查 ──→ HFLinkCLI 执行 ──→ 捕获 stdout/stderr 与退出码
   ▲                                                     │
   └────────────────── 贴回输出，AI 解读并迭代 ◄─────────┘
```

让 AI 高效生成可运行脚本，关键是喂足上下文：

1. 附上 {doc}`lua_mode` 全文作为 API 字典（接口签名、错误约定、生命周期规则都在其中）。
2. 说明环境事实：探针型号、目标芯片与 Pack 选择器、镜像路径、关注的变量名或地址。
3. 要求输出**完整脚本**：逐调用检查错误三元组、按创建逆序释放资源、关键步骤 `print` 进度。

下面是一个可直接使用的提示词模板：

```text
你是一名嵌入式调试助手，我将提供 HFLinkCLI Lua 模式的 API 文档（hf 全局表）。
请编写一个完整可运行的 Lua 脚本：

目标：连接探针，烧录 app.elf 到 STM32H723ZGTx@STM32H7xx_DFP，然后 halt 并读取
全局变量 g_errorCount（uint32_t）；大于 0 时打印其值并以退出码 1 结束，
否则复位运行并以退出码 0 结束。

要求：
- 每个调用按错误三元组约定检查，失败时用 error() 给出中文原因
- 按创建逆序释放资源
- 关键步骤用 print 输出
只输出完整脚本文件内容。
```

## 案例一：自动烧录

让 AI 生成一条"插上就烧"的产线/桌面脚本：烧录 `app.elf` 并复位运行，全程无人工干预。

```lua
-- ai_flash.lua —— 用法：HFLinkCLI ai_flash.lua
local function check(value, message, code)
    if value == nil then
        error(string.format("%s (%d)", message, code))
    end
    return value
end

local device = check(hf.open(check(hf.devices())[1]))
check(device:start_dap())
check(device:set_interface("swd"):set_speed(10000000))
device:pack("STM32H723ZGTx@STM32H7xx_DFP")   -- 保存 Pack 选择器，connect() 时打开会话
check(device:connect())

-- 完整下载（擦除 + 编程 + 校验，自动跳过内容已一致的扇区），完成后复位运行
local result, message, primary, cleanup = hf.flash.program(device, "app.elf", {
    reset_after = true,
})
if not result then
    error(string.format("烧录失败：%s (primary=%d, cleanup=%d)", message, primary, cleanup))
end

print("烧录成功，已复位运行")
device:disconnect()
device:close()
```

两个可选的增强，都很适合让 AI 补全：

- **先预演再执行**：把 `hf.flash.program(...)` 换成 `hf.flash.dry_run(device, "app.elf")`
  离线生成擦写计划（涉及哪些 bank、扇区、操作），贴给 AI 或人工确认无误后再真正烧录。
- **批量与差异**：AI 可以把脚本扩展成遍历多台探针、逐台烧录并汇总结果的版本。

## 案例二：采集数据，交给 AI 分析

调试器是目标板旁唯一的"数据入口"。用高速内存采样（HSS）把某个变量录下来，输出 CSV，
再让 AI 分析趋势、毛刺与周期抖动。变量地址不必手查 map 文件——用表达式引擎的 `sym()`
直接从 ELF 按符号名解析：

```lua
-- ai_hss.lua —— 用法：HFLinkCLI ai_hss.lua > samples.csv
local function check(value, message, code)
    if value == nil then
        error(string.format("%s (%d)", message, code))
    end
    return value
end

local device = check(hf.open(check(hf.devices())[1]))
check(device:start_dap())
check(device:set_interface("swd"):set_speed(10000000))
device:pack("STM32H723ZGTx@STM32H7xx_DFP")
check(device:connect())

local target = check(device:pack_target("P0"))

-- 从 ELF 解析变量地址，AI 只需知道变量名
local sess = check(hf.expr.open("app.elf"))
local addr = check(sess:sym("g_counter"))
sess:close()

local session = check(hf.hss.create(target))
check(session:add_block(addr, 4))
check(session:start(1000))                   -- 采样周期 1000 µs ≈ 1 kHz

local frames = {}
while #frames < 2000 do                      -- 采 2000 帧（约 2 秒）
    local batch = check(session:read(64))
    for _, frame in ipairs(batch) do
        frames[#frames + 1] = frame
    end
end
check(session:stop())

print("ts_ns,counter")
for _, frame in ipairs(frames) do
    print(string.format("%d,%d", frame.ts, frame.blocks[1]))
end

device:disconnect()
device:close()
```

随后把 `samples.csv` 交给 AI：让它统计均值与方差、标出异常跳变、判断是否存在周期性抖动。
固件走 RTT 输出日志时也是同一模式——`hf.rtt.create(target)` 启动会话、循环 `rtt:read()`
把日志落成文本（见 {doc}`cli_rtt`），AI 归纳错误模式、聚类失败用例的效率远高于人眼翻屏。

## 案例三：烧录后自动验证（冒烟测试）

烧录成功不等于固件正常。让脚本烧完后停机检查全局状态、断言版本号，把"烧录 + 验证"合成
一个可进 CI 的步骤；断言直接写在源码符号层面，AI 从源码就能生成：

```lua
-- ai_smoke.lua —— 用法：HFLinkCLI ai_smoke.lua
local function check(value, message, code)
    if value == nil then
        error(string.format("%s (%d)", message, code))
    end
    return value
end

local device = check(hf.open(check(hf.devices())[1]))
check(device:start_dap())
check(device:set_interface("swd"):set_speed(10000000))
device:pack("STM32H723ZGTx@STM32H7xx_DFP")
check(device:connect())

local result, message, primary, cleanup = hf.flash.program(device, "app.elf")
if not result then
    error(string.format("烧录失败：%s (primary=%d, cleanup=%d)", message, primary, cleanup))
end

local target = check(device:pack_target("P0"))
check(target:halt())                         -- 停机读内存，避免读到撕裂数据

-- 表达式引擎：按 GDB 语法直接读 C 全局变量，类型信息来自 ELF
local sess = check(hf.expr.open("app.elf"))
sess:set_reader(function(addr, size)
    return target:read_memory(addr, size)    -- 失败返回 nil 即视为读取失败
end)

local version = check(sess:eval("g_fw.buildVersion"))
print(string.format("固件版本：0x%08X", version))
if version ~= 0x00030007 then
    error("固件版本不符合预期")
end

local errors = check(sess:eval("g_sysStats.errorCount"))
print("错误计数：", errors)
if errors > 0 then
    error("启动后出现错误计数")
end

sess:close()
check(target:resume())
device:disconnect()
device:close()
```

退出码即测试结论（`0` 通过、`1` 失败），CI 或产线 MES 只看退出码就能联动；排查失败时
把 stderr 里的错误三元组贴回给 AI 继续定位。

## 案例四：故障现场速查

目标跑飞、疑似 hardfault 时，先让脚本把现场"拍照"，再把照片交给 AI 结合源码与 map 文件解读：

```lua
-- ai_crash.lua —— 用法：HFLinkCLI ai_crash.lua > crash_report.txt
local function check(value, message, code)
    if value == nil then
        error(string.format("%s (%d)", message, code))
    end
    return value
end

local device = check(hf.open(check(hf.devices())[1]))
check(device:start_dap())
check(device:set_interface("swd"):set_speed(10000000))
device:pack("STM32H723ZGTx@STM32H7xx_DFP")
check(device:connect())

local target = check(device:pack_target("P0"))
check(target:halt())

local names = { [13] = "SP", [14] = "LR", [15] = "PC" }
for number = 13, 15 do
    print(string.format("%s = 0x%08X", names[number], check(target:read_reg(number))))
end

local sp = check(target:read_reg(13))
local stack = check(target:read_memory(sp, 128))
print("栈顶 128 字节：")
for i = 0, #stack - 1, 4 do
    local word = stack:byte(i + 1) | (stack:byte(i + 2) << 8)
    word = word | (stack:byte(i + 3) << 16) | (stack:byte(i + 4) << 24)
    print(string.format("0x%08X: 0x%08X", sp + i, word))
end

device:disconnect()
device:close()
```

把 `crash_report.txt` 连同问题现象贴给 AI："PC 落在 0x0800xxxx，帮我判断是空指针、
栈溢出还是中断里调用了不可重入函数"。AI 通常会追问更多现场——异常状态寄存器 CFSR
（`0xE000ED28`）、完整的入栈寄存器组、调用栈回溯——这些都能用 `pack_context():read32()`、
`target:read_memory()` 继续采集。一问一答之间，人和 AI 就完成了一次完整的远程诊断。

## 实践建议与安全边界

- **先审查，再执行**。AI 生成的脚本可能误解意图或 API；执行前人工通读一遍，尤其是涉及
  烧录、整片擦除的脚本。拿不准时先让脚本走 `dry_run` / `probe` 等只读路径。
- **默认 safe 模式**。案例中全部脚本都可在 safe 模式运行；只有确需读写本地文件
  （如导出二进制采集文件）时才加 `--privileged`，此时更应严格审查脚本内容。
  safe 模式不是完整沙箱，不要对不受信任来源的脚本运行（见 {doc}`lua_mode`）。
- **一次一个目标**。多台探针同时在线时，脚本默认取 `hf.devices()[1]`；批量场景应让 AI
  按序列号显式选择，避免烧错目标。
- **反馈闭环要完整**。执行后把 stdout、stderr 与退出码**原样**回贴给 AI，不要只转述结论；
  错误三元组里的 `message` 与 `code` 往往直接指出修正方向。
- **把可复用的脚本沉淀下来**。验证过的脚本进版本库，成为团队的调试资产——下次遇到同类
  问题，AI 在旧脚本基础上小改即可，比从零生成更可靠。
