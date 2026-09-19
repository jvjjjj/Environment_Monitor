#include "protocol.h"
#include "usart.h"

void Protocol_SendEnvData(float temp, float humi)
{
    uint8_t frame[9];
    uint8_t checksum = 0;
    
    // 物理量放大 10 倍转为整数
    int16_t t_int = (int16_t)(temp * 10.0f);
    uint16_t h_int = (uint16_t)(humi * 10.0f);

    // 1. 组包
    frame[0] = FRAME_HEADER_1;
    frame[1] = FRAME_HEADER_2;
    frame[2] = TYPE_ENV_DATA;
    
    // 大端模式（高位在前）存放数据
    frame[3] = (uint8_t)(t_int >> 8);
    frame[4] = (uint8_t)(t_int & 0xFF);
    frame[5] = (uint8_t)(h_int >> 8);
    frame[6] = (uint8_t)(h_int & 0xFF);

    // 2. 计算校验和 (只校验有效载荷：Type ~ Data)
    for (uint8_t i = 2; i <= 6; i++) {
        checksum += frame[i];
    }
    frame[7] = checksum;
    
    // 3. 帧尾
    frame[8] = FRAME_TAIL;

    // 4. 调用 HAL 库串口阻塞发送 9 个字节
    HAL_UART_Transmit(&huart1, frame, 9, 100);
}
