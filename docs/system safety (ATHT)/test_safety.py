import unittest
from safety import *


def make(now=1000.0):
    m = Machine("M1", last_heartbeat=now, slots={"S1": Slot("S1", "chanel-1")})
    return SafetyController(m), now


def order(**kw):
    d = dict(order_id="O1", paid_verified=True, already_sprayed=False, slot_id="S1", perfume_id="chanel-1")
    d.update(kw); return Order(**d)


def cmd(now, cid="C1", **kw):
    d = dict(command_id=cid, expires_at=now + 30, signature_valid=True)
    d.update(kw); return Command(**d)


class T(unittest.TestCase):
    def test_ok(self):
        c, n = make(); self.assertTrue(c.check_spray_allowed(order(), cmd(n), n).allowed)

    def test_each_condition_blocks(self):
        cases = {
            "P1": lambda c, n: (order(paid_verified=False), cmd(n)),
            "P2": lambda c, n: (order(already_sprayed=True), cmd(n)),
            "P3": lambda c, n: (order(), cmd(n, signature_valid=False)),
            "P6": lambda c, n: (order(perfume_id="dior-1"), cmd(n)),
        }
        for code, f in cases.items():
            c, n = make(); o, k = f(c, n)
            self.assertIn(code, c.check_spray_allowed(o, k, n).failed)

    def test_expired_and_replayed_command(self):
        c, n = make()
        self.assertIn("P3", c.check_spray_allowed(order(), cmd(n, expires_at=n - 1), n).failed)
        self.assertTrue(c.start_spray(order(), cmd(n), n).allowed)
        c.state = State.READY
        self.assertIn("P3", c.check_spray_allowed(order(), cmd(n), n).failed)  # dùng lại ID lệnh

    def test_machine_conditions(self):
        c, n = make(); c.m.last_heartbeat = n - 120
        self.assertIn("P4", c.check_spray_allowed(order(), cmd(n), n).failed)
        c, n = make(); c.m.door_closed = False
        self.assertIn("P8", c.check_spray_allowed(order(), cmd(n), n).failed)
        c, n = make(); c.m.temp_c = 60
        self.assertIn("P9", c.check_spray_allowed(order(), cmd(n), n).failed)
        c, n = make(); c.m.slots["S1"].stock_ml = 2
        self.assertIn("P7", c.check_spray_allowed(order(), cmd(n), n).failed)
        c, n = make(); c.start_spray(order(), cmd(n), n)
        self.assertIn("P10", c.check_spray_allowed(order("O2"), cmd(n, "C2"), n).failed
                      if False else c.check_spray_allowed(order(order_id="O2"), cmd(n, "C2"), n).failed)

    def test_emergency_blocks_and_reset_rules(self):
        c, n = make(); c.emergency_stop("button", "test", n)
        self.assertIn("P5", c.check_spray_allowed(order(), cmd(n), n).failed)
        self.assertFalse(c.reset_emergency("CU", True, True, n))
        self.assertFalse(c.reset_emergency("TE", False, True, n))
        self.assertFalse(c.reset_emergency("TE", True, False, n))
        self.assertTrue(c.reset_emergency("TE", True, True, n))
        self.assertEqual(c.state, State.READY)

    def test_watchdog_triggers_emergency(self):
        c, n = make(); c.start_spray(order(), cmd(n), n)
        self.assertEqual(c.finish_spray(order(), 5.0, 1.0, n), "WATCHDOG")
        self.assertEqual(c.state, State.EMERGENCY)
        self.assertTrue(c.m.slots["S1"].locked)

    def test_flow_deviation_locks_slot_only(self):
        c, n = make(); c.m.slots["S2"] = Slot("S2", "dior-1")
        for i in range(2):
            c.start_spray(order(order_id=f"O{i}"), cmd(n, f"C{i}"), n)
            r = c.finish_spray(order(), 1.0, 2.0, n)
        self.assertEqual(r, "FLOW_LOCK")
        self.assertTrue(c.m.slots["S1"].locked)
        self.assertFalse(c.m.slots["S2"].locked)
        self.assertIn("P6", c.check_spray_allowed(order(order_id="O9"), cmd(n, "C9"), n).failed)
        self.assertTrue(c.unlock_slot("S1", "TE", n))
        self.assertFalse(c.unlock_slot("S1", "CU", n))

    def test_heartbeat_lost_and_recover(self):
        c, n = make(); c.check_connection(n + 100)
        self.assertEqual(c.state, State.OFFLINE)
        c.heartbeat(n + 101); self.assertEqual(c.state, State.READY)

    def test_maintenance_needs_post_test(self):
        c, n = make(); self.assertTrue(c.enter_maintenance(n))
        self.assertIn("P5", c.check_spray_allowed(order(), cmd(n), n).failed)
        self.assertFalse(c.exit_maintenance(False, n))
        self.assertTrue(c.exit_maintenance(True, n))

    def test_audit_written(self):
        c, n = make(); c.emergency_stop("OM", "x", n)
        self.assertEqual(c.audit[-1][1], "emergency_stop")


if __name__ == "__main__":
    unittest.main()