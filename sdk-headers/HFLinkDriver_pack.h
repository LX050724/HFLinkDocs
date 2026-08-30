/**
 * @file HFLinkDriver_pack.h
 * @brief CMSIS-Pack 查询 API：已安装 Pack 枚举、芯片信息查询（含内存映射与 Flash 算法描述）。
 */
#pragma once

#include "HFLinkDriver_import.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief CMSIS-Pack processor 短文本字段容量，包含字符串终止符。 */
#define HFLINK_PACK_PROCESSOR_TEXT_SIZE 32
/** @brief debugport 协议名称容量，包含字符串终止符。 */
#define HFLINK_PACK_DEBUG_PROTOCOL_SIZE 8
/** @brief 内存区域名称容量，包含字符串终止符。 */
#define HFLINK_PACK_MEMORY_NAME_SIZE 32
/** @brief 内存访问权限字符串容量，包含字符串终止符。 */
#define HFLINK_PACK_MEMORY_ACCESS_SIZE 16
/** @brief 复位序列名称容量，包含字符串终止符。 */
#define HFLINK_PACK_SEQUENCE_NAME_SIZE 32

/** @brief CMSIS-Pack 下载算法信息。 */
typedef struct
{
    char *path;
    char processor_name[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    uint64_t start;
    uint64_t size;
    uint64_t ram_start;
    uint64_t ram_size;
    int is_default;
} HFLink_PackAlgorithm;

/** @brief CMSIS-Pack 设备内存区域。 */
typedef struct
{
    char name[HFLINK_PACK_MEMORY_NAME_SIZE];
    /** @brief 内存区域所属处理器；共享区域为空串。 */
    char processor_name[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char access[HFLINK_PACK_MEMORY_ACCESS_SIZE];
    uint64_t start;
    uint64_t size;
    int is_default;
    int is_startup;
    int is_init;
} HFLink_PackMemory;

/** @brief CMSIS-Pack processor 信息。 */
typedef struct
{
    char name[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    uint32_t units;
    char core[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char fpu[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char mpu[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char trust_zone[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char dsp[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char mve[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    char endian[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    uint64_t clock;
    char core_version[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
} HFLink_PackProcessor;

/** @brief processor 对应的传统 DP/AP 调试访问路径。 */
typedef struct
{
    char processor_name[HFLINK_PACK_PROCESSOR_TEXT_SIZE];
    uint64_t dp_index;
    uint64_t ap_index;
    uint64_t apid;
    uint64_t address;
    uint32_t punit;
    int has_ap_index;
    int has_apid;
    int has_address;
    int has_punit;
    char default_reset_sequence[HFLINK_PACK_SEQUENCE_NAME_SIZE];
} HFLink_PackDebugAccessPath;

/** @brief CMSIS-Pack debugport 声明。 */
typedef struct
{
    uint64_t dp_index;
    char protocol[HFLINK_PACK_DEBUG_PROTOCOL_SIZE];
    uint64_t tap_index;
    uint64_t idcode;
    uint32_t ir_length;
    uint64_t dp_id;
    uint64_t instance_id;
    uint32_t targetsel;
    int has_tap_index;
    int has_idcode;
    int has_ir_length;
    int has_dp_id;
    int has_instance_id;
    int has_targetsel;
} HFLink_PackDebugPort;

/** @brief CMSIS-Pack accessportV1/accessportV2 声明。 */
typedef struct
{
    uint64_t apid;
    uint64_t dp_index;
    uint64_t index;
    uint64_t address;
    uint64_t parent_apid;
    uint64_t hprot;
    uint64_t sprot;
    int is_v2;
    int has_parent_apid;
    int has_hprot;
    int has_sprot;
} HFLink_PackAccessPort;

/** @brief 按 device/subFamily/family 逐字段合并后的 CMSIS-Pack debugconfig。 */
typedef struct
{
    /** @brief 默认调试协议：swd、jtag 或 cjtag。 */
    char protocol[HFLINK_PACK_DEBUG_PROTOCOL_SIZE];
    uint32_t clock_hz;
    int swj;
    int dormant;
    int has_protocol;
    int has_clock;
    int has_swj;
    int has_dormant;
} HFLink_PackDebugConfig;

/** @brief CMSIS-Pack Debug Access Sequence 及其处理器选择元数据。 */
typedef struct
{
    char *name;
    /** @brief 为空时为全部处理器通配。 */
    char *processor_name;
    char *path;
    int disabled;
    int has_disable;
} HFLink_PackSequence;

/** @brief 芯片在已安装 CMSIS-Pack 中的完整解析结果。 */
typedef struct
{
    char *pack_vendor;
    char *pack_name;
    char *pack_version;
    char *package_vendor;
    char *family;
    char *sub_family;
    char *device;
    /** @brief 当前 Target 的 processor 名称；未显式选择核心时为 NULL。 */
    char *processor_name;

    char *svd_path;
    /** @brief 合并后的 debugconfig.sdf 路径；也兼容旧 debugDescription 字段。 */
    char *debug_description_path;
    HFLink_PackDebugConfig debug_config;

    HFLink_PackAlgorithm *algorithms;
    uint32_t algorithm_count;
    HFLink_PackMemory *memories;
    uint32_t memory_count;
    HFLink_PackProcessor *processors;
    uint32_t processor_count;
    HFLink_PackDebugAccessPath *debug_access_paths;
    uint32_t debug_access_path_count;
    HFLink_PackDebugPort *debug_ports;
    uint32_t debug_port_count;
    HFLink_PackAccessPort *access_ports;
    uint32_t access_port_count;

    char **debugvars_paths;
    uint32_t debugvars_count;
    HFLink_PackSequence *sequences;
    /** @brief Sequence 数量，同时也是 sequence_paths 兼容镜像的元素数量。 */
    uint32_t sequence_count;
    /** @brief 兼容旧调用方的路径镜像；新代码应使用 sequences。 */
    char **sequence_paths;
} HFLink_PackDeviceInfo;

/**
 * @brief 从当前用户安装的 CMSIS-Pack 中查询芯片信息。
 * @param device_selector UTF-8“芯片名”、“芯片名@包名/版本”或“芯片名/核心@包名/版本”。
 * @param out_info 成功时接收独立分配的结果，失败时置为 NULL。
 * @return HFLINK_OK 或 HFLINK_ERR_PACK_*。
 */
HFLINK_API int HFLink_PackDevice_Query(const char *device_selector, HFLink_PackDeviceInfo **out_info);

/**
 * @brief 释放芯片包查询结果。
 * @param info 查询结果，可为 NULL。
 */
HFLINK_API void HFLink_PackDevice_Free(HFLink_PackDeviceInfo *info);

/* ========== CMSIS-Pack 枚举导出（供外部工具列包/选芯片） ========== */

/** @brief 单个已安装包的信息（枚举用）。 */
typedef struct
{
    char *key;         /**< 包标识：注册表 key，缺省派生为 "vendor.name"。 */
    char *vendor;      /**< 包厂商。 */
    char *name;        /**< 包名。 */
    char *version;     /**< 包版本。 */
    char *install_dir; /**< 可 NULL，安装目录。 */
    char *database;    /**< 可 NULL，Database 目录下 json 相对路径。 */
} HFLink_PackInfo;

/** @brief 设备树叶子：一个芯片设备（含概览信息）。 */
typedef struct
{
    char *name;           /**< 设备名。 */
    char *description;    /**< 可 NULL。 */
    uint32_t algorithm_count;
    char **algorithm_names; /**< 每个下载算法名（Pname 或路径基准），长度 = algorithm_count。 */
} HFLink_PackDeviceNode;

/** @brief 家族下的子家族。 */
typedef struct
{
    char *name;
    uint32_t device_count;
    HFLink_PackDeviceNode *devices;
} HFLink_PackSubFamily;

/** @brief 家族：直属设备 + 子家族。 */
typedef struct
{
    char *name;
    uint32_t device_count;
    HFLink_PackDeviceNode *devices;
    uint32_t sub_family_count;
    HFLink_PackSubFamily *sub_families;
} HFLink_PackFamily;

/** @brief 某包的完整设备树（family→subFamily→device）。 */
typedef struct
{
    char *key;                 /**< 本包标识（同 HFLink_PackInfo::key）。 */
    uint32_t family_count;
    HFLink_PackFamily *families;
} HFLink_PackDeviceTree;

/**
 * @brief 枚举当前用户已安装的 CMSIS-Pack。
 * @param count 成功返回条目数（可为 0）。
 * @param out 成功接收数组；失败置 NULL；无包时 *out=NULL、count=0 且返回 HFLINK_OK。
 * @return HFLINK_OK / HFLINK_ERR_PACK_NOT_FOUND（注册表缺失） / HFLINK_ERR_PACK_FORMAT / HFLINK_ERR。
 */
HFLINK_API int HFLink_Pack_ListInstalled(int *count, HFLink_PackInfo **out);

/** @brief 释放 HFLink_Pack_ListInstalled 结果。 */
HFLINK_API void HFLink_Pack_ListInstalled_Free(HFLink_PackInfo *info, int count);

/**
 * @brief 列出某包的 family/subFamily/device 设备树。
 * @param pack_selector 包标识：HFLink_PackInfo::key、包名 name，或 "vendor.name"。
 * @param out 成功分配 HFLink_PackDeviceTree；失败置 NULL。用 HFLink_PackDeviceList_Free 释放。
 * @return HFLINK_OK / HFLINK_ERR_PACK_NOT_FOUND / HFLINK_ERR_PACK_AMBIGUOUS / HFLINK_ERR_PACK_FORMAT / HFLINK_ERR_PACK_VERSION。
 */
HFLINK_API int HFLink_Pack_ListDevices(const char *pack_selector, HFLink_PackDeviceTree **out);

/** @brief 释放 HFLink_Pack_ListDevices 结果。 */
HFLINK_API void HFLink_PackDeviceList_Free(HFLink_PackDeviceTree *tree);

#ifdef __cplusplus
}
#endif
