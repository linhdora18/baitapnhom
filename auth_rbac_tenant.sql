=====================================================================
-- PHẦN 5: THIẾT KẾ LUỒNG ĐĂNG NHẬP, XÁC THỰC, PHÂN QUYỀN CHI TIẾT 
-- VÀ QUẢN TRỊ THƯƠNG HIỆU, RÀ SOÁT VÀ KIỂM THỬ SQL/QUYỀN TENANT (SQLite)
-- =====================================================================

/*
====================================================================
5.1. Thiết kế luồng đăng nhập và xác thực (Authentication Flow)
- Quản lý tài khoản (users): Lưu định danh, mật khẩu băm, trạng thái, 
  kiểm soát số lần sai (failed_attempts), khóa tạm (locked_until).
- Lịch sử đăng nhập (login_attempts): Ghi nhận vết xác thực (thành công/thất bại, IP).
- Phiên làm việc (sessions): Quản lý refresh token (family_id, refresh_hash), 
  thời hạn, và thông tin thiết bị.

5.2. Thiết kế phân quyền chi tiết (RBAC & Scopes)
- permissions: Danh mục quyền hệ thống.
- roles: Vai trò hệ thống hoặc vai trò theo từng brand.
- role_permissions: Ánh xạ quyền cho vai trò.
- user_roles: Gán vai trò cho người dùng theo phạm vi (tenant, location, machine).

5.3. Quản trị thương hiệu (Tenant Administration)
- tenants: Quản lý thông tin brand, trạng thái, cấu hình giao diện kiosk dạng text/JSON.

5.4. Kiểm thử SQL / Quyền Tenant trên SQLite
- Cách ly dữ liệu thương hiệu: Thực thi ở tầng ứng dụng qua tenantrepo (do SQLite không hỗ trợ RLS).
- Khóa ngoại kép (tenant_id, id) ngăn chặn liên kết chéo dữ liệu.
- audit_logs: Thiết lập cơ chế chỉ ghi thêm (append-only) bằng SQLite Trigger (UPDATE/DELETE).
====================================================================
*/

PRAGMA foreign_keys = ON;

-- 1. Bảng tenants (Quản lý đối tác thương hiệu)
CREATE TABLE IF NOT EXISTS tenants (
  id TEXT PRIMARY KEY,
  code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active','suspended')),
  logo_url TEXT, description TEXT, contact_email TEXT, contact_phone TEXT,
  kiosk_display TEXT NOT NULL DEFAULT '{}',
  created_at TEXT NOT NULL, updated_at TEXT NOT NULL
);

-- 2. Bảng permissions (Danh mục quyền hạn)
CREATE TABLE IF NOT EXISTS permissions (
  code TEXT PRIMARY KEY, 
  level TEXT NOT NULL CHECK (level IN ('platform','tenant')), 
  description TEXT NOT NULL
);

-- 3. Bảng roles (Quản lý vai trò)
CREATE TABLE IF NOT EXISTS roles (
  id TEXT PRIMARY KEY,
  tenant_id TEXT REFERENCES tenants(id) ON DELETE CASCADE,
  code TEXT NOT NULL, name TEXT NOT NULL,
  level TEXT NOT NULL CHECK (level IN ('platform','tenant')),
  is_system INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL,
  CHECK ((tenant_id IS NULL) = (is_system = 1))
);
CREATE UNIQUE INDEX IF NOT EXISTS roles_code_uq ON roles (IFNULL(tenant_id,''), code);

-- 4. Bảng role_permissions (Phân quyền chi tiết cho vai trò)
CREATE TABLE IF NOT EXISTS role_permissions (
  role_id TEXT NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
  permission_code TEXT NOT NULL REFERENCES permissions(code),
  PRIMARY KEY (role_id, permission_code)
);

-- 5. Bảng users (Quản lý tài khoản người dùng)
CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  tenant_id TEXT REFERENCES tenants(id) ON DELETE CASCADE,
  email TEXT NOT NULL, password_hash TEXT NOT NULL, full_name TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active','inactive','locked')),
  failed_attempts INTEGER NOT NULL DEFAULT 0, locked_until TEXT,
  must_change_password INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL, updated_at TEXT NOT NULL,
  UNIQUE (tenant_id, id)
);
CREATE UNIQUE INDEX IF NOT EXISTS users_email_uq ON users (LOWER(email));

-- 6. Bảng sessions (Quản lý phiên làm việc)
CREATE TABLE IF NOT EXISTS sessions (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  tenant_id TEXT REFERENCES tenants(id) ON DELETE CASCADE,
  family_id TEXT NOT NULL,
  refresh_hash TEXT NOT NULL UNIQUE,
  expires_at TEXT NOT NULL, revoked_at TEXT, revoke_reason TEXT,
  ip TEXT, user_agent TEXT, created_at TEXT NOT NULL
);

-- 7. Bảng login_attempts (Lịch sử đăng nhập)
CREATE TABLE IF NOT EXISTS login_attempts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  email TEXT NOT NULL, user_id TEXT, tenant_id TEXT,
  success INTEGER NOT NULL, reason TEXT, ip TEXT, created_at TEXT NOT NULL
);

-- 8. Bảng audit_logs (Nhật ký kiểm toán)
CREATE TABLE IF NOT EXISTS audit_logs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  tenant_id TEXT, actor_id TEXT, actor_email TEXT,
  action TEXT NOT NULL, target_type TEXT, target_id TEXT,
  before_data TEXT, after_data TEXT,
  severity TEXT NOT NULL DEFAULT 'info' CHECK (severity IN ('info','warning','critical')),
  source_ip TEXT, created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS audit_tenant_time_idx ON audit_logs (tenant_id, created_at);

-- Trigger bảo vệ audit_logs (Chỉ cho phép ghi thêm, cấm Sửa/Xóa trên SQLite)
CREATE TRIGGER IF NOT EXISTS audit_no_update BEFORE UPDATE ON audit_logs
  BEGIN SELECT RAISE(ABORT, 'audit_logs is append-only'); END;

CREATE TRIGGER IF NOT EXISTS audit_no_delete BEFORE DELETE ON audit_logs
  BEGIN SELECT RAISE(ABORT, 'audit_logs is append-only'); END;

