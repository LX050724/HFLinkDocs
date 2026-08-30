/**
 * @file    HFLinkDriver_rtt.h
 * @brief   SEGGER RTT（Real-Time Transfer）主机侧支持 API（SEGGER J-Link RTT API 形式）
 *
 * 接口形状对齐 SEGGER UM08002 第 7 章主机端 RTT API（JLINK_RTTERMINAL_* 三函数 +
 * 四子命令）：HFLink_RTT_Control 执行 START/STOP/GETNUMBUF/GETDESC，HFLink_RTT_Read
 * /Write 完成数据面。核心语义与 SEGGER 一致：START 后 SDK 在 CoreSight 轮询调度器上
 * 注册周期收割任务，把 up 通道新数据搬运进主机侧环形缓冲；Read 纯内存拷贝、非阻塞、
 * 不触碰调试总线，可随时安全调用。
 *
 * 控制块定位三通道（优先级从高到低，任一命中即停止）：
 * 1. **方式三（显式地址）**：START 的 ConfigBlockAddress 字段传入
 *    （调用方通常经 `HFLink_Elf_FindSymbol(elf, "_SEGGER_RTT")` 解析 ELF 符号得到）；
 * 2. **方式二（Lua 配置）**：会话绑定 Lua 运行时后，`hf.rtt.set_control_block(addr)`
 *    配置脚本在 START 前设置的地址；
 * 3. **方式一（自动扫描）**：在 `HFLink_RTT_SetScanRange` 指定的 RAM 范围内
 *    搜索 "SEGGER RTT" 魔数并对候选做完整结构校验。
 *
 * 显式/Lua 地址校验失败时输出告警并自动回退方式一扫描。
 */
#pragma once

#include "HFLinkDriver.h"
#include "HFLinkDriver_import.h"
#include "HFLinkDriver_lua.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HFLink_RTTHandle HFLink_RTTHandle;

/** @brief 缓冲方向（对照 SEGGER：0 = Up/target→host，1 = Down/host→target）。 */
#define HFLINK_RTT_DIRECTION_UP 0u
#define HFLINK_RTT_DIRECTION_DOWN 1u

/** @brief 通道名最大长度（含 NUL）；与 SEGGER BUFDESC.acName[32] 对齐。 */
#define HFLINK_RTT_NAME_SIZE 32u

/**
 * @brief RTT 控制命令（对照 SEGGER JLINKARM_RTTERMINAL_CMD_*）
 */
typedef enum HFLink_RTTCommand
{
    HFLINK_RTT_CMD_START = 0,     /**< 启动 RTT（定位控制块 + 注册后台收割任务）；p = HFLink_RTT_START* 或 NULL */
    HFLINK_RTT_CMD_STOP = 1,      /**< 停止 RTT；p = HFLink_RTT_STOP* 或 NULL */
    HFLINK_RTT_CMD_GETNUMBUF = 2, /**< 查询 up/down 缓冲数量；p = HFLink_RTT_NUMBUF*（必填） */
    HFLINK_RTT_CMD_GETDESC = 3,   /**< 查询缓冲描述（名称/大小/标志）；p = HFLink_RTT_BUFDESC*（必填） */
    HFLINK_RTT_CMD_SETTELNETCFG = 4 /**< 配置 telnet 服务端（须在 START 前调用）；p = HFLink_RTT_TELNETCFG*（必填） */
} HFLink_RTTCommand;

/** @brief START 命令参数（对照 SEGGER JLINK_RTTERMINAL_START）。 */
typedef struct HFLink_RTT_START
{
    uint64_t ConfigBlockAddress;      /**< 控制块地址；0 = 清除显式地址，走 Lua 配置/自动扫描 */
    uint32_t Dummy0;                  /**< 保留（须为 0） */
    uint32_t Dummy1;                  /**< 保留（须为 0） */
    uint32_t Dummy2;                  /**< 保留（须为 0） */
} HFLink_RTT_START;

/** @brief STOP 命令参数（对照 SEGGER JLINK_RTTERMINAL_STOP）。 */
typedef struct HFLink_RTT_STOP
{
    uint8_t InvalidateTargetCB; /**< 置 1 时向 target 侧控制块写入无效 ID（固件下次重初始化） */
    uint8_t acDummy[3];         /**< 保留 */
    uint32_t Dummy0;            /**< 保留（须为 0） */
    uint32_t Dummy1;            /**< 保留（须为 0） */
    uint32_t Dummy2;            /**< 保留（须为 0） */
} HFLink_RTT_STOP;

/** @brief GETNUMBUF 命令参数（对照 SEGGER：Direction 入，NumBuffers 出）。 */
typedef struct HFLink_RTT_NUMBUF
{
    uint32_t Direction;   /**< 入：HFLINK_RTT_DIRECTION_UP / HFLINK_RTT_DIRECTION_DOWN */
    uint32_t NumBuffers;  /**< 出：该方向缓冲（通道）数量 */
} HFLink_RTT_NUMBUF;

/** @brief GETDESC 命令参数（对照 SEGGER JLINK_RTTERMINAL_BUFDESC）。 */
typedef struct HFLink_RTT_BUFDESC
{
    int32_t BufferIndex;               /**< 入：方向内缓冲索引（从 0 起） */
    uint32_t Direction;                /**< 入：HFLINK_RTT_DIRECTION_UP / HFLINK_RTT_DIRECTION_DOWN */
    char acName[HFLINK_RTT_NAME_SIZE]; /**< 出：缓冲名（target 侧 sName 快照，NUL 结尾；无名为空串） */
    uint32_t SizeOfBuffer;             /**< 出：缓冲字节数（未配置通道为 0） */
    uint32_t Flags;                    /**< 出：target 侧 SEGGER_RTT_MODE_* 标志 */
} HFLink_RTT_BUFDESC;

/**
 * @brief SETTELNETCFG 命令参数：配置 telnet 服务端（HFLinkSDK 扩展，无 SEGGER 对应）
 *
 * telnet 服务端随 RTT START/STOP 自动启停（默认 127.0.0.1:19021）；本命令须在
 * START 前调用，RTT 运行中调用返回 HFLINK_ERR_BUSY。端口被占用时静默跳过
 * （不打告警、不影响 START，仅经 GetLastError 可查），RTT API 数据面不受影响。
 */
typedef struct HFLink_RTT_TELNETCFG
{
    uint32_t Enable;      /**< 入：1 启用（默认）；0 禁用 telnet 服务端 */
    char acBindAddr[64];  /**< 入：绑定地址（空串 = 默认 127.0.0.1；"0.0.0.0" 开放远程） */
    uint32_t Port;        /**< 入：端口（0 = 默认 19021；范围 1..65535） */
    uint32_t Dummy0;      /**< 保留（须为 0） */
} HFLink_RTT_TELNETCFG;

/**
 * @brief 执行 RTT 控制命令（对照 SEGGER JLINK_RTTERMINAL_Control）
 *
 * - CMD_START：定位并校验控制块（首次）后向 CoreSight 轮询调度器注册收割任务；
 *   已在运行时幂等返回 0。target 未挂调试会话返回 HFLINK_ERR_NOT_INITIALIZED。
 * - CMD_STOP：注销收割任务（返回时在途回调已退出）。返回 0 表示 RTT 原本在运行、
 *   1 表示原本未运行（SEGGER 语义）；负数为错误。
 * - CMD_GETNUMBUF / CMD_GETDESC：未 START 时返回 HFLINK_ERR_NOT_INITIALIZED（-2，
 *   对应 SEGGER "RTT Control Block not found yet" 的 -2）。
 *
 * @param cmd HFLINK_RTT_CMD_* 命令
 * @param p   命令参数结构指针（见各命令说明；START/STOP 可为 NULL）
 * @return >= 0 成功（CMD_STOP 的 0/1 区分原状态）；< 0 错误码
 */
HFLINK_API int HFLink_RTT_Control(HFLink_RTTHandle *rtt, uint32_t cmd, void *p);

/**
 * @brief 从主机侧缓冲读取 up 通道数据（对照 SEGGER JLINK_RTTERMINAL_Read）
 *
 * 纯内存拷贝：数据由后台收割任务从 target 搬入主机缓冲，本调用不访问调试总线、
 * 永不阻塞。读走即失（与 SEGGER 一致）。
 *
 * @param buffer_index up 通道索引（从 0 起）
 * @param s_buffer     输出缓冲
 * @param buffer_size  输出缓冲容量
 * @return >= 0 实际读取字节数（无新数据为 0）；< 0 错误码
 */
HFLINK_API int HFLink_RTT_Read(HFLink_RTTHandle *rtt, uint32_t buffer_index, uint8_t *s_buffer, uint32_t buffer_size);

/**
 * @brief 向 down 通道写入数据（host → target，对照 SEGGER JLINK_RTTERMINAL_Write）
 *
 * 同步写入 target 侧环形缓冲；空间不足时写满即停，返回实际写入字节数
 * （与 SEGGER 部分写语义一致）。M0/M0+ 降级模式下 target 运行中写入返回 0。
 *
 * @param buffer_index down 通道索引（从 0 起）
 * @param s_buffer     待写数据
 * @param buffer_size  待写字节数
 * @return >= 0 实际写入字节数；< 0 错误码
 */
HFLINK_API int HFLink_RTT_Write(HFLink_RTTHandle *rtt, uint32_t buffer_index, const uint8_t *s_buffer,
                                uint32_t buffer_size);

/**
 * @brief 查询主机缓冲中可读的字节数（SEGGER 新版 JLINK_RTTERMINAL_GetNumBytesOnBuffer 语义）
 *
 * 纯内存查询，不触碰调试总线。水位仅含已收割到主机缓冲的数据；target 侧
 * 尚未收割的新数据不计入。
 *
 * @param buffer_index up 通道索引（从 0 起）
 * @param num_bytes    输出可读字节数
 * @return HFLINK_OK；HFLINK_ERR_NOT_INITIALIZED 未 START；HFLINK_ERR 通道无效
 */
HFLINK_API int HFLink_RTT_GetNumBytesOnBuffer(const HFLink_RTTHandle *rtt, uint32_t buffer_index,
                                              uint32_t *num_bytes);

/** @brief 创建 RTT 会话。target 为 NULL 时仅允许配置类操作（START 返回错误）。 */
HFLINK_API HFLink_RTTHandle *HFLink_RTT_Create(HFLink_CoreSightTarget *target);

/** @brief 销毁会话（自动注销收割任务并释放全部资源）。 */
HFLINK_API void HFLink_RTT_Destroy(HFLink_RTTHandle *rtt);

/**
 * @brief 设置自动扫描范围（方式一）
 * @note  未设置范围时若最终走扫描路径将返回错误；范围同时也是显式地址的结构校验边界
 */
HFLINK_API int HFLink_RTT_SetScanRange(HFLink_RTTHandle *rtt, uint64_t base, uint64_t size);

/** @brief 绑定 Lua 运行时（方式二：START 时读取 hf.rtt.set_control_block 配置）。 */
HFLINK_API int HFLink_RTT_SetLuaState(HFLink_RTTHandle *rtt, HFLink_LuaState *state);

/**
 * @brief 设置后台收割周期（毫秒，默认 2；范围 1..1000）
 * @note  RTT 运行中修改即时生效（内部经调度器注销 + 重注册实现）
 */
HFLINK_API int HFLink_RTT_SetPollInterval(HFLink_RTTHandle *rtt, uint32_t interval_ms);

/** @brief 查询已定位的控制块地址（未定位时返回 HFLINK_ERR_NOT_INITIALIZED）。 */
HFLINK_API int HFLink_RTT_GetControlBlockAddress(const HFLink_RTTHandle *rtt, uint64_t *address);

/** @brief 查询最近一次错误描述（无错误时输出空串）。 */
HFLINK_API int HFLink_RTT_GetLastError(const HFLink_RTTHandle *rtt, char *buffer, int length);

/**
 * @brief 读取 Lua 运行时中 hf.rtt.set_control_block 配置的地址（方式二）
 * @return HFLINK_OK 且 *address 有效；未配置返回 HFLINK_ERR
 */
HFLINK_API int HFLink_RTT_GetLuaConfiguredAddress(HFLink_LuaState *state, uint64_t *address);

#ifdef __cplusplus
}
#endif
