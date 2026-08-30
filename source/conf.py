"""HFLinkSDK 文档 Sphinx 配置（Sphinx + Breathe + MyST）。"""

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# -- 项目信息 ---------------------------------------------------------------


def _git(*args):
    """执行 git 命令，失败返回 None。"""
    try:
        result = subprocess.run(
            ["git", "-C", str(ROOT)] + list(args), capture_output=True, text=True, timeout=15
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip() or None


def _read_sdk_version():
    """从头文件获取记录（SYNC_INFO）读取 SDK 来源：优先 release tag，回退 commit。"""
    info = ROOT / "sdk-headers" / "SYNC_INFO"
    if not info.is_file():
        return "unknown"
    text = info.read_text(encoding="utf-8")
    tag = re.search(r"^sdk_tag:\s*(\S+)", text, re.MULTILINE)
    commit = re.search(r"^sdk_commit:\s*(\S+)", text, re.MULTILINE)
    if tag:
        return tag.group(1)
    return commit.group(1) if commit else "unknown"


_author = "HFLink Project"
_release = _git("describe", "--tags", "--always") or "latest"
_sdk_version = _read_sdk_version()

project = "HFLinkSDK"
copyright = f"2026, {_author} · API 参考基于 SDK {_sdk_version}"
author = _author
version = _release
release = _release

# -- 通用配置 ---------------------------------------------------------------

extensions = [
    "breathe",
    "myst_parser",
]

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# -- MyST（Markdown 手写页）-------------------------------------------------

myst_enable_extensions = [
    "tasklist",
    "deflist",
    "substitution",
]

# -- Breathe（Doxygen XML）--------------------------------------------------

breathe_projects = {"HFLinkSDK": str(ROOT / "doxygen" / "xml")}
breathe_default_project = "HFLinkSDK"
breathe_domain_by_extension = {"h": "c"}
breathe_show_define_initializer = True

# -- HTML 输出（sphinx_rtd_theme + 中文）--------------------------------------

html_theme = "sphinx_rtd_theme"
html_theme_options = {
    "style_nav_header_background": "#1e3a5f",
    "navigation_depth": 4,
    "collapse_navigation": False,
}
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_title = f"HFLinkSDK 文档 ({release})"
html_show_sourcelink = False
html_show_sphinx = False

language = "zh_CN"
