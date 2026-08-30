/**
 * @file    HFLinkDriver_hss.h
 * @brief   HSS（High-Speed Sampling）高速内存采样 API
 *
 * 语义对照 SEGGER J-Link HSS（UM08002 Ch.6）：
 * - Start 重复调用换配置时自动停止旧采样、清空缓冲并重启
 * - 采样率"尽力满足"：原生探针由固件 DAP_Delay 微秒级节拍保证，第三方探针
 *   禁用 delay、退化为主机调度（Windows 粒度 ~15.6ms，尽力而为）
 * - Read 无需先 Stop；CPU 暂停期间宿主应 SetPaused(1)（对照 SEGGER halt 暂停语义）
 * - 采样期间不影响正常 CoreSight 调试访问（设备事务锁串行化 + 批后缓存失效）
 *
 * 与 SEGGER 的差异：
 * - 时间戳统一为 u64 纳秒（相对 Start 时刻），无 ms/us 单位 flag
 * - 读取返回结构化 SampleView 数组而非扁平字节流，配套按块解码 API
 * - 单轮采样（全部块）必须能编码进一个 CMSIS-DAP Transfer 包（512B），
 *   超限 Start 直接返回 HFLINK_ERR_HSS_TOO_MANY_BLOCKS，不做自动拆包
 */
#pragma once

#include "HFLinkDriver.h"
#include "HFLinkDriver_import.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 单次 Start 允许的最大采样块数。 */
#define HFLINK_HSS_MAX_BLOCKS 16
/** 单轮采样所有块的字节总和上限（单轮编码必须装入单个 Transfer 包）。 */
#define HFLINK_HSS_MAX_FRAME_BYTES 400

/** HSS 会话句柄（不透明）。 */
typedef struct HFLink_HSSHandle HFLink_HSSHandle;

/** 采样布局对象（不透明）：块描述的解码视图，可独立于会话使用。 */
typedef struct HFLink_HSS_Layout HFLink_HSS_Layout;

/** @brief 采样块描述（对照 JLINK_HSS_MEM_BLOCK_DESC；地址任意字节对齐）。 */
typedef struct
{
    uint32_t addr;      ///< 采样起始地址
    uint32_t num_bytes; ///< 每轮读取的字节数（>=1）
} HFLink_HSS_MemBlock;

/** @brief 单帧样本视图：data 指向调用方缓冲内该帧数据区，块按 Start 顺序紧凑拼接。 */
typedef struct
{
    uint64_t timestamp_ns; ///< 相对 Start 的纳秒时间戳
    uint8_t *data;         ///< 帧数据（块按序拼接，无填充）
    uint32_t data_size;    ///< 帧数据字节数 = Σ num_bytes
} HFLink_HSS_SampleView;

/** HSS 时序控制模式（HFLink_HSS_Stats::delay_mode）。 */
enum
{
    HFLINK_HSS_MODE_DELAY = 0,     ///< 原生探针 + 固件 DAP_Delay 周期节拍（帧距均匀逼近 period）
    HFLINK_HSS_MODE_FULL_SPEED = 1, ///< 原生探针 + 无 delay 全速：批内背靠背线速采样，流水线
                                    ///< 压满不节流，吞吐=调试接口带宽，帧真实时刻由 TS 标注
    HFLINK_HSS_MODE_HOST_PACED = 2, ///< 第三方探针：主机 deadline 调度 + 单周期同步批
};

/** @brief HSS 运行统计。 */
typedef struct
{
    uint64_t total_samples;   ///< 已写入环形缓冲的总帧数
    uint64_t dropped_samples; ///< 环形缓冲溢出时丢弃的旧帧数
    uint64_t dropped_batches; ///< 传输错误丢弃的整批帧数（不含溢出）
    uint64_t mean_period_ns;  ///< 最近一批实测平均采样周期（0 = 尚未实测）
    int paused;               ///< 是否处于手动暂停状态（SetPaused）
    int target_halted;        ///< 目标 halt/复位/算法执行中的自动暂停状态（CoreSight state 驱动）
    int ts_from_probe;        ///< 时间戳来源：1 = 探针固件 TD_TimeStamp，0 = 主机单调时钟
    int delay_mode;           ///< 时序控制模式（HFLINK_HSS_MODE_*；Start 时自动判定）
} HFLink_HSS_Stats;

/*===========================================================================*/
/* 布局与解码 API（传入请求格式后即可像数组一样访问采样数据）                  */
/*===========================================================================*/

/**
 * @brief 创建采样布局（块描述的解码视图）
 * @param blocks    块描述数组（内部拷贝）
 * @param num_blocks 块数量（1..HFLINK_HSS_MAX_BLOCKS）
 * @return 布局指针；参数非法或单轮编码超出单 Transfer 包限制时返回 NULL
 */
HFLINK_API HFLink_HSS_Layout *HFLink_HSS_LayoutCreate(const HFLink_HSS_MemBlock *blocks, uint32_t num_blocks);

/** @brief 销毁布局对象；可为 NULL。 */
HFLINK_API void HFLink_HSS_LayoutDestroy(HFLink_HSS_Layout *layout);

/**
 * @brief 取样本中指定块的原始数据指针
 * @param layout      布局（定义块布局）
 * @param sample      样本视图（data 必须按该布局的帧格式组织）
 * @param block_index 块索引（0 起）
 * @param num_bytes   可为 NULL；输出该块字节数
 * @return 块数据指针（指向 sample->data 内部）；索引越界返回 NULL
 */
HFLINK_API const uint8_t *HFLink_HSS_LayoutGetBlock(const HFLink_HSS_Layout *layout,
                                                    const HFLink_HSS_SampleView *sample, uint32_t block_index,
                                                    uint32_t *num_bytes);

/** @brief 取指定块的 32-bit 小端值（块字节数不足 4 时高位补零）。 */
HFLINK_API uint32_t HFLink_HSS_LayoutGetU32(const HFLink_HSS_Layout *layout, const HFLink_HSS_SampleView *sample,
                                            uint32_t block_index);

/** @brief 取指定块的 64-bit 小端值（块字节数不足 8 时高位补零）。 */
HFLINK_API uint64_t HFLink_HSS_LayoutGetU64(const HFLink_HSS_Layout *layout, const HFLink_HSS_SampleView *sample,
                                            uint32_t block_index);

/*===========================================================================*/
/* 会话 API                                                                   */
/*===========================================================================*/

/** @brief 创建 HSS 会话。target 为 NULL 时 Start 返回 HFLINK_ERR_NOT_INITIALIZED。 */
HFLINK_API HFLink_HSSHandle *HFLink_HSS_Create(HFLink_CoreSightTarget *target);

/** @brief 销毁会话（自动停止采样线程并释放全部资源；环形缓冲数据一并丢弃）。 */
HFLINK_API void HFLink_HSS_Destroy(HFLink_HSSHandle *hss);

/**
 * @brief 启动周期采样（已在运行时：停止旧采样、清空缓冲、应用新配置并重启）
 * @param hss        会话
 * @param blocks     采样块描述数组
 * @param num_blocks 块数量（1..HFLINK_HSS_MAX_BLOCKS）
 * @param period_us  采样周期（微秒，>=1；尽力满足）
 * @param flags      保留，必须为 0
 * @return HFLINK_OK；HFLINK_ERR_HSS_INVALID_PARAM 参数非法；
 *         HFLINK_ERR_HSS_TOO_MANY_BLOCKS 单轮编码超出单 Transfer 包限制；
 *         HFLINK_ERR_HSS_UNSUPPORTED 探针/目标能力不足（如 APv2 分层路径）
 */
HFLINK_API int HFLink_HSS_Start(HFLink_HSSHandle *hss, const HFLink_HSS_MemBlock *blocks, uint32_t num_blocks,
                                uint32_t period_us, uint32_t flags);

/** @brief 停止采样（幂等；停止后环形缓冲数据保留，仍可 ReadSamples）。 */
HFLINK_API int HFLink_HSS_Stop(HFLink_HSSHandle *hss);

/**
 * @brief 读取完整样本帧（无需先 Stop）
 * @param hss         会话
 * @param buffer      输出缓冲（帧数据拷贝目标）
 * @param buffer_size 缓冲容量
 * @param views       样本视图数组（data 指向 buffer 内各帧数据区）
 * @param max_views   views 容量
 * @return >=0 读取的样本数（仅完整帧）；负数为 HFLINK_ERR_* 错误
 * @note 每帧消耗 buffer 中 data_size 字节；受 max_views 与 buffer_size 双重限制
 */
HFLINK_API int HFLink_HSS_ReadSamples(HFLink_HSSHandle *hss, uint8_t *buffer, uint32_t buffer_size,
                                      HFLink_HSS_SampleView *views, uint32_t max_views);

/** @brief 会话布局版本的取块数据（等价 LayoutGetBlock(hss 的布局)）。 */
HFLINK_API const uint8_t *HFLink_HSS_GetBlock(const HFLink_HSSHandle *hss, const HFLink_HSS_SampleView *sample,
                                              uint32_t block_index, uint32_t *num_bytes);

/**
 * @brief 查询当前连接的 HSS 能力
 * @param target      目标（用于读取 SWD 时钟；可为 NULL，按保守值估算）
 * @param max_blocks  可为 NULL；输出最大块数
 * @param max_freq_hz 可为 NULL；输出估算的最大采样频率（尽力语义的参考值）
 */
HFLINK_API int HFLink_HSS_GetCaps(const HFLink_CoreSightTarget *target, uint32_t *max_blocks, uint32_t *max_freq_hz);

/** @brief 查询运行统计。 */
HFLINK_API int HFLink_HSS_GetStats(HFLink_HSSHandle *hss, HFLink_HSS_Stats *stats);

/**
 * @brief 暂停/恢复采样（目标 halt 时宿主应暂停；对应 SEGGER CPU halt 自动暂停语义）
 * @note 连续传输错误触发的自动暂停同样通过本接口恢复
 */
HFLINK_API int HFLink_HSS_SetPaused(HFLink_HSSHandle *hss, int paused);

#ifdef __cplusplus
}
#endif
