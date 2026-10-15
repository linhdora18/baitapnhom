-- =====================================================================
-- SCENTSTATION - PHẦN 5: XÁC THỰC, RBAC VÀ KIỂM THỬ (5 SLOTS / 5 BRANDS)
-- Hệ thống Kiosk Nước Hoa Đa Thuê Bao (Multi-Tenant)
-- Brands: Chanel (Slot 1), YSL (Slot 2), Dior (Slot 3), Gucci (Slot 4), Hermès (Slot 5)
-- =====================================================================

PRAGMA foreign_keys = ON;

-- 1. BẢNG TENANTS (Quản lý các thương hiệu nước hoa trên Kiosk)
CREATE TABLE IF NOT EXISTS tenants (
    id TEXT PRIMARY KEY,
    code TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    status TEXT CHECK(status IN ('active', 'suspended', 'inactive')) DEFAULT 'active',
    logo_url TEXT,
    description TEXT,
    kiosk_display TEXT, -- JSON cấu hình vị trí Slot trên Kiosk (ví dụ: {"slot_position": 1})
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. BẢNG ROLES (Phân quyền người dùng - RBAC)
CREATE TABLE IF NOT EXISTS roles (
    id TEXT PRIMARY KEY,
    tenant_id TEXT,
    code TEXT NOT NULL,
    name TEXT NOT NULL,
    level TEXT CHECK(level IN ('system', 'tenant')) NOT NULL,
    is_system INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 3. BẢNG USERS (Tài khoản người dùng)
CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    tenant_id TEXT,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    full_name TEXT NOT NULL,
    status TEXT CHECK(status IN ('active', 'inactive', 'locked')) DEFAULT 'active',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL
);

-- 4. BẢNG USER_ROLES (Gán vai trò cho người dùng)
CREATE TABLE IF NOT EXISTS user_roles (
    user_id TEXT NOT NULL,
    role_id TEXT NOT NULL,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE
);

-- 5. BẢNG AUDIT_LOGS (Nhật ký kiểm toán hệ thống - Append-only)
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id TEXT,
    user_id TEXT,
    action TEXT NOT NULL,
    target_table TEXT NOT NULL,
    record_id TEXT,
    old_values TEXT,
    new_values TEXT,
    ip_address TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- ---------------------------------------------------------------------
-- TRIGGER BẢO VỆ NHẬT KÝ AUDIT LOGS (Chống sửa/xóa nhật ký)
-- ---------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS prevent_audit_log_update
BEFORE UPDATE ON audit_logs
BEGIN
    SELECT RAISE(FAIL, 'Lỗi bảo mật: Không được phép chỉnh sửa dữ liệu Audit Log!');
END;

CREATE TRIGGER IF NOT EXISTS prevent_audit_log_delete
BEFORE DELETE ON audit_logs
BEGIN
    SELECT RAISE(FAIL, 'Lỗi bảo mật: Không được phép xóa dữ liệu Audit Log!');
END;

-- =====================================================================
-- NẠP DỮ LIỆU MẪU: 5 BRAND NƯỚC HOA TƯƠNG ỨNG 5 SLOTS KIOSK
-- =====================================================================

-- Thêm 5 Brand (Slot 1 đến Slot 5)
INSERT OR REPLACE INTO tenants (id, code, name, status, logo_url, description, kiosk_display) 
VALUES 
('tenant_chanel', 'CHANEL', 'Chanel Paris', 'active', 'https://example.com/logos/chanel.png', 'Thương hiệu nước hoa Chanel', '{"slot_position": 1}'),
('tenant_ysl',    'YSL',    'Yves Saint Laurent', 'active', 'https://example.com/logos/ysl.png', 'Thương hiệu nước hoa YSL', '{"slot_position": 2}'),
('tenant_dior',   'DIOR',   'Christian Dior', 'active', 'https://example.com/logos/dior.png', 'Thương hiệu nước hoa Dior', '{"slot_position": 3}'),
('tenant_gucci',  'GUCCI',  'Gucci', 'active', 'https://example.com/logos/gucci.png', 'Thương hiệu nước hoa Gucci', '{"slot_position": 4}'),
('tenant_hermes', 'HERMES', 'Hermès', 'active', 'https://example.com/logos/hermes.png', 'Thương hiệu nước hoa Hermès', '{"slot_position": 5}');

-- Thêm Vai trò Tenant Admin cho 5 Brand
INSERT OR REPLACE INTO roles (id, tenant_id, code, name, level, is_system)
VALUES 
('role_admin_chanel', 'tenant_chanel', 'TENANT_ADMIN', 'Quản trị viên Chanel', 'tenant', 0),
('role_admin_ysl',    'tenant_ysl',    'TENANT_ADMIN', 'Quản trị viên YSL', 'tenant', 0),
('role_admin_dior',   'tenant_dior',   'TENANT_ADMIN', 'Quản trị viên Dior', 'tenant', 0),
('role_admin_gucci',  'tenant_gucci',  'TENANT_ADMIN', 'Quản trị viên Gucci', 'tenant', 0),
('role_admin_hermes', 'tenant_hermes', 'TENANT_ADMIN', 'Quản trị viên Hermès', 'tenant', 0);

-- Thêm tài khoản Quản lý cho từng Brand
INSERT OR REPLACE INTO users (id, tenant_id, email, password_hash, full_name, status)
VALUES 
('usr_chanel', 'tenant_chanel', 'admin@chanel.com', '$2b$10$abcdefghijklmnopqrstuvwxyz123456789012345678901234567', 'Chanel Manager', 'active'),
('usr_ysl',    'tenant_ysl',    'admin@ysl.com',    '$2b$10$abcdefghijklmnopqrstuvwxyz123456789012345678901234567', 'YSL Manager', 'active'),
('usr_dior',   'tenant_dior',   'admin@dior.com',   '$2b$10$abcdefghijklmnopqrstuvwxyz123456789012345678901234567', 'Dior Manager', 'active'),
('usr_gucci',  'tenant_gucci',  'admin@gucci.com',  '$2b$10$abcdefghijklmnopqrstuvwxyz123456789012345678901234567', 'Gucci Manager', 'active'),
('usr_hermes', 'tenant_hermes', 'admin@hermes.com', '$2b$10$abcdefghijklmnopqrstuvwxyz123456789012345678901234567', 'Hermes Manager', 'active');