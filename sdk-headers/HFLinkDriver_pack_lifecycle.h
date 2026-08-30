/**
 * @file HFLinkDriver_pack_lifecycle.h
 * @brief CMSIS-Pack 生产连接生命周期：从设备选择器建立完整连接（拓扑构建、核会话、复位、Flash 绑定）的一站式 API。
 */
#pragma once

#include "HFLinkDriver.h"
#include "HFLinkDriver_lua.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief CMSIS-Pack 生产连接生命周期对象。 */
typedef struct HFLink_PackDebugSession HFLink_PackDebugSession;
typedef struct HFLink_PackDebugSessionOptions HFLink_PackDebugSessionOptions;

typedef enum
{
    HFLINK_PACK_TRANSPORT_UNSPECIFIED = 0,
    HFLINK_PACK_TRANSPORT_SWD = 1,
    HFLINK_PACK_TRANSPORT_JTAG = 2,
    HFLINK_PACK_TRANSPORT_CJTAG = 3,
} HFLink_PackTransport;

typedef enum
{
    HFLINK_PACK_JTAG_UNSPECIFIED = 0,
    HFLINK_PACK_JTAG_AUTO = 1,
    HFLINK_PACK_JTAG_EXPLICIT = 2,
} HFLink_PackJtagMode;

/**
 * @brief atomic 块的约束策略。
 * @note 零初始化（未显式设置）即 IGNORE：探针无缓冲支持时按普通块执行，不报错。
 */
typedef enum
{
    HFLINK_PACK_ATOMIC_IGNORE = 0, ///< 忽略 atomic 约束（默认）：atomic_begin/end 为 no-op
    HFLINK_PACK_ATOMIC_ERROR = 1,  ///< atomic 块在探针无缓冲支持时返回 unsupported
} HFLink_PackAtomicPolicy;

typedef struct
{
    uint32_t ir_length;
    uint32_t idcode;
    int has_idcode;
} HFLink_PackJtagTap;

typedef struct
{
    uint64_t pack_dp_index;
    uint32_t device_tap_index;
    uint32_t absolute_tap_index;
} HFLink_PackJtagTapMapping;

/** @brief 用户提供的 SWD Multidrop 物理选择器；Pack PDSC 不作为 TARGETSEL 来源。 */
typedef struct
{
    uint64_t pack_dp_index;
    uint32_t targetsel;
} HFLink_PackSwdTarget;

typedef int (*HFLink_PackConfigTargetSettings)(HFLink_PackDebugSessionOptions *options,
                                               const HFLink_PackDeviceInfo *device_info, void *userdata);
typedef int (*HFLink_PackInitTarget)(HFLink_DebugDeviceSession *device_session, const HFLink_DebugPortRouteInfo *route,
                                     void *userdata);
typedef int (*HFLink_PackCoreHook)(HFLink_CoreSession *core_session, void *userdata);

/**
 * @brief 脚本加载器返回的 Sequence 运行时。
 * @note Open 在加载器返回后接管 userdata；Close 在最后一次 Sequence 调用之后调用 destroy。
 *       若资源所有权已转交 DebugDeviceSession，可将 destroy 置 NULL。
 */
typedef struct
{
    HFLink_DebugSequenceResolver resolver;
    void *userdata;
    HFLink_DebugDeviceSessionResourceDestroy destroy;
    HFLink_PackConfigTargetSettings config_target_settings;
    HFLink_PackInitTarget init_target;
    HFLink_PackCoreHook setup_target;
    HFLink_PackCoreHook before_reset_target;
    HFLink_PackCoreHook reset_target;
    HFLink_PackCoreHook after_reset_target;
    HFLink_PackCoreHook on_disconnect_target;
    HFLink_PackCoreHook before_flash_program;
    HFLink_PackCoreHook after_flash_program;
} HFLink_PackSequenceRuntime;

/** @brief JTAG 链配置回调；为空时使用 HFLink_DebugDeviceSession_ConfigureJtagChain()。 */
typedef int (*HFLink_PackConfigureJtag)(HFLink_DebugDeviceSession *device_session,
                                        const HFLink_PackDeviceInfo *device_info, void *userdata);

/** @brief 准备核心所属 route（不得执行普通 DP/AP 初始化）；为空时调用 ContextPrepareRoute()。 */
typedef int (*HFLink_PackPrepareCore)(HFLink_CoreSession *core_session, int swd_mode, void *userdata);

/**
 * @brief 加载 Pack debugvars/Sequence 脚本并返回解析器。
 * @note runtime 进入回调前已清零。回调返回后，生命周期对象接管其中资源，包括失败路径。
 */
typedef int (*HFLink_PackLoadSequences)(HFLink_DebugDeviceSession *device_session,
                                        const HFLink_PackDeviceInfo *device_info, HFLink_PackSequenceRuntime *runtime,
                                        void *userdata);

/** @brief target examine/拓扑校验回调；为空时使用 HFLink_DebugDeviceSession_ValidatePackTopology()。 */
typedef int (*HFLink_PackValidateTopology)(HFLink_DebugDeviceSession *device_session,
                                           const HFLink_PackDeviceInfo *device_info, void *userdata);
/** @brief 连接前设置探针 SWJ 时钟；主要供宿主适配层和无硬件测试替换默认实现。 */
typedef int (*HFLink_PackSetClock)(HFLink_DebugDeviceSession *device_session, uint32_t clock_hz, void *userdata);
/** @brief 在 InitTarget 前选择物理 transport；主要供宿主适配层和无硬件测试替换默认实现。 */
typedef int (*HFLink_PackConnectTransport)(HFLink_DebugDeviceSession *device_session, HFLink_PackTransport transport,
                                           void *userdata);

/**
 * @brief Pack 生产连接参数；除 Sequence runtime 外的 callback userdata 均由调用方借用并须覆盖 Close。
 * @note 调用方必须通过 host body 或 load_sequences 返回的 resolver 提供 DEFAULT_HOST Sequence；C
 * 生命周期层不猜测目标时序。
 */
struct HFLink_PackDebugSessionOptions
{
    /** @brief 必须由调用方或 ConfigTargetSettings 显式选择。 */
    HFLink_PackTransport transport;
    /** @brief 必须由调用方或 ConfigTargetSettings 设置为非零值；不回退 Pack clock。 */
    uint32_t clock_hz;
    HFLink_PackJtagMode jtag_mode;
    const HFLink_PackJtagTap *jtag_taps;
    uint32_t jtag_tap_count;
    const HFLink_PackJtagTapMapping *jtag_mappings;
    uint32_t jtag_mapping_count;
    const HFLink_PackSwdTarget *swd_targets;
    uint32_t swd_target_count;
    /** @brief UTF-8 用户 Hook 文件路径；Session 深拷贝。 */
    const char *user_hook_script_path;
    HFLink_PackAtomicPolicy atomic_policy;
    /** @brief 独立 Hook/Sequence Lua runtime 的可选输出与 Message/Query 宿主回调。 */
    HFLink_LuaOutputCallback lua_output_callback;
    void *lua_output_userdata;
    HFLink_LuaMessageCallback lua_message_callback;
    HFLink_LuaQueryCallback lua_query_callback;
    void *lua_debug_access_userdata;

    HFLink_DebugConnectionType connection_type;
    HFLink_DebugResetType reset_type;
    int under_hardware_reset;
    int pre_connection_reset;

    HFLink_PackConfigureJtag configure_jtag;
    void *configure_jtag_userdata;
    HFLink_PackPrepareCore prepare_core;
    void *prepare_core_userdata;
    HFLink_PackLoadSequences load_sequences;
    void *load_sequences_userdata;
    HFLink_PackValidateTopology validate_topology;
    void *validate_topology_userdata;
    HFLink_PackSetClock set_clock;
    void *set_clock_userdata;
    HFLink_PackConnectTransport connect_transport;
    void *connect_transport_userdata;

    HFLink_DebugSequenceBody debug_port_setup;
    void *debug_port_setup_userdata;
    HFLink_DebugSequenceBody debug_port_start;
    void *debug_port_start_userdata;
    HFLink_DebugSequenceBody debug_port_stop;
    void *debug_port_stop_userdata;

    HFLink_DebugSequenceBody reset_body;
    void *reset_body_userdata;
    HFLink_DebugSequenceBody reset_catch_set;
    void *reset_catch_set_userdata;
    HFLink_DebugSequenceBody reset_catch_clear;
    void *reset_catch_clear_userdata;

    /** @brief C 宿主 Hook；Lua runtime 返回的同名 Hook 在这些回调之后执行。 */
    HFLink_PackConfigTargetSettings config_target_settings;
    HFLink_PackInitTarget init_target;
    HFLink_PackCoreHook setup_target;
    HFLink_PackCoreHook before_reset_target;
    HFLink_PackCoreHook reset_target;
    HFLink_PackCoreHook after_reset_target;
    HFLink_PackCoreHook on_disconnect_target;
    HFLink_PackCoreHook before_flash_program;
    HFLink_PackCoreHook after_flash_program;
    void *hook_userdata;
};

/**
 * @brief 打开完整 Pack 调试生命周期。
 *
 * 顺序固定为 Query -> 加载内置/Pack/Hook -> ConfigTargetSettings -> 冻结配置 ->
 * PreparePackTopology（仅描述）-> 设置接口/时钟 -> 选择物理 transport ->
 * 每 port InitTarget -> JTAG 物理链 -> BuildPackTopology（创建 CoreSession）->
 * 逐 (Pname, Punit) ConnectSequences ->
 * ValidatePackTopology/examine -> SetupTarget。
 * 任一步失败时逆序断开已连接核心并释放。
 * options 必填。DEFAULT_HOST Sequence 必须由宿主回调或 load_sequences 返回的 resolver 提供。
 *
 * @param device_selector 传给 HFLink_PackDevice_Query() 的设备选择器。
 * @param options 必填连接参数；结构本身会复制，其中 callback userdata 借用至 Close。
 * @param out_session 成功时接收生命周期对象；失败时置 NULL。
 */
HFLINK_API int HFLink_PackDebugSession_Open(const char *device_selector, const HFLink_PackDebugSessionOptions *options,
                                            HFLink_PackDebugSession **out_session);

/**
 * @brief 逆序断开所有核心并释放 PackDeviceInfo、DebugDeviceSession 和 CoreSession 列表。
 * @return 首个断开错误；资源仍会全部释放。重复调用安全并返回 HFLINK_OK。
 */
HFLINK_API int HFLink_PackDebugSession_Close(HFLink_PackDebugSession *session);

/** @brief Close 后释放生命周期对象；可传 NULL。 */
HFLINK_API void HFLink_PackDebugSession_Destroy(HFLink_PackDebugSession *session);

/** @brief 返回生命周期持有的 Pack 查询结果借用指针；Close 后返回 NULL。 */
HFLINK_API const HFLink_PackDeviceInfo *HFLink_PackDebugSession_GetDeviceInfo(const HFLink_PackDebugSession *session);

/** @brief 返回生命周期持有的设备会话借用指针；Close 后返回 NULL。 */
HFLINK_API HFLink_DebugDeviceSession *HFLink_PackDebugSession_GetDeviceSession(HFLink_PackDebugSession *session);

/** @brief 返回按 Pack processor/Punit 顺序保存的 CoreSession 数量。 */
HFLINK_API uint32_t HFLink_PackDebugSession_GetCoreCount(const HFLink_PackDebugSession *session);

/** @brief 按稳定顺序返回 CoreSession 借用指针；越界或 Close 后返回 NULL。 */
HFLINK_API HFLink_CoreSession *HFLink_PackDebugSession_GetCoreAt(HFLink_PackDebugSession *session, uint32_t index);

/** @brief 按 (Pname, Punit) 精确返回 CoreSession 借用指针；Close 后返回 NULL。 */
HFLINK_API HFLink_CoreSession *HFLink_PackDebugSession_GetCore(HFLink_PackDebugSession *session, const char *pname,
                                                               uint32_t punit);

/** @brief 使用 Open 时加载的 resolver 和复位宿主回调执行指定核心的 Pack 默认复位。 */
HFLINK_API int HFLink_PackDebugSession_ResetDefault(HFLink_PackDebugSession *session, const char *pname, uint32_t punit,
                                                    uint32_t *affected_route_count);

/** @brief 使用显式影响范围执行指定核心的 defaultResetSequence。 */
HFLINK_API int HFLink_PackDebugSession_ResetDefaultScoped(HFLink_PackDebugSession *session, const char *pname,
                                                          uint32_t punit, HFLink_ResetScope scope,
                                                          uint32_t *affected_route_count);

/**
 * @brief 将 FlashSession 绑定到本 Pack Session 的核心及 C/Lua Flash 前后 Hook。
 * @note FlashSession 必须在 Pack Session Close/Destroy 前完成 Close。
 */
HFLINK_API int HFLink_PackDebugSession_BindFlashSession(HFLink_PackDebugSession *session,
                                                        HFLink_FlashSession *flash_session, const char *pname,
                                                        uint32_t punit);

#ifdef __cplusplus
}
#endif
