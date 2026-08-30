#!/usr/bin/env python3
"""获取用于文档构建的 HFLinkDriver 公开头文件（写入 sdk-headers/，该目录不入库）。

两种来源（二选一）：
- --release <tag|latest>：从 GitHub 下载 HFLink_SDK 发布的 SDK 开发包
  （HFLinkSDK-<tag>-win64.zip），解出 include/*.h 与 VERSION.txt
- --sdk <本地路径>：从本地 HFLinkSDK 仓库的 Driver/include 复制（本地开发调试用）

SYNC_INFO 记录来源（release tag 或源 commit），conf.py 读取后展示在页脚。

安全约束：仅允许向 https + GitHub 固定域名发起请求，tag 参数做字符白名单校验，
下载地址由校验通过的常量与资产名拼接（不使用 API 返回的任意 URL）。
"""

import argparse
import datetime
import io
import json
import re
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADERS_DIR = ROOT / "sdk-headers"
SDK_REPO = "LX050724/HFLink_SDK"
ASSET_PATTERN = re.compile(r"HFLinkSDK.*\.zip$", re.IGNORECASE)
TAG_PATTERN = re.compile(r"[A-Za-z0-9._-]+")
TRUSTED_HOSTS = {"api.github.com", "github.com"}


def open_url(url):
    """仅允许 https + GitHub 受信域名的 GET 请求。"""
    parts = urllib.parse.urlparse(url)
    if parts.scheme != "https" or parts.hostname not in TRUSTED_HOSTS:
        print(f"错误：拒绝请求非受信地址：{url}", file=sys.stderr)
        sys.exit(1)
    request = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response


def fetch_from_release(tag):
    """从 SDK release 的开发包 zip 提取头文件；返回版本标识（tag）。"""
    if not TAG_PATTERN.fullmatch(tag):
        print(f"错误：非法 tag：{tag}", file=sys.stderr)
        sys.exit(1)
    api_url = f"https://api.github.com/repos/{SDK_REPO}/releases/tags/{tag}"
    if tag == "latest":
        api_url = f"https://api.github.com/repos/{SDK_REPO}/releases/latest"
    with open_url(api_url) as response:
        release = json.load(response)
    assets = [a for a in release.get("assets", []) if ASSET_PATTERN.search(a["name"])]
    if not assets:
        print(f"错误：release {release.get('tag_name', tag)} 未找到 SDK 开发包（HFLinkSDK-*-win64.zip）",
              file=sys.stderr)
        sys.exit(1)
    asset_name = assets[0]["name"]
    download_url = f"https://github.com/{SDK_REPO}/releases/download/{tag}/{asset_name}"
    print(f"[docs] 下载 {asset_name} ...")
    with open_url(download_url) as response:
        archive = zipfile.ZipFile(io.BytesIO(response.read()))
    extracted = 0
    for name in archive.namelist():
        parts = name.split("/")
        if len(parts) >= 2 and parts[-2] == "include" and parts[-1].endswith(".h"):
            (HEADERS_DIR / parts[-1]).write_bytes(archive.read(name))
            extracted += 1
    if not extracted:
        print("错误：开发包内未找到 include/*.h，包结构已变更", file=sys.stderr)
        sys.exit(1)
    version = tag
    for name in archive.namelist():
        if name.endswith("VERSION.txt"):
            version = archive.read(name).decode("utf-8").strip()
    return version


def fetch_from_local(sdk_root):
    """从本地 SDK 仓库复制头文件；返回 commit 短哈希。"""
    include_dir = Path(sdk_root).resolve() / "Driver" / "include"
    if not include_dir.is_dir():
        print(f"错误：SDK 头文件目录不存在：{include_dir}", file=sys.stderr)
        sys.exit(1)
    headers = sorted(include_dir.glob("*.h"))
    if not headers:
        print(f"错误：{include_dir} 下没有头文件", file=sys.stderr)
        sys.exit(1)
    for header in headers:
        (HEADERS_DIR / header.name).write_bytes(header.read_bytes())
    try:
        commit = subprocess.run(
            ["git", "-C", str(sdk_root), "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=15, check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        commit = "unknown"
    return commit


def main():
    parser = argparse.ArgumentParser(description="获取文档构建所需头文件")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--release", metavar="TAG",
                       help="从 HFLink_SDK release 获取（tag 名或 latest）")
    group.add_argument("--sdk", metavar="PATH", help="从本地 HFLinkSDK 仓库获取")
    args = parser.parse_args()

    HEADERS_DIR.mkdir(parents=True, exist_ok=True)
    synced_at = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    if args.release:
        version = fetch_from_release(args.release)
        source = f"sdk_tag: {version}"
    else:
        version = fetch_from_local(args.sdk)
        source = f"sdk_commit: {version}"
    info = f"sdk_repo: {SDK_REPO}\n{source}\nsynced_at: {synced_at}\n"
    (HEADERS_DIR / "SYNC_INFO").write_text(info, encoding="utf-8", newline="\n")
    print(f"已获取 {len(list(HEADERS_DIR.glob('*.h')))} 个头文件到 sdk-headers/（来源：{version}）")


if __name__ == "__main__":
    main()
