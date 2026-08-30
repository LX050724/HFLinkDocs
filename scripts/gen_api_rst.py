#!/usr/bin/env python3
"""按功能域生成 API 参考存根页到 source/api/_generated/。

用法：
    python scripts/gen_api_rst.py

- 设备基础页：HFLinkDriver.h 中按白名单收录 18 个函数 + 相关类型（伞头文件的其余组不公开）
- 其余功能模块页：从头文件扫描函数/枚举/结构体/typedef 逐成员渲染
- 函数指针 typedef 不渲染（Sphinx C domain 不支持其声明格式）
- 自检：白名单函数必须真实存在于头文件；快照缺失或无法解析时报错
"""

import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADERS_DIR = ROOT / "sdk-headers"
OUTPUT_DIR = ROOT / "source" / "api" / "_generated"

API_PATTERN = re.compile(r"HFLINK_API\b[^;]*?\b(HFLink_\w+)\s*\(")

# ---------------------------------------------------------------- 分组配置 ---

# 设备基础与升级（HFLinkDriver.h 白名单；其余 DebugSequence/DeviceSession/CoreSession/
# CoreSight Context/Target 组属于内部调试会话层，不公开）
DEVICE_FUNCTIONS = [
    "HFLink_GetDllVersion",
    "HFLink_Initialize",
    "HFLink_Cleanup",
    "HFLink_GetDeviceInfo",
    "HFLink_Open",
    "HFLink_Close",
    "HFLink_GetSerialNumber",
    "HFLink_GetModelName",
    "HFLink_UpgradeBitstream",
    "HFLink_UpgradeFile",
    "HFLink_UpgradeTick",
    "HFLink_UpgradeFree",
    "HFLink_Configure_SetItem",
    "HFLink_Configure_GetItem",
    "HFLink_Configure_Save",
    "HFLink_Sensor_Read",
    "HFLink_SWO_Start",
    "HFLink_SWO_Stop",
]

DEVICE_STRUCTS = [
    "HFLink_DeviceInfo",
    "HFLink_Upgrade",
    "HFLink_Config_FreqMap",
    "HFLink_Config_IODELAY",
]
DEVICE_ENUMS = [
    "HFLink_Interface",
    "HFLink_LedMode",
    "HFLink_Config",
    "HFLink_Sensor",
]
DEVICE_TYPEDEFS = [
    "HFLink_Handle",
]

FILE_PAGES = [
    ("HFLinkDriver_flash.h", "Flash 下载", "flash"),
    ("HFLinkDriver_pack.h", "CMSIS-Pack 查询", "pack"),
    ("HFLinkDriver_pack_lifecycle.h", "Pack 生产连接生命周期", "pack_lifecycle"),
    ("HFLinkDriver_rtt.h", "RTT 主机侧", "rtt"),
    ("HFLinkDriver_hss.h", "HSS 高速采样", "hss"),
    ("HFLinkDriver_semihosting.h", "Semihosting", "semihosting"),
]

BANNER = (".. 此文件由 scripts/gen_api_rst.py 自动生成，请勿手工编辑。\n"
          ".. 修改分组请编辑脚本内的配置清单。\n\n")


def extract_api_functions(header_text):
    """从头文件文本提取全部 HFLINK_API 函数名。"""
    functions = []
    for match in API_PATTERN.finditer(header_text):
        name = match.group(1)
        if name not in functions:
            functions.append(name)
    return functions


def extract_type_statements(header_text):
    """提取全部 typedef 语句（花括号配对到语句结束分号）。"""
    statements = []
    for match in re.finditer(r"\btypedef\b", header_text):
        depth = 0
        cursor = match.end()
        while cursor < len(header_text):
            char = header_text[cursor]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            elif char == ";" and depth == 0:
                statements.append(header_text[match.start():cursor])
                break
            cursor += 1
    return statements


def classify_types(header_text):
    """把 typedef 语句分为 enums / structs / typedefs；函数指针 typedef 返回在 func_ptrs 中不渲染。

    - 带花括号的 typedef enum/struct 按关键字归类
    - 无花括号的 `typedef struct Tag Name;`：Tag 与 Name 同名时 Doxygen（TYPEDEF_HIDES_STRUCT）
      生成 struct compound，交给独立 struct 定义扫描；Tag 不同于 Name 的（不透明句柄）按 typedef 渲染
    - 独立的 `struct HFLink_X { ... };` 完整定义归入 structs
    """
    enums, structs, typedefs, func_ptrs = [], [], [], []
    for statement in extract_type_statements(header_text):
        if "(*" in statement.replace(" *(", "(*"):
            pointer_name = re.search(r"\(\s*\*\s*(\w+)\s*\)", statement)
            if pointer_name:
                func_ptrs.append(pointer_name.group(1))
            continue
        body_match = re.search(r"\{", statement)
        name_match = re.search(r"(\w+)\s*$", statement)
        if not name_match:
            continue
        name = name_match.group(1)
        if not name.startswith("HFLink_"):
            continue
        if body_match and re.match(r"typedef\s+enum\b", statement):
            enums.append(name)
        elif body_match and re.match(r"typedef\s+struct\b", statement):
            structs.append(name)
        elif not body_match:
            tag_match = re.match(r"typedef\s+struct\s+(\w+)", statement)
            if tag_match and tag_match.group(1) == name:
                continue
            typedefs.append(name)
    for match in re.finditer(r"\bstruct\s+(HFLink_\w+)\s*\{", header_text):
        if match.group(1) not in structs:
            structs.append(match.group(1))
    return enums, structs, typedefs, func_ptrs


def display_width(text):
    """docutils 按显示列宽校验标题下划线：宽字符（中文）计 2 列。"""
    return sum(2 if unicodedata.east_asian_width(char) in "WF" else 1 for char in text)


def section(title, level):
    underline = "=" if level == 1 else "-"
    return [title, underline * display_width(title) + "\n"]


def render_members(enums, structs, typedefs, seen):
    """按 枚举 → 结构体 → 类型 分节渲染成员指令；seen 跨页去重（不透明句柄可能多文件声明）。"""
    lines = []
    enums = [name for name in enums if not (name in seen or seen.add(name))]
    structs = [name for name in structs if not (name in seen or seen.add(name))]
    typedefs = [name for name in typedefs if not (name in seen or seen.add(name))]
    if enums:
        lines += section("枚举", 2)
        lines += [f".. doxygenenum:: {name}\n" for name in enums]
    if structs:
        lines += section("结构体", 2)
        for name in structs:
            lines += [f".. doxygenstruct:: {name}", "   :members:\n"]
    if typedefs:
        lines += section("类型", 2)
        lines += [f".. doxygentypedef:: {name}\n" for name in typedefs]
    return lines


def render_device_page(header_functions, driver_types, seen):
    """设备基础页：白名单校验后按 类型 → API 分节。"""
    missing = [name for name in DEVICE_FUNCTIONS if name not in header_functions]
    if missing:
        print("错误：设备基础白名单中的函数在 HFLinkDriver.h 中不存在：", file=sys.stderr)
        for name in missing:
            print(f"  {name}", file=sys.stderr)
        return None
    hidden_count = len(header_functions) - len(DEVICE_FUNCTIONS)
    enums = [name for name in DEVICE_ENUMS if name in driver_types["enums"]]
    structs = [name for name in DEVICE_STRUCTS if name in driver_types["structs"]]
    typedefs = [name for name in DEVICE_TYPEDEFS if name in driver_types["typedefs"]]
    lines = [
        BANNER,
        ".. _api_device:\n",
        "设备基础与升级",
        "=" * display_width("设备基础与升级") + "\n",
        f"本页收录 HFLinkDriver.h 中面向应用的设备基础 API（{len(DEVICE_FUNCTIONS)} 个函数）；"
        f"同文件中的调试会话层接口（{hidden_count} 个，设备会话/CoreSight 上下文/目标控制）"
        "不属于公开文档范围。\n",
    ]
    lines += render_members(enums, structs, typedefs, seen)
    lines += section("API", 2)
    lines += [f".. doxygenfunction:: {name}\n" for name in DEVICE_FUNCTIONS]
    return "\n".join(lines)


def render_file_page(header, title, slug, header_functions, types, seen):
    """模块页：类型分节 + 全部公开函数逐条渲染。"""
    enums, structs, typedefs, _ = types
    lines = [
        BANNER,
        f".. _api_{slug}:\n",
        title,
        "=" * display_width(title) + "\n",
        f"以下内容自动抽取自 ``sdk-headers/{header}`` ，含 {len(header_functions)} 个公开函数"
        "及全部公开类型。函数指针回调类型不单独列出，见所属结构体成员说明。\n",
    ]
    lines += render_members(enums, structs, typedefs, seen)
    lines += section("API", 2)
    lines += [f".. doxygenfunction:: {name}\n" for name in header_functions]
    return "\n".join(lines)


def main():
    if not HEADERS_DIR.is_dir():
        print(f"错误：缺少快照目录 {HEADERS_DIR}，请先运行 sync_headers.py", file=sys.stderr)
        return 1

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    pages = {}
    seen = set()

    driver_text = (HEADERS_DIR / "HFLinkDriver.h").read_text(encoding="utf-8")
    driver_functions = extract_api_functions(driver_text)
    driver_types = dict(zip(
        ["enums", "structs", "typedefs", "func_ptrs"],
        classify_types(driver_text),
    ))
    pages["device"] = render_device_page(driver_functions, driver_types, seen)

    for header, title, slug in FILE_PAGES:
        text = (HEADERS_DIR / header).read_text(encoding="utf-8")
        functions = extract_api_functions(text)
        types = classify_types(text)
        pages[slug] = render_file_page(header, title, slug, functions, types, seen)

    failed = [slug for slug, content in pages.items() if content is None]
    if failed:
        print(f"错误：{len(failed)} 个页面生成失败：{', '.join(failed)}", file=sys.stderr)
        return 1

    for slug, content in pages.items():
        (OUTPUT_DIR / f"{slug}.rst").write_text(content, encoding="utf-8", newline="\n")
        print(f"生成 api/{slug}.rst")

    total = len(DEVICE_FUNCTIONS) + sum(
        len(extract_api_functions((HEADERS_DIR / header).read_text(encoding="utf-8")))
        for header, _, _ in FILE_PAGES
    )
    print(f"共收录 {total} 个公开函数（设备基础 {len(DEVICE_FUNCTIONS)} + 模块页）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
