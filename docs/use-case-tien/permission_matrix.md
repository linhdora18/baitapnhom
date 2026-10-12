# MA TRẬN PHÂN QUYỀN (vai trò x chức năng)
Để phục vụ cho việc vẽ Use Case Diagram ==> giúp vẽ đúng chức năng của từng actor

## Ký hiệu

| Ký hiệu | Ý nghĩa |
|---|---|
| **V** | Xem (View) |
| **C** | Tạo (Create) |
| **U** | Sửa / cập nhật (Update) |
| **D** | Vô hiệu hóa / xóa mềm (Deactivate / Delete) |
| **X** | Thực hiện một hành động (Execute) |
| **+A** | Cần xác thực bổ sung (UC-04) |
| **-** | Không có quyền |

Viết tắt vai trò: **SA** = Platform Super Administrator, **BA** = Brand Administrator, **OM** = Operations Manager, **TE** = Technician, **IS** = Inventory Staff, **CU** = Customer / Kiosk User

## Nguyên tắc chung

1. Mọi quyền của BA, OM, TE, IS bị giới hạn trong tenant (brand) của người dùng và trong phạm vi địa điểm / máy được giao. Không ai truy cập được dữ liệu của brand khác
2. Chỉ SA có báo cáo và giám sát toàn nền tảng (mục 15 trong đề tài)
3. CU không có tài khoản, chỉ tương tác với máy đang dùng
4. Mọi thao tác ghi (C, U, D, X) trong bảng dưới đây đều ghi vào audit log (mục 16 trong đề tài)
5. Quyền có thể được BA chỉnh theo vai trò, địa điểm, máy (mục 7 trong đề tài). Bảng dưới là quyền mặc định

## 1. Nền tảng và tài khoản

| Chức năng | UC | SA | BA | OM | TE | IS | CU |
|---|---|---|---|---|---|---|---|
| Quản lý tenant (brand) | UC-05 | C U D V | - | - | - | - | - |
| Quản lý tài khoản Brand Admin | UC-06 | C U D V | - | - | - | - | - |
| Cấu hình nền tảng (thanh toán, chính sách, thông báo) | UC-07 | C U V | - | - | - | - | - |
| Quản lý người dùng nhân sự trong brand | UC-12 | - | C U D V | - | - | - | - |
| Cấu hình vai trò và phân quyền trong brand | UC-12 | - | U V | - | - | - | - |
| Thu hồi quyền người dùng / thiết bị | UC-10 | X | X (trong brand) | - | - | - | - |
| Đăng nhập, đăng xuất, đặt lại mật khẩu | UC-01..03 | X | X | X | X | X | - |
| Xem và tìm audit log | UC-09 | V (toàn nền tảng) | V (trong brand) | - | - | - | - |

## 2. Brand, sản phẩm, máy

| Chức năng | UC | SA | BA | OM | TE | IS | CU |
|---|---|---|---|---|---|---|---|
| Thông tin brand, nội dung hiển thị kiosk | UC-11 | V | U V | - | - | - | - |
| Sản phẩm nước hoa | UC-13 | V | C U D V | V | - | V | V (xem trên kiosk) |
| Địa điểm và gán máy | UC-14 | V | C U V | V | V (được giao) | V | - |
| Đăng ký máy mới (serial, định danh thiết bị) | - | C U V | - | - | - | - | - |
| Cấu hình slot (sản phẩm, giá, ngưỡng, liều phun) | UC-15 | V | U +A | V | - | - | - |
| Khuyến mãi / giá tạm | UC-16 | - | C U D V | V | - | - | - |
| Giám sát toàn bộ máy | UC-08 | V | - | - | - | - | - |
| Giám sát máy trong brand | UC-30 | - | V | V | V (được giao) | V | - |
| Bật / tắt máy hoặc slot | UC-31 | - | X | X | - | - | - |

## 3. Giao dịch và vận hành

| Chức năng | UC | SA | BA | OM | TE | IS | CU |
|---|---|---|---|---|---|---|---|
| Xem danh sách mùi hương và thông tin chi tiết | UC-20 | - | - | - | - | - | V |
| Tạo order và thanh toán QR | UC-21, 22 | - | - | - | - | - | X |
| Nhận một lần phun | UC-23 | - | - | - | - | - | X |
| Xử lý khi đã trả tiền nhưng phun lỗi (phía khách: nhận hướng dẫn) | UC-24 | - | - | - | - | - | V |
| Quét QR sản phẩm | UC-25 | - | - | - | - | - | X |
| Xem và tìm giao dịch | UC-32 | V (thống kê) | V | V | - | - | - |
| Xử lý giao dịch lỗi (retry có kiểm soát / review) | UC-33 | - | - | X | - | - | - |
| Hoàn tiền | UC-34 | - | - | X +A | - | - | - |
| Cảnh báo (xem, xác nhận, giao việc, đóng, mở lại) | UC-35 | V | V | V X | V (được giao) | V | - |
| Báo cáo, dashboard vận hành | UC-37, 17 | V (toàn nền tảng) | V X (xuất) | V | - | V (tồn kho) | - |

## 4. Bảo trì

| Chức năng | UC | SA | BA | OM | TE | IS | CU |
|---|---|---|---|---|---|---|---|
| Tạo, giao, đặt ưu tiên ticket bảo trì | UC-36 | - | V | C U V | V (được giao) | - | - |
| Xem máy, cảnh báo, ticket được giao | UC-40 | - | - | V | V (được giao) | - | - |
| Bật chế độ bảo trì | UC-41 | - | - | - | X (được giao) | - | - |
| Chẩn đoán máy / slot | UC-42 | - | - | V | X | - | - |
| Phun thử (test spray) | UC-43 | - | - | - | X +A | - | - |
| Thực hiện bảo trì theo checklist | UC-44 | - | - | V | C U | - | - |
| Kiểm tra sau bảo trì | UC-45 | - | - | V | X | - | - |
| Đóng ticket bảo trì | UC-46 | - | - | V | X (sau khi ghi kết quả) | - | - |
| Xem lịch sử bảo trì và lỗi | UC-47 | - | V | V | V | - | - |

## 5. Tồn kho

| Chức năng | UC | SA | BA | OM | TE | IS | CU |
|---|---|---|---|---|---|---|---|
| Lô nước hoa | UC-50 | - | V | V | - | C U V | - |
| Đăng ký chai / cartridge | UC-51 | - | V | V | - | C U V | - |
| Lắp / thay chai vào slot | UC-52, 57 | - | V | V | - | X | - |
| Đối soát tồn kho ước tính với cảm biến | UC-53 | - | V | V | - | X | - |
| Điều chỉnh tồn kho (bắt buộc có lý do) | UC-54 | - | - | - | - | X +A | - |
| Lịch sử và báo cáo tồn kho | UC-55 | - | V | V | - | V X (xuất) | - |
| Nhận cảnh báo tồn kho | UC-56 | - | V | V | - | V | - |

## Use case do hệ thống bên ngoài kích hoạt (không phân quyền theo vai trò)

Các use case sau do Payment Provider hoặc IoT Device thực hiện, không thuộc ma trận vai trò. Chúng được bảo vệ bằng xác thực hệ thống (chữ ký webhook, định danh và thu hồi thiết bị) thay vì RBAC:

| UC | Tên | Actor | Cơ chế bảo vệ |
|---|---|---|---|
| UC-26 | Xác minh webhook thanh toán | PP | Kiểm tra chữ ký, mã tham chiếu, số tiền, chống xử lý trùng |
| UC-27 | Gửi lệnh phun và nhận kết quả | DV | Lệnh có chữ ký, hạn dùng, định danh duy nhất; thiết bị từ chối lệnh không hợp lệ |
| UC-38 | Nhận heartbeat và telemetry | DV | Chỉ nhận từ thiết bị đã đăng ký, chưa bị thu hồi |
| UC-39 | Tạo cảnh báo và ticket tự động | Hệ thống | Chạy theo quy tắc cấu hình, tránh tạo cảnh báo trùng |
