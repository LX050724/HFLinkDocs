# 快速开始

本页演示用 HFLinkSDK 完成「枚举探针 → 打开设备 → 烧录镜像」的最小流程，分别给出 C API 与 HFLinkCLI 两种方式。

## 前置条件

- 一台 HFLink 探针（如 HSLink Pro），或部分受支持的第三方 CMSIS-DAP 探针
  
- 已安装目标芯片的 CMSIS-Pack（例如 `STM32H7xx_DFP`），Pack 管理工具或 `HFLink_Pack_ListInstalled` 可查询
- C 集成：链接 `HFLinkDriver.dll`（含导入库），头文件包含路径指向 SDK `Driver/include/` 目录

支持的第三方CMSIS-DAP：

- [HSLink Pro CherryDAP](https://cherrydap.cherry-embedded.org/projects/HSLink%20Pro.html)


## C API：枚举并打开探针

```c
#include <HFLinkDriver.h>
#include <stdio.h>

int main(void)
{
    /* 1. 初始化驱动（use_async 非 0 时启用异步发送线程） */
    if (HFLink_Initialize(0) != HFLINK_OK) {
        fprintf(stderr, "driver init failed\n");
        return 1;
    }

    /* 2. 枚举探针 */
    HFLink_DeviceInfo devices[8];
    int count = HFLink_GetDeviceInfo(devices, 8);
    if (count <= 0) {
        fprintf(stderr, "no probe found\n");
        HFLink_Cleanup();
        return 1;
    }
    printf("probe: %s (serial %s)\n", devices[0].device_name, devices[0].serial_number);

    /* 3. 打开第一个探针 */
    HFLink_Handle handle = NULL;
    if (HFLink_Open(&devices[0], &handle) != HFLINK_OK) {
        fprintf(stderr, "open failed\n");
        HFLink_Cleanup();
        return 1;
    }
    printf("model=%s serial=%s\n", HFLink_GetModelName(handle), HFLink_GetSerialNumber(handle));

    /* 4. 关闭设备并清理 */
    HFLink_Close(handle);
    HFLink_Cleanup();
    return 0;
}
```

## C API：烧录镜像（Pack 生产连接 + 一站式下载）

生产级烧录推荐走 Pack 生命周期会话 `HFLink_PackDebugSession_Open`，再调用一站式 Flash API：

```c
#include <HFLinkDriver.h>

/* options 中其余回调可全部留空，使用驱动内置的默认 Debug Access Sequence */
static HFLink_PackDebugSessionOptions make_options(void)
{
    HFLink_PackDebugSessionOptions options = {0};
    options.transport = HFLINK_PACK_TRANSPORT_SWD;   /* 必填 */
    options.clock_hz = 10000000;                     /* 必填：10 MHz */
    return options;
}

int program(const char *device_selector, const char *image_path)
{
    if (HFLink_Initialize(0) != HFLINK_OK) {
        return -1;
    }

    /* 1. 建立完整 Pack 调试连接（拓扑构建、核会话、examine） */
    HFLink_PackDebugSessionOptions options = make_options();
    HFLink_PackDebugSession *session = NULL;
    int rc = HFLink_PackDebugSession_Open(device_selector, &options, &session);
    if (rc != HFLINK_OK) {
        HFLink_Cleanup();
        return rc;
    }

    /* 2. 打开镜像（auto 识别 ELF/BIN/Intel HEX） */
    HFLink_FlashImage *image = NULL;
    rc = HFLink_FlashImage_Open(image_path, HFLINK_FLASH_IMAGE_AUTO, NULL, &image);
    if (rc != HFLINK_OK) {
        HFLink_PackDebugSession_Close(session);
        HFLink_PackDebugSession_Destroy(session);
        HFLink_Cleanup();
        return rc;
    }

    /* 3. 一站式完整下载（erase + program + verify） */
    HFLink_FlashPlanOptions plan_options = {0};
    plan_options.erase_mode = HFLINK_FLASH_ERASE_SECTORS;  /* 只擦镜像覆盖的扇区 */
    plan_options.restore_after_erase = 0;                  /* 未覆盖区域保持擦除态 */
    int primary = 0, cleanup = 0;
    rc = HFLink_Flash_ProgramEx(session, image, &plan_options, NULL, &primary, &cleanup);

    /* 4. 逆序释放：镜像 → 会话 → 驱动 */
    HFLink_FlashImage_Destroy(image);
    HFLink_PackDebugSession_Close(session);
    HFLink_PackDebugSession_Destroy(session);
    HFLink_Cleanup();
    return rc != HFLINK_OK ? rc : primary;
}
```

```{note}
- `device_selector` 语法为 `设备名[@Pack名[/版本]][/处理器名]`，例如
  `STM32H723ZGTx@STM32H7xx_DFP`；省略版本时自动匹配已安装的最高版本。
- `primary_error` / `cleanup_error` 分别是下载主错误与清理阶段错误，含义见
  [错误码参考](api/errno.md)。
- 需要自定义进度回调、下载后复位/运行时，见 API 参考的
  {ref}`Flash 下载 <api_flash>` 与 {ref}`Pack 生命周期 <api_pack_lifecycle>` 页。
```

## HFLinkCLI：等价命令行

不写代码时，同样的烧录流程一条命令完成：

```bash
HFLinkCLI flash program --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --image app.elf
```

其他常用命令：

```bash
# 离线查看芯片 Flash 布局与下载计划（不接触探针）
HFLinkCLI flash probe --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --image app.elf

# 启动 GDB 服务器（多核自动分配端口）+ RTT + semihosting
HFLinkCLI gdb --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 \
    --rtt --elf app.elf --semihosting

# RTT 日志守护，telnet 127.0.0.1:19021 查看
HFLinkCLI rtt --pack STM32H723ZGTx@STM32H7xx_DFP --interface swd --speed 10000000 --elf app.elf
```

CLI 的完整用法见 [HFLinkCLI 使用指南](guide/cli_overview.md)。
