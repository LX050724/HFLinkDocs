/**
 * @file HFLinkDriver_flash.h
 * @brief Flash 下载 API：镜像构建、擦除/写/校验计划生成、FLM 算法探测与同步/Turbo 执行。
 */
#pragma once

#include "HFLinkDriver_import.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明: HFLink_CoreSightContext 在 HFLinkDriver_debug_access.h 是不透明结构,
 * 但 flash.h 作为公开 header 不在 driver/internal 路径, 故仅以不透明指针使用,
 * 由实现 .c 负责 include debug_access.h 拿到完整声明. */
typedef struct HFLink_CoreSightContext HFLink_CoreSightContext;

/** @brief FLM FlashDevice 扇区布局项。 */
typedef struct
{
    uint32_t size;    /**< 扇区大小，单位为字节。 */
    uint32_t address; /**< 扇区相对 Flash 起始地址的偏移。 */
} HFLink_FlashSector;

/** @brief 物理 Flash 芯片及其 FLM 几何信息。 */
typedef struct HFLink_FlashDevice
{
    uint16_t version;            /**< FLM FlashDevice ABI 版本。 */
    char name[128];              /**< FLM 提供的设备名称。 */
    uint16_t device_type;        /**< FLM FlashDevice 设备类型。 */
    uint64_t start;              /**< Flash 起始地址。 */
    uint64_t size;               /**< Flash 总容量，单位为字节。 */
    uint32_t page_size;          /**< 最小编程页大小，单位为字节。 */
    uint32_t erased_value;       /**< 擦除后的字节值。 */
    uint32_t program_timeout_ms; /**< 单页编程超时时间，单位为毫秒。 */
    uint32_t erase_timeout_ms;   /**< 单扇区擦除超时时间，单位为毫秒。 */
    HFLink_FlashSector *sectors; /**< 扇区布局数组。 */
    size_t sector_count;         /**< 扇区布局项数量。 */
} HFLink_FlashDevice;

/** @brief Flash 执行核心描述的不透明句柄。 */
typedef struct HFLink_FlashCore HFLink_FlashCore;

/** @brief Flash 调试访问路径的不透明句柄。 */
typedef struct HFLink_FlashAccessPath HFLink_FlashAccessPath;

/** @brief FLM 下载算法的不透明句柄。 */
typedef struct HFLink_FlashAlgorithm HFLink_FlashAlgorithm;

/** @brief 统一稀疏固件镜像的不透明句柄。 */
typedef struct HFLink_FlashImage HFLink_FlashImage;

/** @brief 可独立擦写 Flash bank 的不透明句柄。 */
typedef struct HFLink_FlashBank HFLink_FlashBank;

/** @brief 一次下载操作计划的不透明句柄。 */
typedef struct HFLink_FlashPlan HFLink_FlashPlan;

/** @brief 一次下载执行会话的不透明句柄。 */
typedef struct HFLink_FlashSession HFLink_FlashSession;
typedef struct HFLink_PackDebugSession HFLink_PackDebugSession;

/** @brief 下载会话状态；失败后仅能经统一清理进入 CLOSED。 */
typedef enum
{
    HFLINK_FLASH_SESSION_CREATED = 0,
    HFLINK_FLASH_SESSION_PREPARED,
    HFLINK_FLASH_SESSION_RUNNING,
    HFLINK_FLASH_SESSION_CLEANING,
    HFLINK_FLASH_SESSION_CLOSED,
    HFLINK_FLASH_SESSION_FAILED,
} HFLink_FlashSessionState;

/** @brief 输入镜像格式。 */
typedef enum
{
    HFLINK_FLASH_IMAGE_AUTO = 0,
    HFLINK_FLASH_IMAGE_ELF,
    HFLINK_FLASH_IMAGE_BIN,
    HFLINK_FLASH_IMAGE_IHEX,
} HFLink_FlashImageFormat;

/** @brief 镜像加载时附加到每个 segment 的地址空间与处理器约束。 */
typedef struct
{
    uint64_t binary_base;       /**< BIN 加载基址。 */
    int has_binary_base;        /**< BIN 必须为 1；允许显式基址 0。 */
    const char *processor_name; /**< 可选 Pname；函数返回后可释放。 */
    uint64_t address_space;     /**< 可选 ADIv6 address space。 */
    int has_address_space;      /**< address_space 是否有效。 */
} HFLink_FlashImageOptions;

/** @brief 稀疏镜像 segment 借用视图。 */
typedef struct
{
    uint64_t address;
    uint64_t length;
    const uint8_t *data;        /**< 由 image 持有，Destroy 后失效。 */
    const char *processor_name; /**< 由 image 持有，可为空。 */
    const char *name;           /**< 来源 ELF section 名（.text 等），由 image 持有；非 ELF 镜像或无名段时为空。 */
    uint64_t address_space;
    int has_address_space;
} HFLink_FlashImageSegmentInfo;

/** @brief 创建空稀疏镜像。 */
HFLINK_API int HFLink_FlashImage_Create(HFLink_FlashImage **out_image);

/**
 * @brief 向镜像添加一个 segment，深拷贝数据与 Pname。
 * @note 同一 (Pname,address-space) 中任意重叠均返回 HFLINK_ERR_PLAN_CONFLICT。
 */
HFLINK_API int HFLink_FlashImage_AddSegment(HFLink_FlashImage *image, uint64_t address, const void *data, size_t size,
                                            const HFLink_FlashImageOptions *options);

/** @brief 从 ELF、BIN 或 Intel HEX 文件创建稀疏镜像。 */
HFLINK_API int HFLink_FlashImage_Open(const char *path, HFLink_FlashImageFormat format,
                                      const HFLink_FlashImageOptions *options, HFLink_FlashImage **out_image);

/** @brief 销毁镜像；可传 NULL。 */
HFLINK_API void HFLink_FlashImage_Destroy(HFLink_FlashImage *image);

/** @brief 返回 segment 数量。 */
HFLINK_API size_t HFLink_FlashImage_GetSegmentCount(const HFLink_FlashImage *image);

/** @brief 按确定性地址顺序返回 segment 借用视图。 */
HFLINK_API int HFLink_FlashImage_GetSegment(const HFLink_FlashImage *image, size_t index,
                                            HFLink_FlashImageSegmentInfo *out_segment);

/** @brief 擦除策略；整片擦除必须由调用方显式选择。 */
typedef enum
{
    HFLINK_FLASH_ERASE_SECTORS = 0,
    HFLINK_FLASH_ERASE_CHIP = 1,
} HFLink_FlashEraseMode;

/** @brief 阶段进度标识；每次执行最多依次回调擦除/编程/校验三个阶段，各自独立进度。 */
typedef enum
{
    HFLINK_FLASH_PROGRESS_PHASE_ERASE = 0,
    HFLINK_FLASH_PROGRESS_PHASE_PROGRAM,
    HFLINK_FLASH_PROGRESS_PHASE_VERIFY,
} HFLink_FlashProgressPhase;

/**
 * @brief 阶段进度回调。
 * @note 在调用执行 API 的同一线程同步调用；回调内不得再次调用本驱动的任何执行 API
 *       （防止重入设备锁）。GUI 侧应将信号桥接到 UI 线程。
 * @param phase       当前阶段。
 * @param done_bytes  本阶段已完成字节数。擦除阶段按扇区字节累计（每个 ERASE_SECTOR 操作
 *                    计入 operation.length，ERASE_CHIP 计入 bank 大小）；编程阶段在页数据
 *                    提交给执行器（Stage 成功）后累计，与目标端实际写页最多相差半页。
 * @param total_bytes 本阶段字节总数；为 0 表示本阶段无操作（该阶段不会回调）。
 * @param userdata    注册时传入的用户数据。
 */
typedef void (*HFLink_FlashProgressCallback)(HFLink_FlashProgressPhase phase, uint64_t done_bytes,
                                             uint64_t total_bytes, void *userdata);

/** @brief planner 的显式消歧与执行选项。 */
typedef struct
{
    const char *processor_name;
    uint32_t punit;
    int has_punit;
    size_t bank_index;
    int has_bank_index;
    const char *algorithm_path; /**< 可选外部 FLM 覆盖路径；指定 bank_index 时覆盖该 bank，未指定时必须唯一匹配候选 bank 的几何。 */
    uint64_t address_space;
    int has_address_space;
    HFLink_FlashEraseMode erase_mode;
    int skip_verify;
    /**
     * @brief 擦除后是否保存-恢复镜像未覆盖区域（显式开关，调用方必须设置）。
     *
     * 0 = 关闭（默认）：擦除后未覆盖区域保持擦除态（擦除=擦空）；下载流程也不保留旧内容。
     * 1 = 开启：擦除前回读保存未覆盖区域，擦除/编程后写回恢复（保留旧内容）。
     * 由 planner 在生成 PRESERVE_READ 操作时裁决，与执行阶段位掩码无关。
     */
    int restore_after_erase;
    /** @brief 可选：阶段进度回调；一站式 API（Erase/WriteImage/VerifyImage/ProgramEx）内部创建 Session 时注入。 */
    HFLink_FlashProgressCallback progress_callback;
    void *progress_userdata;
} HFLink_FlashPlanOptions;

/** @brief 离线 Pack planner 的用户连接选择；只用于约束拓扑，不接触探针。 */
typedef enum
{
    HFLINK_FLASH_TRANSPORT_SWD = 1,
    HFLINK_FLASH_TRANSPORT_JTAG = 2,
} HFLink_FlashTransport;

typedef struct
{
    HFLink_FlashTransport transport; /**< 必须显式为 SWD 或 JTAG。 */
    uint32_t clock_hz;               /**< 必须非零；仅校验/展示，不驱动硬件。 */
} HFLink_FlashOfflineOptions;

/** @brief 一个候选/已激活 Flash bank 的借用视图。 */
typedef struct
{
    size_t bank_index;
    uint32_t pack_algorithm_index;
    const char *algorithm_path;
    const char *algorithm_processor_name;
    const char *executor_processor_name;
    uint32_t executor_punit;
    const char *memory_processor_name;
    uint32_t memory_punit;
    uint64_t start;
    uint64_t device_start; /**< FLM FlashDevice 起始地址（扇区地址的基准）。 */
    uint64_t device_size;  /**< FLM FlashDevice 总大小（末扇区 region 的终点）。 */
    uint64_t size;
    uint64_t ram_start;
    uint64_t ram_size;
    uint64_t pack_dp_index;
    uint64_t access_selector;
    int use_apid;
    const char *flash_device_name;
    uint32_t page_size;
    uint32_t erased_value;
    size_t sector_region_count;
} HFLink_FlashBankInfo;

/** @brief dry-run 与生产状态机共享的操作类型。 */
typedef enum
{
    HFLINK_FLASH_PLAN_PRESERVE_READ = 0,
    HFLINK_FLASH_PLAN_ERASE_SECTOR,
    HFLINK_FLASH_PLAN_ERASE_CHIP,
    HFLINK_FLASH_PLAN_PROGRAM_PAGE,
    HFLINK_FLASH_PLAN_VERIFY,
} HFLink_FlashPlanOperationType;

/** @brief 计划操作借用视图。 */
typedef struct
{
    HFLink_FlashPlanOperationType type;
    size_t bank_index;
    size_t image_segment_index; /**< 不直接对应 segment 时为 SIZE_MAX。 */
    uint64_t address;
    uint64_t length;
    int requires_preserve;
} HFLink_FlashPlanOperationInfo;

/**
 * @brief 从 Pack algorithms 与镜像构建确定性计划；image 为空时仅探测候选 banks。
 * @return 重叠候选或多核共享算法未显式选择时返回 HFLINK_ERR_PLAN_CONFLICT。
 */
HFLINK_API int HFLink_FlashPlan_Create(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                       const HFLink_FlashPlanOptions *options, HFLink_FlashPlan **out_plan);

/** @brief 销毁计划及其解析的 FLM/bank 资源；可传 NULL。 */
HFLINK_API void HFLink_FlashPlan_Destroy(HFLink_FlashPlan *plan);

HFLINK_API size_t HFLink_FlashPlan_GetBankCount(const HFLink_FlashPlan *plan);
HFLINK_API int HFLink_FlashPlan_GetBank(const HFLink_FlashPlan *plan, size_t index, HFLink_FlashBankInfo *out_bank);
/**
 * @brief 返回指定 bank 的 FLM 扇区 region 表（借用视图，plan 销毁后失效）。
 * @param plan 已探测/创建的计划；bank_index 越界或 out_sectors/out_count 为 NULL 返回 HFLINK_ERR_NOT_INITIALIZED。
 * @note 扇区 address 为相对 bank.device_start 的偏移；region 表为"重复同尺寸扇区"的压缩表示，
 *       末 region 终点为 bank 地址末尾（参考驱动 flash_plan_collect_sectors 的裁剪语义）。
 */
HFLINK_API int HFLink_FlashPlan_GetBankSectors(const HFLink_FlashPlan *plan, size_t bank_index,
                                               const HFLink_FlashSector **out_sectors, size_t *out_count);
HFLINK_API size_t HFLink_FlashPlan_GetOperationCount(const HFLink_FlashPlan *plan);
HFLINK_API int HFLink_FlashPlan_GetOperation(const HFLink_FlashPlan *plan, size_t index,
                                             HFLink_FlashPlanOperationInfo *out_operation);

/** @brief 高层执行阶段位掩码。 */
typedef enum
{
    HFLINK_FLASH_EXECUTE_ERASE = 1U << 0,
    HFLINK_FLASH_EXECUTE_PROGRAM = 1U << 1,
    HFLINK_FLASH_EXECUTE_VERIFY = 1U << 2,
    HFLINK_FLASH_EXECUTE_ALL = HFLINK_FLASH_EXECUTE_ERASE | HFLINK_FLASH_EXECUTE_PROGRAM | HFLINK_FLASH_EXECUTE_VERIFY,
} HFLink_FlashExecutionPhase;

/** @brief 下载完成后的复位影响范围；数值与 HFLink_ResetScope 一致，但避免公共头循环依赖。 */
typedef enum
{
    HFLINK_FLASH_RESET_SCOPE_CORE = 0,
    HFLINK_FLASH_RESET_SCOPE_CHIP = 1,
    HFLINK_FLASH_RESET_SCOPE_PHYSICAL_CHAIN = 2,
} HFLink_FlashResetScope;

/**
 * @brief 完整下载成功后的可选动作。
 * @note 复位与运行在 Flash gate、FLM 和设备事务锁全部释放后执行；默认不执行任何后置动作。
 */
typedef struct
{
    /** @brief 可选执行核心 Pname；多执行核心计划需要显式消歧。 */
    const char *processor_name;
    uint32_t punit;
    int has_punit;
    int reset_after;
    HFLink_FlashResetScope reset_scope;
    int has_reset_scope;
    int run_after;
} HFLink_FlashProgramOptions;

/** @brief 创建绑定 Pack Session 与确定计划的执行会话；plan 必须覆盖 session 生命周期。 */
HFLINK_API int HFLink_FlashSession_Create(HFLink_PackDebugSession *pack_session, HFLink_FlashPlan *plan,
                                          HFLink_FlashSession **out_session);

/**
 * @brief 注册阶段进度回调；传 NULL 清除。
 * @note 回调在调用执行 API 的线程同步触发，所有触发点均位于设备事务锁释放之后；
 *       可在 ExecutePhases 前任意时刻调用（session 未并发执行，无需加锁）。
 */
HFLINK_API int HFLink_FlashSession_SetProgressCallback(HFLink_FlashSession *session,
                                                       HFLink_FlashProgressCallback callback, void *userdata);

/** @brief 执行指定阶段并始终完成 FLM/Core/Hook 清理。 */
HFLINK_API int HFLink_FlashSession_ExecutePhases(HFLink_FlashSession *session, uint32_t phases);

/** @brief 执行 erase/program/verify 完整链。 */
HFLINK_API int HFLink_FlashSession_Execute(HFLink_FlashSession *session);

/** @brief 销毁已关闭或尚未执行的 Session；可传 NULL。 */
HFLINK_API void HFLink_FlashSession_Destroy(HFLink_FlashSession *session);

HFLINK_API HFLink_FlashSessionState HFLink_FlashSession_GetState(const HFLink_FlashSession *session);
HFLINK_API int HFLink_FlashSession_GetPrimaryError(const HFLink_FlashSession *session);
HFLINK_API int HFLink_FlashSession_GetCleanupError(const HFLink_FlashSession *session);

/** @brief 不访问硬件地解析所有候选 bank。 */
HFLINK_API int HFLink_Flash_ProbeBanks(HFLink_PackDebugSession *pack_session, HFLink_FlashPlan **out_plan);

/** @brief 不访问硬件地生成结构化 dry-run。 */
HFLINK_API int HFLink_Flash_DryRun(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                   const HFLink_FlashPlanOptions *options, HFLink_FlashPlan **out_plan);

/** @brief 仅查询 Pack/FLM 并离线列出 bank；不会枚举、打开或访问物理探针。 */
HFLINK_API int HFLink_Flash_ProbePackBanks(const char *device_selector,
                                           const HFLink_FlashOfflineOptions *offline_options,
                                           HFLink_FlashPlan **out_plan);

/**
 * @brief 解析单个 CMSIS FLM 文件并返回其 FlashDevice 几何（页大小/擦除值/设备起止/扇区表）。
 *
 * 供 GUI 对每个下载算法独立取真实几何（不依赖 plan 的代表算法去重/兜底）。
 * @param flm_path   FLM 绝对路径（ELF）。
 * @param ram_start  目标 RAM 工作区起始（用于解析入口符号；可传 0）。
 * @param ram_size   目标 RAM 工作区大小（可传 0）。
 * @param out_device 成功时返回深拷贝的 FlashDevice（含扇区表）；调用方用 HFLink_FlashDevice_Free 释放。
 * @return HFLINK_OK 成功；否则负错误码，*out_device 置 NULL。
 */
HFLINK_API int HFLink_Flash_ProbeAlgorithm(const char *flm_path, uint64_t ram_start, uint64_t ram_size,
                                           HFLink_FlashDevice **out_device);

/** @brief 释放 HFLink_Flash_ProbeAlgorithm() 返回的 FlashDevice。 */
HFLINK_API void HFLink_FlashDevice_Free(HFLink_FlashDevice *device);

/** @brief 在任何硬件访问前，从 Pack、FLM 与镜像生成结构化计划。 */
HFLINK_API int HFLink_Flash_DryRunPack(const char *device_selector, const HFLink_FlashOfflineOptions *offline_options,
                                       const HFLink_FlashImage *image, const HFLink_FlashPlanOptions *options,
                                       HFLink_FlashPlan **out_plan);

/** @brief 通过统一 Plan/Session 服务执行指定阶段，并分别返回主错误与清理错误。 */
HFLINK_API int HFLink_Flash_ExecutePhases(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                          const HFLink_FlashPlanOptions *options, uint32_t phases, int *primary_error,
                                          int *cleanup_error);

/** @brief 按镜像范围执行 sector/chip erase，不执行编程和校验。 */
HFLINK_API int HFLink_Flash_Erase(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                  const HFLink_FlashPlanOptions *options, int *primary_error, int *cleanup_error);

/** @brief 仅执行镜像编程；不隐式擦除。 */
HFLINK_API int HFLink_Flash_WriteImage(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                       const HFLink_FlashPlanOptions *options, int *primary_error, int *cleanup_error);

/** @brief 执行 DebugCodeMemRemap 后按选定 memory access path 读回校验。 */
HFLINK_API int HFLink_Flash_VerifyImage(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                        const HFLink_FlashPlanOptions *options, int *primary_error, int *cleanup_error);

/** @brief 便捷一站式完整下载；内部仍使用同一 Plan/Session 服务层。 */
HFLINK_API int HFLink_Flash_Program(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                    const HFLink_FlashPlanOptions *options, int *primary_error, int *cleanup_error);

/**
 * @brief 完整下载并在事务完全清理后执行可选 default reset 与 resume。
 * @note post_options 为 NULL 时与 HFLink_Flash_Program() 完全等价；后置动作失败作为主错误返回。
 */
HFLINK_API int HFLink_Flash_ProgramEx(HFLink_PackDebugSession *pack_session, const HFLink_FlashImage *image,
                                      const HFLink_FlashPlanOptions *options,
                                      const HFLink_FlashProgramOptions *post_options, int *primary_error,
                                      int *cleanup_error);

/**
 * @brief 选择 Flash 执行模式: 0 = Classic 同步 runner, 1 = Turbo 加速外壳 runner（默认）。
 *
 * 写入 CoreSight 上下文预定义变量 __FlashMode; FlashSession 在创建
 * runner 前读取该变量选择唯一执行器。Turbo 模式不可用、RAM 不足或启动失败时
 * 直接返回错误，不自动回退到 Classic。
 *
 * @param context 已 examine 的 CoreSight 上下文 (与未来 FlashSession 绑定)
 * @param enable  非 0 启用 Turbo; 0 切回同步 RPC
 * @return HFLINK_OK 或 HFLINK_ERR; 上下文为 NULL 或值非法返回 HFLINK_ERR
 */
HFLINK_API int HFLink_Flash_SetTurboMode(HFLink_CoreSightContext *context, int enable);

/**
 * @brief 读取上下文当前 __FlashMode (0=Classic, 1=Turbo).
 * @param context CoreSight 上下文
 * @param out_enable 输出 0 / 1; 上下文为 NULL 仍返回 HFLINK_ERR
 * @return HFLINK_OK; HFLINK_ERR 参数非法
 */
HFLINK_API int HFLink_Flash_GetTurboMode(HFLink_CoreSightContext *context, int *out_enable);

/** @brief Flash 校验模式。Classic 始终忽略该值并执行逐字节回读比较。 */
typedef enum
{
    HFLINK_FLASH_VERIFY_READBACK = 0,
    HFLINK_FLASH_VERIFY_CRC = 1,
} HFLink_FlashVerifyMode;

/** @brief 设置 __FlashVerifyMode；默认 HFLINK_FLASH_VERIFY_CRC。 */
HFLINK_API int HFLink_Flash_SetVerifyMode(HFLink_CoreSightContext *context, HFLink_FlashVerifyMode mode);

/** @brief 读取当前 __FlashVerifyMode。 */
HFLINK_API int HFLink_Flash_GetVerifyMode(HFLink_CoreSightContext *context, HFLink_FlashVerifyMode *out_mode);

#ifdef __cplusplus
}
#endif
