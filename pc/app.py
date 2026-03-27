"""
Flask 应用入口
资产租借管理与可视化运营系统

启动方式：
  cd C:\\Users\\Subway\\Desktop\\赛题
  python -m pip install -r requirements.txt
  python init_db.py          # 首次运行初始化数据库
  python -c "from pc.app import init_serial; init_serial()"
  python pc/app.py           # 或 python app.py（从赛题根目录）

访问地址：http://127.0.0.1:5000
默认管理员：admin / admin123
"""
import os
import sys
import sqlite3
import datetime
import time
import logging
import threading
from functools import wraps

from flask import (
    Flask, request, session, redirect, url_for, render_template,
    flash, jsonify, abort
)
from flask_sqlalchemy import SQLAlchemy
from flask_login import (
    LoginManager, UserMixin, login_user, logout_user,
    login_required, current_user
)
from werkzeug.security import generate_password_hash, check_password_hash

# ============================================================
# 应用初始化
# ============================================================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, 'pc', 'app.db')

app = Flask(__name__, template_folder='templates', static_folder='static')
app.config['SECRET_KEY'] = 'asset-management-secret-key-2026'
app.config['SQLALCHEMY_DATABASE_URI'] = f'sqlite:///{DB_PATH}'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['PERMANENT_SESSION_LIFETIME'] = datetime.timedelta(hours=8)

db = SQLAlchemy(app)
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'
login_manager.login_message = '请先登录'

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)


# ============================================================
# 模板上下文处理器（所有页面共享）
# ============================================================
@app.context_processor
def inject_global_vars():
    with app.app_context():
        open_overdues = OverdueReport.query.filter_by(status='open').count()
        open_damages = DamageReport.query.filter_by(status='open').count()
        return dict(
            open_overdue_count=open_overdues,
            open_damage_count=open_damages,
            now_str=datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        )

# ============================================================
# 数据库模型
# ============================================================
class User(UserMixin, db.Model):
    __tablename__ = 'users'
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password_hash = db.Column(db.String(200), nullable=False)
    name = db.Column(db.String(100), nullable=False)
    role = db.Column(db.String(20), default='user')  # admin / user
    created_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    def set_password(self, pwd):
        self.password_hash = generate_password_hash(pwd)

    def check_password(self, pwd):
        return check_password_hash(self.password_hash, pwd)


class Asset(db.Model):
    __tablename__ = 'assets'
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(200), nullable=False)
    asset_code = db.Column(db.String(50), unique=True, nullable=False)
    category = db.Column(db.String(50), default='未分类')
    status = db.Column(db.String(20), default='available')
    description = db.Column(db.Text)
    created_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    updated_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))


class BorrowRecord(db.Model):
    __tablename__ = 'borrow_records'
    id = db.Column(db.Integer, primary_key=True)
    asset_id = db.Column(db.Integer, db.ForeignKey('assets.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    action = db.Column(db.String(20), nullable=False)  # borrow / return
    status = db.Column(db.String(20), default='pending')  # pending / confirmed / rejected / timeout
    hw_confirmed = db.Column(db.Integer, default=0)
    hw_timestamp = db.Column(db.String(20))
    operator_id = db.Column(db.Integer, db.ForeignKey('users.id'))
    due_date = db.Column(db.String(30))
    returned_at = db.Column(db.String(30))
    note = db.Column(db.Text)
    created_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    asset = db.relationship('Asset', backref='borrow_records')
    user = db.relationship('User', foreign_keys=[user_id], backref='borrow_records')


class OverdueReport(db.Model):
    __tablename__ = 'overdue_reports'
    id = db.Column(db.Integer, primary_key=True)
    borrow_record_id = db.Column(db.Integer, db.ForeignKey('borrow_records.id'), nullable=False)
    report_date = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    status = db.Column(db.String(20), default='open')  # open / resolved
    resolved_by = db.Column(db.Integer, db.ForeignKey('users.id'))
    resolved_at = db.Column(db.String(30))
    note = db.Column(db.Text)
    borrow_record = db.relationship('BorrowRecord', backref='overdue_reports')


class DamageReport(db.Model):
    __tablename__ = 'damage_reports'
    id = db.Column(db.Integer, primary_key=True)
    asset_id = db.Column(db.Integer, db.ForeignKey('assets.id'), nullable=False)
    report_user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    description = db.Column(db.Text)
    status = db.Column(db.String(20), default='open')
    resolution = db.Column(db.Text)
    created_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    asset = db.relationship('Asset', backref='damage_reports')
    report_user = db.relationship('User', backref='damage_reports')


class SystemLog(db.Model):
    __tablename__ = 'system_logs'
    id = db.Column(db.Integer, primary_key=True)
    event_type = db.Column(db.String(30), nullable=False)
    target_type = db.Column(db.String(20))
    target_id = db.Column(db.Integer)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'))
    detail = db.Column(db.Text)
    hw_result = db.Column(db.String(20))
    created_at = db.Column(db.String(30), default=lambda: datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))


# ============================================================
# Flask-Login 回调
# ============================================================
@login_manager.user_loader
def load_user(user_id):
    return db.session.get(User, int(user_id))


# ============================================================
# 辅助函数
# ============================================================
def get_serial():
    """获取全局串口实例（懒加载）"""
    from pc.hardware.serial_comm import get_serial_comm
    return get_serial_comm()


def is_admin():
    return current_user.is_authenticated and current_user.role == 'admin'


def admin_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if not is_admin():
            flash('需要管理员权限', 'warning')
            return redirect(url_for('index'))
        return f(*args, **kwargs)
    return decorated


def log_event(event_type, target_type, target_id, detail='', hw_result='NONE'):
    log = SystemLog(
        event_type=event_type,
        target_type=target_type,
        target_id=target_id,
        user_id=current_user.id if current_user.is_authenticated else None,
        detail=detail,
        hw_result=hw_result
    )
    db.session.add(log)
    db.session.commit()


def get_asset_by_code(code):
    return Asset.query.filter_by(asset_code=code.strip()).first()


def format_dt():
    return datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def calculate_due_date(days=7):
    return (datetime.datetime.now() + datetime.timedelta(days=days)).strftime('%Y-%m-%d %H:%M:%S')


# ============================================================
# 路由定义
# ============================================================

@app.route('/')
@login_required
def index():
    return redirect(url_for('asset_list'))


@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        user = User.query.filter_by(username=username).first()
        if user and user.check_password(password):
            login_user(user, remember=True)
            log_event('login', 'user', user.id, f'用户 {user.name} 登录')
            next_page = request.args.get('next')
            return redirect(next_page or url_for('asset_list'))
        flash('用户名或密码错误', 'danger')
    return render_template('index.html')


@app.route('/logout')
@login_required
def logout():
    log_event('logout', 'user', current_user.id, f'用户 {current_user.name} 登出')
    logout_user()
    return redirect(url_for('login'))


# ---------- 资产管理 ----------
@app.route('/assets')
@login_required
def asset_list():
    status_filter = request.args.get('status', '')
    category_filter = request.args.get('category', '')
    search = request.args.get('search', '').strip()

    query = Asset.query
    if status_filter:
        query = query.filter_by(status=status_filter)
    if category_filter:
        query = query.filter_by(category=category_filter)
    if search:
        query = query.filter(
            db.or_(
                Asset.name.contains(search),
                Asset.asset_code.contains(search)
            )
        )
    assets = query.order_by(Asset.created_at.desc()).all()
    categories = [c[0] for c in db.session.query(Asset.category).distinct().all()]
    return render_template('asset_list.html', assets=assets, categories=categories,
                           status_filter=status_filter, search=search)


@app.route('/assets/new', methods=['GET', 'POST'])
@login_required
@admin_required
def asset_new():
    if request.method == 'POST':
        name = request.form.get('name', '').strip()
        asset_code = request.form.get('asset_code', '').strip()
        category = request.form.get('category', '未分类').strip()
        description = request.form.get('description', '').strip()

        if not name or not asset_code:
            flash('名称和资产编号不能为空', 'danger')
            return render_template('asset_form.html', asset=None)

        if get_asset_by_code(asset_code):
            flash(f'资产编号 {asset_code} 已存在', 'danger')
            return render_template('asset_form.html', asset=None)

        asset = Asset(name=name, asset_code=asset_code,
                      category=category, description=description, status='available')
        db.session.add(asset)
        db.session.commit()
        log_event('in_stock', 'asset', asset.id, f'新增资产: {name}')
        flash(f'资产 [{name}] 入库成功', 'success')
        return redirect(url_for('asset_list'))

    return render_template('asset_form.html', asset=None)


@app.route('/assets/<int:asset_id>/edit', methods=['GET', 'POST'])
@login_required
@admin_required
def asset_edit(asset_id):
    asset = db.session.get(Asset, asset_id)
    if not asset:
        abort(404)
    if request.method == 'POST':
        asset.name = request.form.get('name', '').strip()
        asset.category = request.form.get('category', '').strip()
        asset.description = request.form.get('description', '').strip()
        new_status = request.form.get('status', '')
        if new_status:
            asset.status = new_status
        asset.updated_at = format_dt()
        db.session.commit()
        log_event('update', 'asset', asset.id, f'更新资产: {asset.name}')
        flash(f'资产 [{asset.name}] 已更新', 'success')
        return redirect(url_for('asset_list'))
    return render_template('asset_form.html', asset=asset)


@app.route('/assets/<int:asset_id>/delete', methods=['POST'])
@login_required
@admin_required
def asset_delete(asset_id):
    asset = db.session.get(Asset, asset_id)
    if not asset:
        return jsonify({'success': False, 'message': '资产不存在'})
    if asset.status in ('borrowed', 'maintenance', 'repair'):
        return jsonify({'success': False, 'message': '该资产当前状态不允许删除'})
    db.session.delete(asset)
    db.session.commit()
    log_event('delete', 'asset', asset_id, f'删除资产: {asset.name}')
    return jsonify({'success': True})


# ---------- 入库流程（扫码+硬件确认）----------
@app.route('/assets/stockin', methods=['GET', 'POST'])
@login_required
@admin_required
def asset_stockin():
    """
    入库流程：
    GET: 显示扫码+表单页面
    POST: 处理入库请求，调用硬件确认
    """
    serial = get_serial()
    simulate_mode = serial.is_simulate_mode()

    if request.method == 'POST':
        asset_code = request.form.get('asset_code', '').strip()
        name = request.form.get('name', '').strip()
        category = request.form.get('category', '未分类').strip()
        description = request.form.get('description', '').strip()

        if not asset_code or not name:
            flash('资产编号和名称不能为空', 'danger')
            return render_template('asset_stockin.html', simulate_mode=simulate_mode)

        existing = get_asset_by_code(asset_code)
        if existing:
            flash(f'资产编号 {asset_code} 已存在（{existing.name}）', 'warning')
            return redirect(url_for('asset_list'))

        # 调用硬件确认
        result = serial.send_confirm_request('IN', asset_code)

        # 创建资产记录（硬件确认后才正式入库）
        asset = Asset(name=name, asset_code=asset_code,
                      category=category, description=description, status='available')
        db.session.add(asset)
        db.session.commit()

        hw_result = result.get('result', 'ERROR')
        log_event('in_stock', 'asset', asset.id,
                  f'入库确认: {name}', hw_result=hw_result)

        if hw_result == 'OK':
            flash(f'资产 [{name}] 入库成功（硬件已确认）', 'success')
        elif hw_result == 'TIMEOUT':
            asset.status = 'maintenance'  # 超时暂存维护区
            db.session.commit()
            flash(f'资产 [{name}] 已暂存（硬件确认超时）', 'warning')
        else:
            flash(f'资产 [{name}] 硬件确认被取消', 'warning')

        return redirect(url_for('asset_list'))

    return render_template('asset_stockin.html', simulate_mode=simulate_mode)


# ---------- 借出流程 ----------
@app.route('/borrow')
@login_required
def borrow_page():
    serial = get_serial()
    simulate_mode = serial.is_simulate_mode()
    # 查询当前借用人（所有借出状态的资产）
    borrowed = db.session.query(BorrowRecord, Asset, User).join(Asset).join(User).filter(
        BorrowRecord.action == 'borrow',
        BorrowRecord.status.in_(['pending', 'confirmed'])
    ).all()
    return render_template('borrow.html', borrowed=borrowed, simulate_mode=simulate_mode)


@app.route('/borrow/do', methods=['POST'])
@login_required
def borrow_do():
    asset_code = request.form.get('asset_code', '').strip()
    due_days = int(request.form.get('due_days', 7))
    note = request.form.get('note', '').strip()

    asset = get_asset_by_code(asset_code)
    if not asset:
        return jsonify({'success': False, 'message': f'未找到资产编号: {asset_code}'})

    if asset.status != 'available':
        return jsonify({'success': False, 'message': f'资产状态不可借出（当前: {asset.status}）'})

    serial = get_serial()
    result = serial.send_confirm_request('OUT', asset_code)
    hw_result = result.get('result', 'ERROR')

    record = BorrowRecord(
        asset_id=asset.id, user_id=current_user.id,
        action='borrow', status='confirmed' if hw_result == 'OK' else 'timeout' if hw_result == 'TIMEOUT' else 'rejected',
        hw_confirmed=1 if hw_result == 'OK' else 0,
        hw_timestamp=result.get('timestamp', ''),
        operator_id=current_user.id,
        due_date=calculate_due_date(due_days),
        note=note
    )
    db.session.add(record)

    if hw_result == 'OK':
        asset.status = 'borrowed'
    elif hw_result == 'TIMEOUT':
        asset.status = 'available'  # 超时视为取消，不借出

    db.session.commit()
    log_event('borrow', 'borrow_record', record.id,
              f'借出资产 {asset.name}', hw_result=hw_result)

    if hw_result == 'OK':
        return jsonify({'success': True, 'message': f'[{asset.name}] 借出成功'})
    elif hw_result == 'TIMEOUT':
        return jsonify({'success': False, 'message': '硬件确认超时，借出取消'})
    else:
        return jsonify({'success': False, 'message': '硬件确认被取消'})


# ---------- 归还流程 ----------
@app.route('/return/do', methods=['POST'])
@login_required
def return_do():
    asset_code = request.form.get('asset_code', '').strip()

    asset = get_asset_by_code(asset_code)
    if not asset:
        return jsonify({'success': False, 'message': f'未找到资产编号: {asset_code}'})

    if asset.status not in ('borrowed', 'maintenance', 'repair'):
        return jsonify({'success': False, 'message': f'该资产当前不在借出状态（当前: {asset.status}）'})

    serial = get_serial()
    result = serial.send_confirm_request('RET', asset_code)
    hw_result = result.get('result', 'ERROR')

    # 查找对应借出记录
    record = BorrowRecord.query.filter_by(
        asset_id=asset.id, action='borrow',
        status='confirmed'
    ).order_by(BorrowRecord.created_at.desc()).first()

    if record:
        record.status = 'confirmed'
        record.returned_at = format_dt()
        record.hw_confirmed = 1 if hw_result == 'OK' else 0
        record.hw_timestamp = result.get('timestamp', '')

    ret_record = BorrowRecord(
        asset_id=asset.id, user_id=current_user.id,
        action='return', status='confirmed' if hw_result == 'OK' else 'rejected',
        hw_confirmed=1 if hw_result == 'OK' else 0,
        hw_timestamp=result.get('timestamp', ''),
        operator_id=current_user.id
    )
    db.session.add(ret_record)

    if hw_result == 'OK':
        asset.status = 'available'
    elif hw_result == 'TIMEOUT':
        asset.status = 'available'  # 超时仍归还

    db.session.commit()
    log_event('return', 'borrow_record', ret_record.id,
              f'归还资产 {asset.name}', hw_result=hw_result)

    if hw_result == 'OK':
        return jsonify({'success': True, 'message': f'[{asset.name}] 归还成功'})
    else:
        return jsonify({'success': False, 'message': '硬件确认未通过，归还已记录'})


# ---------- 异常申报 ----------
@app.route('/reports')
@login_required
def reports_page():
    # 逾期申报需要的借出记录（所有 confirmed 状态的）
    all_borrows = db.session.query(BorrowRecord, Asset, User).join(Asset).join(User).filter(
        BorrowRecord.action == 'borrow',
        BorrowRecord.status == 'confirmed'
    ).all()

    # 损坏申报需要的资产列表
    all_assets = Asset.query.filter(Asset.status != 'scrap').order_by(Asset.name).all()

    # 历史申报记录
    overdues = db.session.query(OverdueReport, BorrowRecord, Asset, User).join(
        BorrowRecord).join(Asset).join(User).order_by(
        OverdueReport.report_date.desc()).all()

    damages = db.session.query(DamageReport, Asset, User).join(
        Asset).join(User).order_by(DamageReport.created_at.desc()).all()

    return render_template('reports.html',
                           all_borrows=all_borrows,
                           all_assets=all_assets,
                           overdues=overdues,
                           damages=damages)


@app.route('/reports/overdue', methods=['POST'])
@login_required
def report_overdue():
    borrow_id = request.form.get('borrow_id', type=int)
    note = request.form.get('note', '').strip()

    record = db.session.get(BorrowRecord, borrow_id)
    if not record:
        return jsonify({'success': False, 'message': '借还记录不存在'})

    report = OverdueReport(borrow_record_id=borrow_id, note=note)
    db.session.add(report)
    db.session.commit()
    log_event('report', 'overdue', report.id, f'逾期申报: 借还记录#{borrow_id}')
    return jsonify({'success': True, 'message': '逾期申报已提交'})


@app.route('/reports/damage', methods=['POST'])
@login_required
def report_damage():
    asset_id = request.form.get('asset_id', type=int)
    description = request.form.get('description', '').strip()

    asset = db.session.get(Asset, asset_id)
    if not asset:
        return jsonify({'success': False, 'message': '资产不存在'})

    report = DamageReport(
        asset_id=asset_id, report_user_id=current_user.id,
        description=description
    )
    db.session.add(report)
    # 资产进入维修状态
    asset.status = 'repair'
    db.session.commit()
    log_event('report', 'damage', report.id, f'损坏申报: {asset.name}')
    return jsonify({'success': True, 'message': '损坏申报已提交'})


@app.route('/reports/resolve/<type_>/<int:report_id>', methods=['POST'])
@login_required
@admin_required
def resolve_report(type_, report_id):
    resolution = request.form.get('resolution', '').strip()
    new_status_asset = request.form.get('asset_status', 'available')

    if type_ == 'overdue':
        report = db.session.get(OverdueReport, report_id)
        if report:
            report.status = 'resolved'
            report.resolved_by = current_user.id
            report.resolved_at = format_dt()
            report.resolution = resolution
            db.session.commit()
    elif type_ == 'damage':
        report = db.session.get(DamageReport, report_id)
        if report:
            report.status = 'resolved'
            report.resolution = resolution
            asset = db.session.get(Asset, report.asset_id)
            if asset:
                asset.status = new_status_asset
            db.session.commit()
    else:
        return jsonify({'success': False, 'message': '无效类型'})

    return jsonify({'success': True, 'message': '处理完成'})


# ---------- 可视化看板 ----------
@app.route('/dashboard')
@login_required
def dashboard():
    now = datetime.datetime.now()

    # 1. 资产状态分布
    status_counts = db.session.query(
        Asset.status, db.func.count(Asset.id)
    ).group_by(Asset.status).all()
    status_data = {s: c for s, c in status_counts}

    # 2. 各类别资产数量
    category_counts = db.session.query(
        Asset.category, db.func.count(Asset.id)
    ).group_by(Asset.category).all()

    # 3. 借用频次 Top 资产（按借出次数排序）
    top_assets = db.session.query(
        Asset.name, Asset.asset_code,
        db.func.count(BorrowRecord.id).label('borrow_count')
    ).join(BorrowRecord).filter(
        BorrowRecord.action == 'borrow'
    ).group_by(Asset.id).order_by(
        db.func.count(BorrowRecord.id).desc()
    ).limit(10).all()

    # 4. 逾期统计
    overdue_records = BorrowRecord.query.filter(
        BorrowRecord.action == 'borrow',
        BorrowRecord.status == 'confirmed',
        BorrowRecord.due_date < now.strftime('%Y-%m-%d %H:%M:%S')
    ).all()
    overdue_count = len(overdue_records)

    # 5. 借用趋势（最近30天，每天借出数量）
    from datetime import timedelta
    trend_data = []
    for i in range(29, -1, -1):
        day = (now - timedelta(days=i)).strftime('%Y-%m-%d')
        cnt = BorrowRecord.query.filter(
            BorrowRecord.action == 'borrow',
            BorrowRecord.created_at.startswith(day)
        ).count()
        trend_data.append({'date': day, 'count': cnt})

    # 6. 申报统计
    open_overdues = OverdueReport.query.filter_by(status='open').count()
    open_damages = DamageReport.query.filter_by(status='open').count()

    total_assets = Asset.query.count()
    available_assets = status_data.get('available', 0)
    borrowed_assets = status_data.get('borrowed', 0)

    # 7. 操作日志（最近20条）
    logs = SystemLog.query.order_by(
        SystemLog.created_at.desc()
    ).limit(20).all()

    return render_template('dashboard.html',
                           total_assets=total_assets,
                           available_assets=available_assets,
                           borrowed_assets=borrowed_assets,
                           status_data=status_data,
                           category_counts=category_counts,
                           top_assets=top_assets,
                           overdue_count=overdue_count,
                           trend_data=trend_data,
                           open_overdues=open_overdues,
                           open_damages=open_damages,
                           logs=logs)


# ---------- 硬件状态 API ----------
@app.route('/api/hardware/status')
@login_required
def hw_status():
    serial = get_serial()
    return jsonify({
        'simulate_mode': serial.is_simulate_mode(),
        'port': serial.port if serial.ser else None
    })


# ---------- 用户管理（管理员） ----------
@app.route('/users')
@login_required
@admin_required
def user_list():
    users = User.query.order_by(User.created_at.desc()).all()
    return render_template('user_list.html', users=users)


@app.route('/users/new', methods=['POST'])
@login_required
@admin_required
def user_new():
    username = request.form.get('username', '').strip()
    name = request.form.get('name', '').strip()
    password = request.form.get('password', '')
    role = request.form.get('role', 'user')

    if not username or not name or not password:
        return jsonify({'success': False, 'message': '请填写所有字段'})

    if User.query.filter_by(username=username).first():
        return jsonify({'success': False, 'message': '用户名已存在'})

    user = User(username=username, name=name, role=role)
    user.set_password(password)
    db.session.add(user)
    db.session.commit()
    log_event('user_add', 'user', user.id, f'新增用户: {name}({role})')
    return jsonify({'success': True, 'message': f'用户 [{name}] 创建成功'})


# ---------- 扫码 API ----------
_scan_result_cache = None

@app.route('/api/scan/start', methods=['POST'])
@login_required
def scan_start():
    global _scan_result_cache
    _scan_result_cache = None
    return jsonify({'started': True})

@app.route('/api/scan/result')
@login_required
def scan_result():
    global _scan_result_cache
    return jsonify({'code': _scan_result_cache})

# ---------- 硬件确认率 API ----------
@app.route('/api/hw-confirm-rate')
@login_required
def hw_confirm_rate():
    total = BorrowRecord.query.filter(BorrowRecord.hw_confirmed >= 0).count()
    confirmed = BorrowRecord.query.filter_by(hw_confirmed=1).count()
    rate = round(confirmed / total * 100, 1) if total > 0 else 0
    return jsonify({'rate': rate, 'total': total, 'confirmed': confirmed})

# ============================================================
# 串口初始化（供外部调用）
# ============================================================
def init_serial(port=None):
    """初始化串口连接（无硬件时自动进入模拟模式）"""
    from pc.hardware.serial_comm import get_serial_comm
    comm = get_serial_comm(port)
    if comm.is_simulate_mode():
        print("[WARN] 串口未连接，已启用模拟模式")
    else:
        print(f"[INFO] 串口已连接: {comm.port}")
    return comm


# ============================================================
# 启动
# ============================================================
if __name__ == '__main__':
    # 确保数据库存在
    with app.app_context():
        db.create_all()
    init_serial()
    app.run(host='0.0.0.0', port=5000, debug=True)
