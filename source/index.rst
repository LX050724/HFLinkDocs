HFLinkSDK 文档
==============

HFLinkSDK 是基于 **CMSIS-DAP 协议** 的调试器驱动 SDK，提供 USB 设备通信、CoreSight/Cortex-M 调试、
CMSIS-Pack 解析与 Flash 下载等能力，由以下组件构成：

- **HFLinkDriver** （C11 动态库）——SDK 核心，所有公开 API 的载体
- **HFLinkCLI** （命令行工具）——``flash`` / ``gdb`` / ``rtt`` 子命令 + 内嵌 Lua 脚本模式
- **HFLink_Flash / HFLink_Configure / HFLink_CMSIS_Pack_Management** （Qt6 GUI 工具，另见各自软件内帮助）

.. tip::
   API 参考由 Doxygen 从 SDK 公开头文件自动抽取，页面底部标注了快照对应的 SDK commit。

.. toctree::
   :maxdepth: 2
   :caption: 入门

   quickstart

.. toctree::
   :maxdepth: 2
   :caption: 使用指南 — HFLinkCLI

   guide/cli_overview
   guide/cli_flash
   guide/cli_gdb
   guide/cli_rtt
   guide/lua_mode

.. toctree::
   :maxdepth: 2
   :caption: API 参考

   api/index

.. toctree::
   :maxdepth: 2
   :caption: 更新日志

   changelog
