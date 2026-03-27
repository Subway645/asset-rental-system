"""
数据库初始化脚本
运行一次即可：python init_db.py
"""
import sqlite3
import os
import sys

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from werkzeug.security import generate_password_hash

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, 'pc', 'app.db')
SCHEMA_PATH = os.path.join(BASE_DIR, 'database', 'schema.sql')


def init_database():
    # 读取 schema
    with open(SCHEMA_PATH, 'r', encoding='utf-8') as f:
        schema = f.read()

    # 删除旧数据库（如果存在）
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)
        print(f"[INFO] 已删除旧数据库: {DB_PATH}")

    # 创建新数据库并执行 schema
    conn = sqlite3.connect(DB_PATH)
    conn.executescript(schema)
    conn.commit()
    print(f"[INFO] 数据库结构创建完成: {DB_PATH}")

    # 插入初始用户
    admin_hash = generate_password_hash('admin123')
    user1_hash = generate_password_hash('user123')
    user2_hash = generate_password_hash('user123')

    users = [
        ('admin', admin_hash, '系统管理员', 'admin'),
        ('zhangsan', user1_hash, '张三', 'user'),
        ('lisi', user2_hash, '李四', 'user'),
    ]

    cursor = conn.cursor()
    cursor.executemany(
        'INSERT INTO users (username, password_hash, name, role) VALUES (?, ?, ?, ?)',
        users
    )

    # 插入示例资产数据
    assets = [
        ('联想ThinkPad笔记本', 'ASSET001', '笔记本电脑', 'available', '2024年采购，性能良好'),
        ('小米投影仪', 'ASSET002', '投影设备', 'available', '小米型号MD-201，含遥控器'),
        ('索尼A7相机', 'ASSET003', '摄影器材', 'available', '含两节电池，一个镜头'),
        ('罗技MX Master 3鼠标', 'ASSET004', '外设', 'borrowed', '无线蓝牙鼠标'),
        ('大疆OM4云台', 'ASSET005', '摄影器材', 'available', '手机云台稳定器'),
        ('iPad Air平板', 'ASSET006', '平板电脑', 'maintenance', '屏幕有轻微划痕'),
        ('华为MateBook笔记本', 'ASSET007', '笔记本电脑', 'available', '14寸，2025年采购'),
        ('小米电动工具箱', 'ASSET008', '工具', 'available', '含螺丝刀套装、万用表'),
    ]

    cursor.executemany(
        'INSERT INTO assets (name, asset_code, category, status, description) VALUES (?, ?, ?, ?, ?)',
        assets
    )

    conn.commit()
    print(f"[INFO] 初始数据插入完成：{len(users)} 个用户，{len(assets)} 条资产记录")
    conn.close()
    print("[SUCCESS] 数据库初始化完成！")


if __name__ == '__main__':
    init_database()
