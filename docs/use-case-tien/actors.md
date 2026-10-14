# XÁC ĐỊNH CÁC TÁC NHÂN (ACTORS)


## 1. Theo đề tài ScentStation, ta có 6 tác nhân (Actor) chính:

| Actor | Vai trò | Quyền hạn tiêu biểu |
|---|---|---|
| Platform Super Administrator | Quản trị toàn nền tảng | Quản lý thương hiệu, tài khoản quản trị thương hiệu máy, thống kê toàn hệ thống và audit log
| Brand Administrator | Quản trị một thương hiệu | Quản lý người dùng, sản phẩm, máy, khe máy, giá và báo cáo trong thương hiệu của mình
| Operations Manager | Quản lý vận hành | Theo dõi máy, xử lý giao dịch lỗi, quản lý cảnh báo và phiếu bảo trì
| Technician | Kỹ thuật viên | Kiểm tra máy, chạy chẩn đoán, phun thử có kiểm soát và ghi nhận sửa chữa
| Inventory Staff | Nhân viên kho | Quản lý chai nước hoa, lắp/thay chai, ghi nhận nạp thêm và điều chỉnh tồn kho
| Customer / Kiosk User | Khách hàng |	Xem mùi hương, chọn sản phẩm, thanh toán QR và theo dõi kết quả phun

## 2. Actor phụ (hệ thống bên ngoài)

| Actor | Vai trò trong hệ thống | Căn cứ trong đề |
|---|---|---|
| **Payment Provider** | Gửi thông báo (webhook) thanh toán; nguồn đối soát giao dịch | (Mục 9 trong đề tài) |
| **IoT Device (Kiosk)** | Gửi heartbeat và telemetry, nhận lệnh phun, xác nhận (ack) và trả kết quả | (Mục 10, 11 trong đề ) |

