# Hướng dẫn đóng góp cho kho mã

## Cấu trúc và tổ chức mã nguồn

Kho mã chứa mô-đun MPU6050 cho STM32. `imu_test.c` thực hiện khởi tạo, đọc I2C qua DMA, hiệu chuẩn 500 mẫu và tính góc nghiêng, chúc, hướng. `imu_test.h` khai báo cấu trúc dữ liệu, biến theo dõi và các hàm `IMU_*`. Chưa có thư mục kiểm thử, tài nguyên hoặc thư viện nhà cung cấp riêng.

Dự án tích hợp phải cung cấp `main.h`, STM32 HAL, `cmsis_os2.h` và `hi2c1`. Góc nghiêng và chúc dùng bộ lọc bù; góc hướng được tích phân từ vận tốc góc nên có thể trôi.

## Biên dịch và phát triển

Chưa có tập lệnh biên dịch, tệp dự án hoặc lệnh kiểm thử độc lập. Thêm `imu_test.c` và đường dẫn chứa tệp khai báo vào dự án STM32, rồi dùng bộ công cụ của dự án đó để biên dịch. Cấu hình liên kết thư viện toán cho các hàm số thực đơn chính xác.

Khởi tạo I2C1, DMA và ngắt trong chương trình tích hợp. Gọi `IMU_Test_Run()` từ một luồng CMSIS-RTOS2; hàm chạy liên tục. Chu kỳ đọc là 10 nhịp hệ điều hành; kiểm tra tần số nhịp trước khi quy đổi thành 10 ms.

## Quy tắc viết mã và đặt tên

Thụt lề bốn dấu cách; đặt dấu ngoặc nhọn mở trên dòng riêng. Dùng `IMU_*` cho hàm công khai, `MPU_*` cho hàm nội bộ, `imu_*` cho biến trạng thái và chữ hoa cho hằng số. Giữ hàm, bộ đệm nội bộ ở phạm vi `static`; giữ cơ chế chống khai báo lặp trong `imu_test.h`. Dùng hậu tố `U`, `f` như mã hiện có. Chưa cấu hình công cụ định dạng hoặc phân tích mã.

## Hướng dẫn kiểm thử

`imu_test.c` chạy trên thiết bị; chưa có bộ kiểm thử tự động hoặc yêu cầu độ bao phủ. Khi kiểm tra, giữ cảm biến nằm ngang, đứng yên lúc hiệu chuẩn. Theo dõi `imu_calibrated`, xác nhận `imu_count` và `imu_angle_count` tăng. Kiểm tra `imu_status`, `imu_error`, `imu_hal_error`, góc khi đứng yên và khi xoay có kiểm soát. Ghi rõ bo mạch, tần số nhịp và kết quả.

## Ghi nhận thay đổi và yêu cầu hợp nhất

Chưa có lịch sử Git hoặc mẫu yêu cầu hợp nhất. Đề xuất dùng tiêu đề ngắn, dạng mệnh lệnh, như `Sửa xử lý quá hạn nhận DMA`. Mô tả hành vi thay đổi, yêu cầu tích hợp và kiểm chứng. Nêu rõ thay đổi thanh ghi, hệ số quy đổi, thời gian hoặc hàm gọi lại HAL; phối hợp định nghĩa hàm gọi lại với dự án tích hợp để tránh trùng ký hiệu.
