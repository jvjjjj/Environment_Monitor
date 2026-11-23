"""
Virtual ESP8266 Gateway (虚拟 ESP8266 串口-MQTT 双向云端网关)
功能：
1. 监听 STM32 发来的标准 ESP8266 AT 指令或 printf 温湿度日志；
2. 充当单片机与公网云服务器的“虚拟网络天线”；
3. 将收到的数据打包推送到公网 MQTT Broker (broker.emqx.io)；
4. 订阅云端下发控制指令 (sensor/env_monitor_25087/cmd)，实现网页/手机远程反向调控单片机！
"""

import sys
import time
import re
import json
import serial
import paho.mqtt.client as mqtt

# ================== 配置项 ==================
SERIAL_PORT = "COM4"           # 单片机串口号
BAUD_RATE   = 115200           # 波特率
MQTT_BROKER = "broker.emqx.io" # 公网免费 MQTT Broker
MQTT_PORT   = 1883             # MQTT TCP 端口
PUB_TOPIC   = "sensor/env_monitor_25087"       # 上报数据主题
CMD_TOPIC   = "sensor/env_monitor_25087/cmd"   # 云端下发控制主题
CLIENT_ID   = f"VirtualESP8266_{int(time.time())}"
# ============================================

g_ser = None

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"[MQTT CLIENT] \033[92mSuccessfully connected to Cloud Broker ({MQTT_BROKER}:{MQTT_PORT})!\033[0m")
        # 订阅云端下发控制命令主题
        client.subscribe(CMD_TOPIC)
        print(f"[MQTT CLIENT] Subscribed to control topic: \033[93m{CMD_TOPIC}\033[0m")
    else:
        print(f"[MQTT CLIENT] \033[91mConnection failed with code: {rc}\033[0m")

def on_message(client, userdata, msg):
    """ 处理来自云端/网页下发的控制指令 """
    global g_ser
    try:
        raw_cmd = msg.payload.decode('utf-8').strip()
        print(f"\n\033[93m[CLOUD CMD RECEIVED]\033[0m Received on {msg.topic}: {raw_cmd}")

        cmd_to_send = ""
        # 尝试作为 JSON 解析
        try:
            data = json.loads(raw_cmd)
            if "cmd" in data:
                cmd_to_send = data["cmd"]
            elif "temp" in data:
                cmd_to_send = f"temp {data['temp']}"
            elif "hum" in data:
                cmd_to_send = f"hum {data['hum']}"
            elif "mode" in data and data["mode"] == "auto":
                cmd_to_send = "auto"
        except Exception:
            # 直接作为普通字符串指令
            cmd_to_send = raw_cmd

        if cmd_to_send and g_ser and g_ser.is_open:
            print(f"\033[92m[ESP8266 -> MCU UART]\033[0m Forwarding command to STM32: {cmd_to_send}")
            g_ser.write((cmd_to_send + "\r\n").encode('utf-8'))
    except Exception as e:
        print(f"[!] Error processing cloud command: {e}")

def main():
    global g_ser
    print("=" * 65)
    print("      STM32 Virtual ESP8266 <-> MQTT Cloud Gateway       ")
    print("=" * 65)
    print(f"[*] Serial Port : {SERIAL_PORT} @ {BAUD_RATE}")
    print(f"[*] MQTT Broker : {MQTT_BROKER}:{MQTT_PORT}")
    print(f"[*] Data Topic  : {PUB_TOPIC}")
    print(f"[*] Cmd Topic   : {CMD_TOPIC}")
    print("=" * 65)

    # 1. 初始化 MQTT 客户端
    try:
        mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
    except AttributeError:
        mqtt_client = mqtt.Client(client_id=CLIENT_ID)

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    print("[*] Connecting to MQTT Broker...")
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"[!] Warning: Could not connect to MQTT Broker directly ({e}). Retrying in background...")

    # 2. 打开串口
    try:
        g_ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"[*] Serial port {SERIAL_PORT} opened successfully. Listening...\n")
    except Exception as e:
        print(f"\033[91m[ERROR] Failed to open {SERIAL_PORT}: {e}\033[0m")
        print("[!] 请检查：串口助手是否占用了 COM4？如果开着请先关闭串口助手。\n")
        return

    buffer = ""

    try:
        while True:
            # 读取串口数据
            if g_ser.in_waiting > 0:
                raw_bytes = g_ser.read(g_ser.in_waiting)
                try:
                    text = raw_bytes.decode('utf-8', errors='ignore')
                except Exception:
                    text = raw_bytes.decode('gbk', errors='ignore')

                buffer += text
                buffer = buffer.replace('\r\n', '\n').replace('\r', '\n')

                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if not line:
                        continue

                    # 处理消息
                    handle_stm32_message(line, g_ser, mqtt_client)

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n[*] Exiting Virtual Gateway...")
    finally:
        if g_ser:
            g_ser.close()
        mqtt_client.loop_stop()
        mqtt_client.disconnect()

def handle_stm32_message(line: str, ser: serial.Serial, mqtt_client: mqtt.Client):
    """ 处理 STM32 发来的消息与 AT 指令 """
    
    # 1. 握手测试 AT
    if line.upper() == "AT":
        print(f"\033[94m[MCU -> ESP8266]\033[0m {line}")
        time.sleep(0.05)
        ser.write(b"OK\r\n")
        print(f"\033[92m[ESP8266 -> MCU]\033[0m OK")

    # 2. WiFi 连接指令 AT+CWJAP
    elif line.upper().startswith("AT+CWJAP"):
        print(f"\033[94m[MCU -> ESP8266]\033[0m {line}")
        time.sleep(0.1)
        resp = "WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n"
        ser.write(resp.encode('utf-8'))
        print(f"\033[92m[ESP8266 -> MCU]\033[0m WiFi Connected! (Virtual Network Ready)")

    # 3. MQTT 连接指令 AT+MQTTCONN
    elif line.upper().startswith("AT+MQTTCONN"):
        print(f"\033[94m[MCU -> ESP8266]\033[0m {line}")
        time.sleep(0.1)
        resp = "+MQTTCONNECTED:0,1,\"broker.emqx.io\",\"1883\",\"\",1\r\nOK\r\n"
        ser.write(resp.encode('utf-8'))
        print(f"\033[92m[ESP8266 -> MCU]\033[0m +MQTTCONNECTED (Broker link established)")

    # 4. MQTT 消息发布指令: AT+MQTTPUB=0,"topic","payload",0,0
    elif line.upper().startswith("AT+MQTTPUB"):
        print(f"\033[94m[MCU -> ESP8266]\033[0m {line}")
        match = re.search(r'AT\+MQTTPUB=\d+,"([^"]+)","(.*)",\d+,\d+', line, re.IGNORECASE)
        if match:
            topic = match.group(1)
            payload = match.group(2).replace(r'\"', '"')
            mqtt_client.publish(topic, payload, qos=0)
            ser.write(b"OK\r\n")
            print(f"\033[95m[CLOUD PUBSUB] >>> Push to Cloud Success! <<<\033[0m")
            print(f"      Topic   : \033[93m{topic}\033[0m")
            print(f"      Payload : \033[96m{payload}\033[0m\n")
        else:
            ser.write(b"ERROR\r\n")

    # 5. 智能嗅探解析普通 printf 打印输出并自动上云
    # 例如：Temp: 24.5 C, Hum: 52.1 %
    elif "Temp:" in line and "Hum:" in line:
        print(f"\033[90m[MCU LOG]\033[0m {line}")
        match = re.search(r'Temp:\s*([0-9.-]+)\s*C.*Hum:\s*([0-9.-]+)\s*%', line)
        if match:
            temp = float(match.group(1))
            hum = float(match.group(2))
            alarm = temp > 30.0 or temp < 10.0
            payload = json.dumps({"temp": temp, "hum": hum, "alarm": alarm})
            mqtt_client.publish(PUB_TOPIC, payload, qos=0)
            print(f"\033[95m[AUTO CLOUD] >>> Auto Uploaded <<<\033[0m {payload}\n")

    # 6. 其他普通单片机日志 (如 Warning / Help / CLI 回显)
    else:
        print(f"\033[90m[MCU LOG]\033[0m {line}")

if __name__ == "__main__":
    main()
