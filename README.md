# HFLinkSDK 文档

HFLinkSDK（基于 CMSIS-DAP 协议的调试器驱动 SDK）的官方文档源，使用 **Sphinx + Read the Docs** 构建，部署于 Read the Docs 多版本站点。

- API 参考由 **Doxygen + Breathe** 从 `sdk-headers/` 快照中的公开头文件自动抽取（含中文 Doxygen 注释）
- CLI / Lua 脚本使用手册为手写 MyST Markdown
- 头文件快照来自 [HFLinkSDK](https://github.com/your-org/HFLinkSDK) 仓库，`sdk-headers/SYNC_INFO` 记录快照对应的 SDK commit

## 本地构建

依赖：Python 3.9+、Doxygen（`doxygen --version` 可执行）。

```bash
pip install -r requirements.txt

# 1. 从本地 HFLinkSDK 仓库同步头文件快照（改动 SDK 头文件后重新执行）
python scripts/sync_headers.py --sdk D:/source/HFLinkSDK

# 2. 一键构建（doxygen → 生成 API 存根 → sphinx-build）
python scripts/build_docs.py
```

构建产物在 `source/_build/html/`，用浏览器打开 `index.html`。

## 目录结构

```
├── Doxyfile              # Doxygen 配置（仅输出 XML，供 Breathe 消费）
├── sdk-headers/          # HFLinkSDK 公开头文件快照（勿手改，用 sync_headers.py 更新）
├── scripts/
│   ├── sync_headers.py   # 从本地 SDK 仓库同步头文件快照
│   ├── gen_api_rst.py    # 按功能域生成 API 参考存根页（含覆盖自检）
│   └── build_docs.py     # 本地一键构建
└── source/
    ├── conf.py           # Sphinx 配置（zh_CN，rtd_theme，MyST + Breathe）
    ├── quickstart.md     # 快速开始
    ├── guide/            # HFLinkCLI 使用手册（flash / gdb / rtt / Lua 脚本）
    └── api/              # API 导览、错误码参考、生成的 API 存根页
```

## 发布

- **Read the Docs**：将本仓库导入 [readthedocs.org](https://readthedocs.org)，每个 git tag 自动构建一个版本站点（配置见 `.readthedocs.yaml`）。
- **GitHub Release**：推送 `v*` tag 时，GitHub Actions 自动构建离线 HTML 并作为附件上传（配置见 `.github/workflows/release.yml`）。
