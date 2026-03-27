"""
二维码扫码模块
支持两种模式：
  1. 摄像头模式：使用 OpenCV + pyzbar 实时扫描
  2. 模拟模式：文本框手动输入资产编号
"""
import cv2
import numpy as np
from pyzbar.pyzbar import decode
import threading
import logging
import time

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# 摄像头配置
CAMERA_INDEX = 0       # 默认摄像头编号，0=第一个摄像头
SCAN_TIMEOUT = 30      # 扫码超时（秒）


class QrScanner:
    def __init__(self, use_camera=True):
        """
        use_camera: True=摄像头模式，False=手动输入模式
        """
        self.use_camera = use_camera
        self.cap = None
        self._scan_thread = None
        self._scan_result = None
        self._scan_event = threading.Event()
        self._stop_event = threading.Event()

    def _init_camera(self):
        """初始化摄像头"""
        if self.cap is not None:
            return True
        try:
            self.cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_DSHOW)
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            if not self.cap.isOpened():
                logger.warning("[QR] 摄像头无法打开，切换到手动模式")
                self.use_camera = False
                return False
            logger.info("[QR] 摄像头已初始化")
            return True
        except Exception as e:
            logger.warning(f"[QR] 摄像头初始化失败: {e}，切换到手动模式")
            self.use_camera = False
            return False

    def scan_once(self, timeout=SCAN_TIMEOUT):
        """
        阻塞扫描一次，返回二维码内容字符串
        超时返回 None
        """
        if not self.use_camera:
            return None

        if not self._init_camera():
            return None

        self._scan_result = None
        self._stop_event.clear()
        self._scan_event.clear()

        def _scan_loop():
            start = time.time()
            while not self._stop_event.is_set() and (time.time() - start < timeout):
                ret, frame = self.cap.read()
                if not ret:
                    time.sleep(0.1)
                    continue
                # 灰度化（pyzbar 要求灰度图）
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                decoded_objs = decode(gray)
                for obj in decoded_objs:
                    self._scan_result = obj.data.decode('utf-8', errors='ignore')
                    self._scan_event.set()
                    self._stop_event.set()
                    return
                # 显示预览窗口（调试用，可注释掉）
                cv2.imshow('QR Scanner - Press Q to quit', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    self._stop_event.set()
                    break
                time.sleep(0.05)

        _scan_loop()
        cv2.destroyAllWindows()
        return self._scan_result

    def scan_async(self, callback):
        """
        异步扫码，找到后调用 callback(result)
        返回一个 stop() 函数
        """
        if not self.use_camera:
            return lambda: None

        if not self._init_camera():
            return lambda: None

        def _run():
            start = time.time()
            while not self._stop_event.is_set() and (time.time() - start < SCAN_TIMEOUT * 10):
                ret, frame = self.cap.read()
                if not ret:
                    continue
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                decoded_objs = decode(gray)
                for obj in decoded_objs:
                    result = obj.data.decode('utf-8', errors='ignore')
                    callback(result)
                    return
                cv2.imshow('QR Scanner', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                time.sleep(0.05)
            cv2.destroyAllWindows()

        self._stop_event.clear()
        self._scan_thread = threading.Thread(target=_run, daemon=True)
        self._scan_thread.start()
        return self.stop

    def stop(self):
        self._stop_event.set()
        if self.cap:
            self.cap.release()
            self.cap = None
        cv2.destroyAllWindows()

    def release(self):
        self.stop()


# 辅助函数：验证二维码是否为有效的资产编号
def is_valid_asset_code(code):
    """验证资产编号格式（可自定义规则）"""
    if not code or len(code.strip()) == 0:
        return False
    # 简单规则：至少4个字符，只含字母数字和连字符
    import re
    return bool(re.match(r'^[A-Za-z0-9_-]{4,32}$', code.strip()))


if __name__ == '__main__':
    # 测试：尝试打开摄像头扫描
    scanner = QrScanner(use_camera=True)
    print("请将二维码放入摄像头前（30秒超时）...")
    result = scanner.scan_once(timeout=30)
    if result:
        print(f"扫描成功: {result}")
    else:
        print("未扫描到二维码")
    scanner.release()
