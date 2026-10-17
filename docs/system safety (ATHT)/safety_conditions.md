# AN TOÀN HỆ THỐNG: điều kiện phun, dừng khẩn cấp, khóa khi lỗi

## 1. Điều kiện cho phép phun (interlock)

Backend chỉ tạo lệnh phun khi tất cả điều kiện bên trái đúng, máy kiểm tra lại các điều kiện phía máy trước khi phun (phòng thủ hai lớp)

| # | Điều kiện | Kiểm tra ở | Không đạt thì |
|---|---|---|---|
| P1 | Order ở trạng thái "đã thanh toán", được backend xác minh (UC-26) | Backend | Không tạo lệnh |
| P2 | Order chưa có lệnh phun nào (tối đa 1 lần phun / order) | Backend | Không tạo lệnh |
| P3 | Lệnh còn hạn, chữ ký hợp lệ, ID lệnh chưa dùng | Máy | Máy từ chối, báo lỗi |
| P4 | Máy online, đã đăng ký, chưa bị thu hồi; heartbeat mới nhất trong 60 giây | Backend | Không tạo lệnh |
| P5 | Máy không ở chế độ bảo trì, không ở trạng thái KHÓA hoặc DỪNG KHẨN CẤP | Backend + Máy | Không tạo lệnh |
| P6 | Slot được chọn đang active, đúng mùi của order (UC-57) | Backend | Không tạo lệnh |
| P7 | Tồn kho slot trên ngưỡng tối thiểu (không phun khi sắp cạn) | Backend + Máy | Chặn slot, cảnh báo |
| P8 | Cửa / nắp máy đóng (cảm biến cửa) | Máy | Không phun |
| P9 | Không có lỗi phần cứng đang mở: bơm, van, cảm biến, nhiệt độ trong khoảng cho phép | Máy | Không phun |
| P10 | Máy không đang phun lệnh khác (một lệnh tại một thời điểm) | Máy | Hàng đợi hoặc từ chối |

## 2. Giới hạn trong lúc phun

- Liều cố định theo cấu hình slot, thời gian phun tối đa (mặc định 3 giây). Quá thời gian thì máy tự ngắt (watchdog)
- Chỉ một lần phun cho mỗi lệnh, không tự phun lại khi mất ack hoặc mất kết nối. Kết quả không chắc chắn thì chuyển UC-24
- Mất mạng giữa chừng: máy hoàn tất hoặc ngắt theo giới hạn đã nhận, lưu kết quả cục bộ và gửi lại khi có mạng
- Khối lượng / lưu lượng đo được lệch quá ±30% liều cấu hình thì ghi lỗi và khóa slot

## 3. Dừng khẩn cấp (Emergency Stop)

| Nguồn kích hoạt | Ai / cái gì |
|---|---|
| Nút dừng khẩn cấp vật lý trên máy | Người dùng, nhân sự tại chỗ |
| Lệnh dừng từ xa (một máy, một địa điểm, hoặc toàn brand) | Operations Manager; Super Admin (toàn nền tảng) |
| Tự động | Máy hoặc backend khi phát hiện lỗi mức Nghiêm trọng (mục 4 trong đề tài) |

Khi kích hoạt:
1. Máy sẽ ngắt ngay bơm và van, hủy lệnh đang chạy và hàng đợi
2. Máy chuyển sang trạng thái DỪNG KHẨN CẤP, kiosk hiển thị thông báo, không nhận order mới
3. Order đã thanh toán nhưng chưa phun được đưa vào UC-24 / UC-33 (review hoặc hoàn tiền); không tự phun lại
4. Tạo cảnh báo Nghiêm trọng và ticket bảo trì, ghi audit log (ai, khi nào, lý do)
5. Dừng khẩn cấp vật lý ưu tiên cao nhất, hoạt động kể cả khi mất mạng

**Thoát khỏi DỪNG KHẨN CẤP:** chỉ nhân sự có quyền (Technician được giao hoặc OM) sau khi xử lý nguyên nhân, xác thực bổ sung (UC-04), chạy kiểm tra sau bảo trì (UC-45) đạt. Khách hàng và việc đăng nhập đơn thuần không thể mở lại

## 4. Khóa cơ cấu khi phát hiện lỗi

| Mức | Ví dụ lỗi | Hành động tự động | Ai mở khóa |
|---|---|---|---|
| **Nghiêm trọng** | Phun không dừng / quá liều; rò rỉ; quá nhiệt; hỏng cảm biến an toàn; lệnh phun trùng ngoài ý muốn | Dừng khẩn cấp cả máy, khóa mọi slot | TE + xác thực bổ sung + UC-45 đạt |
| **Cao** | Bơm / van của một slot lỗi; lưu lượng lệch quá ngưỡng 2 lần liên tiếp; chai sai mùi | Khóa **slot đó**, máy vẫn bán slot khác | TE hoặc IS (tùy lỗi) sau khi xử lý |
| **Trung bình** | Mất heartbeat > 60 giây; mất kết nối; tồn kho dưới ngưỡng | Máy ngừng nhận order mới (hoặc chặn slot) | Tự phục hồi khi hết lỗi, hoặc OM xác nhận |
| **Thấp** | Cảnh báo sắp hết, nhiệt độ gần ngưỡng | Chỉ cảnh báo | Tự đóng khi hết điều kiện |

Quy tắc khóa:
- Khóa là mặc định an toàn (fail-safe): nếu không chắc chắn về trạng thái máy thì không phun
- Khóa không tự mở với lỗi mức Nghiêm trọng và Cao; không xóa lịch sử lỗi
- Phun thử chẩn đoán khi bảo trì (UC-43) được phép trong lúc khóa nhưng chỉ do TE thực hiện, cần xác thực bổ sung, và ghi riêng (không giảm tồn kho bán hàng)

## 5. Trạng thái máy (đầu vào cho phần máy trạng thái)

| Trạng thái | Nhận order | Phun | Vào bằng | Ra bằng |
|---|---|---|---|---|
| SẴN SÀNG | Có | Có | Qua UC-45, hoặc lỗi tự phục hồi | Mọi trạng thái dưới |
| ĐANG PHUN | Không | 1 lệnh | Lệnh phun hợp lệ | Hoàn tất / lỗi / dừng khẩn cấp |
| NGOẠI TUYẾN | Không | Không | Mất heartbeat | Có lại heartbeat |
| BẢO TRÌ | Không | Chỉ phun thử | UC-41 | UC-45 đạt |
| KHÓA (slot hoặc máy) | Chặn phần bị khóa | Không (phần bị khóa) | Lỗi mức Cao / Nghiêm trọng | Mở khóa theo mục 4 |
| DỪNG KHẨN CẤP | Không | Không | Nút / lệnh / lỗi nghiêm trọng | Mục 3 |

