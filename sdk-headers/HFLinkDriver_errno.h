/**
 * @file HFLinkDriver_errno.h
 * @brief HFLinkDriver 统一错误码：公开 API 以 0（HFLINK_OK）表示成功，负数表示错误。
 */
#pragma once

#define HFLINK_OK 0
#define HFLINK_ERR -1
#define HFLINK_ERR_NOT_INITIALIZED -2
#define HFLINK_ERR_UNKNOWN_MODEL -3
#define HFLINK_ERR_NO_TRANSFER -4
#define HFLINK_ERR_BUSY -5
#define HFLINK_ERR_COMMUNICATION -6
#define HFLINK_ERR_NONEXECUTION -7
#define HFLINK_ERR_ACK_FAULT -8

/** 已安装的 CMSIS-Pack 中未找到指定芯片。 */
#define HFLINK_ERR_PACK_NOT_FOUND -9
/** 多个已安装 CMSIS-Pack 匹配指定芯片。 */
#define HFLINK_ERR_PACK_AMBIGUOUS -10
/** Pack 注册表或 Database 文件格式无效。 */
#define HFLINK_ERR_PACK_FORMAT -11
/** Pack 注册表或 Database schema 版本不受支持。 */
#define HFLINK_ERR_PACK_VERSION -12
/** 当前构建或探针不支持请求的规范能力。 */
#define HFLINK_ERR_UNSUPPORTED -13
/** @brief DAP 在配置的重试次数耗尽后仍返回 WAIT；驱动已尝试发送 DAPABORT。 */
#define HFLINK_ERR_ACK_WAIT -14

/** Lua 代码存在语法错误。 */
#define HFLINK_ERR_LUA_SYNTAX -20
/** Lua 代码执行期间发生错误。 */
#define HFLINK_ERR_LUA_RUNTIME -21
/** Lua 运行时内存分配失败。 */
#define HFLINK_ERR_LUA_MEMORY -22
/** Lua 脚本文件无法打开或读取。 */
#define HFLINK_ERR_LUA_FILE -23

/** @brief 物理拓扑中没有满足 Pack 约束的目标。 */
#define HFLINK_ERR_TOPOLOGY_NOT_FOUND -30
/** @brief 物理拓扑中有多个目标满足 Pack 约束，无法唯一选择。 */
#define HFLINK_ERR_TOPOLOGY_AMBIGUOUS -31
/** @brief Flash 镜像格式或校验无效。 */
#define HFLINK_ERR_IMAGE_FORMAT -32
/** @brief Flash bank、算法、地址空间或执行核心选择冲突。 */
#define HFLINK_ERR_PLAN_CONFLICT -33
/** @brief 调试或 Flash 操作超时。 */
#define HFLINK_ERR_TIMEOUT -34
/** @brief 目标在算法执行期间进入 Fault 或意外停止。 */
#define HFLINK_ERR_TARGET_FAULT -35

/** @brief HSS 会话未启动采样（需先 HFLink_HSS_Start）。 */
#define HFLINK_ERR_HSS_NOT_STARTED -36
/** @brief HSS 采样块数或总字节数超出单批能力上限。 */
#define HFLINK_ERR_HSS_TOO_MANY_BLOCKS -37
/** @brief HSS 参数无效（周期/块大小/地址对齐等）。 */
#define HFLINK_ERR_HSS_INVALID_PARAM -38
/** @brief 当前探针（如第三方 DAP）或固件能力不满足 HSS 请求。 */
#define HFLINK_ERR_HSS_UNSUPPORTED -39
