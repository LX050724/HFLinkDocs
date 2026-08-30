/**
 * @file HFLinkDriver.h
 * @brief HFLinkDriver 伞头文件：设备初始化与枚举、探针打开/关闭、固件升级、配置项、传感器与 SWO 等设备基础 API。
 *
 * 其余功能模块的公开头文件（flash/pack/rtt/hss/semihosting 等）相互独立，按需包含。
 */
#pragma once

#include "HFLinkDriver_flash.h"
#include "HFLinkDriver_import.h"
#include "HFLinkDriver_pack.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 探针物理连接类型。 */
typedef enum
{
    HFLINK_INFERFACE_USB, ///< USB 连接
    HFLINK_INFERFACE_IP,  ///< 网络连接
} HFLink_Interface;

/** @brief 探针设备信息（HFLink_GetDeviceInfo 填充）。 */
typedef struct
{
    int speed;                  ///< 调试时钟（Hz）
    HFLink_Interface interface; ///< 连接类型
    char serial_number[33];     ///< 序列号（32 字符 + 终止符）
    char device_name[129];      ///< 产品名（128 字符 + 终止符）
    /** 非零表示第三方 DAP（如 CherryUSB CMSIS-DAP）：禁用 configure/升级/传感器/SWO，速度上限 10MHz，FIFO 深度 1 */
    int is_thirdparty;
} HFLink_DeviceInfo;

/** @brief 探针固件升级状态机控制块（调用方分配，见 HFLink_Upgrade* 系列）。 */
typedef struct
{
    uint8_t *buff;     ///< 固件数据；UpgradeFile 路径下为堆缓冲，由 HFLink_UpgradeFree 释放
    int len;           ///< 固件总长度（字节）
    int wlen;          ///< 已发送长度（字节）
    uint32_t checksum; ///< 固件 CRC32 校验和
    enum
    {
        HFLINK_UPG_FAULT,  ///< 故障终态
        HFLINK_UPG_IDLE,   ///< 空闲（已准备好，待首次 Tick）
        HFLINK_UPG_ERASE,  ///< 擦除/写入进行中
        HFLINK_UPG_FLASH,  ///< 写入进行中
        HFLINK_UPG_VERIFY, ///< 校验进行中（CRC32 回读比对）
        HFLINK_UPG_SUCCESS,///< 成功终态
        HFLINK_UPG_TIMEOUT,///< 超时终态
    } status;          ///< 状态机当前状态
    int delay_cnt;     ///< 内部轮询计数（VERIFY 阶段）
    int timeout_cnt;   ///< 内部超时/重试计数
} HFLink_Upgrade;

/** @brief 调试时钟频率映射表（每通道使能位图，按 HFLINK_CONFIG_FREQ_MAP 配置项读写）。 */
typedef struct
{
    uint32_t freq_10MHz_map;
    uint32_t freq_5MHz_map;
    uint32_t freq_2MHz_map;
    uint32_t freq_1MHz_map;
    uint32_t freq_500Khz_map;
    uint32_t freq_200KHz_map;
    uint32_t freq_100KHz_map;
    uint32_t freq_50KHz_map;
    uint32_t freq_20KHz_map;
    uint32_t freq_10KHz_map;
    uint32_t freq_5KHz_map;
} HFLink_Config_FreqMap;

/** @brief 输入输出信号延迟配置（按 HFLINK_CONFIG_IODELAY 配置项读写，单位为内部时钟节拍）。 */
typedef struct
{
    uint8_t TCK_DELAY;   ///< JTAG TCK 延迟
    uint8_t TMS_T_DELAY; ///< TMS 转向延迟
    uint8_t TMS_O_DELAY; ///< TMS 输出延迟
    uint8_t TMS_I_DELAY; ///< TMS 输入延迟
    uint8_t TDO_DELAY;   ///< TDO 延迟
    uint8_t TDI_DELAY;   ///< TDI 延迟
} HFLink_Config_IODELAY;

/** @brief LED 显示模式。 */
typedef enum
{
    HFLINK_LED_MODE_DEFAULT = 0,
    HFLINK_LED_MODE_CMSISDAP = 1,
} HFLink_LedMode;

typedef enum
{
    /// 5V电源控制，参数为字符0或1
    HFLINK_CONFIG_5V_SUPPLY = 0,

    /// 独立串口接口设置，参数为字符0或1
    HFLINK_CONFIG_INDEPENDENT_UART = 1,

    /// 频率映射表使能设置，参数为字符0或1
    HFLINK_CONFIG_FREQ_MAP_EN = 2,

    /// 频率映射表 参数{@see HFLink_Config_FreqMap}
    HFLINK_CONFIG_FREQ_MAP = 3,

    /// 输入输出延迟控制 参数{@see HFLink_Config_IODELAY}
    HFLINK_CONFIG_IODELAY = 4,

    /// LED模式 参数{@see HFLink_LedMode}
    HFLINK_CONFIG_LEDMODE = 5,

    /// 别名 参数 char[32]
    HFLINK_CONFIG_NICKNAME = 6,

    /// 构建日期 参数 (char *buf, int size)
    HFLINK_CONFIG_BUILD_DATE = 0xFE,
    HFLINK_CONFIG_FW_VERSION = 0xFF,
} HFLink_Config;

/** @brief 传感器编号（当前仅基础组：电压/电流类）。 */
typedef enum
{
    HFLINK_SENSOR_BASE = 0,
} HFLink_Sensor;

/** @brief 升级进度回调（percent 百分比，status 为 HFLINK_UPG_* 状态值）。 */
typedef void (*HFLink_upgrade_cb)(int percent, int status);
/** @brief 设备对象句柄（HFLink_Open 创建，HFLink_Close 释放）。 */
typedef struct HFLink_Device *HFLink_Handle;

typedef struct coresight_dp HFLink_CoreSightDP;
typedef struct coresight_target HFLink_CoreSightTarget;
typedef struct HFLink_DebugDeviceSession HFLink_DebugDeviceSession;
typedef struct HFLink_CoreSession HFLink_CoreSession;
typedef struct HFLink_CoreSightContext HFLink_CoreSightContext;

typedef enum HFLink_DebugPortRouteType
{
    HFLINK_DP_ROUTE_SINGLE = 0,
    HFLINK_DP_ROUTE_JTAG_TAP,
    HFLINK_DP_ROUTE_SWD_MULTIDROP
} HFLink_DebugPortRouteType;

typedef enum HFLink_DebugPortPhase
{
    HFLINK_DP_PHASE_CONFIGURED = 0,
    HFLINK_DP_PHASE_TRANSPORT_READY,
    HFLINK_DP_PHASE_SETUP,
    HFLINK_DP_PHASE_STARTED,
    HFLINK_DP_PHASE_CORE_READY,
    HFLINK_DP_PHASE_FAILED,
} HFLink_DebugPortPhase;

typedef struct HFLink_DebugPortRouteInfo
{
    uint64_t pack_dp_index;
    HFLink_DebugPortRouteType type;
    uint32_t tap_index;
    uint32_t idcode;
    uint32_t targetsel;
    uint32_t clock_hz;
    uint8_t dap_index;
    uint8_t ir_length;
    int swd_mode;
    int swj_enabled;
    int dormant_enabled;
    int has_clock;
    HFLink_DebugPortPhase phase;
} HFLink_DebugPortRouteInfo;

/**
 * @brief 设备内精确 address-space 键。
 *
 * use_apid 非零时 selector 是 APID（0 是有效值），registry
 * 由
 * `(DP,
 * APID)` 唯一解析
 * 完整 parent AP 链；传统模式下 selector 是实际 APSEL。该键只描述 Pack
 * 已声明的

 * * 访问空间，不引入新的 PDSC/Database 字段。
 */
typedef struct HFLink_DebugAddressSpaceKey
{
    uint64_t pack_dp_index;
    uint64_t selector;
    int use_apid;
} HFLink_DebugAddressSpaceKey;

/** @brief 设备级 Pack access-path registry 中的规范化访问路径。 */
typedef struct HFLink_DebugAccessPathInfo
{
    /** @brief Pack debug/accessport 所属 __dp。 */
    uint64_t pack_dp_index;
    /** @brief 实际 ADIv5 AP index 或 ADIv6 顶层 AP address。 */
    uint64_t actual_ap;
    /** @brief accessportV1/V2 的 __apid；use_apid 为零时固定为 0。 */
    uint64_t apid;
    /** @brief accessportV2 parent APID；has_parent_apid 为零时忽略。 */
    uint64_t parent_apid;
    uint64_t hprot;
    uint64_t sprot;
    int use_apid;
    int is_v2;
    int has_parent_apid;
    int has_hprot;
    int has_sprot;
    /** @brief 与本路径完全对应的显式 address-space 选择键。 */
    HFLink_DebugAddressSpaceKey address_space;
} HFLink_DebugAccessPathInfo;
/** @brief Sequence 函数体实现；C engine 负责调用边界，Lua/默认实现只负责函数体。 */
typedef int (*HFLink_DebugSequenceBody)(HFLink_CoreSightContext *context, const char *name, void *userdata);
typedef HFLink_DebugSequenceBody (*HFLink_DebugSequenceResolver)(HFLink_CoreSightContext *context, const char *name,
                                                                 void *userdata, void **body_userdata);

/** @brief Sequence 前置逻辑已完成，请继续执行该 Sequence 的默认 EMPTY/HOST/FLM 实现。 */
#define HFLINK_DEBUG_SEQUENCE_FALLTHROUGH 1

/** @brief 预定义 Debug Access Sequence 所属生命周期阶段。 */
typedef enum
{
    HFLINK_DEBUG_SEQUENCE_STAGE_CONNECT,
    HFLINK_DEBUG_SEQUENCE_STAGE_FLASH,
    HFLINK_DEBUG_SEQUENCE_STAGE_VERIFY,
    HFLINK_DEBUG_SEQUENCE_STAGE_RESET,
    HFLINK_DEBUG_SEQUENCE_STAGE_DISCONNECT
} HFLink_DebugSequenceStage;

/** @brief Pack 未覆盖预定义 Sequence 时 C engine 采用的策略。 */
typedef enum
{
    /** 未提供实现时视为成功，用于规范允许省略的通知型 Sequence。 */
    HFLINK_DEBUG_SEQUENCE_DEFAULT_EMPTY,
    /** 必须由 C 宿主生命周期实现，不能以空调用跳过。 */
    HFLINK_DEBUG_SEQUENCE_DEFAULT_HOST,
    /** 未提供 Pack 实现时回退到 FLM erase/program 操作。 */
    HFLINK_DEBUG_SEQUENCE_DEFAULT_FLM
} HFLink_DebugSequenceDefault;

/** @brief 预定义 Sequence 的只读元数据。 */
typedef struct
{
    const char *name;
    HFLink_DebugSequenceStage stage;
    HFLink_DebugSequenceDefault default_behavior;
} HFLink_DebugSequenceDefinition;

/** @brief 返回预定义 Sequence 数量。 */
HFLINK_API uint32_t HFLink_DebugSequence_GetDefinitionCount(void);

/** @brief 按稳定索引返回预定义 Sequence；索引越界返回 NULL。 */
HFLINK_API const HFLink_DebugSequenceDefinition *HFLink_DebugSequence_GetDefinition(uint32_t index);

/** @brief 按区分大小写的规范名称查找预定义 Sequence；未知名称返回 NULL。 */
HFLINK_API const HFLink_DebugSequenceDefinition *HFLink_DebugSequence_FindDefinition(const char *name);
typedef void (*HFLink_DebugDeviceSessionResourceDestroy)(void *resource);

typedef enum
{
    HFLINK_CORESIGHT_REG_CLASS_ALL = 0,
    HFLINK_CORESIGHT_REG_CLASS_CORE = 1,
} HFLink_CoreSightRegClass;

/** @brief target 运行状态（数值与内部 coresight_target_state 枚举一致，GDB Server 停止事件判定用）。 */
typedef enum
{
    HFLINK_CORESIGHT_TARGET_UNKNOWN = 0,
    HFLINK_CORESIGHT_TARGET_RUNNING = 1,
    HFLINK_CORESIGHT_TARGET_HALTED = 2,
    HFLINK_CORESIGHT_TARGET_RESET = 3,
    HFLINK_CORESIGHT_TARGET_DEBUG_RUNNING = 4,
    HFLINK_CORESIGHT_TARGET_UNAVAILABLE = 5,
} HFLink_CoreSightTargetState;

/** @brief halt 原因（数值与内部 coresight_target_debug_reason 枚举一致，GDB 停止信号映射用）。 */
typedef enum
{
    HFLINK_CORESIGHT_DBG_REASON_UNKNOWN = 0,
    HFLINK_CORESIGHT_DBG_REASON_DBGRQ = 1,
    HFLINK_CORESIGHT_DBG_REASON_BREAKPOINT = 2,
    HFLINK_CORESIGHT_DBG_REASON_WATCHPOINT = 3,
    HFLINK_CORESIGHT_DBG_REASON_SINGLESTEP = 4,
    HFLINK_CORESIGHT_DBG_REASON_NOTHALTED = 5,
    HFLINK_CORESIGHT_DBG_REASON_UNDEFINED = 6,
    HFLINK_CORESIGHT_DBG_REASON_EXCEPTION = 7,
    HFLINK_CORESIGHT_DBG_REASON_SWBREAK = 8,
} HFLink_CoreSightDebugReason;

typedef enum
{
    HFLINK_CORESIGHT_WP_READ = 0,
    HFLINK_CORESIGHT_WP_WRITE = 1,
    HFLINK_CORESIGHT_WP_ACCESS = 2,
} HFLink_CoreSightWatchpointMode;

typedef enum
{
    HFLINK_CORESIGHT_BP_HW = 0,
    HFLINK_CORESIGHT_BP_SW = 1,
} HFLink_CoreSightBreakpointType;
typedef struct
{
    uint64_t address;
    uint32_t elem_size;
    uint32_t count;
    uint8_t *buffer;
    uint32_t operation;
    uint32_t delay_us;
} HFLink_CoreSightMemorySegment;

/** SWO 数据回调类型，在内部线程中调用，需保证线程安全 */
typedef void (*HFLink_SWO_Callback)(const uint8_t *data, uint32_t len, void *userdata);

/** @brief 获取驱动 DLL 版本号，格式 0xMMmmpp（主.次.补丁，如 0x00010100 为 1.1.0）。 */
HFLINK_API uint32_t HFLink_GetDllVersion(void);

/**
 * @brief 初始化驱动：建立 libusb 上下文，可选启动后台 USB 事件线程。
 * @param use_async 非 0 启用 libusb 事件后台线程；0 时事件在调用线程内处理。
 * @return HFLINK_OK 成功；HFLINK_ERR 初始化失败。
 * @note 必须在枚举/打开设备之前调用；Windows 下会将系统定时器精度提升到 1ms，由 HFLink_Cleanup() 还原。
 */
HFLINK_API int HFLink_Initialize(int use_async);

/** @brief 清理驱动：停止事件线程并释放 libusb 上下文；未初始化时安全返回 HFLINK_OK。 */
HFLINK_API int HFLink_Cleanup(void);

/**
 * @brief 枚举当前接入的探针（HFLink 原生 + 白名单第三方 CMSIS-DAP）。
 * @param[out] info    调用方分配的设备信息数组。
 * @param      max_num 数组容量，超出部分被截断。
 * @return 实际枚举到的设备数量（0 表示无探针）；HFLINK_ERR_NOT_INITIALIZED 未初始化；负数为错误。
 */
HFLINK_API int HFLink_GetDeviceInfo(HFLink_DeviceInfo *info, uint32_t max_num);

/**
 * @brief 打开枚举到的探针并创建设备对象。
 * @param       info   目标设备信息（来自 HFLink_GetDeviceInfo）。
 * @param[out]  handle 接收设备句柄。
 * @return HFLINK_OK 成功；负数为错误。第三方探针 is_thirdparty 非 0，部分能力不可用。
 */
HFLINK_API int HFLink_Open(HFLink_DeviceInfo *info, HFLink_Handle *handle);

/** @brief 关闭设备并释放设备对象；句柄此后不可再使用。 */
HFLINK_API int HFLink_Close(HFLink_Handle handle);

/** @brief 读取探针序列号；返回设备内部缓冲借用指针（设备生存期有效，无需释放），失败返回 NULL。 */
HFLINK_API char *HFLink_GetSerialNumber(HFLink_Handle handle);

/** @brief 读取探针型号名；返回借用指针，语义同 HFLink_GetSerialNumber()。 */
HFLINK_API char *HFLink_GetModelName(HFLink_Handle handle);

/**
 * @brief 用内存位流准备探针固件升级状态机。
 * @param upg_ctl 升级控制块（调用方分配并保持生存期）；data 被借用至升级结束，不拷贝。
 * @return HFLINK_OK 成功；HFLINK_ERR_UNSUPPORTED 第三方 DAP 不支持；参数非法返回 -1。
 * @note 之后需周期调用 HFLink_UpgradeTick() 驱动状态机，直至 status 到达终态。
 */
HFLINK_API int HFLink_UpgradeBitstream(HFLink_Handle handle, HFLink_Upgrade *upg_ctl, uint8_t *data, uint32_t len);

/**
 * @brief 用固件文件准备升级状态机；文件读入堆缓冲，所有权转移给 upg_ctl。
 * @return 同 HFLink_UpgradeBitstream()；文件读取失败返回负数且 status 置 HFLINK_UPG_FAULT。
 */
HFLINK_API int HFLink_UpgradeFile(HFLink_Handle handle, HFLink_Upgrade *upg_ctl, const char *path);

/**
 * @brief 升级状态机单步驱动；调用方以固定周期（约 1~10ms）调用直至 status 到达终态。
 * @return 0 本步执行成功（含仍在进行中）；-1 失败（status 已置 FAULT 或 TIMEOUT）。
 * @note 状态流转 IDLE →（复位+启动）→ ERASE/FLASH（按 256 字节页）→ VERIFY（CRC32 回读比对）→ SUCCESS。
 */
HFLINK_API int HFLink_UpgradeTick(HFLink_Handle handle, HFLink_Upgrade *upg_ctl);

/** @brief 结束升级并释放升级缓冲（含 UpgradeFile 的堆缓冲），控制块清零；始终返回 0。 */
HFLINK_API int HFLink_UpgradeFree(HFLink_Handle handle, HFLink_Upgrade *upg_ctl);

/**
 * @brief 设置探针配置项，附加参数类型随 conf 而定：
 *        - `HFLINK_CONFIG_5V_SUPPLY` / `INDEPENDENT_UART` / `FREQ_MAP_EN`：`char`（'0' 或 '1'）
 *        - `HFLINK_CONFIG_FREQ_MAP`：`HFLink_Config_FreqMap *`
 *        - `HFLINK_CONFIG_IODELAY`：`HFLink_Config_IODELAY *`
 *        - `HFLINK_CONFIG_LEDMODE`：`HFLink_LedMode`
 *        - `HFLINK_CONFIG_NICKNAME`：`char *`（UTF-8 别名，超长按 31 字符截断）
 * @return HFLINK_OK 成功；HFLINK_ERR_UNSUPPORTED 第三方 DAP 不支持。
 */
HFLINK_API int HFLink_Configure_SetItem(HFLink_Handle handle, HFLink_Config conf, ...);

/**
 * @brief 读取探针配置项，附加参数为对应类型的传出指针：
 *        - 布尔三项（见 HFLink_Configure_SetItem()）：`char *`（接收 '0'/'1'）
 *        - `FREQ_MAP` / `IODELAY`：对应结构体指针
 *        - `LEDMODE`：`HFLink_LedMode *`
 *        - `NICKNAME`：`char *`（容量至少 32 字节）
 *        - `BUILD_DATE` / `FW_VERSION`：`(char *buf, int size)`，接收构建日期/固件版本字符串
 * @return HFLINK_OK 成功；负数为错误。
 */
HFLINK_API int HFLink_Configure_GetItem(HFLink_Handle handle, HFLink_Config conf, ...);

/** @brief 将已写入的配置保存到探针持久存储（掉电保留）。 */
HFLINK_API int HFLink_Configure_Save(HFLink_Handle handle);

/**
 * @brief 读取传感器数据（当前 HFLINK_SENSOR_BASE：电压/电流类）。
 * @param[out] data 接收数据缓冲（长度按传感器约定，建议 ≥ 64 字节）。
 * @return HFLINK_OK 成功；HFLINK_ERR_UNSUPPORTED 第三方 DAP 不支持。
 */
HFLINK_API int HFLink_Sensor_Read(HFLink_Handle handle, HFLink_Sensor sensor, uint8_t *data, uint32_t len);

/**
 * @brief 启动 SWO 异步流式读取。
 *
 * 使用多缓冲 libusb 异步 transfer 实现零间隙连续接收；回调在内部事件线程中调用，需保证线程安全。
 *
 * @param callback    SWO 数据回调，每收到一包数据即调用
 * @param userdata    回调用户数据
 * @param buffer_size 每个 transfer 的缓冲区大小（字节），0 则使用默认 4096
 * @return HFLINK_OK 成功，HFLINK_ERR_BUSY 已启动，HFLINK_ERR 参数无效
 */
HFLINK_API int HFLink_SWO_Start(HFLink_Handle handle, HFLink_SWO_Callback callback, void *userdata,
                                uint32_t buffer_size);

/** @brief 停止 SWO 异步流式读取：取消所有在途 transfer 并释放资源。 */
HFLINK_API int HFLink_SWO_Stop(HFLink_Handle handle);

/**
 * @brief 创建 CoreSight DP 上下文并绑定默认 HFLink DAP 后端。
 * @note 调用前必须已打开设备并启动 DAP 线程。上下文拥有内部 HAL 和 DP，
 *       销毁其创建的所有 Target 后再销毁上下文。
 * @return 成功返回上下文句柄，失败返回 NULL。
 */
HFLINK_API HFLink_CoreSightContext *HFLink_CoreSight_ContextCreate(void);

/** @brief 创建一个物理调试设备会话；同一双核设备的核心上下文共享该对象及其 DP。 */
HFLINK_API HFLink_DebugDeviceSession *HFLink_DebugDeviceSession_Create(void);

/** @brief 释放设备会话的调用方引用；最后一个核心上下文销毁后才释放底层 HAL/DP。 */
HFLINK_API void HFLink_DebugDeviceSession_Destroy(HFLink_DebugDeviceSession *session);

/** @brief 从同一设备会话创建一个独立核心上下文。 */
HFLINK_API HFLink_CoreSightContext *HFLink_DebugDeviceSession_CreateCoreContext(HFLink_DebugDeviceSession *session);

/**
 * @brief 创建设备内具名核心单元会话并注册到 (Pname, Punit) registry。
 * @param session
 * 所属物理设备会话。

 * *
 * @param pname CMSIS-Pack processor Pname，必须非空。
 * @param punit processor 实例编号；同一 Pname
 * 下必须唯一。

 * *
 * @param default_ap 传统 AP 路由的默认 APSEL 或 ADIv6 顶层 AP 地址。
 * @param apid accessportV1/V2
 * 标识；use_apid
 * 非零时有效。
 * @param use_apid 非零表示使用 APID 路由，此时预定义变量 __ap 保持为零。
 *
 * @return 成功返回 CoreSession；参数非法、(Pname, Punit) 重复或内存不足时返回 NULL。
 */
HFLINK_API HFLink_CoreSession *HFLink_DebugDeviceSession_CreateCoreSessionForUnit(HFLink_DebugDeviceSession *session,
                                                                                  const char *pname, uint32_t punit,
                                                                                  uint64_t default_ap, uint64_t apid,
                                                                                  int use_apid);

/**
 * @brief 创建设备内 Punit 0 核心会话的兼容入口。
 * @param session 所属物理设备会话。
 * @param
 * pname CMSIS-Pack
 * processor Pname，必须非空；等价于显式传入 Punit 0。
 * @param default_ap 传统 AP 路由的默认 APSEL 或 ADIv6
 * 顶层 AP 地址。
 * @param apid accessportV1/V2 标识；use_apid 非零时有效。
 * @param use_apid 非零表示使用 APID 路由，此时预定义变量 __ap 保持为零。
 * @return 成功返回 CoreSession；参数非法、Punit 0 已存在或内存不足时返回 NULL。
 * @note
 * CoreSession 拥有 CoreSightContext 和其创建的 target，并通过 context 持有设备会话引用。
 */
HFLINK_API HFLink_CoreSession *HFLink_DebugDeviceSession_CreateCoreSession(HFLink_DebugDeviceSession *session,
                                                                           const char *pname, uint64_t default_ap,
                                                                           uint64_t apid, int use_apid);

/** @brief 按 (Pname, Punit) 精确查找 CoreSession；返回借用指针，不增加引用计数。 */
HFLINK_API HFLink_CoreSession *HFLink_DebugDeviceSession_FindCoreSessionForUnit(HFLink_DebugDeviceSession *session,
                                                                                const char *pname, uint32_t punit);

/**
 * @brief 按 Pname 查找设备内唯一 CoreSession；同名存在多个 Punit 时返回 NULL。
 * @return
 * 返回借用指针，不增加引用计数。
 */
HFLINK_API HFLink_CoreSession *HFLink_DebugDeviceSession_FindCoreSession(HFLink_DebugDeviceSession *session,
                                                                         const char *pname);

/** @brief 注销并销毁 CoreSession、所属 target 和 CoreSightContext；可传 NULL。 */
HFLINK_API void HFLink_CoreSession_Destroy(HFLink_CoreSession *core_session);

/** @brief 获取 CoreSession 的 Pname；返回由 CoreSession 持有的只读字符串。 */
HFLINK_API const char *HFLink_CoreSession_GetPname(const HFLink_CoreSession *core_session);

/** @brief 获取 CoreSession 的 Punit。 */
HFLINK_API int HFLink_CoreSession_GetPunit(const HFLink_CoreSession *core_session, uint32_t *punit);

/** @brief 获取从 Pack debug path 深拷贝的默认复位 Sequence 名称；缺失时返回 NULL。 */
HFLINK_API const char *HFLink_CoreSession_GetDefaultResetSequence(const HFLink_CoreSession *core_session);

/**
 * @brief 使用 CoreSession 的 Pack defaultResetSequence 执行 CHIP 范围复位。
 * @return 缺少名称返回 HFLINK_ERR_NOT_INITIALIZED；否则返回自定义或内置 Sequence 的执行结果。
 * @note Sequence 名称不再推导影响范围；需要其他范围时使用 HFLink_CoreSession_ResetDefaultScoped()。
 */
HFLINK_API int HFLink_CoreSession_ResetDefault(HFLink_CoreSession *core_session, uint32_t *affected_route_count,
                                               HFLink_DebugSequenceResolver resolver, void *resolver_userdata,
                                               HFLink_DebugSequenceBody reset_body, void *reset_body_userdata,
                                               HFLink_DebugSequenceBody reset_catch_set, void *reset_catch_set_userdata,
                                               HFLink_DebugSequenceBody reset_catch_clear,
                                               void *reset_catch_clear_userdata);

/** @brief 获取 CoreSession 的 CoreSightContext 借用指针。 */
HFLINK_API HFLink_CoreSightContext *HFLink_CoreSession_GetContext(HFLink_CoreSession *core_session);
/** @brief 返回 Context 所属 CoreSession 的 Pname；裸 Context 返回 NULL。 */
HFLINK_API const char *HFLink_CoreSight_ContextGetPname(const HFLink_CoreSightContext *context);

/** @brief 获取 CoreSession 的 target 借用指针；尚未创建时返回 NULL。 */
HFLINK_API HFLink_CoreSightTarget *HFLink_CoreSession_GetTarget(HFLink_CoreSession *core_session);

/** @brief 获取 CoreSession 绑定的 Pack debug port 路由。 */
HFLINK_API int HFLink_CoreSession_GetDebugPortRoute(const HFLink_CoreSession *core_session,
                                                    HFLink_DebugPortRouteInfo *route_info);

/**
 * @brief 按 CoreSession 路由创建并绑定唯一 target。
 * @param type_name CPU target 类型，NULL 时默认为 cortex_m。
 * @return 成功返回 target；已绑定 target 或创建失败时返回 NULL。
 * @note APID 路由需由 topology registry 解析为具体 access path，解析完成前返回 NULL。
 */
HFLINK_API HFLink_CoreSightTarget *HFLink_CoreSession_CreateTarget(HFLink_CoreSession *core_session,
                                                                   const char *type_name);

/**
 * @brief 根据已解析的 CMSIS-Pack 元数据为每个 processor unit 创建 CoreSession 和 target。
 *
 * @return HFLINK_OK；元数据缺失、路由不唯一或核心类型不支持时返回 HFLINK_ERR_PACK_FORMAT。
 * @note 支持传统 debug@__ap 和可解析为实际 AP 的 accessport APID 路由。同一 debug port 可同时声明
 * JTAG/SWD capability，但调用方必须在构建前按用户选择过滤出实际 transport。

 */
HFLINK_API int HFLink_DebugDeviceSession_BuildPackTopology(HFLink_DebugDeviceSession *session,
                                                           const HFLink_PackDeviceInfo *device_info);

/** @brief 仅规范化并注册 Pack ports/access paths，不创建 CoreSession 或访问硬件。 */
HFLINK_API int HFLink_DebugDeviceSession_PreparePackTopology(HFLink_DebugDeviceSession *session,
                                                             const HFLink_PackDeviceInfo *device_info);

/** @brief 获取已准备的 debug port 数量及按 pack dp index 升序排列的 route。 */
HFLINK_API uint32_t HFLink_DebugDeviceSession_GetDebugPortRouteCount(HFLink_DebugDeviceSession *session);
HFLINK_API int HFLink_DebugDeviceSession_GetDebugPortRouteAt(HFLink_DebugDeviceSession *session, uint32_t index,
                                                             HFLink_DebugPortRouteInfo *route_info);
/** @brief 生命周期调度器推进 route 阶段；除 FAILED 外只允许单调前进。 */
HFLINK_API int HFLink_DebugDeviceSession_SetDebugPortPhase(HFLink_DebugDeviceSession *session, uint64_t pack_dp_index,
                                                           HFLink_DebugPortPhase phase);

/** @brief InitTarget 专用 raw probe API；这些入口不执行隐式 CoreSight DP 初始化。 */
HFLINK_API int HFLink_DebugDeviceSession_ProbeReadDP(HFLink_DebugDeviceSession *session,
                                                     const HFLink_DebugPortRouteInfo *route, uint32_t reg,
                                                     uint32_t *value);
HFLINK_API int HFLink_DebugDeviceSession_ProbeWriteDP(HFLink_DebugDeviceSession *session,
                                                      const HFLink_DebugPortRouteInfo *route, uint32_t reg,
                                                      uint32_t value);
HFLINK_API int HFLink_DebugDeviceSession_ProbeWriteAbort(HFLink_DebugDeviceSession *session,
                                                         const HFLink_DebugPortRouteInfo *route, uint32_t value);
HFLINK_API int HFLink_DebugDeviceSession_ProbeSWJSequence(HFLink_DebugDeviceSession *session, const uint8_t *data,
                                                          uint8_t bit_count);
HFLINK_API int HFLink_DebugDeviceSession_ProbeSWJPins(HFLink_DebugDeviceSession *session, uint8_t *input,
                                                      uint8_t output, uint8_t select, uint32_t wait_time_us);
HFLINK_API int HFLink_DebugDeviceSession_ProbeJtagSequence(HFLink_DebugDeviceSession *session,
                                                           const HFLink_DebugPortRouteInfo *route, uint32_t bit_count,
                                                           uint32_t tms_value, uint64_t tdi, uint64_t *tdo);
HFLINK_API int HFLink_DebugDeviceSession_ProbeDelay(HFLink_DebugDeviceSession *session, uint32_t delay_us);
HFLINK_API int HFLink_DebugDeviceSession_ProbeSetClock(HFLink_DebugDeviceSession *session,
                                                       const HFLink_DebugPortRouteInfo *route, uint32_t clock_hz);

/**
 * @brief 按最终 (__dp, __ap/__apid) 组合查询设备级规范化 access path。
 * @param ap 传统模式的
 * __ap；APID
 * 模式必须为 0。
 * @param apid APID 模式的 __apid（0 是有效标识）；传统模式必须为 0。
 * @param use_apid
 * 非零查询 APID 模式，否则查询传统 AP 模式。
 * @return
 * HFLINK_OK；组合不存在或模式字段冲突时返回 HFLINK_ERR。

 */
HFLINK_API int HFLink_DebugDeviceSession_ResolveAccessPath(HFLink_DebugDeviceSession *session, uint64_t dp, uint64_t ap,
                                                           uint64_t apid, int use_apid,
                                                           HFLink_DebugAccessPathInfo *path_info);

/**
 * @brief 按显式 address-space 键解析路径。
 * @return 键不存在、模式冲突或 parent
 * 链不唯一时返回 HFLINK_ERR。

 */
HFLINK_API int HFLink_DebugDeviceSession_ResolveAddressSpace(HFLink_DebugDeviceSession *session,
                                                             const HFLink_DebugAddressSpaceKey *address_space,
                                                             HFLink_DebugAccessPathInfo *path_info);

/**
 * @brief 通过设备 registry 中的显式 access path 执行连续块读取。
 * @note
 * 整个块访问在设备事务锁内完成；不会修改任一 CoreSession 的默认 AP。
 */
HFLINK_API int HFLink_DebugDeviceSession_ReadMemoryPath(HFLink_DebugDeviceSession *session,
                                                        const HFLink_DebugAccessPathInfo *path_info, uint64_t address,
                                                        void *buffer, size_t size);

/**
 * @brief 通过设备 registry 中的显式 access path 执行连续块写入。
 * @note
 * 整个块访问在设备事务锁内完成；不会修改任一 CoreSession 的默认 AP。
 */
HFLINK_API int HFLink_DebugDeviceSession_WriteMemoryPath(HFLink_DebugDeviceSession *session,
                                                         const HFLink_DebugAccessPathInfo *path_info, uint64_t address,
                                                         const void *buffer, size_t size);

/** @brief 按 Context 当前 Sequence 变量查询最终 access path，不执行硬件访问。 */
HFLINK_API int HFLink_DebugContext_GetResolvedAccessPath(HFLink_CoreSightContext *context,
                                                         HFLink_DebugAccessPathInfo *path_info);

/** @brief 逐核心选择 Pack 路由并校验 MEM-AP、CPUID 与 Dcore。 */
HFLINK_API int HFLink_DebugDeviceSession_ValidatePackTopology(HFLink_DebugDeviceSession *session,
                                                              const HFLink_PackDeviceInfo *device_info);

/**
 * @brief 按 Pack debugport 原始探测并配置 JTAG 菊花链。
 * @note tapindex 必须从物理 TDO 侧的 0
 * 开始连续；IR
 * 长度可完整给出或按严格唯一规则推导。
 *       无论 Pack 是否完整给出 IR，都会先用
 * DAP_JTAG_Sequence
 * 核验链顺序、边界、总长度和 IDCODE，
 *       再执行 DAP_JTAG_Configure 并二次读取 IDCODE。
 */
HFLINK_API int HFLink_DebugDeviceSession_ConfigureJtagChain(HFLink_DebugDeviceSession *session,
                                                            const HFLink_PackDeviceInfo *device_info);
/**
 * @brief raw 扫描完整物理 JTAG 链，并将 device-local tapindex 唯一映射到绝对 DAP index。
 * @param device_info
 * 可写的、已按用户 transport 过滤的设备描述；成功时更新其中 JTAG tapindex。
 * @return 无匹配返回
 * HFLINK_ERR_TOPOLOGY_NOT_FOUND，多匹配返回 HFLINK_ERR_TOPOLOGY_AMBIGUOUS。
 */
HFLINK_API int HFLink_DebugDeviceSession_ConfigureJtagChainAuto(HFLink_DebugDeviceSession *session,
                                                                HFLink_PackDeviceInfo *device_info);
/** @brief 使用板级完整物理链配置 JTAG；Pack route 的 dap_index 必须已映射为绝对 TAP index。 */
HFLINK_API int HFLink_DebugDeviceSession_ConfigureJtagChainExplicit(HFLink_DebugDeviceSession *session,
                                                                    const HFLink_PackDebugPort *physical_taps,
                                                                    uint32_t tap_count);

/**
 * @brief 按 algorithm@Pname 解析执行核心；单核设备允许省略 Pname，多核省略时返回 NULL。
 */
HFLINK_API HFLink_CoreSession *HFLink_DebugDeviceSession_ResolveAlgorithmCore(HFLink_DebugDeviceSession *session,
                                                                              const HFLink_PackDeviceInfo *device_info,
                                                                              const HFLink_PackAlgorithm *algorithm);

/** @brief 获取核心上下文所属设备会话的借用指针，不增加引用计数。 */
HFLINK_API HFLink_DebugDeviceSession *HFLink_CoreSight_ContextGetDeviceSession(HFLink_CoreSightContext *context);

/** @brief 进入/离开一个完整设备事务；同线程允许递归，以支持嵌套 Sequence。 */
HFLINK_API int HFLink_DebugDeviceSession_Lock(HFLink_DebugDeviceSession *session);
HFLINK_API int HFLink_DebugDeviceSession_Unlock(HFLink_DebugDeviceSession *session);

/**
 * @brief 设置单 route 设备会话的物理 SWJ 时钟。
 * @note 多 route Pack 会话必须通过
 * HFLink_CoreSight_ContextSetClock() 指定所属 route。
 */
HFLINK_API int HFLink_DebugDeviceSession_SetClock(HFLink_DebugDeviceSession *session, uint32_t clock_hz);

/** @brief 进入 CoreSession 完整事务并选择其物理 Debug Port route。 */
HFLINK_API int HFLink_CoreSession_BeginTransaction(HFLink_CoreSession *core_session);

/** @brief 结束 CoreSession 完整事务。 */
HFLINK_API int HFLink_CoreSession_EndTransaction(HFLink_CoreSession *core_session);

/**
 * @brief 设置设备级脚本资源及其最终释放回调。
 * @param session 设备会话。
 * @param resource 脚本环境所有权对象；可由 Lua binding 封装 registry 引用。
 * @param destroy 最后一个 session 引用释放时调用的清理函数。
 * @return HFLINK_OK；参数非法或资源已设置时返回 HFLINK_ERR。
 * @note 一个设备会话只能设置一次。Sequence registry 和用户 debugvars 应归属于该资源，
 *       不应以某个 Lua device/context userdata 的 uservalue 作为唯一生命周期根。
 */
HFLINK_API int HFLink_DebugDeviceSession_SetScriptResource(HFLink_DebugDeviceSession *session, void *resource,
                                                           HFLink_DebugDeviceSessionResourceDestroy destroy);

/** @brief 获取设备级脚本资源借用指针；未设置或参数非法时返回 NULL。 */
HFLINK_API void *HFLink_DebugDeviceSession_GetScriptResource(HFLink_DebugDeviceSession *session);

/**
 * @brief 初始化 CoreSight DP 连接。
 * @param context CoreSight 上下文。
 * @param swd_mode 非零选择 SWD，零选择 JTAG。
 * @return HFLINK_OK 或 HFLINK_ERR_*。
 */
HFLINK_API int HFLink_CoreSight_ContextInitialize(HFLink_CoreSightContext *context, int swd_mode);

/**
 * @brief 仅准备 Pack route 协议，不执行 coresight_dp_init 或普通 DP/AP 访问。
 * @note 随后的
 * ConnectSequences 会在 DebugPortSetup 已开始后，允许 Sequence 内首个 DP 访问完成硬件初始化。
 */
HFLINK_API int HFLink_CoreSight_ContextPrepareRoute(HFLink_CoreSightContext *context, int swd_mode);

/**
 * @brief 在当前核心的设备事务内发送原始 SWJ bit 序列。
 * @note
 * 原始时序可能改变物理协议状态；无论调用成功或失败都会失效设备全部 route 和 JTAG 链。

 */
HFLINK_API int HFLink_CoreSight_ContextSWJSequence(HFLink_CoreSightContext *context, const uint8_t *data,
                                                   uint8_t bit_count);

/**
 * @brief 在当前核心的设备事务内读写 SWJ 引脚。
 * @note select
 * 非零表示可能驱动物理线，无论调用成功或失败都会失效设备全部 route 和 JTAG 链；纯读取不失效。

 */
HFLINK_API int HFLink_CoreSight_ContextSWJPins(HFLink_CoreSightContext *context, uint8_t *input, uint8_t output,
                                               uint8_t select, uint32_t wait_time_us);

/** @brief 对当前 route 发送 CMSIS-DAP WriteABORT，不执行隐式 DP 初始化。 */
HFLINK_API int HFLink_CoreSight_ContextWriteAbort(HFLink_CoreSightContext *context, uint32_t value);

/**
 * @brief 对当前 JTAG 物理链发送一段固定 TMS 的原始序列并捕获 TDO。
 * @param bit_count 1..64 个 TCK。
 * @param
 * tms_value 固定 TMS 电平，只能为 0 或 1。
 * @param tdi LSB 优先的 TDI 数据。
 * @param tdo 成功时返回 LSB 优先的 TDO
 * 数据，可为 NULL。
 */
HFLINK_API int HFLink_CoreSight_ContextJtagSequence(HFLink_CoreSightContext *context, uint32_t bit_count,
                                                    uint32_t tms_value, uint64_t tdi, uint64_t *tdo);

/** @brief 在当前核心的设备事务内执行 DAP 延时；允许 0，较长延时自动拆分。 */
HFLINK_API int HFLink_CoreSight_ContextDelay(HFLink_CoreSightContext *context, uint32_t delay_us);

/** @brief 设置当前核心 route 的 SWJ 时钟，并记录为该 route 后续重选时的配置。 */
HFLINK_API int HFLink_CoreSight_ContextSetClock(HFLink_CoreSightContext *context, uint32_t clock_hz);

/**
 * @brief 开始/结束跨多个 Context 操作的设备原子事务。
 * @note 必须在同一线程严格配对；EndAtomic 未配对时返回
 * HFLINK_ERR。
 */
HFLINK_API int HFLink_CoreSight_ContextAtomicBegin(HFLink_CoreSightContext *context);
HFLINK_API int HFLink_CoreSight_ContextAtomicEnd(HFLink_CoreSightContext *context);

/** @brief 获取当前 Context 尚未配对的 atomic 层数。 */
HFLINK_API int HFLink_CoreSight_ContextGetAtomicDepth(HFLink_CoreSightContext *context, uint32_t *depth);

/** @brief Debug Access Sequence 的连接用途。 */
typedef enum
{
    HFLINK_DEBUG_CONNECTION_TYPE_DISCONNECTED = 0,
    HFLINK_DEBUG_CONNECTION_TYPE_DEBUG = 1,
    HFLINK_DEBUG_CONNECTION_TYPE_DOWNLOAD = 2,
} HFLink_DebugConnectionType;

/** @brief Debug Access Sequence 的连接复位策略。 */
typedef enum
{
    HFLINK_DEBUG_RESET_TYPE_NONE = 0,
    HFLINK_DEBUG_RESET_TYPE_HARDWARE = 1,
    HFLINK_DEBUG_RESET_TYPE_SYSTEM = 2,
    HFLINK_DEBUG_RESET_TYPE_PROCESSOR = 3,
} HFLink_DebugResetType;

/** @brief 多芯片拓扑中的复位影响范围。 */
typedef enum
{
    HFLINK_RESET_SCOPE_CORE = 0,
    HFLINK_RESET_SCOPE_CHIP = 1,
    HFLINK_RESET_SCOPE_PHYSICAL_CHAIN = 2,
} HFLink_ResetScope;

/**
 * @brief 以显式影响范围执行 CoreSession 的 defaultResetSequence。
 * @note Sequence 名称只选择函数体，scope 单独决定 cache/route 失效范围。
 */
HFLINK_API int HFLink_CoreSession_ResetDefaultScoped(
    HFLink_CoreSession *core_session, HFLink_ResetScope scope, uint32_t *affected_route_count,
    HFLink_DebugSequenceResolver resolver, void *resolver_userdata, HFLink_DebugSequenceBody reset_body,
    void *reset_body_userdata, HFLink_DebugSequenceBody reset_catch_set, void *reset_catch_set_userdata,
    HFLink_DebugSequenceBody reset_catch_clear, void *reset_catch_clear_userdata);

/**
 * @brief 配置 __connection 反映本次调试或下载连接的用途与复位策略。
 * @param under_hardware_reset 非零表示在硬件复位保持期间连接。
 * @param pre_connection_reset 非零表示 ResetHardware 是连接前复位。
 * @return HFLINK_OK；枚举或组合非法时返回 HFLINK_ERR。
 */
HFLINK_API int HFLink_CoreSight_ContextConfigureConnection(HFLink_CoreSightContext *context,
                                                           HFLink_DebugConnectionType connection_type,
                                                           HFLink_DebugResetType reset_type, int under_hardware_reset,
                                                           int pre_connection_reset);

/**
 * @brief 获取上下文持有的 DP 句柄。
 * @return 借用指针，不得由调用方释放。
 */
HFLINK_API HFLink_CoreSightDP *HFLink_CoreSight_ContextGetDP(HFLink_CoreSightContext *context);

/**
 * @brief 设置调试会话的默认 AP。
 * @param context CoreSight 调试上下文。
 * @param apsel ADIv5 APSEL 或 HAL 支持的顶层 AP 地址。
 * @return HFLINK_OK 或 HFLINK_ERR_NOT_INITIALIZED。
 * @note C、Lua、Sequence 和 Flash 共用该值；临时 Sequence 路由应由执行帧覆盖。
 */
HFLINK_API int HFLink_CoreSight_ContextSetDefaultAP(HFLink_CoreSightContext *context, uint64_t apsel);

/**
 * @brief 获取调试会话的默认 AP。
 * @param context CoreSight 调试上下文。
 * @param apsel 接收当前默认 AP。
 * @return HFLINK_OK 或 HFLINK_ERR_NOT_INITIALIZED。
 */
HFLINK_API int HFLink_CoreSight_ContextGetDefaultAP(const HFLink_CoreSightContext *context, uint64_t *apsel);

/**
 * @brief 为一次顶层 Debug Access Sequence 初始化路由和调用状态。
 * @param context CoreSight 调试上下文。
 * @param dp Pack debug 元素定义的 DP。
 * @param ap 传统 AP 路由值。
 * @param apid accessportV1/V2 路由标识。
 * @param use_apid 非零时使用 APID，并将 __ap 清零。
 * @note 此入口会将 __errorcontrol 和 __Result 清零，并同步上下文默认 AP。
 */
HFLINK_API int HFLink_CoreSight_ContextBeginTopLevelSequence(HFLink_CoreSightContext *context, uint64_t dp, uint64_t ap,
                                                             uint64_t apid, int use_apid);

/**
 * @brief 进入嵌套 Debug Access Sequence 并保存当前路由变量。
 * @note 仅供 Sequence engine 使用；普通 Lua/C 函数调用不得调用此接口。
 */
HFLINK_API int HFLink_CoreSight_ContextPushSequence(HFLink_CoreSightContext *context);

/**
 * @brief 离开嵌套 Debug Access Sequence 并恢复调用者路由变量。
 * @note __Result 不随执行帧恢复。
 */
HFLINK_API int HFLink_CoreSight_ContextPopSequence(HFLink_CoreSightContext *context);

/**
 * @brief 在完整设备事务内执行一个嵌套 Debug Access Sequence 函数体。
 * @param context 当前核心上下文。
 * @param name Sequence 名称，仅用于实现选择和诊断，不得为 NULL 或空字符串。
 * @param body Pack Lua 覆盖或 C 默认实现；不得为 NULL。
 * @param userdata 原样传给 body。
 * @return body 的结果；进入、恢复或解锁失败返回 HFLINK_ERR_*。
 * @note engine 始终恢复 __dp/__ap/__apid/__errorcontrol；__Result 保留函数体写入值。
 */
HFLINK_API int HFLink_CoreSight_ContextExecuteSequence(HFLink_CoreSightContext *context, const char *name,
                                                       HFLink_DebugSequenceBody body, void *userdata);

/**
 * @brief 按 Pack 覆盖优先规则选择并执行 Sequence 实现。
 * @param override_body Pack Lua 或用户自定义实现；非 NULL 时始终优先。
 * @param host_body HOST 缺省策略的 C 宿主实现，可为 NULL。
 * @param flm_body FLM 缺省策略的算法回退实现，可为 NULL。
 * @return EMPTY 策略在无覆盖时返回 HFLINK_OK；HOST/FLM 缺少对应实现时返回 HFLINK_ERR_NOT_INITIALIZED；
 *         未知自定义名称且无覆盖时返回 HFLINK_ERR。
 */
HFLINK_API int HFLink_CoreSight_ContextDispatchSequence(HFLink_CoreSightContext *context, const char *name,
                                                        HFLink_DebugSequenceBody override_body, void *override_userdata,
                                                        HFLink_DebugSequenceBody host_body, void *host_userdata,
                                                        HFLink_DebugSequenceBody flm_body, void *flm_userdata);

/**
 * @brief 按 CMSIS-Pack 规范执行连接生命周期。
 * @note 在一个设备事务中依次执行 DebugPortSetup、DebugPortStart、DebugDeviceUnlock、DebugCoreStart。
 *       resolver 对每个名称返回 Pack 覆盖；返回 NULL 时使用注册表缺省策略。
 */
HFLINK_API int HFLink_CoreSight_ContextConnectSequences(
    HFLink_CoreSightContext *context, HFLink_DebugConnectionType connection_type, HFLink_DebugResetType reset_type,
    int under_hardware_reset, int pre_connection_reset, HFLink_DebugSequenceResolver resolver, void *resolver_userdata,
    HFLink_DebugSequenceBody debug_port_setup, void *debug_port_setup_userdata,
    HFLink_DebugSequenceBody debug_port_start, void *debug_port_start_userdata);

/**
 * @brief 带失败补偿宿主实现的连接生命周期入口。
 * @note 与
 * HFLink_CoreSight_ContextConnectSequences()
 * 顺序相同；DebugCoreStart 失败时尝试
 *       DebugCoreStop，本次 DebugPortStart
 * 已尝试但未完整连接时尝试 DebugPortStop。原始连接错误优先返回。
 */
HFLINK_API int HFLink_CoreSight_ContextConnectSequencesEx(
    HFLink_CoreSightContext *context, HFLink_DebugConnectionType connection_type, HFLink_DebugResetType reset_type,
    int under_hardware_reset, int pre_connection_reset, HFLink_DebugSequenceResolver resolver, void *resolver_userdata,
    HFLink_DebugSequenceBody debug_port_setup, void *debug_port_setup_userdata,
    HFLink_DebugSequenceBody debug_port_start, void *debug_port_start_userdata,
    HFLink_DebugSequenceBody debug_port_stop, void *debug_port_stop_userdata);

/** @brief 执行 ResetCatchSet、指定 Reset Sequence、ResetCatchClear；Clear 在复位失败后仍会执行。 */
HFLINK_API int HFLink_CoreSight_ContextResetSequences(HFLink_CoreSightContext *context, const char *reset_sequence_name,
                                                      HFLink_DebugSequenceResolver resolver, void *resolver_userdata,
                                                      HFLink_DebugSequenceBody reset_body, void *reset_body_userdata,
                                                      HFLink_DebugSequenceBody reset_catch_set,
                                                      void *reset_catch_set_userdata,
                                                      HFLink_DebugSequenceBody reset_catch_clear,
                                                      void *reset_catch_clear_userdata);

/**
 * @brief 在明确作用域内执行复位，并报告受影响的物理 Debug Port route 数量。
 * @note CORE 保持
 * route 缓存；CHIP
 * 失效当前 route 的 DP/AP 缓存；PHYSICAL_CHAIN 失效设备会话全部
 *       route 的 DP/AP 缓存。
 *
 * ResetHardware 只能使用 PHYSICAL_CHAIN，ResetSystem 只能使用 CHIP，ResetProcessor 只能使用 CORE。
 */
HFLINK_API int HFLink_CoreSight_ContextResetSequencesScoped(
    HFLink_CoreSightContext *context, HFLink_ResetScope scope, uint32_t *affected_route_count,
    const char *reset_sequence_name, HFLink_DebugSequenceResolver resolver, void *resolver_userdata,
    HFLink_DebugSequenceBody reset_body, void *reset_body_userdata, HFLink_DebugSequenceBody reset_catch_set,
    void *reset_catch_set_userdata, HFLink_DebugSequenceBody reset_catch_clear, void *reset_catch_clear_userdata);

/**
 * @brief 执行 DebugCoreStop，并在当前连接是对应 debug port 最后一个用户时自动执行 DebugPortStop。

 * *
 * @note
 * 每个 debug port 独立计数，调用方不能指定最后用户。
 *
 * 最后用户的 DebugCoreStop 即使失败也会继续尝试 DebugPortStop，并使当前 route
 * 失效；首个错误优先返回。

 */
HFLINK_API int HFLink_CoreSight_ContextDisconnectSequences(HFLink_CoreSightContext *context,
                                                           HFLink_DebugSequenceResolver resolver,
                                                           void *resolver_userdata,
                                                           HFLink_DebugSequenceBody debug_port_stop,
                                                           void *debug_port_stop_userdata);

/**
 * @brief 通过指定 MEM-AP 原子读取 32 位目标内存。
 * @param context CoreSight 上下文。
 * @param apsel AP 编号。
 * @param address 目标地址。
 * @param value 读取结果。
 * @return HFLINK_OK 或 HFLINK_ERR_*。
 */
HFLINK_API int HFLink_CoreSight_ContextRead32(HFLink_CoreSightContext *context, uint64_t apsel, uint64_t address,
                                              uint32_t *value);

/** @brief 通过指定 MEM-AP 原子写入 32 位目标内存。 */
HFLINK_API int HFLink_CoreSight_ContextWrite32(HFLink_CoreSightContext *context, uint64_t apsel, uint64_t address,
                                               uint32_t value);
/** @brief 通过当前显式 access path 读取单个 1/2/4/8 字节元素。 */
HFLINK_API int HFLink_CoreSight_ContextReadMemory(HFLink_CoreSightContext *context, uint64_t address, uint32_t size,
                                                  uint8_t *buffer);
/** @brief 通过当前显式 access path 写入单个 1/2/4/8 字节元素。 */
HFLINK_API int HFLink_CoreSight_ContextWriteMemory(HFLink_CoreSightContext *context, uint64_t address, uint32_t size,
                                                   const uint8_t *buffer);

/** @brief 原子读取指定 AP 寄存器。 */
HFLINK_API int HFLink_CoreSight_ContextReadAP(HFLink_CoreSightContext *context, uint64_t apsel, uint32_t reg,
                                              uint32_t *value);

/** @brief 原子写入指定 AP 寄存器。 */
HFLINK_API int HFLink_CoreSight_ContextWriteAP(HFLink_CoreSightContext *context, uint64_t apsel, uint32_t reg,
                                               uint32_t value);
/** @brief ADIv6 顶层 AP 地址空间读取；address 为完整系统 APv2 地址。 */
HFLINK_API int HFLink_CoreSight_ContextReadAccessAP(HFLink_CoreSightContext *context, uint64_t address,
                                                    uint32_t *value);
/** @brief ADIv6 顶层 AP 地址空间写入；address 为完整系统 APv2 地址。 */
HFLINK_API int HFLink_CoreSight_ContextWriteAccessAP(HFLink_CoreSightContext *context, uint64_t address,
                                                     uint32_t value);

/** @brief 原子读取 DP 寄存器。 */
HFLINK_API int HFLink_CoreSight_ContextReadDP(HFLink_CoreSightContext *context, uint32_t reg, uint32_t *value);

/** @brief 原子写入 DP 寄存器。 */
HFLINK_API int HFLink_CoreSight_ContextWriteDP(HFLink_CoreSightContext *context, uint32_t reg, uint32_t value);

/**
 * @brief 销毁 CoreSight 上下文并释放其内部软件状态。
 * @param context 上下文句柄，可为 NULL。

 * *
 * @note
 * 调用方必须先对已连接的 context 调用 HFLink_CoreSight_ContextDisconnectSequences()。直接销毁已连接的

 * *
 * context 不等价于正常断开，且不能保证执行物理 DebugPortStop。调用线程尚有未配对 atomic 时会先对称释放；
 * atomic
 * 由其他线程持有时本次销毁不执行，必须由 owner 线程结束事务后重试。
 */
HFLINK_API void HFLink_CoreSight_ContextDestroy(HFLink_CoreSightContext *context);

/**
 * @brief 从核心上下文创建 target，并继承其设备会话事务边界。
 * @param context target 所属核心上下文。
 * @param apsel 目标调试 MEM-AP；传 UINT64_MAX 使用 context 的默认 AP。
 * @param type_name CPU target 类型，NULL 时默认为 cortex_m。
 * @return 成功返回 target；失败返回 NULL。
 * @note target 持有设备会话引用，因此可晚于创建它的 context 销毁。
 */
HFLINK_API HFLink_CoreSightTarget *HFLink_CoreSight_ContextCreateTarget(HFLink_CoreSightContext *context,
                                                                        uint64_t apsel, const char *type_name);

/**
 * @brief 从裸 DP 创建 target 的兼容入口。
 * @note 此入口无法关联 DebugDeviceSession，不提供跨核心的完整事务串行保证；新代码应使用
 *       HFLink_CoreSight_ContextCreateTarget。
 */
HFLINK_API HFLink_CoreSightTarget *HFLink_CoreSight_TargetCreate(HFLink_CoreSightDP *dp, uint64_t apsel,
                                                                 const char *type_name);
/** @brief 销毁 target，并释放其持有的设备会话引用。 */
HFLINK_API void HFLink_CoreSight_TargetDestroy(HFLink_CoreSightTarget *target);
/** @brief 探测目标处理器及调试资源；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetExamine(HFLink_CoreSightTarget *target);
/** @brief 轮询目标状态；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetPoll(HFLink_CoreSightTarget *target);
/** @brief 请求目标停机；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetHalt(HFLink_CoreSightTarget *target);
/** @brief 恢复目标执行；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetResume(HFLink_CoreSightTarget *target, int current, uint64_t addr, int handle_bps,
                                             int debug_exec);
/** @brief 单步执行目标；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetStep(HFLink_CoreSightTarget *target, int current, uint64_t addr, int handle_bps);
/** @brief 断言目标复位；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetAssertReset(HFLink_CoreSightTarget *target);
/** @brief 释放目标复位；关联设备会话时自动串行。 */
HFLINK_API int HFLink_CoreSight_TargetDeassertReset(HFLink_CoreSightTarget *target);
/** @brief 读取目标内存；一次调用作为一个完整设备事务。 */
HFLINK_API int HFLink_CoreSight_TargetReadMemory(HFLink_CoreSightTarget *target, uint64_t addr, uint32_t size,
                                                 uint32_t count, uint8_t *buf);
/** @brief 写入目标内存；一次调用作为一个完整设备事务。 */
HFLINK_API int HFLink_CoreSight_TargetWriteMemory(HFLink_CoreSightTarget *target, uint64_t addr, uint32_t size,
                                                  uint32_t count, const uint8_t *buf);
HFLINK_API int HFLink_CoreSight_TargetAccessMemorySegments(HFLink_CoreSightTarget *target,
                                                           const HFLink_CoreSightMemorySegment *segments,
                                                           uint32_t segment_count);
HFLINK_API int HFLink_CoreSight_TargetReadReg(HFLink_CoreSightTarget *target, uint32_t regsel, uint32_t *val);
HFLINK_API int HFLink_CoreSight_TargetWriteReg(HFLink_CoreSightTarget *target, uint32_t regsel, uint32_t val);
HFLINK_API int HFLink_CoreSight_TargetSetBreakpoint(HFLink_CoreSightTarget *target, uint64_t addr, uint32_t size,
                                                    HFLink_CoreSightBreakpointType type);
HFLINK_API int HFLink_CoreSight_TargetUnsetBreakpoint(HFLink_CoreSightTarget *target, uint64_t addr,
                                                      HFLink_CoreSightBreakpointType type);
HFLINK_API int HFLink_CoreSight_TargetSetWatchpoint(HFLink_CoreSightTarget *target, uint64_t addr, uint32_t size,
                                                    HFLink_CoreSightWatchpointMode mode);
HFLINK_API int HFLink_CoreSight_TargetUnsetWatchpoint(HFLink_CoreSightTarget *target, uint64_t addr,
                                                      HFLink_CoreSightWatchpointMode mode);
HFLINK_API const char *HFLink_CoreSight_TargetGetGdbArch(const HFLink_CoreSightTarget *target);
HFLINK_API int HFLink_CoreSight_TargetHitWatchpoint(HFLink_CoreSightTarget *target, uint64_t *out_addr,
                                                    HFLink_CoreSightWatchpointMode *out_mode);

/**
 * @brief GDB 寄存器描述（主机侧拷贝视图，供 GDB g/G/p/P 包与 target.xml 生成）
 *
 * name/feature/group/data_type 为借用指针（指向 target 寄存器表静态字符串，
 * target 生存期内有效）。
 */
typedef struct HFLink_CoreSightGdbRegDesc
{
    const char *name;     ///< 寄存器名（"r0"/"xpsr"/...）
    uint32_t number;      ///< GDB regnum
    uint32_t size;        ///< 位宽（32/64）
    const char *feature;  ///< "org.gnu.gdb.arm.*"（可为 NULL）
    const char *group;    ///< 分组（可为 NULL）
    const char *data_type;///< 类型（可为 NULL，默认 int）
    int hidden;           ///< 对 GDB 隐藏（打包寄存器）
    int exist;            ///< 当前硬件是否存在（examine 按 FPU/TZ 裁剪）
} HFLink_CoreSightGdbRegDesc;

/** @brief 获取 GDB 寄存器描述数组（堆分配拷贝，用 FreeGdbRegList 释放）。 */
HFLINK_API int HFLink_CoreSight_TargetGetGdbRegList(HFLink_CoreSightTarget *target,
                                                    HFLink_CoreSightGdbRegDesc **out_descs, uint32_t *out_count);
/** @brief 释放 GetGdbRegList 返回的数组。 */
HFLINK_API void HFLink_CoreSight_FreeGdbRegList(HFLink_CoreSightGdbRegDesc *descs);

/** @brief 按 GDB regnum 读寄存器（小端字节写入 value，容量需 >= size/8）。 */
HFLINK_API int HFLink_CoreSight_TargetReadGdbReg(HFLink_CoreSightTarget *target, uint32_t regnum, uint8_t *value,
                                                 uint32_t capacity);
/** @brief 按 GDB regnum 写寄存器（value 为小端字节，长度需 >= size/8）。 */
HFLINK_API int HFLink_CoreSight_TargetWriteGdbReg(HFLink_CoreSightTarget *target, uint32_t regnum,
                                                  const uint8_t *value, uint32_t length);
/** @brief 查询 target 当前运行状态。 */
HFLINK_API int HFLink_CoreSight_TargetGetState(const HFLink_CoreSightTarget *target, HFLink_CoreSightTargetState *state);
/** @brief 查询最近一次 halt 原因（poll 更新）。 */
HFLINK_API int HFLink_CoreSight_TargetGetDebugReason(const HFLink_CoreSightTarget *target,
                                                     HFLink_CoreSightDebugReason *reason);

/**
 * @brief 目标停靠事件（内部轮询调度器在 halt 上升沿产生，含最近停靠原因）
 */
typedef struct HFLink_CoreSightEvent
{
    HFLink_CoreSightDebugReason reason; ///< 停靠原因（停靠沿 poll 后的 debug_reason）
} HFLink_CoreSightEvent;

/**
 * @brief 等待并消费一个目标停靠事件（halt 上升沿）
 *
 * 事件由驱动内部轮询调度器周期 poll 检测上升沿产生（同步 TargetPoll 路径同样
 * 记录）；等待期间该核的轮询字段自动升级为全字段（含停靠原因/寄存器采样）。
 * @param target      目标（需经 CoreSession 创建，裸 target 不支持）
 * @param out_event   输出事件；NULL 仅测试挂起事件
 * @param timeout_ms  超时毫秒；0 为非阻塞检查
 * @return HFLINK_OK 消费到事件；HFLINK_ERR_TIMEOUT 超时；负数为参数/状态错误
 */
HFLINK_API int HFLink_CoreSight_TargetWaitEvent(HFLink_CoreSightTarget *target, HFLink_CoreSightEvent *out_event,
                                                uint32_t timeout_ms);
/** @brief 丢弃该核全部挂起的停靠事件（monitor reset 等过程事件清除）。 */
HFLINK_API int HFLink_CoreSight_TargetClearEvents(HFLink_CoreSightTarget *target);

#ifdef __cplusplus
}
#endif
