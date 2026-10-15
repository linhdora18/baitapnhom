# Báo cáo kiểm thử ScentStation — sửa quy ước slot

## Thay đổi

- Chuẩn hóa `Command.slot` thành số slot giao thức **1–5**.
- `Device::on_command()` từ chối slot `0`, số âm và slot lớn hơn số slot cấu hình bằng `BAD_SLOT`.
- Mã chuyển số slot giao thức sang chỉ số mảng nội bộ bằng `idx = slot - 1` trước khi truy cập cấu hình, cảm biến, van và bộ đếm.
- API nội bộ `Device::slot(i)` và `install_bottle(i, ...)` vẫn dùng chỉ số mảng **0–4**.
- Thêm test chứng minh lệnh `slot=5` chỉ bật van nội bộ số 4; cập nhật các test trước đó để tuân theo quy ước này.

## Kết quả kiểm thử C++

Chạy bằng `make clean && make test` trong thư mục dự án.

**Kết quả: 29 tests, 0 failed, 0 failed checks.**

Đã chạy các nhóm test calibration, repeatability, SHA-256/HMAC, chữ ký, kiểm tra lệnh, slot 5, trạng thái an toàn, cooldown, chống replay, khởi động lại/mất điện, cửa/E-STOP, watchdog, no-flow/partial-flow, bảo trì, tồn kho, event buffer và ledger.

## Đối chiếu trace.json

Chạy `python3 validate_trace.py`.

- 22 kịch bản được ghi trong file có `pass: true`.
- 345 frame; mỗi frame có đúng 5 trạng thái slot.
- Kịch bản slot 5 thể hiện van thứ năm hoạt động trong khi bốn van còn lại tắt.

**Giới hạn quan trọng:** `trace.json` là trace đã có sẵn từ dự án được cung cấp. Script trên kiểm tra cấu trúc và dữ liệu được ghi lại, không tái tạo 345 frame bằng cách chạy C++ core. Vì vậy không nên diễn giải kết quả này là toàn bộ trace đã được phát sinh lại từ mã C++ mới.

## Cách chạy lại

```sh
make clean
make test
python3 validate_trace.py
```
