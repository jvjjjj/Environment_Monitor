#include "oled.h"
#include "oledfont.h"
#include "i2c.h"
#include <string.h>

// 1KB 显存缓冲区：8 个 Page，每个 Page 128 列
static uint8_t OLED_Buffer[8][128];

/**
  * @brief  向 SSD1306 写入单字节控制指令
  * @note   MemoryAddress 传入 0x00 代表接下来发送的是命令 (Co=0, D/C#=0)
  */
static void OLED_WriteCmd(uint8_t cmd)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

/**
  * @brief  清空单片机内部 SRAM 的显存画布
  * @note   仅清空内存，调用 OLED_UpdateScreen() 后才会真正清空屏幕
  */
void OLED_Clear(void)
{
    memset(OLED_Buffer, 0x00, sizeof(OLED_Buffer));
}

/**
  * @brief  将显存画布中的 1024 字节全部同步推送到屏幕物理 GDDRAM
  */
void OLED_UpdateScreen(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        // 1. 设置写入的目标 Page 地址 (0xB0 ~ 0xB7)
        OLED_WriteCmd(0xB0 + page);
        // 2. 将光标列地址复位到最左端第 0 列
        OLED_WriteCmd(0x00); // 列低 4 位
        OLED_WriteCmd(0x10); // 列高 4 位
        
        // 3. 一口气向 0x40 (数据模式) 连续突发倾倒该页 128 字节
        HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, OLED_Buffer[page], 128, 100);
    }
}

/**
  * @brief  在显存画布上点亮或熄灭一个像素点 (X: 0~127, Y: 0~63)
  * @param  color: 1-点亮, 0-熄灭
  */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64) return; // 越界保护

    if (color)
    {
        OLED_Buffer[y / 8][x] |= (1 << (y % 8));   // 染色点亮
    }
    else
    {
        OLED_Buffer[y / 8][x] &= ~(1 << (y % 8));  // 精准熄灭
    }
}

/**
  * @brief  在显存画布上绘制单个 8x16 字符
  */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr)
{
    if (x > 120 || y > 48) return; // 8x16 字符边界保护
    uint8_t c = chr - ' ';        // 计算 ASCII 相对偏移

    for (uint8_t col = 0; col < 8; col++)
    {
        // 绘制上半身 8 个像素 (第 0~7 行)
        uint8_t top_bits = OLED_F8x16[c][col];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            OLED_DrawPoint(x + col, y + bit, (top_bits & (1 << bit)) ? 1 : 0);
        }

        // 绘制下半身 8 个像素 (第 8~15 行)
        uint8_t bottom_bits = OLED_F8x16[c][col + 8];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            OLED_DrawPoint(x + col, y + 8 + bit, (bottom_bits & (1 << bit)) ? 1 : 0);
        }
    }
}

/**
  * @brief  在显存画布上绘制以 '\0' 结尾的字符串
  */
void OLED_ShowString(uint8_t x, uint8_t y, char *str)
{
    while (*str != '\0')
    {
        if (x > 120) // 自动折行
        {
            x = 0;
            y += 16;
        }
        if (y > 48) break; // 超出屏幕高度停止
        
        OLED_ShowChar(x, y, *str);
        x += 8;
        str++;
    }
}

/**
  * @brief  SSD1306 初始化序列 (工业级官方推荐流程)
  */
void OLED_Init(void)
{
    HAL_Delay(100); // 屏幕上电延时稳定

    OLED_WriteCmd(0xAE); // 1. 关闭显示 (Display OFF)
    OLED_WriteCmd(0xD5); // 2. 设置时钟分频因子
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); // 3. 设置多路复用率 (Multiplex Ratio)
    OLED_WriteCmd(0x3F); // 对应 64 行高
    OLED_WriteCmd(0xD3); // 4. 设置显示偏移
    OLED_WriteCmd(0x00); // 0 无偏移
    OLED_WriteCmd(0x40); // 5. 设置屏幕起始行 (Line 0)
    
    // 【最关键一步：开启内部电荷泵倍压升压至 8V】
    OLED_WriteCmd(0x8D); // 电荷泵设置
    OLED_WriteCmd(0x14); // 0x14 启用电荷泵！(0x10 为关闭)
    
    OLED_WriteCmd(0x20); // 6. 设置内存寻址模式
    OLED_WriteCmd(0x02); // 页寻址模式 (Page Addressing Mode)
    OLED_WriteCmd(0xA1); // 7. 段重映射 (0xA1 左右不镜像)
    OLED_WriteCmd(0xC8); // 8. 行扫描方向 (0xC8 上下不镜像)
    OLED_WriteCmd(0xDA); // 9. 设置 COM 引脚硬件配置
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); // 10. 设置对比度 (亮度)
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); // 11. 设置预充电周期
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); // 12. 设置 VCOMH 反选电平
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4); // 13. 全局显示跟随 RAM 内容
    OLED_WriteCmd(0xA6); // 14. 正常显示 (0xA6: 1亮0暗；0xA7为反相)
    
    // 强制清空显存并同步刷屏，防止开机雪花乱码
    OLED_Clear();
    OLED_UpdateScreen();
    
    OLED_WriteCmd(0xAF); // 15. 开启显示 (Display ON)
}