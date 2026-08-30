/**
 * @file HFLinkDriver_semihosting.h
 * @brief ARM Semihosting 主机服务代理：在主机侧响应目标通过 BKPT #0xAB 发起的标准 semihosting 请求。
 *
 * 处理器覆盖 SYS_OPEN/CLOSE/WRITEC/WRITE0/WRITE/READ/SEEK/FLEN/TMPNAM/REMOVE/RENAME/CLOCK/TIME/
 * ERRNO/SYSTEM/GET_CMDLINE/HEAPINFO/EXIT/ELAPSED/TICKFREQ 等操作码；文件操作落在主机文件系统
 * （相对路径以工作目录为根，含 ".." 的相对路径被拒绝）。注册 Lua 状态机后，Lua 处理器可优先接管请求。
 */
#pragma once

#include "HFLinkDriver_import.h"
#include "HFLinkDriver.h"
#include "HFLinkDriver_lua.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief TryHandle 未命中 semihosting 请求（PC 处不是 BKPT #0xAB）时的返回值。 */
#define HFLINK_SEMIHOSTING_NOT_HANDLED 0
/** @brief TryHandle 成功处理一次 semihosting 请求时的返回值。 */
#define HFLINK_SEMIHOSTING_HANDLED 1

typedef struct HFLink_SemihostingHandle HFLink_SemihostingHandle;

/** @brief Semihosting 输出回调；text 在回调返回后失效。 */
typedef void (*HFLink_SemihostingOutputCallback)(const char *text, size_t length, void *userdata);

/** @brief Semihosting 请求的可变视图，Lua 处理器通过 userdata 访问。 */
typedef struct HFLink_SemihostingRequest HFLink_SemihostingRequest;

/**
 * @brief 创建 Semihosting 处理器并绑定目标。
 *
 * 工作目录默认为主机进程当前目录；文件描述符 0/1/2 预置为主机 stdin/stdout/stderr。
 *
 * @param target 目标控制句柄（借用引用，调用方保证其生命周期覆盖本句柄）。
 * @return 成功返回句柄；内存分配或工作目录获取失败返回 NULL。
 */
HFLINK_API HFLink_SemihostingHandle *HFLink_Semihosting_Create(HFLink_CoreSightTarget *target);

/**
 * @brief 销毁处理器并关闭所有仍打开的主机文件（fd ≥ 3）。
 * @param handle 可为 NULL。
 */
HFLINK_API void HFLink_Semihosting_Destroy(HFLink_SemihostingHandle *handle);

/**
 * @brief 使能处理器；未使能时 TryHandle 直接返回 HFLINK_SEMIHOSTING_NOT_HANDLED。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_Enable(HFLink_SemihostingHandle *handle);

/**
 * @brief 停用处理器（不销毁，配置保留）。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_Disable(HFLink_SemihostingHandle *handle);

/**
 * @brief 设置相对路径的解析根目录（SYS_OPEN/REMOVE/RENAME/TMPNAM 使用）。
 * @param directory 非空目录字符串。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 参数为空；HFLINK_ERR 内存不足。
 * @note 含 ".." 段的相对路径在解析时会被直接拒绝，不落盘。
 */
HFLINK_API int HFLink_Semihosting_SetWorkingDirectory(HFLink_SemihostingHandle *handle, const char *directory);

/**
 * @brief 设置控制台输出回调（SYS_WRITEC/WRITE0 及 SYS_WRITE 到 fd 1/2 的输出）。
 * @param callback 传 NULL 恢复默认（写主机 stdout）。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_SetOutputCallback(HFLink_SemihostingHandle *handle,
                                                    HFLink_SemihostingOutputCallback callback, void *userdata);

/**
 * @brief 设置 SYS_GET_CMDLINE 返回的命令行字符串。
 * @param command_line 传 NULL 清除（此后 SYS_GET_CMDLINE 失败）。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空；HFLINK_ERR 内存不足。
 */
HFLINK_API int HFLink_Semihosting_SetCmdLine(HFLink_SemihostingHandle *handle, const char *command_line);

/**
 * @brief 设置 SYS_EXIT 后是否保持目标 halt。
 * @param halt 非 0：目标停在断点处等待调试器接管（默认）；0：自动恢复运行。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_SetExitHalt(HFLink_SemihostingHandle *handle, int halt);

/**
 * @brief 是否允许 SYS_SYSTEM 执行主机命令行。
 * @param enabled 非 0 允许；默认禁止（请求直接失败）。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 * @note 允许后目标固件可在主机上执行任意命令，仅在完全可信场景开启。
 */
HFLINK_API int HFLink_Semihosting_SetSystemEnabled(HFLink_SemihostingHandle *handle, int enabled);

/**
 * @brief 绑定 Lua 状态机；已注册的 Lua 处理器将优先于内置默认实现接管请求。
 * @param state 传 NULL 移除绑定，恢复纯内置实现。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_SetLuaState(HFLink_SemihostingHandle *handle, HFLink_LuaState *state);

/** @brief 直接分发一个请求；供 GDB/测试路径调用，返回目标应写回 R0 的值。 */
HFLINK_API int HFLink_Semihosting_Dispatch(HFLink_SemihostingHandle *handle, uint32_t opcode, uint64_t argument,
                                            uint32_t *result);

/** @brief 在目标已 halt 时检测并处理 Thumb `BKPT #0xAB`。 */
HFLINK_API int HFLink_Semihosting_TryHandle(HFLink_SemihostingHandle *handle);

/**
 * @brief 查询目标是否已请求退出（SYS_EXIT）。
 * @param[out] reason 输出退出原因（SYS_EXIT 请求的 R1 值），可为 NULL。
 * @return 1 已请求退出；0 未请求；HFLINK_ERR_NOT_INITIALIZED 句柄为空。
 */
HFLINK_API int HFLink_Semihosting_IsExitRequested(const HFLink_SemihostingHandle *handle, uint32_t *reason);

/* 以下函数供 Lua request 对象和扩展处理器使用。 */

/** @brief 返回请求操作码（目标 R0），request 为 NULL 时返回 0。 */
HFLINK_API uint32_t HFLink_SemihostingRequest_GetOpcode(const HFLink_SemihostingRequest *request);

/** @brief 返回请求参数（目标 R1），request 为 NULL 时返回 0。 */
HFLINK_API uint64_t HFLink_SemihostingRequest_GetArgument(const HFLink_SemihostingRequest *request);

/**
 * @brief 读取参数块第 index 个 u32（argument + index*4，目标内存小端）。
 * @return HFLINK_OK 成功；HFLINK_ERR 读取失败或 value 为空。
 */
HFLINK_API int HFLink_SemihostingRequest_ReadU32(HFLink_SemihostingRequest *request, uint32_t index,
                                                 uint32_t *value);

/**
 * @brief 向参数块第 index 个 u32 写入（argument + index*4，小端）。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 参数为空；写内存失败返回对应错误码。
 */
HFLINK_API int HFLink_SemihostingRequest_WriteU32(HFLink_SemihostingRequest *request, uint32_t index,
                                                  uint32_t value);

/**
 * @brief 读取目标内存到主机缓冲。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 参数为空或 size 为 0；读内存失败返回对应错误码。
 */
HFLINK_API int HFLink_SemihostingRequest_ReadMemory(HFLink_SemihostingRequest *request, uint64_t address,
                                                    uint32_t size, uint8_t *buffer);

/**
 * @brief 将主机缓冲写入目标内存。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED 参数为空或 size 为 0；写内存失败返回对应错误码。
 */
HFLINK_API int HFLink_SemihostingRequest_WriteMemory(HFLink_SemihostingRequest *request, uint64_t address,
                                                     uint32_t size, const uint8_t *buffer);

/**
 * @brief 设置返回给目标的 R0 结果值。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED request 为空。
 */
HFLINK_API int HFLink_SemihostingRequest_SetResult(HFLink_SemihostingRequest *request, uint32_t value);

/**
 * @brief 设置主机 errno，供后续 SYS_ERRNO 请求查询。
 * @return HFLINK_OK 成功；HFLINK_ERR_NOT_INITIALIZED request 为空。
 */
HFLINK_API int HFLink_SemihostingRequest_SetErrno(HFLink_SemihostingRequest *request, int value);

#ifdef __cplusplus
}
#endif
