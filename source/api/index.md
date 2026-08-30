# API 参考

HFLinkDriver 公开 API 按 `HFLINK_API` 宏导出，全部为 C 接口（`extern "C"`），前缀 `HFLink_`。
API 参考由 Doxygen 从公开头文件自动抽取，各页面右上角标注来源头文件。

## 模块总览

| 模块 | 头文件 | 内容 |
|------|--------|------|
| {doc}`_generated/device` | `HFLinkDriver.h` | 驱动初始化、探针枚举/打开、固件升级、配置项、传感器、SWO |
| {doc}`_generated/pack` | `HFLinkDriver_pack.h` | 已安装 CMSIS-Pack 枚举与芯片信息查询 |
| {doc}`_generated/pack_lifecycle` | `HFLinkDriver_pack_lifecycle.h` | 生产连接生命周期（拓扑/核会话/复位/Flash 绑定） |
| {doc}`_generated/flash` | `HFLinkDriver_flash.h` | 镜像、计划、执行会话与一站式下载 |
| {doc}`_generated/rtt` | `HFLinkDriver_rtt.h` | SEGGER RTT 主机侧 |
| {doc}`_generated/hss` | `HFLinkDriver_hss.h` | HSS 高速内存采样 |
| {doc}`_generated/semihosting` | `HFLinkDriver_semihosting.h` | ARM Semihosting 主机服务代理 |
| {doc}`errno` | `HFLinkDriver_errno.h` | 统一错误码 |

DAP 直通层（`HFLinkDriver_dap.h`）、调试会话层（设备会话/CoreSight 上下文/目标控制）、Lua C 运行时
与日志接口属于内部集成层，不在公开文档范围。

## 通用约定

- **错误码返回**：API 返回 `HFLINK_OK`（0）表示成功，负数为 {doc}`errno` 中的错误码；
  输出参数通过指针回传。
- **资源释放**：失败路径统一 `goto err` 清理；每个 `*_Create` / `*_Open` 都有对应的
  `*_Destroy` / `*_Close`，两者均可安全传入 NULL。
- **借用指针**：标注"借用"的返回指针由 SDK 持有，不增加引用计数，随源对象销毁失效；
  标注"深拷贝/持有"的返回值由调用方释放。
- **线程安全**：单设备会话内部有事务锁；进度/输出类回调可能在内部线程触发，
  回调内不得调用会引发重入的驱动 API（详见各函数 `@note`）。

```{toctree}
:maxdepth: 1

errno
_generated/device
_generated/pack
_generated/pack_lifecycle
_generated/flash
_generated/rtt
_generated/hss
_generated/semihosting
```
