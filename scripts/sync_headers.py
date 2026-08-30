#!/usr/bin/env python3
"""从本地 HFLinkSDK 仓库同步公开头文件快照到 sdk-headers/。

用法：
    python scripts/sync_headers.py --sdk D:/source/HFLinkSDK

- 只复制 API 参考收录的功能层公开头文件（含 import 宏定义头）
- SYNC_INFO 记录快照来源（SDK commit / 分支 / 同步时间），conf.py 读取后展示在页脚
- 每个头文件必须存在且能解析出 HFLINK_API 声明（errno/import 纯宏头除外），否则报错退出
"""

import argparse
import datetime
import re
import subprocess
import sys
from pathlib import Path

# 快照清单：API 参考收录的功能层头文件 + HFLINK_API 宏定义头。
# 注意：DAP 直通、Lua C API、日志、调试会话层（DebugDeviceSession 等）不在清单中，故意不公开。
SNAPSHOT_FILES = [
    "HFLinkDriver.h",
    "HFLinkDriver_flash.h",
    "HFLinkDriver_pack.h",
    "HFLinkDriver_pack_lifecycle.h",
    "HFLinkDriver_rtt.h",
    "HFLinkDriver_hss.h",
    "HFLinkDriver_semihosting.h",
    "HFLinkDriver_errno.h",
    "HFLinkDriver_import.h",
]

# 纯宏/类型头，不要求解析出函数
NO_FUNCTION_FILES = {"HFLinkDriver_errno.h", "HFLinkDriver_import.h"}

API_PATTERN = re.compile(r"HFLINK_API\b[^;]*?\b(HFLink_\w+)\s*\(")


def extract_api_functions(header_text):
    """从头文件文本提取全部 HFLINK_API 函数名（函数名与 HFLINK_API 同行声明）。"""
    functions = []
    for match in API_PATTERN.finditer(header_text):
        name = match.group(1)
        if name not in functions:
            functions.append(name)
    return functions


def run_git(sdk_root, args):
    """在 SDK 仓库执行 git 命令，失败返回 None。"""
    try:
        result = subprocess.run(
            ["git", "-C", str(sdk_root)] + args,
            capture_output=True,
            text=True,
            timeout=15,
            encoding="utf-8",
            errors="replace",
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip() or None


def main():
    parser = argparse.ArgumentParser(description="同步 HFLinkSDK 公开头文件快照")
    parser.add_argument("--sdk", required=True, help="本地 HFLinkSDK 仓库路径")
    args = parser.parse_args()

    sdk_root = Path(args.sdk).resolve()
    include_dir = sdk_root / "Driver" / "include"
    output_dir = Path(__file__).resolve().parent.parent / "sdk-headers"

    if not include_dir.is_dir():
        print(f"错误：SDK 头文件目录不存在：{include_dir}", file=sys.stderr)
        return 1
    output_dir.mkdir(parents=True, exist_ok=True)

    total_functions = 0
    summary = []
    for filename in SNAPSHOT_FILES:
        source = include_dir / filename
        if not source.is_file():
            print(f"错误：缺少头文件 {source}", file=sys.stderr)
            return 1
        text = source.read_text(encoding="utf-8")
        functions = extract_api_functions(text)
        if not functions and filename not in NO_FUNCTION_FILES:
            print(f"错误：{filename} 未解析到任何 HFLINK_API 函数，疑似清单或格式变更", file=sys.stderr)
            return 1
        (output_dir / filename).write_text(text, encoding="utf-8", newline="\n")
        total_functions += len(functions)
        summary.append(f"  {filename}: {len(functions)} 个 HFLINK_API 函数")

    commit = run_git(sdk_root, ["rev-parse", "--short", "HEAD"]) or "unknown"
    branch = run_git(sdk_root, ["rev-parse", "--abbrev-ref", "HEAD"]) or "unknown"
    remote = run_git(sdk_root, ["config", "--get", "remote.origin.url"]) or "(no remote)"
    synced_at = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

    sync_info = "\n".join(
        [
            f"sdk_repo: {remote}",
            f"sdk_commit: {commit}",
            f"sdk_branch: {branch}",
            f"synced_at: {synced_at}",
            f"files: {len(SNAPSHOT_FILES)}",
        ]
    )
    (output_dir / "SYNC_INFO").write_text(sync_info + "\n", encoding="utf-8", newline="\n")

    print(f"已同步 {len(SNAPSHOT_FILES)} 个头文件到 {output_dir}")
    print("\n".join(summary))
    print(f"共 {total_functions} 个 HFLINK_API 函数")
    print(f"SDK 来源：{commit} ({branch})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
