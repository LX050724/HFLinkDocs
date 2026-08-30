#!/usr/bin/env python3
"""HFLinkDocs 本地一键构建入口（自动管理虚拟环境）。

用法：
    python scripts/build_docs.py

流程：
1. 确保 .venv 虚拟环境存在（缺失时用系统 python 创建）
2. 通过 venv 环境检测并安装 requirements.txt 依赖
3. doxygen 生成 XML -> gen_api_rst.py 生成 API 存根页 -> sphinx-build -W 构建站点
4. 产物位于 source/_build/html/

所有子命令均以固定字面量参数列表执行（无 shell），虚拟环境通过 PATH/VIRTUAL_ENV
环境变量注入，不依赖进程重启。
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENV_DIR = ROOT / ".venv"

# venv 内必须可导入的依赖包
DEPENDENCY_IMPORTS = "import sphinx, breathe, myst_parser, sphinx_rtd_theme"


def venv_python():
    """venv 解释器路径（按平台）。"""
    if os.name == "nt":
        return VENV_DIR / "Scripts" / "python.exe"
    return VENV_DIR / "bin" / "python"


def activate_venv():
    """把 venv 前置到当前进程 PATH（CreateProcess 按父进程 PATH 解析可执行文件）。"""
    bin_dir = VENV_DIR / "Scripts" if os.name == "nt" else VENV_DIR / "bin"
    os.environ["VIRTUAL_ENV"] = str(VENV_DIR)
    os.environ["PATH"] = str(bin_dir) + os.pathsep + os.environ.get("PATH", "")


def ensure_venv():
    """venv 不存在时用系统 python 创建。"""
    if not venv_python().is_file():
        print("[docs] 创建虚拟环境 .venv ...")
        subprocess.run(["python", "-m", "venv", ".venv"], check=True, cwd=str(ROOT))


def ensure_dependencies():
    """用 venv 探测关键依赖包，缺失则安装 requirements.txt。"""
    probe = subprocess.run(
        ["python", "-c", DEPENDENCY_IMPORTS],
        check=False,
        cwd=str(ROOT),
        capture_output=True,
    )
    if probe.returncode == 0:
        return
    print("[docs] 安装依赖 requirements.txt ...")
    subprocess.run(
        ["python", "-m", "pip", "install", "-r", "requirements.txt"],
        check=True,
        cwd=str(ROOT),
    )


def check_doxygen():
    """检测 doxygen（>= 1.9 才支持 OUTPUT_LANGUAGE=Chinese）。"""
    doxygen = shutil.which("doxygen")
    if not doxygen:
        print(
            "错误：未找到 doxygen。请安装后重试：\n"
            "  Windows: winget install Doxygen.Doxygen  或官网 https://www.doxygen.nl/download.html\n"
            "  macOS:   brew install doxygen\n"
            "  Debian/Ubuntu: sudo apt install doxygen",
            file=sys.stderr,
        )
        raise SystemExit(1)
    try:
        output = subprocess.run(
            ["doxygen", "--version"], check=True, capture_output=True, text=True
        ).stdout
        version = tuple(int(part) for part in re.match(r"(\d+)\.(\d+)", output).groups())
    except (subprocess.SubprocessError, AttributeError):
        return
    if version < (1, 9):
        print(f"错误：doxygen {output.strip()} 过旧，中文注释解析需要 1.9 以上", file=sys.stderr)
        raise SystemExit(1)


def main():
    ensure_venv()
    activate_venv()
    ensure_dependencies()
    check_doxygen()

    print("[docs] doxygen Doxyfile")
    (ROOT / "doxygen" / "xml").mkdir(parents=True, exist_ok=True)
    subprocess.run(["doxygen", "Doxyfile"], check=True, cwd=str(ROOT))

    print("[docs] gen_api_rst.py")
    subprocess.run(["python", "scripts/gen_api_rst.py"], check=True, cwd=str(ROOT))

    print("[docs] sphinx-build")
    subprocess.run(
        ["python", "-m", "sphinx", "-b", "html", "-W", "--keep-going",
         "source", "source/_build/html"],
        check=True,
        cwd=str(ROOT),
    )
    print(f"\n[docs] 构建完成：{ROOT / 'source' / '_build' / 'html' / 'index.html'}")


if __name__ == "__main__":
    main()
