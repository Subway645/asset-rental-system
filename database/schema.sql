-- ============================================================
-- 办公室资产租借管理系统 - SQLite 数据库结构
-- ============================================================

PRAGMA foreign_keys = ON;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    name TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'user',   -- admin / user
    created_at TEXT DEFAULT (datetime('now', 'localtime'))
);

-- 资产表
CREATE TABLE IF NOT EXISTS assets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    asset_code TEXT UNIQUE NOT NULL,      -- 二维码内容，全局唯一
    category TEXT DEFAULT '未分类',
    status TEXT NOT NULL DEFAULT 'available',  -- available / borrowed / maintenance / repair / scrap
    description TEXT,
    created_at TEXT DEFAULT (datetime('now', 'localtime')),
    updated_at TEXT DEFAULT (datetime('now', 'localtime'))
);

-- 借还记录表
CREATE TABLE IF NOT EXISTS borrow_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    action TEXT NOT NULL,               -- borrow / return
    status TEXT NOT NULL DEFAULT 'pending',  -- pending / confirmed / rejected / timeout
    hw_confirmed INTEGER DEFAULT 0,      -- 硬件确认标志：0未确认 1确认
    hw_timestamp TEXT,                   -- 硬件确认时间（STM32返回）
    operator_id INTEGER,                 -- 操作人ID（谁在PC端操作）
    due_date TEXT,                       -- 应还日期
    returned_at TEXT,                    -- 实际归还时间
    note TEXT,
    created_at TEXT DEFAULT (datetime('now', 'localtime')),
    FOREIGN KEY (asset_id) REFERENCES assets(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);

-- 逾期申报表
CREATE TABLE IF NOT EXISTS overdue_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    borrow_record_id INTEGER NOT NULL,
    report_date TEXT DEFAULT (datetime('now', 'localtime')),
    status TEXT DEFAULT 'open',         -- open / resolved
    resolved_by INTEGER,
    resolved_at TEXT,
    note TEXT,
    FOREIGN KEY (borrow_record_id) REFERENCES borrow_records(id)
);

-- 损坏申报表
CREATE TABLE IF NOT EXISTS damage_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id INTEGER NOT NULL,
    report_user_id INTEGER NOT NULL,
    description TEXT,
    status TEXT DEFAULT 'open',         -- open / assessing / resolved / rejected
    resolution TEXT,
    created_at TEXT DEFAULT (datetime('now', 'localtime')),
    FOREIGN KEY (asset_id) REFERENCES assets(id),
    FOREIGN KEY (report_user_id) REFERENCES users(id)
);

-- 系统日志表（记录关键操作）
CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,           -- in_stock / borrow / return / report / login
    target_type TEXT,                    -- asset / user / borrow_record
    target_id INTEGER,
    user_id INTEGER,
    detail TEXT,
    hw_result TEXT,                      -- OK / NO / TIMEOUT / NONE
    created_at TEXT DEFAULT (datetime('now', 'localtime'))
);

-- ============================================================
-- 初始数据：管理员账号 admin / admin123
-- ============================================================
-- 密码用 Python werkzeug 生成：
-- from werkzeug.security import generate_password_hash
-- print(generate_password_hash('admin123'))
-- 结果: pbkdf2:sha256:600000$... （完整哈希从 init_db.py 中自动生成）
-- 此处先插入一条占位，init_db.py 会替换它
-- INSERT INTO users (username, password_hash, name, role)
-- VALUES ('admin', '{HASH}', '系统管理员', 'admin');
