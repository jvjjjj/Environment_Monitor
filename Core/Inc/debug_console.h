#ifndef __DEBUG_CONSOLE_H__
#define __DEBUG_CONSOLE_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 调试控制台启动参数结构体
 */
typedef struct {
    bool print_menu_on_boot; // 开机时是否自动打印调试指令菜单
    bool echo_enabled;       // 输入字符时是否开启终端回显
} DebugConsole_Config_t;

/**
 * @brief  初始化调试控制台模块
 * @param  config: 启动参数配置指针（传入 NULL 则使用默认配置）
 */
void DebugConsole_Init(const DebugConsole_Config_t *config);

/**
 * @brief  调试控制台非阻塞轮询处理函数
 * @note   检测串口是否有新指令输入并执行
 */
void DebugConsole_Process(void);

/**
 * @brief  带实时串口指令监听的智能延时
 * @param  delay_ms: 延时毫秒数
 * @note   用它替代传统的 HAL_Delay()，延时期间串口输入仍然立即响应，不会卡顿丢字
 */
void DebugConsole_Delay(uint32_t delay_ms);

#endif /* __DEBUG_CONSOLE_H__ */
