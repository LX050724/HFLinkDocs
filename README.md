# HFLinkSDK 文档

HFLinkSDK（基于 CMSIS-DAP 协议的调试器驱动 SDK）的官方文档源，使用 **Sphinx + Read the Docs** 构建。

**在线阅读**：<https://lx050724.github.io/HFLinkDocs/>

- API 参考由 **Doxygen + Breathe** 从 HFLinkDriver 公开头文件自动抽取（含中文 Doxygen 注释）
- CLI / Lua 脚本使用手册为手写 MyST Markdown
- 头文件**不入库**：构建时从 [HFLinkSDK](https://github.com/LX050724/HFLink_SDK) 的 release 开发包获取（`sdk-headers/SYNC_INFO` 记录来源版本），或从本地 SDK 仓库复制

## 本地构建

依赖：Python 3.9+、Doxygen（`doxygen --version` 可执行）。

```bash
pip install -r requirements.txt

# 1. 获取头文件（二选一）
python scripts/fetch_headers.py --release latest          # 从 SDK 最新 release 下载
python scripts/fetch_headers.py --sdk D:/source/HFLinkSDK # 或从本地 SDK 仓库复制（开发调试）

# 2. 一键构建（doxygen → 生成 API 存根 → sphinx-build）
python scripts/build_docs.py
```

构建产物在 `source/_build/html/`，用浏览器打开 `index.html`。

## 目录结构

```
├── Doxyfile              # Doxygen 配置（仅输出 XML，供 Breathe 消费）
├── sdk-headers/          # 头文件获取目录（不入库，fetch_headers.py 生成）
├── scripts/
│   ├── fetch_headers.py  # 从 SDK release 开发包或本地仓库获取头文件
│   ├── gen_api_rst.py    # 按功能域生成 API 参考存根页（含覆盖自检）
│   └── build_docs.py     # 本地一键构建（自动 venv + 依赖）
└── source/
    ├── conf.py           # Sphinx 配置（zh_CN，rtd_theme，MyST + Breathe）
    ├── quickstart.md     # 快速开始
    ├── guide/            # HFLinkCLI 使用手册（flash / gdb / rtt / Lua 脚本）
    └── api/              # API 导览、错误码参考、生成的 API 存根页
```

## 发布

- **GitHub Pages**：仓库 Settings → Pages → Source 选 **GitHub Actions** 后，每次推送 master/main 自动部署最新文档站（配置见 `.github/workflows/pages.yml`），地址 `https://<用户名>.github.io/<仓库名>/`。
- **SDK release 联动**：HFLink_SDK 发布 release 时自动通过 `repository_dispatch` 触发 `sdk-release.yml`：从该 release 的 SDK 开发包获取头文件 → 构建文档 → 以同名 tag 发布文档 release，附离线 HTML zip。
- **手动 tag 发布**：推送 `v*` tag 也会按对应 SDK release 构建并发布（配置见 `.github/workflows/release.yml`）。
- **Read the Docs**（可选）：导入 [readthedocs.org](https://readthedocs.org) 可获得多版本站点（配置见 `.readthedocs.yaml`）。
