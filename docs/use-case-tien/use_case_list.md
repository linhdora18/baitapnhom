# ĐẶC TẢ CÁC USE CASE
# ĐẶC TẢ CÁC USE CASE (13 use case chi tiết)

## 1. UC-01 Đăng nhập
- Actor: SA, BA, OM, TE, IS
- Tiền điều kiện: Tài khoản đã được tạo và đang hoạt động.
- Hậu điều kiện: Phiên đăng nhập được tạo, gắn với brand và vai trò; lần đăng nhập được ghi log
- Luồng chính:
+ Người dùng nhập email và mật khẩu
+ Hệ thống xác thực thông tin
+ Hệ thống xác định brand và vai trò, tạo phiên
+ Chuyển đến màn hình theo vai trò
- Ngoại lệ: Sai thông tin (thông báo chung, không lộ tài khoản có tồn tại hay không), nhập sai nhiều lần thì khóa tạm; tài khoản bị khóa hoặc brand bị tạm ngưng thì từ chối

## 2. UC-05 Quản lý tenant (brand)
- Actor: SA
- Tiền điều kiện: SA đã đăng nhập
- Hậu điều kiện: Brand được tạo, sửa hoặc đổi trạng thái; ghi audit log
- Luồng chính:
+ SA chọn tạo hoặc sửa brand
+ Nhập thông tin (tên, mã brand, liên hệ)
+ Hệ thống kiểm tra dữ liệu hợp lệ, mã brand không trùng
+ Lưu và xác nhận
- Thay thế: Tạm ngưng brand thì mọi người dùng và máy của brand bị chặn hoạt động, kích hoạt lại để mở
- Quy tắc: Dữ liệu mỗi brand tách biệt hoàn toàn; không xóa cứng brand đã có dữ liệu

## 3. UC-21 Chọn mùi, xác nhận giá và tạo order
- Actor: CU
- Tiền điều kiện: Máy đang hoạt động (không bảo trì), slot còn nước hoa
Hậu điều kiện: Order ở trạng thái "chờ thanh toán", cố định mùi và giá
- Luồng chính:
+ Khách chọn một mùi trên kiosk (include UC-20)
+ Hệ thống hiển thị giá hiện hành
+ Khách xác nhận
+ Hệ thống tạo order, khóa mùi và giá tại thời điểm này
- Ngoại lệ: Slot hết hoặc lỗi thì không cho chọn; máy đang bảo trì thì không tạo order
- Quy tắc: Mỗi order chỉ một lần phun; giá không đổi sau khi order được tạo

## 4. UC-22 Thanh toán bằng QR
- Actor: CU, PP (Payment Provider)
- Tiền điều kiện: Order ở trạng thái "chờ thanh toán"
- Hậu điều kiện: Order chuyển sang "đã thanh toán", "thất bại" hoặc "hết hạn"
- Luồng chính:
+ Hệ thống tạo mã QR động gắn với order
+ Kiosk hiển thị QR và đếm ngược
+ Khách quét và thanh toán
+ Hệ thống nhận kết quả đã xác minh (include UC-26) và cập nhật order
- Ngoại lệ: Hết hạn QR thì order "hết hạn", không phun; thanh toán thất bại thì cho thử lại hoặc hủy
- Quy tắc: Chỉ kết quả từ backend mới được coi là đã thanh toán

## 5. UC-23 Nhận một lần phun
- Actor: CU, DV (IoT Device)
- Tiền điều kiện: Order "đã thanh toán" và chưa phun
- Hậu điều kiện: Phun đúng một lần; ghi kết quả; tồn kho giảm nếu thành công
- Luồng chính:
+ Backend tạo lệnh phun (include UC-27)
+ Máy xác nhận nhận lệnh
+ Máy phun đúng một lần
+ Máy trả kết quả; kiosk thông báo cho khách
- Ngoại lệ: Phun lỗi hoặc không có kết quả thì chuyển UC-24 (extend)
- Quy tắc: Không tự động phun lần thứ hai

## 6. UC-24 Xử lý khi đã trả tiền nhưng phun lỗi
- Actor: CU
- Điều kiện kích hoạt: Thanh toán thành công nhưng phun thất bại hoặc không chắc chắn
- Hậu điều kiện: Giao dịch ở trạng thái "chờ review / hoàn tiền"; cảnh báo cho OM
- Luồng chính:
+ Kiosk thông báo sự cố và mã giao dịch
+ Hệ thống đánh dấu giao dịch cần xử lý
+ Tạo cảnh báo cho Operations Manager (UC-33)
- Quy tắc: Không tự phun lại, không tự hoàn tiền; kết luận do OM quyết định

## 7. UC-26 Xác minh webhook thanh toán
- Actor: PP
- Tiền điều kiện: Có order đang chờ thanh toán
- Hậu điều kiện: Order được cập nhật đúng một lần
- Luồng chính:
+ PP gửi webhook
+ Hệ thống kiểm tra chữ ký
+ Đối chiếu mã tham chiếu, số tiền, tiền tệ, trạng thái với order
+ Cập nhật order và ghi log
- Ngoại lệ: Sai chữ ký hoặc sai số tiền thì từ chối và ghi cảnh báo; webhook trùng thì bỏ qua (idempotent)

## 8. UC-27 Gửi lệnh phun và nhận kết quả
- Actor: DV
- Tiền điều kiện: Order đã thanh toán; máy online và đã đăng ký
- Hậu điều kiện: Lệnh có kết quả: thành công, thất bại hoặc quá hạn
- Luồng chính:
+ Backend tạo lệnh (ID duy nhất, hạn dùng, chữ ký)
+ Máy kiểm tra lệnh hợp lệ và gửi ack
+ Máy thực thi và trả kết quả
+ Backend lưu kết quả
- Ngoại lệ: Không có ack hoặc kết quả trong thời hạn thì đánh dấu "không chắc chắn" và chuyển UC-24; máy từ chối lệnh trùng, hết hạn hoặc sai chữ ký
- Quy tắc: Tối đa một lần phun cho mỗi order

## 9. UC-33 Xử lý giao dịch lỗi
- Actor: OM (BA xem)
- Tiền điều kiện: Giao dịch ở trạng thái "chờ review / hoàn tiền"
- Hậu điều kiện: Giao dịch được đóng bằng retry, hoàn tiền hoặc từ chối; ghi lý do
- Luồng chính:
+ OM mở giao dịch lỗi, xem log thanh toán và phun
+ OM chọn xử lý: retry có kiểm soát, hoàn tiền hoặc từ chối
+ Với hoàn tiền cần xác thực bổ sung (UC-04)
+ Hệ thống thực hiện, ghi audit log và đóng giao dịch
- Ngoại lệ: Máy đang lỗi thì không cho retry, hoàn tiền thất bại thì giữ trạng thái chờ và báo lỗi
- Quy tắc: Retry không được tạo phun thứ hai nếu lần trước đã phun thành công

## 10. UC-41 Bật chế độ bảo trì
- Actor: TE
- Tiền điều kiện: Máy thuộc phạm vi được giao cho TE
- Hậu điều kiện: Máy ở chế độ bảo trì; kiosk chặn order mới
- Luồng chính:
+ TE chọn máy và bật bảo trì
+ Hệ thống kiểm tra không có order đang xử lý
+ Chuyển máy sang bảo trì, kiosk hiển thị thông báo
- Ngoại lệ: Còn order đang xử lý thì chờ hoàn tất
- Quy tắc: Máy chỉ hoạt động lại sau khi qua UC-45

## 11. UC-45 Kiểm tra sau bảo trì
- Actor: TE, DV
- Tiền điều kiện: Đã hoàn tất checklist bảo trì
- Hậu điều kiện: Máy trở lại hoạt động nếu đạt; nếu không, vẫn ở chế độ bảo trì
- Luồng chính:
+ TE chạy kiểm tra (kết nối, cảm biến, phun thử)
+ Máy trả kết quả (có thể cần UC-44, UC-43)
+ Nếu đạt, TE xác nhận; hệ thống tắt chế độ bảo trì
- Ngoại lệ: Không đạt thì giữ chế độ bảo trì, ghi lỗi, TE tiếp tục sửa
- Quy tắc: Phun thử được ghi riêng, không làm giảm tồn kho bán hàng


## 12. UC-52 Lắp / thay chai vào slot
- Actor: IS
- Tiền điều kiện: Chai đã đăng ký (UC-51); slot thuộc máy trong brand
- Hậu điều kiện: Slot có đúng một chai active; lịch sử thay chai được lưu
- Luồng chính:
+ IS chọn máy và slot
+ Quét mã chai mới
+ Hệ thống kiểm tra đúng mùi, đúng slot (UC-57)
+ Ghi khối lượng trước và sau, vô hiệu chai cũ, kích hoạt chai mới
- Ngoại lệ: Sai mùi hoặc chai đã dùng ở nơi khác thì từ chối
- Quy tắc: Mỗi slot chỉ một chai active; ghi nhân viên, thời gian, máy, slot, chai cũ và mới

## 13. UC-54 Điều chỉnh tồn kho
- Actor: IS
- Tiền điều kiện: Có chênh lệch giữa tồn kho ước tính và thực tế
- Hậu điều kiện: Tồn kho được điều chỉnh; lưu giá trị cũ, mới và lý do
- Luồng chính:
+ IS chọn chai hoặc slot cần điều chỉnh
+ Nhập số lượng mới và lý do
+ Xác thực bổ sung (UC-04)
+ Hệ thống cập nhật và ghi audit log
- Ngoại lệ: Thiếu lý do hoặc xác thực thất bại thì không lưu
- Quy tắc: Không sửa hoặc xóa lịch sử điều chỉnh
