#include "env_analyzer.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    float temp;
    float hum;
} EnvSample_t;

/* 内部环形缓冲区 */
static EnvSample_t s_buffer[ENV_MAX_WINDOW_SIZE];
static uint8_t s_head = 0;   // 下一个写入位置
static uint8_t s_count = 0;  // 缓冲区当前有效元素数

static EnvAnalyzer_Config_t s_cfg = {
    .window_size = 15,
    .trend_threshold = 0.2f
};

/**
 * @brief  初始化环境数据分析器
 */
void EnvAnalyzer_Init(const EnvAnalyzer_Config_t *config)
{
    if (config != NULL)
    {
        s_cfg = *config;
        if (s_cfg.window_size > ENV_MAX_WINDOW_SIZE)
        {
            s_cfg.window_size = ENV_MAX_WINDOW_SIZE;
        }
        if (s_cfg.window_size < 3)
        {
            s_cfg.window_size = 3;
        }
    }
    s_head = 0;
    s_count = 0;
    memset(s_buffer, 0, sizeof(s_buffer));
}

/**
 * @brief  将新采样的一对温湿度压入环形缓冲区
 */
void EnvAnalyzer_Push(float temp, float hum)
{
    s_buffer[s_head].temp = temp;
    s_buffer[s_head].hum  = hum;

    s_head = (s_head + 1) % s_cfg.window_size;
    if (s_count < s_cfg.window_size)
    {
        s_count++;
    }
}

/**
 * @brief  计算当前滑动窗口的统计指标
 */
bool EnvAnalyzer_GetStats(EnvStats_t *out_stats)
{
    if (out_stats == NULL || s_count == 0)
    {
        return false;
    }

    float t_max = -999.0f, t_min = 999.0f, t_sum = 0.0f;
    float h_max = -999.0f, h_min = 999.0f, h_sum = 0.0f;

    for (uint8_t i = 0; i < s_count; i++)
    {
        float t = s_buffer[i].temp;
        float h = s_buffer[i].hum;

        if (t > t_max) t_max = t;
        if (t < t_min) t_min = t;
        t_sum += t;

        if (h > h_max) h_max = h;
        if (h < h_min) h_min = h;
        h_sum += h;
    }

    out_stats->temp_max = t_max;
    out_stats->temp_min = t_min;
    out_stats->temp_avg = t_sum / s_count;

    out_stats->hum_max  = h_max;
    out_stats->hum_min  = h_min;
    out_stats->hum_avg  = h_sum / s_count;

    out_stats->count    = s_count;
    out_stats->trend    = EnvAnalyzer_GetTrend();

    return true;
}

/**
 * @brief  判断当前温度变化趋势
 */
TempTrend_t EnvAnalyzer_GetTrend(void)
{
    if (s_count < 3)
    {
        return TREND_STABLE;
    }

    // 比较最新 3 个样本的均值与较早 3 个样本的均值
    uint8_t latest_idx = (s_head + s_cfg.window_size - 1) % s_cfg.window_size;
    uint8_t mid_idx    = (s_head + s_cfg.window_size - 2) % s_cfg.window_size;
    uint8_t older_idx  = (s_head + s_cfg.window_size - (s_count > 6 ? 6 : s_count)) % s_cfg.window_size;

    float recent_avg = (s_buffer[latest_idx].temp + s_buffer[mid_idx].temp) / 2.0f;
    float older_val  = s_buffer[older_idx].temp;
    float diff = recent_avg - older_val;

    if (diff > s_cfg.trend_threshold)
    {
        return TREND_RISING;
    }
    else if (diff < -s_cfg.trend_threshold)
    {
        return TREND_FALLING;
    }
    else
    {
        return TREND_STABLE;
    }
}

/**
 * @brief  获取当前趋势的文字描述与箭头符号
 */
const char* EnvAnalyzer_GetTrendString(void)
{
    TempTrend_t tr = EnvAnalyzer_GetTrend();
    if (tr == TREND_RISING)
    {
        return "Rising  [^]";
    }
    else if (tr == TREND_FALLING)
    {
        return "Falling [v]";
    }
    else
    {
        return "Stable  [-]";
    }
}

/**
 * @brief  串口打印统计分析综合报告
 */
void EnvAnalyzer_PrintReport(void)
{
    EnvStats_t stats;
    if (!EnvAnalyzer_GetStats(&stats))
    {
        printf("[STAT] No enough samples yet. Please wait a few seconds.\r\n");
        return;
    }

    printf("\r\n===================================================\r\n");
    printf("        Environment Analytics Report (Window: %d)   \r\n", stats.count);
    printf("===================================================\r\n");
    printf(" Metric      |  Maximum   |  Minimum   |  Average  \r\n");
    printf("-------------+------------+------------+-----------\r\n");
    printf(" Temperature | %6.1f C   | %6.1f C   | %6.1f C  \r\n", stats.temp_max, stats.temp_min, stats.temp_avg);
    printf(" Humidity    | %6.1f %%   | %6.1f %%   | %6.1f %%  \r\n", stats.hum_max,  stats.hum_min,  stats.hum_avg);
    printf("-------------+------------+------------+-----------\r\n");
    printf(" Trend Status: %s (Threshold: %.1f C)\r\n", EnvAnalyzer_GetTrendString(), s_cfg.trend_threshold);
    printf("===================================================\r\n\r\n");
}

/**
 * @brief  串口打印最近 N 次历史时间线采样数据
 */
void EnvAnalyzer_PrintHistory(void)
{
    if (s_count == 0)
    {
        printf("[HISTORY] Buffer is empty.\r\n");
        return;
    }

    printf("\r\n--- Recent %d Samples History (Oldest -> Newest) ---\r\n", s_count);
    
    // 计算最早的一条数据索引
    uint8_t start_idx = (s_count < s_cfg.window_size) ? 0 : s_head;

    for (uint8_t i = 0; i < s_count; i++)
    {
        uint8_t cur = (start_idx + i) % s_cfg.window_size;
        float t = s_buffer[cur].temp;
        float h = s_buffer[cur].hum;

        // 生成 ASCII 小柱状条可视化
        int bar_len = (int)((t - 15.0f) / 1.5f);
        if (bar_len < 0) bar_len = 0;
        if (bar_len > 15) bar_len = 15;

        char bar[16];
        for (int b = 0; b < bar_len; b++) bar[b] = '#';
        bar[bar_len] = '\0';

        printf(" [#%02d] Temp: %4.1f C [%-15s] | Hum: %4.1f %%\r\n", i + 1, t, bar, h);
    }
    printf("----------------------------------------------------\r\n\r\n");
}
