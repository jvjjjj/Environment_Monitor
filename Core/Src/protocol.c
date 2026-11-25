#include "protocol.h"
#include "usart.h"

static bool s_proto_enabled = true;

void Protocol_SetEnabled(bool enable)
{
    s_proto_enabled = enable;
}

bool Protocol_IsEnabled(void)
{
    return s_proto_enabled;
}

void Protocol_SendEnvData(float temp, float humi)
{
    if (!s_proto_enabled)
    {
        return;
    }

    uint8_t frame[9];
    uint8_t checksum = 0U;

    /* 定点化：物理量放大 10 倍，规避跨平台 IEEE-754 浮点传输歧义 */
    int16_t  t_int = (int16_t)(temp * 10.0f);
    uint16_t h_int = (uint16_t)(humi * 10.0f);

    frame[0] = FRAME_HEADER_1;
    frame[1] = FRAME_HEADER_2;
    frame[2] = TYPE_ENV_DATA;

    /* 统一采用网络大端序 (Big-Endian) 封包 */
    frame[3] = (uint8_t)((uint16_t)t_int >> 8);
    frame[4] = (uint8_t)((uint16_t)t_int & 0xFFU);
    frame[5] = (uint8_t)(h_int >> 8);
    frame[6] = (uint8_t)(h_int & 0xFFU);

    for (uint8_t i = 2U; i <= 6U; i++)
    {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    frame[7] = checksum;
    frame[8] = FRAME_TAIL;

    HAL_UART_Transmit(&huart1, frame, 9U, 50U);
}
