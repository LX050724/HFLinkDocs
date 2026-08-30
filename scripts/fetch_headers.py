#!/usr/bin/env python3
"""获取用于文档构建的 HFLinkDriver 公开头文件（写入 sdk-headers/，该目录不入库）。

两种来源（二选一）：
- --release <tag|latest>：从 GitHub 下载 HFLink_SDK 发布的 SDK 开发包
  （HFLinkSDK-<tag>-win64.zip），解出 include/*.h 与 VERSION.txt
- --sdk <本地路径>：从本地 HFLinkSDK 仓库的 Driver/include 复制（本地开发调试用）

认证：HFLink_SDK 为私有仓库时必须提供 token——优先环境变量 HF_LINK_SDK_TOKEN，
否则自动从本机 git credential（github.com）获取；release 资产走 API octet-stream
端点下载，token 不会跟随重定向泄漏到外部域。

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
import urllib.error
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

_TOKEN = None


def get_token():
    """解析 token：环境变量 HF_LINK_SDK_TOKEN 优先，其次本机 git credential（github.com）。"""
    global _TOKEN
    if _TOKEN is not None:
        return _TOKEN
    import os

    _TOKEN = os.environ.get("HF_LINK_SDK_TOKEN") or None
    if not _TOKEN:
        try:
            fill = subprocess.run(
                ["git", "credential", "fill"],
                input="protocol=https\nhost=github.com\n\n",
                capture_output=True,
                text=True,
                timeout=15,
            ).stdout
            match = re.search(r"^password=(.+)$", fill, re.MULTILINE)
            _TOKEN = match.group(1).strip() if match else None
        except (OSError, subprocess.SubprocessError):
            _TOKEN = None
    return _TOKEN


def open_url(url, accept="application/vnd.github+json", follow_redirect_token=False):
    """仅允许 https + GitHub 受信域名的 GET 请求；有 token 时附认证。

    follow_redirect_token=False 时禁止自动跟随重定向（302 时返回
    (None, location)），避免认证头被带到外部下载域。
    """
    parts = urllib.parse.urlparse(url)
    if parts.scheme != "https" or parts.hostname not in TRUSTED_HOSTS:
        print(f"错误：拒绝请求非受信地址：{url}", file=sys.stderr)
        sys.exit(1)
    request = urllib.request.Request(url, headers={"Accept": accept})
    token = get_token()
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    if not follow_redirect_token:
        opener = urllib.request.build_opener(NoRedirect)
        try:
            return opener.open(request, timeout=120), None
        except urllib.error.HTTPError as error:
            if error.code in (301, 302, 303, 307, 308):
                return None, error.headers.get("Location")
            raise
    return urllib.request.urlopen(request, timeout=120), None


class NoRedirect(urllib.request.HTTPRedirectHandler):
    """禁止 urllib 自动跟随重定向（防止 Authorization 头外泄）。"""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def fetch_from_release(tag):
    """从 SDK release 的开发包 zip 提取头文件；返回版本标识（tag）。"""
    if not TAG_PATTERN.fullmatch(tag):
        print(f"错误：非法 tag：{tag}", file=sys.stderr)
        sys.exit(1)
    api_url = f"https://api.github.com/repos/{SDK_REPO}/releases/tags/{tag}"
    if tag == "latest":
        api_url = f"https://api.github.com/repos/{SDK_REPO}/releases/latest"
    try:
        with open_url(api_url) as response:
            release = json.load(response)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            print(
                f"错误：无法访问 release（{tag}）。HFLink_SDK 为私有仓库时需要 token：\n"
                "  设置环境变量 HF_LINK_SDK_TOKEN（对 HFLink_SDK 有 contents:read 的 PAT），\n"
                "  或在本机 git credential 中保存 github.com 凭证；\n"
                "  另请确认该 release 已发布且存在 HFLinkSDK 附件 zip。",
                file=sys.stderr,
            )
            sys.exit(1)
        raise
    assets = [a for a in release.get("assets", []) if ASSET_PATTERN.search(a["name"])]
    if not assets:
        print(f"错误：release {release.get('tag_name', tag)} 未找到 SDK 开发包（HFLinkSDK*.zip）",
              file=sys.stderr)
        sys.exit(1)
    asset = assets[0]
    print(f"[docs] 下载 {asset['name']} ...")
    # 私有仓库资产下载：走 API octet-stream 端点（认证在 api.github.com 上完成），
    # 302 到的预签名下载 URL 不携带 token。
    asset_url = f"https://api.github.com/repos/{SDK_REPO}/releases/assets/{asset['id']}"
    response, location = open_url(asset_url, accept="application/octet-stream")
    if response is None and location:
        parts = urllib.parse.urlparse(location)
        if parts.scheme != "https":
            print("错误：下载重定向地址非 https，已中止", file=sys.stderr)
            sys.exit(1)
        response = urllib.request.urlopen(location, timeout=300)
    archive = zipfile.ZipFile(io.BytesIO(response.read()))
    extracted = 0
    for name in archive.namelist():
        parts = name.split("/")
        if len(parts) >= 2 and parts[-2] == "include" and parts[-1].endswith(".h"):
            (HEADERS_DIR / parts[-1]).write_bytes(archive.read(name))
            extracted += 1
    if not extracted:
        print("错误：开发包内未找到 include 目录或头文件，包结构已变更", file=sys.stderr)
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
