-- =====================================================================
-- FILE: test.sql  (ScentStation - kiểm thử Row-Level Security)
-- MỤC ĐÍCH: Chứng minh chính sách RLS của PostgreSQL cách ly dữ liệu giữa
--           các brand đúng như thiết kế.
-- CÁCH CHẠY (chạy init_postgres.sql trước):
--   psql -v ON_ERROR_STOP=1 -f init_postgres.sql
--   psql -v ON_ERROR_STOP=1 -f test.sql
-- KẾT QUẢ: mỗi kiểm tra sai sẽ RAISE EXCEPTION; đúng hết thì in
--          "ALL RLS CHECKS PASSED". Toàn bộ chạy trong BEGIN...ROLLBACK
--          nên không để lại dữ liệu.
--
-- DỮ LIỆU MẪU: 2 brand (Dior, Gucci), mỗi brand có 1 user và 1 sản phẩm;
--   tạo bằng quyền superuser (bỏ qua RLS) rồi chuyển sang role scent_app.
-- CÁC NHÓM KIỂM TRA:
--   1. Không có ngữ cảnh -> không thấy dòng nào (fail closed).
--   2. Brand Dior chỉ thấy dữ liệu của Dior.
--   3. Dior không UPDATE/DELETE được dữ liệu của Gucci.
--   4. Dior không INSERT được dòng thuộc Gucci.
--   5. Dior không tạo được tenant mới, không chuyển sản phẩm sang brand khác.
--   6. Khóa ngoại kép chặn user_roles trỏ chéo brand (kể cả khi bypass).
--   7. audit_logs chỉ ghi thêm, không sửa/xóa được.
--   8. Brand Gucci chỉ thấy dữ liệu của mình; nhật ký của brand khác không lộ.
--   9. Chế độ bypass (nền tảng) thấy tất cả.
--  10. tenant_id sai định dạng gây lỗi, không bao giờ lộ toàn bộ dữ liệu.
-- =====================================================================
\set on_error_stop on
begin;
-- fixtures created as superuser (bypasses rls)
insert into tenants(id,code,name) values
 ('aaaaaaaa-0000-0000-0000-000000000001','dior','dior'),
 ('bbbbbbbb-0000-0000-0000-000000000002','gucci','gucci');
insert into users(id,tenant_id,email,password_hash,full_name) values
 ('a1000000-0000-0000-0000-000000000001','aaaaaaaa-0000-0000-0000-000000000001','admin@dior.vn','x','dior admin'),
 ('b1000000-0000-0000-0000-000000000002','bbbbbbbb-0000-0000-0000-000000000002','admin@gucci.vn','x','gucci admin');
insert into products(tenant_id,name,price_per_spray) values
 ('aaaaaaaa-0000-0000-0000-000000000001','dior rose',10000),
 ('bbbbbbbb-0000-0000-0000-000000000002','gucci oud',20000);
insert into audit_logs(tenant_id,action) values ('aaaaaaaa-0000-0000-0000-000000000001','t');

set local role scent_app;

do $$
declare n int;
begin
  -- 1. no context => fail closed
  select count(*) into n from products;            if n <> 0 then raise exception 'fail 1: products visible without context (%)', n; end if;
  select count(*) into n from users;               if n <> 0 then raise exception 'fail 1b: users visible without context'; end if;

  -- 2. tenant a context sees only a
  perform set_config('app.tenant_id','aaaaaaaa-0000-0000-0000-000000000001', true);
  select count(*) into n from products;            if n <> 1 then raise exception 'fail 2: a sees % products', n; end if;
  select count(*) into n from products where name = 'gucci oud';  if n <> 0 then raise exception 'fail 2b: a reads b product by name'; end if;
  select count(*) into n from tenants;             if n <> 1 then raise exception 'fail 2c: a sees % tenants', n; end if;
  select count(*) into n from users;               if n <> 1 then raise exception 'fail 2d: a sees % users', n; end if;

  -- 3. a cannot update / delete b rows
  update products set price_per_spray = 1 where name = 'gucci oud';
  get diagnostics n = row_count;                   if n <> 0 then raise exception 'fail 3: a updated b product'; end if;
  delete from users where id = 'b1000000-0000-0000-0000-000000000002';
  get diagnostics n = row_count;                   if n <> 0 then raise exception 'fail 3b: a deleted b user'; end if;

  -- 4. a cannot insert a row for b
  begin
    insert into products(tenant_id,name,price_per_spray) values ('bbbbbbbb-0000-0000-0000-000000000002','evil',1);
    raise exception 'fail 4: cross-tenant insert allowed';
  exception when insufficient_privilege then null; end;

  -- 5. a cannot create tenants nor move a row to another tenant
  begin
    insert into tenants(code,name) values ('brand-evil','evil');
    raise exception 'fail 5: brand user created tenant';
  exception when insufficient_privilege then null; end;
  begin
    update products set tenant_id = 'bbbbbbbb-0000-0000-0000-000000000002' where name = 'dior rose';
    raise exception 'fail 5b: moved product to another tenant';
  exception when insufficient_privilege then null; end;

  -- 6. composite fk: even with bypass, a tenant-a user_roles row cannot point to a tenant-b user
  perform set_config('app.tenant_id','', true);
  perform set_config('app.bypass','on', true);
  insert into roles(id,code,name,level,is_system) values ('c0000000-0000-0000-0000-000000000009','t_role','t','tenant',true);
  begin
    insert into user_roles(tenant_id,user_id,role_id)
      values ('aaaaaaaa-0000-0000-0000-000000000001','b1000000-0000-0000-0000-000000000002','c0000000-0000-0000-0000-000000000009');
    raise exception 'fail 6: cross-tenant user_roles accepted';
  exception when foreign_key_violation then null; end;
  perform set_config('app.bypass','off', true);
  perform set_config('app.tenant_id','aaaaaaaa-0000-0000-0000-000000000001', true);

  -- 7. audit log is append-only
  begin
    update audit_logs set action = 'hack';
    raise exception 'fail 7: audit update allowed';
  exception when insufficient_privilege or raise_exception then
    if sqlerrm like 'fail%' then raise; end if;
  end;
  begin
    delete from audit_logs;
    raise exception 'fail 7b: audit delete allowed';
  exception when insufficient_privilege or raise_exception then
    if sqlerrm like 'fail%' then raise; end if;
  end;

  -- 8. tenant b context sees only b; audit rows isolated
  perform set_config('app.tenant_id','bbbbbbbb-0000-0000-0000-000000000002', true);
  select count(*) into n from audit_logs;          if n <> 0 then raise exception 'fail 8: b sees a audit rows'; end if;
  select count(*) into n from products;            if n <> 1 then raise exception 'fail 8b: b sees % products', n; end if;

  -- 9. bypass (platform) sees everything
  perform set_config('app.tenant_id','', true);
  perform set_config('app.bypass','on', true);
  select count(*) into n from products;            if n <> 2 then raise exception 'fail 9: bypass sees % products', n; end if;
  select count(*) into n from tenants;             if n <> 2 then raise exception 'fail 9b: bypass sees % tenants', n; end if;

  -- 10. malformed tenant id must not leak (cast error, never "all rows")
  perform set_config('app.bypass','off', true);
  perform set_config('app.tenant_id','not-a-uuid', true);
  begin
    perform count(*) from products;
    raise exception 'fail 10: malformed tenant id accepted';
  exception when invalid_text_representation then null; end;

  raise notice 'all rls checks passed';
end $$;
rollback;