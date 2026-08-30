#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HFLINK_API 控制符号导出/导入：
 * - 静态链接（定义了 HFLINK_STATIC）：HFLINK_API 为空，所有符号静态绑定
 * - 动态库构建（HFLINK_DRIVER_EXPORTS）：__declspec(dllexport) 导出
 * - 动态库消费（默认）：__declspec(dllimport) 从 DLL 导入
 */
#if defined(HFLINK_STATIC)
#  define HFLINK_API
#elif defined(_WIN32) || defined(_WIN64)
#  ifdef HFLINK_DRIVER_EXPORTS
#    define HFLINK_API __declspec(dllexport)
#  else
#    define HFLINK_API __declspec(dllimport)
#  endif
#else
#  define HFLINK_API
#endif

#ifdef __cplusplus
}
#endif
