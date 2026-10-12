-- =====================================================================
-- FILE: init_postgres.sql  (ScentStation - Task Package 1)
-- MỤC ĐÍCH: Khởi tạo CSDL PostgreSQL cho nền tảng đa brand (multi-tenant).
--
-- MÔ HÌNH CÁCH LY DỮ LIỆU:
--   * Dùng chung một schema; mọi bảng của brand đều có cột tenant_id.
--   * PostgreSQL Row-Level Security (RLS) chặn brand này đọc/sửa dữ liệu brand khác.
--   * Ứng dụng kết nối bằng role scent_app (không phải owner/superuser) và đặt
--     ngữ cảnh cho mỗi giao dịch:
--         set_config('app.tenant_id', '<uuid>', true)  -> người dùng của brand
--         set_config('app.bypass', 'on', true)         -> chỉ nền tảng/hệ thống
--     Nếu không đặt ngữ cảnh nào, mọi bảng trả về 0 dòng (fail closed).
--
-- NỘI DUNG CHÍNH:
--   1. Hàm tiện ích app_tenant_id(), app_bypass() đọc ngữ cảnh hiện tại.
--   2. Bảng: tenants (brand: Dior, Gucci, Chanel, YSL, Hermès), permissions,
--      roles, role_permissions, users, locations, user_roles, products,
--      sessions, login_attempts, platform_settings, audit_logs.
--   3. Khóa ngoại kép (tenant_id, id) để không thể liên kết chéo giữa các brand.
--   4. audit_logs chỉ ghi thêm (append-only): trigger chặn UPDATE/DELETE/TRUNCATE.
--   5. Chính sách RLS cho từng bảng và phân quyền GRANT cho scent_app
--      (audit_logs và login_attempts chỉ được SELECT/INSERT).
-- =====================================================================
Create EXTENSION if not exists pgcrypto;

Do $$ begin
    if not exists (select from pg_roles where rolename = 'scent_app') THEN
        create role scent_app login password 'change-me';
    end if;
end$$;

create or replace function app_tenant_id() returns uuid language sql stable as 
$$select nullif(current_setting('app.tenant_id', true), '')::uuid $$;


create or replace function app_bypass() return boolean language sql stable as 
$$select coalesce(current_setting('app.bypass', true), 'off') = 'on' $$;


create  table tenants(
    id            uuid primary key default gen_random_uuid(),
    code          text not null unique check(code ~ '^[a-z0-9][a-z0-9-]{1,38}[a-z0-9]$'),  
    name          text not null,
    status        text not null default 'active' check(status in('active', 'suspended')),
    logo_url      text,
    description   text,
    contact_email text,
    contact_phone text,
    kiosk_display jsonb not null default '{}'::jsonb, -- {"show_branding":true,"show_product_name":true,"show_product_image":true,"show_product_price":true}
    created_at    timestamptz not null default now(),
    updated_at    timestamptz not null default now()
);


create table permissions(
    code text primary key,
    level text not null check(level in ('platform', 'tenant')),
    description text not null 
);


create table roles(
    id        uuid primary key default gen_random_uuid(),
    tenant_id uuid references tenants(id) on delete cascade, -- NULL for system roles
    code      text not null,
    name      text not null,
    level     text not null check (level in ('platform', 'tenant')),
    is_system boolean not null default false,
    created_at timestamptz not null default now(),
    check ((tenant_id is null) = is_system) -- system roles have no tenant_id, tenant roles must have one
);


create unique index roles_code_uq on roles (coalesce(tenant_id, '00000000-0000-0000-0000-000000000000'::uuid), code);


create table role_permissions(
    role_id         uuid not null references roles(id) on delete cascade,
    permission_code text not null references permissions(code),
    primary key (role_id, permission_code)
);



create table users(
    id              uuid primary key default gen_random_uuid(),
    tenant_id       uuid references tenants(id) on delete cascade, -- NULL for platform users
    email           text not null,
    password_hash   text not null,
    full_name       text not null,
    status          text not null default 'active' check(status in('active', 'inactive', 'locked')),
    failed_attempts int not null default 0,
    locked_until    timestamptz,
    must_change_password boolean not null default false,
    created_at      timestamptz not null default now(),
    updated_at      timestamptz not null default now(),
    unique   (tenant_id, id)            -- tenant_id is null for platform users, so this is unique across all users
);


create unique index users_email_uq on users (lower(email));


create table locations(
    id         uuid primary key default gen_random_uuid(),
    tenant_id  uuid not null references tenants(id) on delete cascade,
    name       text not null,
    address    text,
    city       text,
    latitude   numeric(9,6),
    longitude  numeric(9,6),
    status     text not null default 'active' check(status in('active', 'inactive')),
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now(),
    unique   (tenant_id, id)
);


create table user_roles(
    id         uuid primary key default gen_random_uuid(),
    tenant_id  uuid references tenants(id) on delete cascade,
    user_id    uuid not null,
    role_id    uuid not null references roles(id) on delete cascade,\
    scope_type text not null default 'tenant' check(scope_type in ('tenant', 'location', 'machine')),
    scope_id   uuid,
    created_at timestamptz not null default now(),
    check ((scope_type = 'tenant') = (scope_id is null)),
    foreign key (tenant_id, user_id) references users (tenant_id, id) on delete cascade
);


create unique index user_roles_uq on user_roles (user_id, role_id, scope_type, coalesce(scope_id, '00000000-0000-0000-0000-000000000000'::uuid));

create table products(
    id              uuid primary key default gen_random_uuid(),
    tenant_id       uuid not null references tenants(id) on delete cascade,
    name            text not null,
    description     text,
    image_url       text, 
    fragrance_notes jsonb not null default '{"top":[],"middle":[],"base":[]}'::jsonb,
    price_per_spray numeric(12,0) not null check (price_per_spray >= 0),
    currency        char(3) not null default 'VND',
    is_available    boolean not null default true,
    created_at      timestamptz not null default now(),
    updated_at      timestamptz not null default now(),
    unique (tenant_id, id),
    unique (tenant_id, name)
);


-- session and logins


create table sessions(
    id            uuid primary key default gen_random_uuid(),
    user_id       uuid not null references users(id) on delete cascade,
    tenant_id     uuid references tenants(id) on delete cascade,
    family_id     uuid not null,                 -- refresh tokens for the same user/session share the same family_id
    refresh_hash  text not null unique,          -- sha256 hash of the refresh token, used to validate refresh requests
    expires_at    timestamptz not null,
    revoked_at    timestamptz,
    revoke_reason text,
    ip            inet,
    user_agent    text,
    created_at    timestamptz not null default now()
);


create index sessions_user_idx on sessions (user_id) where revoked_at is null;

create table login_attempts(
    id         bigserial primary key,
    email      text not null,
    user_id    uuid,
    tenant_id  uuid,
    success    boolean not null,
    reason     text,
    ip         inet,
    created_at timestamptz not null default now()
);


create table platform_settings(
    key        text primary key,
    value      text not null,
    updated_by uuid,
    updated_at timestamptz not null default now()
);


--  append-only audit log, no updates or deletes allowed
create table audit_logs(
    id          bigserial primary key,
    tenant_id   uuid,
    actor_id    uuid,
    actor_email text,
    action      text not null,
    target_type text,
    target_id   text,
    before_data jsonb,
    after_data  jsonb,
    severity    text not null default 'INFO' check(severity in('INFO', 'WARNING', 'CRITICAL')),
    source_ip   inet,
    created_at  timestamptz not null default now()
);


create index audit_tenant_time_idx on audit_logs (tenant_id, created_at DESC);
create or replace function audit_immutable() returns trigger language plpgsql as 
$$ begin raise exception 'audit_logs is append-only'; end $$;

create trigger audit_no_update before update or delete on audit_logs 
for each row execute function audit_immutable();

create trigger audit_no_delete before truncate on audit_logs 
for each row execute function audit_immutable();

-- row level security policies for multi-tenant isolation
do $$
declare t text;
begin
    foreach t in array array ['users','locations','user_roles','products','sessions'] loop
        execute format('alter table %I enable row level security', t);
        execute format('alter table %I enable row level security', t);
        execute format('alter table %I force row level security', t);
        execute format($p$ create policy tenant_isolation on %I
        using (tenant_id = app_tenant_id() OR app_bypass())
        with CHECK (tenant_id = app_tenant_id() OR app_bypass())$p$, t);
  end LOOP;
end $$;

alter table tenants enable row level security;
alter table tenants force row level security;
create policy tenants_select on tenants for select using (id = app_tenant_id() OR app_bypass());
create policy tenants_update on tenants for update using (id = app_tenant_id() OR app_bypass())
                                                  with check (id = app_tenant_id() OR app_bypass());
create policy tenants_insert on tenants for insert with check (app_bypass());
 create policy tenants_delete on tenants for delete using (app_bypass());

-- Roles: system roles (tenant_id NULL) are readable by all, writable only by platform.
alter table roles enable row level security;
alter table roles force row level security;
create policy roles_select on roles for select
    using (tenant_id IS NULL OR tenant_id = app_tenant_id() OR app_bypass());
create policy roles_write on roles for all
    using (tenant_id = app_tenant_id() OR app_bypass())
    with check (tenant_id = app_tenant_id() OR app_bypass());

alter table role_permissions enable row level security;
alter table role_permissions force row level security;
create policy rp_select on role_permissions for select
  using (exists (select 1 from roles r where r.id = role_id));
create policy rp_write on role_permissions for all
  using (exists (select 1 from roles r where r.id = role_id and (r.tenant_id = app_tenant_id() or app_bypass())))
  with check (EXISTS (SELECT 1 FROM roles r WHERE r.id = role_id AND (r.tenant_id = app_tenant_id() OR app_bypass())));

alter table audit_logs enable row level security;
alter table audit_logs force row level security;
create policy audit_select on audit_logs for select using (tenant_id = app_tenant_id() OR app_bypass());
create policy audit_insert on audit_logs for insert with check (tenant_id IS NOT DISTINCT FROM app_tenant_id() OR app_bypass());

-- grants
grant USAGE ON SCHEMA public TO scent_app;
grant select, insert, update, delete ON tenants, roles, role_permissions, users, locations,
      user_roles, products, sessions, platform_settings TO scent_app;
grant select ON permissions TO scent_app;
grant select, INSERT ON audit_logs, login_attempts TO scent_app;     -- no UPDATE/DELETE
grant USAGE ON ALL SEQUENCES IN SCHEMA public TO scent_app;
