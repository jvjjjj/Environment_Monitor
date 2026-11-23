#ifndef __ENV_ANALYZER_H__
#define __ENV_ANALYZER_H__

#include <stdint.h>
#include <stdbool.h>

#define ENV_MAX_WINDOW_SIZE 30

/**
 * @brief 温度变化趋势枚举
 */
typedef enum {
    TREND_STABLE = 0,   // 平稳 (Stable)
    TREND_RISING = 1,   // 升温 (Rising)
    TREND_FALLING = -1  // 降温 (Falling)
} TempTrend_t;

/**
 * @brief 统计分析结果结构体
 */
typedef struct {
    float temp_max;     // 窗口内最高温度 (°C)
    float temp_min;     // 窗口内最低温度 (°C)
    float temp_avg;     // 窗口内平均温度 (°C)
    float hum_max;      // 窗口内最高湿度 (%)
    float hum_min;      // 窗口内最低湿度 (%)
    float hum_avg;      // 窗口内平均湿度 (%)
    TempTrend_t trend;  // 当前温度趋势
    uint8_t count;      // 当前已统计的有效样本数
} EnvStats_t;

/**
 * @brief 分析器启动配置参数
 */
typedef struct {
    uint8_t window_size;    // 滑动窗口样本数 (5 ~ 30)
    float trend_threshold;  // 判定升/降温的温差阈值 (°C)
} EnvAnalyzer_Config_t;

/* API 接口 */
void EnvAnalyzer_Init(const EnvAnalyzer_Config_t *config);
void EnvAnalyzer_Push(float temp, float hum);
bool EnvAnalyzer_GetStats(EnvStats_t *out_stats);
TempTrend_t EnvAnalyzer_GetTrend(void);
const char* EnvAnalyzer_GetTrendString(void);

/* 串口报表打印 */
void EnvAnalyzer_PrintReport(void);
void EnvAnalyzer_PrintHistory(void);

#endif /* __ENV_ANALYZER_H__ */
