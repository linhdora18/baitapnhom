-- file: init_sqlite.sql  (scentstation - task package 1)
-- mục đích: schema sqlite dùng cho backend tham chiếu và bộ kiểm thử tự động
--           (unittest). phản chiếu cấu trúc của init_postgres.sql.
--
-- khác biệt so với postgresql:
--   * uuid lưu dạng text, json lưu dạng text, boolean là integer 0/1,
--     thời gian là text (iso 8601).
--   * sqlite không có row-level security. cách ly giữa các brand được thực thi
--     ở tầng ứng dụng bằng lớp tenantrepo (scentstation/db.py), luôn lọc theo
--     tenant_id.
--
-- nội dung: cùng các bảng như bản postgresql (tenants, permissions, roles,
--   role_permissions, users, locations, user_roles, products, sessions,
--   login_attempts, platform_settings, audit_logs), khóa ngoại kép
--   (tenant_id, id), và trigger giữ audit_logs ở dạng chỉ ghi thêm.
-- dùng create ... if not exists nên có thể chạy lại nhiều lần an toàn.
pragma foreign_keys = on;

create table if not exists tenants (
  id text primary key,
  code text not null unique,
  name text not null,
  status text not null default 'active' check (status in ('active','suspended')),
  logo_url text, description text, contact_email text, contact_phone text,
  kiosk_display text not null default '{}',
  created_at text not null, updated_at text not null
);
create table if not exists permissions (
  code text primary key, level text not null check (level in ('platform','tenant')), description text not null
);
create table if not exists roles (
  id text primary key,
  tenant_id text references tenants(id) on delete cascade,
  code text not null, name text not null,
  level text not null check (level in ('platform','tenant')),
  is_system integer not null default 0,
  created_at text not null,
  check ((tenant_id is null) = (is_system = 1))
);
create unique index if not exists roles_code_uq on roles (ifnull(tenant_id,''), code);
create table if not exists role_permissions (
  role_id text not null references roles(id) on delete cascade,
  permission_code text not null references permissions(code),
  primary key (role_id, permission_code)
);
create table if not exists users (
  id text primary key,
  tenant_id text references tenants(id) on delete cascade,
  email text not null, password_hash text not null, full_name text not null,
  status text not null default 'active' check (status in ('active','inactive','locked')),
  failed_attempts integer not null default 0, locked_until text,
  must_change_password integer not null default 0,
  created_at text not null, updated_at text not null,
  unique (tenant_id, id)
);
create unique index if not exists users_email_uq on users (lower(email));
create table if not exists locations (
  id text primary key,
  tenant_id text not null references tenants(id) on delete cascade,
  name text not null, address text, city text, latitude real, longitude real,
  status text not null default 'active' check (status in ('active','inactive')),
  created_at text not null, updated_at text not null,
  unique (tenant_id, id)
);
create table if not exists user_roles (
  id text primary key,
  tenant_id text references tenants(id) on delete cascade,
  user_id text not null,
  role_id text not null references roles(id) on delete cascade,
  scope_type text not null default 'tenant' check (scope_type in ('tenant','location','machine')),
  scope_id text,
  created_at text not null,
  check ((scope_type = 'tenant') = (scope_id is null)),
  foreign key (tenant_id, user_id) references users (tenant_id, id) on delete cascade
);
create unique index if not exists user_roles_uq on user_roles (user_id, role_id, scope_type, ifnull(scope_id,''));
create table if not exists products (
  id text primary key,
  tenant_id text not null references tenants(id) on delete cascade,
  name text not null, description text, image_url text,
  fragrance_notes text not null default '{"top":[],"middle":[],"base":[]}',
  price_per_spray integer not null check (price_per_spray >= 0),
  currency text not null default 'vnd',
  is_available integer not null default 1,
  created_at text not null, updated_at text not null,
  unique (tenant_id, id), unique (tenant_id, name)
);
create table if not exists sessions (
  id text primary key,
  user_id text not null references users(id) on delete cascade,
  tenant_id text references tenants(id) on delete cascade,
  family_id text not null,
  refresh_hash text not null unique,
  expires_at text not null, revoked_at text, revoke_reason text,
  ip text, user_agent text, created_at text not null
);
create table if not exists login_attempts (
  id integer primary key autoincrement,
  email text not null, user_id text, tenant_id text,
  success integer not null, reason text, ip text, created_at text not null
);
create table if not exists platform_settings (
  key text primary key, value text not null, updated_by text, updated_at text not null
);
create table if not exists audit_logs (
  id integer primary key autoincrement,
  tenant_id text, actor_id text, actor_email text,
  action text not null, target_type text, target_id text,
  before_data text, after_data text,
  severity text not null default 'info' check (severity in ('info','warning','critical')),
  source_ip text, created_at text not null
);
create index if not exists audit_tenant_time_idx on audit_logs (tenant_id, created_at);
create trigger if not exists audit_no_update before update on audit_logs
  begin select raise(abort, 'audit_logs is append-only'); end;
create trigger if not exists audit_no_delete before delete on audit_logs
  begin select raise(abort, 'audit_logs is append-only'); end;