"""
串口通信模块
负责与 STM32 硬件的通信，支持模拟模式（无硬件时自动降级）
通信协议：
  PC -> STM32:  IN,<asset_code>\n  /  OUT,<asset_code>\n  /  RET,<asset_code>\n  /  PING\n
  STM32 -> PC:  OK,<asset_code>,<timestamp>\n  /  NO,<asset_code>,<timestamp>\n  /  TIMEOUT,<asset_code>\n  /  PONG\n
波特率：9600，数据位8，停止位1，无校验
"""
import serial
import serial.tools.list_ports
import time
import threading
import random
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

# 串口配置
BAUD_RATE = 9600
TIMEOUT_SEC = 35  # 等待硬件回复超时（秒）
EXPECTED_CMD_PREFIXES = ('OK,', 'NO,', 'TIMEOUT,', 'PONG')


class SerialComm:
    def __init__(self, port=None):
        """
        port: 串口名如 'COM3'，None 则自动检测
        自动降级：找不到串口时启用模拟模式
        """
        self.port = port
        self.ser = None
        self.simulate_mode = False
        self._lock = threading.Lock()
        self._pending_code = None  # 等待确认的资产编号

        if port:
            self._connect(port)
        else:
            self._auto_detect()

    def _auto_detect(self):
        """自动检测可用串口"""
        ports = list(serial.tools.list_ports.comports())
        logger.info(f"正在检测串口，当前可用: {[p.device for p in ports]}")
        for p in ports:
            try:
                self._connect(p.device)
                return
            except Exception:
                continue
        self._enable_simulate()

    def _connect(self, port):
        """尝试连接指定串口"""
        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=BAUD_RATE,
                bytesize=serial.EIGHTBITS,
                stopbits=serial.STOPBITS_ONE,
                timeout=2,
                write_timeout=5
            )
            time.sleep(0.5)  # 等待稳定
            self.simulate_mode = False
            logger.info(f"[SERIAL] 串口已连接: {port}")
            # 尝试ping确认
            if self._send_and_wait('PING\n', 'PONG', timeout=3):
                logger.info("[SERIAL] STM32 心跳正常")
            else:
                logger.warning("[SERIAL] STM32 未响应 PING，继续尝试通信")
        except serial.SerialException as e:
            logger.warning(f"[SERIAL] 串口 {port} 连接失败: {e}，启用模拟模式")
            self._enable_simulate()

    def _enable_simulate(self):
        self.simulate_mode = True
        logger.warning("[SIMULATE] === 模拟模式已启用（无硬件）===")

    def is_simulate_mode(self):
        return self.simulate_mode

    def _send_and_wait(self, cmd, expected_response, timeout=TIMEOUT_SEC):
        """发送命令并等待回复"""
        if self.simulate_mode:
            return False
        with self._lock:
            try:
                self.ser.flushInput()
                self.ser.write(cmd.encode('utf-8'))
                logger.info(f"[SERIAL] 发送: {cmd.strip()}")
                start = time.time()
                while time.time() - start < timeout:
                    line = self.ser.readline()
                    if line:
                        decoded = line.decode('utf-8', errors='ignore').strip()
                        logger.info(f"[SERIAL] 收到: {decoded}")
                        if decoded.startswith(expected_response):
                            return True
                    time.sleep(0.05)
                logger.warning(f"[SERIAL] 等待 {expected_response} 超时")
                return False
            except Exception as e:
                logger.error(f"[SERIAL] 通信异常: {e}")
                return False

    def send_confirm_request(self, action, asset_code):
        """
        向 STM32 发送确认请求
        action: 'IN'（入库）/ 'OUT'（借出）/ 'RET'（归还）
        asset_code: 资产二维码编号
        返回: {'result': 'OK'|'NO'|'TIMEOUT'|'ERROR', 'timestamp': str, 'asset_code': str}
        """
        if self.simulate_mode:
            return self._simulate_confirm(action, asset_code)

        cmd = f"{action},{asset_code}\n"
        with self._lock:
            self._pending_code = asset_code
            try:
                self.ser.flushInput()
                self.ser.write(cmd.encode('utf-8'))
                logger.info(f"[SERIAL] 发送确认请求: {cmd.strip()}")
                start = time.time()
                while time.time() - start < TIMEOUT_SEC:
                    line = self.ser.readline()
                    if line:
                        decoded = line.decode('utf-8', errors='ignore').strip()
                        if decoded.startswith(('OK,', 'NO,', 'TIMEOUT,')):
                            parts = decoded.split(',')
                            return {
                                'result': parts[0],  # OK / NO / TIMEOUT
                                'asset_code': parts[1] if len(parts) > 1 else asset_code,
                                'timestamp': parts[2] if len(parts) > 2 else ''
                            }
                    time.sleep(0.05)
                return {'result': 'TIMEOUT', 'asset_code': asset_code, 'timestamp': ''}
            except Exception as e:
                logger.error(f"[SERIAL] 发送请求异常: {e}")
                return {'result': 'ERROR', 'asset_code': asset_code, 'timestamp': ''}

    def _simulate_confirm(self, action, asset_code):
        """模拟模式：随机返回一个确认结果（70%确认，20%取消，10%超时）"""
        time.sleep(1.5)  # 模拟硬件处理延迟
        r = random.random()
        if r < 0.7:
            result = 'OK'
        elif r < 0.9:
            result = 'NO'
        else:
            result = 'TIMEOUT'
        ts = time.strftime('%Y%m%d%H%M%S')
        logger.info(f"[SIMULATE] 模拟确认: action={action}, asset={asset_code}, result={result}")
        return {'result': result, 'asset_code': asset_code, 'timestamp': ts}

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            logger.info("[SERIAL] 串口已关闭")


# 全局单例（整个应用共享一个串口连接）
_serial_instance = None


def get_serial_comm(port=None):
    global _serial_instance
    if _serial_instance is None:
        _serial_instance = SerialComm(port)
    return _serial_instance
