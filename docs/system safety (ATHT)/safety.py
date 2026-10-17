# Module an toàn hệ thống ScentStation

from __future__ import annotations
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class State(Enum):
    READY = "SẴN SÀNG"
    SPRAYING = "ĐANG PHUN"
    OFFLINE = "NGOẠI TUYẾN"
    MAINTENANCE = "BẢO TRÌ"
    LOCKED = "KHÓA"
    EMERGENCY = "DỪNG KHẨN CẤP"


class Severity(Enum):
    LOW = 1
    MEDIUM = 2
    HIGH = 3
    CRITICAL = 4


@dataclass
class SafetyConfig:
    heartbeat_timeout_s: float = 60.0
    max_spray_s: float = 3.0
    flow_tolerance: float = 0.30      # ±30% so với liều cấu hình
    min_stock_ml: float = 5.0
    max_temp_c: float = 45.0
    min_temp_c: float = 0.0
    flow_fail_limit: int = 2          # lệch 2 lần liên tiếp thì khóa slot


@dataclass
class Order:
    order_id: str
    paid_verified: bool          # đã được backend xác minh (UC-26)
    already_sprayed: bool
    slot_id: str
    perfume_id: str


@dataclass
class Command:
    command_id: str
    expires_at: float
    signature_valid: bool


@dataclass
class Slot:
    slot_id: str
    perfume_id: str
    active: bool = True
    stock_ml: float = 100.0
    dose_ml: float = 1.0
    locked: bool = False
    flow_fail_count: int = 0


@dataclass
class Machine:
    machine_id: str
    registered: bool = True
    revoked: bool = False
    last_heartbeat: float = 0.0
    door_closed: bool = True
    temp_c: float = 25.0
    hw_fault: bool = False
    slots: dict = field(default_factory=dict)


@dataclass
class Decision:
    allowed: bool
    failed: list  # danh sách mã điều kiện không đạt (P1..P10)


class SafetyController:
    def __init__(self, machine: Machine, config: Optional[SafetyConfig] = None):
        self.m = machine
        self.cfg = config or SafetyConfig()
        self.state = State.READY
        self.maintenance = False
        self.used_command_ids: set = set()
        self.audit: list = []   # (thời gian, hành động, chi tiết)

    # ghi log
    def _log(self, now, action, **detail):
        self.audit.append((now, action, detail))

    # 1. Điều kiện cho phép phun 
    def check_spray_allowed(self, order: Order, cmd: Command, now: float) -> Decision:
        m, cfg = self.m, self.cfg
        slot: Optional[Slot] = m.slots.get(order.slot_id)
        f = []
        if not order.paid_verified: f.append("P1")
        if order.already_sprayed: f.append("P2")
        if (not cmd.signature_valid or now > cmd.expires_at
                or cmd.command_id in self.used_command_ids): f.append("P3")
        if (not m.registered or m.revoked
                or now - m.last_heartbeat > cfg.heartbeat_timeout_s): f.append("P4")
        if self.maintenance or self.state in (State.LOCKED, State.EMERGENCY,
                                              State.MAINTENANCE, State.OFFLINE):
            f.append("P5")
        if slot is None or not slot.active or slot.locked or slot.perfume_id != order.perfume_id:
            f.append("P6")
        if slot is None or slot.stock_ml < max(cfg.min_stock_ml, slot.dose_ml):
            f.append("P7")
        if not m.door_closed: f.append("P8")
        if m.hw_fault or not (cfg.min_temp_c <= m.temp_c <= cfg.max_temp_c): f.append("P9")
        if self.state == State.SPRAYING: f.append("P10")
        d = Decision(not f, f)
        self._log(now, "check_spray", order=order.order_id, allowed=d.allowed, failed=f)
        return d

    # 2. Phun có giới hạn 
    def start_spray(self, order: Order, cmd: Command, now: float) -> Decision:
        d = self.check_spray_allowed(order, cmd, now)
        if d.allowed:
            self.used_command_ids.add(cmd.command_id)   # mỗi lệnh chỉ dùng một lần
            self.state = State.SPRAYING
        return d

    def finish_spray(self, order: Order, duration_s: float, measured_ml: float, now: float) -> str:
        """Trả về: 'OK', 'WATCHDOG', 'FLOW_LOCK' hoặc 'FLOW_WARN'. Không bao giờ tự phun lại."""
        slot = self.m.slots[order.slot_id]
        result = "OK"
        if duration_s > self.cfg.max_spray_s:        # watchdog: quá thời gian
            self.report_fault("SPRAY_OVERTIME", Severity.CRITICAL, now)
            return "WATCHDOG"
        if abs(measured_ml - slot.dose_ml) > self.cfg.flow_tolerance * slot.dose_ml:
            slot.flow_fail_count += 1
            result = "FLOW_WARN"
            if slot.flow_fail_count >= self.cfg.flow_fail_limit:
                self.report_fault("FLOW_DEVIATION", Severity.HIGH, now, slot_id=slot.slot_id)
                result = "FLOW_LOCK"
        else:
            slot.flow_fail_count = 0
        slot.stock_ml -= measured_ml
        if self.state == State.SPRAYING:
            self.state = State.READY
        self._log(now, "finish_spray", order=order.order_id, result=result)
        return result

    #  3. Dừng khẩn cấp 
    def emergency_stop(self, source: str, reason: str, now: float):
        """source: 'button' | 'OM' | 'SA' | 'auto'. Ưu tiên cao nhất, luôn thành công."""
        self.state = State.EMERGENCY
        self.maintenance = False
        self._log(now, "emergency_stop", source=source, reason=reason)

    def reset_emergency(self, role: str, extra_auth: bool, post_test_passed: bool, now: float) -> bool:
        """Chỉ TE/OM, có xác thực bổ sung và đã qua kiểm tra sau bảo trì (UC-45)."""
        ok = role in ("TE", "OM") and extra_auth and post_test_passed
        self._log(now, "reset_emergency", role=role, ok=ok)
        if ok:
            self.state = State.READY
        return ok

    # 4. Khóa khi có lỗi 
    def report_fault(self, code: str, severity: Severity, now: float, slot_id: Optional[str] = None):
        self._log(now, "fault", code=code, severity=severity.name, slot=slot_id)
        if severity == Severity.CRITICAL:
            self.emergency_stop("auto", code, now)
            for s in self.m.slots.values():
                s.locked = True
        elif severity == Severity.HIGH:
            if slot_id and slot_id in self.m.slots:
                self.m.slots[slot_id].locked = True
            else:
                self.state = State.LOCKED
        elif severity == Severity.MEDIUM:
            if self.state == State.READY:
                self.state = State.OFFLINE

    def unlock_slot(self, slot_id: str, role: str, now: float) -> bool:
        ok = role in ("TE", "IS", "OM")
        self._log(now, "unlock_slot", slot=slot_id, role=role, ok=ok)
        if ok:
            s = self.m.slots[slot_id]
            s.locked, s.flow_fail_count = False, 0
        return ok

    def heartbeat(self, now: float):
        """Có lại heartbeat thì tự thoát NGOẠI TUYẾN (lỗi mức Trung bình tự phục hồi)."""
        self.m.last_heartbeat = now
        if self.state == State.OFFLINE:
            self.state = State.READY

    def check_connection(self, now: float):
        if self.state == State.READY and now - self.m.last_heartbeat > self.cfg.heartbeat_timeout_s:
            self.report_fault("HEARTBEAT_LOST", Severity.MEDIUM, now)

    #  bảo trì 
    def enter_maintenance(self, now: float) -> bool:
        if self.state in (State.READY, State.OFFLINE):
            self.maintenance = True
            self.state = State.MAINTENANCE
            self._log(now, "maintenance_on")
            return True
        return False

    def exit_maintenance(self, post_test_passed: bool, now: float) -> bool:
        if self.state == State.MAINTENANCE and post_test_passed:    # UC-45
            self.maintenance = False
            self.state = State.READY
            self._log(now, "maintenance_off")
            return True
        return False