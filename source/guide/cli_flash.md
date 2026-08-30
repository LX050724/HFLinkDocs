# flash — Flash 下载

```
Usage: HFLinkCLI flash <command> [options]
```

六个子操作（位置参数 `<command>`）：

| 子操作 | 别名 | 在线 | 说明 |
|--------|------|------|------|
| `probe` | `banks` | 否 | 解析 Pack/FLM，列出候选 Flash bank（离线，不接触探针） |
| `dry-run` | — | 否 | 生成结构化下载计划：segments / banks / operations（离线） |
| `erase` | — | 是 | 按镜像范围或整片擦除 |
| `write` | `write-image` | 是 | 仅编程镜像（不隐式擦除） |
| `verify` | — | 是 | 读回校验镜像 |
| `program` | — | 是 | 完整下载：erase + program + verify（+ 可选复位/运行） |

离线命令（`probe` / `dry-run`）输出计划文本后退出；在线命令在目标上执行。

## 选项

### 连接选项（全局）

见 {doc}`cli_overview`：`flash` 必须提供 `--pack`、`--interface`、非零 `--speed`。

### Flash 选项

| 选项 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `--image` | `<path>` | 无 | 镜像文件；除 `probe` 外必填 |
| `--format` | `auto\|elf\|bin\|hex\|ihex` | `auto` | 输入格式（`ihex` 是 `hex` 的别名） |
| `--base` | `<address>` | 无 | BIN 镜像加载基址（十六进制可用） |
| `--algorithm` | `[0xADDR@]FLM` | 无 | 覆盖 Pack 默认算法：Pack FLM 基名或自定义 FLM 路径；含 `/`、`\`、盘符时视为路径；匹配 bank 数必须恰好为 1 |
| `--address-space` | `<id>` | 无 | 内存 AP / 地址空间选择（十六进制可用） |
| `--erase` | `sectors\|chip` | `sectors` | 擦除策略；`chip` 为整片擦除 |
| `--verify-mode` | `crc\|readback` | `crc` | 校验模式（Classic 恒为 readback） |
| `--no-turbo` | flag | off | 强制 Classic 同步 runner（默认 Turbo；Turbo 失败不回退） |
| `--pname` | `<name>` | 无 | 选择执行处理器（Pname） |
| `--punit` | `<index>` | 无 | 选择处理器单元 |
| `--hook` | `<path>` | 无 | 连接前加载用户 Hook 脚本（Lua） |

### 复位选项（仅 `program`）

| 选项 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `--reset` | `none\|system\|processor\|hardware` | `none` | 连接阶段复位类型 |
| `--under-reset` | flag | off | nRESET 有效期间连接（under-reset） |
| `--pre-reset` | flag | off | 正常连接序列前先复位 |
| `--reset-after` | flag | off | 编程完成后执行 Pack 默认复位 |
| `--reset-scope` | `core\|chip\|chain\|physical-chain` | `chip` | 后复位影响范围；必须与 `--reset-after` 同用 |
| `--run` | flag | off | 编程/复位后恢复所选核运行；未显式选复位时隐含 `--reset-after` |
| `--jtag-auto` | flag | off | JTAG 下自动扫描物理链并映射 TAP；SWD 下禁止 |

## 交叉校验规则

以下组合会在解析阶段报错（退出码 2）：

- 缺 `--pack` / `--interface` / 非零 `--speed`
- 在线命令（erase/write/verify/program）缺 `--image`
- SWD 下使用 `--jtag-auto`
- JTAG 在线命令未提供 `--jtag-auto`
- `--reset-after` / `--run` / `--reset-scope` 用于非 `program` 子操作

## 示例

```bash
# 离线查看 Flash 布局与计划
HFLinkCLI flash probe --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --image app.elf

# 完整下载 + 编程后复位并运行
HFLinkCLI flash program --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 \
    --image app.elf --run

# 指定自定义 FLM 算法与整片擦除
HFLinkCLI flash erase --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 \
    --image app.elf --erase chip --algorithm CustomFlm/STM32H7xx.flm

# BIN 镜像需要基址
HFLinkCLI flash program --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 \
    --image app.bin --base 0x08000000
```
