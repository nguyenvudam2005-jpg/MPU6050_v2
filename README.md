# MPU6050 – Cấu hình và hướng dẫn sử dụng trên STM32F103C8T6

**Phiên bản tài liệu:** TEST 03 hiện tại  
**Nguồn:** `imu_test(3).c`, `imu_test(3).h` do người dùng cung cấp; thông tin chân/CubeMX từ cấu hình project đã trao đổi.  
**Phạm vi:** đọc MPU6050 qua I²C + DMA + FreeRTOS, hiệu chuẩn 500 mẫu, tính Roll/Pitch/Yaw, đọc kết quả ở `main.c`. **Không thay đổi thư viện.**

## 1. Cấu trúc phần mềm

| Thành phần | Vai trò |
|---|---|
| `Core/Inc/imu_test.h` | Kiểu dữ liệu, khai báo biến và API |
| `Core/Src/imu_test.c` | Khởi tạo MPU6050, DMA, hiệu chuẩn, tính góc |
| `Core/Src/main.c` | CubeMX khởi tạo ngoại vi và chạy `Task_IMU`; có thể đọc góc để truyền CAN |
| STM32 HAL + CMSIS-RTOS2 | I²C/DMA, semaphore, task, thời gian |

**Không gọi `IMU_Test_Run()` hai lần** và không để task khác đồng thời đọc/ghi I²C1 khi chưa có cơ chế đồng bộ.

## 2. Đấu dây phần cứng

| MPU6050 (GY-521) | STM32F103C8T6 | Ghi chú |
|---|---|---|
| VCC | 3V3 | Kiểm tra module cụ thể hỗ trợ 3,3 V |
| GND | GND | Chung mass |
| SCL | PB6 | I²C1_SCL |
| SDA | PB7 | I²C1_SDA |
| AD0 | GND | Địa chỉ 7-bit `0x68` |
| INT | PA3 (tùy chọn) | Firmware hiện tại **không sử dụng** Data Ready INT |

SDA/SCL cần điện trở kéo lên **3,3 V** (module đã có pull-up thì không nhất thiết lắp thêm).

## 3. Cấu hình STM32CubeMX đang dùng

| Mục | Thiết lập |
|---|---|
| MCU | STM32F103C8T6 |
| SYSCLK | 72 MHz |
| APB1 | 36 MHz |
| I²C1 | PB6=SCL, PB7=SDA, 100 kHz, 7-bit |
| DMA I²C1 RX | DMA1_Channel7, Peripheral→Memory, Normal, byte, MemInc Enable |
| DMA I²C1 TX | DMA1_Channel6, Memory→Peripheral, Normal, byte, MemInc Enable |
| IRQ | I²C1 EV/ER và DMA Channel6/7, priority 6 (theo project đã trao đổi) |
| RTOS | CMSIS-RTOS2, `Task_IMU` (entry `IMU`) |
| HAL timebase | TIM4 |

Lưu ý: `MX_DMA_Init()` phải chạy **trước** `MX_I2C1_Init()`. Liên kết DMA và IRQ phải được CubeMX sinh đầy đủ ở MSP/IT files.

## 4. Thông số MPU6050 thực tế trong file `.c`

| Thanh ghi / hằng | Giá trị | Ý nghĩa |
|---|---|---|
| `MPU_ADDR` | `(0x68U << 1)` | Địa chỉ đưa vào HAL I²C |
| `WHO_AM_I` | `0x75`, kiểm tra `0x68` | Xác nhận cảm biến |
| `PWR_MGMT_1` | `0x01` | Đánh thức, dùng PLL gyro X |
| `SMPLRT_DIV` | `0x09` | Tần số xuất dữ liệu mục tiêu 100 Hz khi DLPF bật |
| `CONFIG` | `0x03` | DLPF mức 3 |
| `GYRO_CONFIG` | `0x08` | **±500°/s**, độ nhạy **65,5 LSB/(°/s)** |
| `ACCEL_CONFIG` | `0x00` | ±2 g, độ nhạy 16384 LSB/g |
| `IMU_CAL_SAMPLES` | `500` | 500 mẫu hiệu chuẩn lúc đứng yên |
| `IMU_ALPHA` | `0.98` | Hệ số Complementary Filter |
| `MPU_DATA_REG` | `0x3B` | Bắt đầu đọc 14 byte |

**Phát hiện trong mã nguồn:** comment ở lệnh `GYRO_CONFIG = 0x08` ghi “Gyro +/-250 deg/s” nhưng **giá trị thực là ±500°/s**; phần chuyển đổi `raw.g* = g* / 65.5f` đã khớp với ±500°/s. Đây là lỗi comment, không phải lỗi thiết lập thanh ghi.

**Nhiệt độ:** DMA vẫn đọc 14 byte để giữ bố cục thanh ghi; byte 6–7 bị bỏ qua, **không tính và không sử dụng nhiệt độ**.

## 5. Các bước tích hợp vào Keil

1. Đặt `imu_test.h` tại `Core/Inc/`, `imu_test.c` tại `Core/Src/`.
2. Thêm `imu_test.c` vào group ứng dụng trong Keil (Add Existing Files).
3. Kiểm tra include path có `../Core/Inc` hoặc đường dẫn tương đương.
4. Trong phần `/* USER CODE BEGIN Includes */` của `main.c`:

```c
#include "imu_test.h"
```

5. Trong task `IMU` do CubeMX tạo ở `main.c`:

```c
void IMU(void *argument)
{
    /* USER CODE BEGIN 5 */
    IMU_Test_Run();  // Hàm này có vòng lặp riêng, không cần for(;;) bên ngoài
    /* USER CODE END 5 */
}
```

6. Giữ nguyên DMA callbacks của `imu_test.c`. Không định nghĩa lại `HAL_I2C_MemRxCpltCallback` hay `HAL_I2C_ErrorCallback` ở file khác.
7. Build (F7), nạp, Debug; kiểm tra lỗi compile/link trước khi chạy.

## 6. Quá trình hoạt động

1. Task tạo `dma_sem`, kiểm tra WHO_AM_I, đánh thức và cấu hình MPU6050.
2. Đọc **14 byte bằng HAL_I2C_Mem_Read_DMA()**; callback nhả semaphore khi hoàn thành.
3. Trong **500 mẫu đầu**, cộng dồn gia tốc/gyro để lấy offset. Phải đặt module **nằm ngang, trục Z+ hướng lên và đứng yên**.
4. Sau hiệu chuẩn, `imu_calibrated = 1`; áp dụng offset cho 6 kênh.
5. `IMU_Angle_Update()` tính Roll/Pitch bằng Accelerometer + Gyro với Complementary Filter `alpha=0.98`, Yaw bằng tích phân Gyro Z.
6. Vòng task hướng tới chu kỳ **10 ms**; `imu_dt` ghi thời gian thực giữa các lần cập nhật góc theo RTOS tick.

### Giới hạn cần biết

- Hiệu chuẩn gia tốc chỉ theo **một tư thế**, không phải hiệu chuẩn 6 mặt.
- **Yaw là góc tương đối**, ban đầu bằng 0 và có thể trôi vì không có từ kế/tham chiếu hướng.
- Công thức Roll/Pitch hiện tại phù hợp để test cơ bản; chuyển động nhanh, gia tốc tuyến tính hoặc xoay đa trục có thể làm sai ước lượng.
- `imu_angle` được task IMU cập nhật; đọc trực tiếp ở task CAN là cách đơn giản nhưng **không bảo đảm ảnh chụp đồng thời tuyệt đối** của hai số float nếu cập nhật chen ngang. Với yêu cầu đồng bộ nghiêm ngặt, cần sao chép có bảo vệ.

## 7. Cách lấy dữ liệu IMU từ `main.c`

Các biến đã được `extern` trong `imu_test.h`:

| Biến | Ý nghĩa / đơn vị |
|---|---|
| `imu_data.ax, ay, az` | Gia tốc sau bù offset, g |
| `imu_data.gx, gy, gz` | Vận tốc góc sau bù offset, °/s |
| `imu_angle.roll` | Góc Roll, ° |
| `imu_angle.pitch` | Góc Pitch, ° |
| `imu_angle.yaw` | Góc Yaw tương đối, °; chuẩn hóa -180 đến +180 |
| `imu_calibrated` | 0: đang hiệu chuẩn; 1: hoàn tất |
| `imu_status` | Trạng thái task |

**Ví dụ đọc Yaw/Pitch tại Task_CAN** (chỉ đọc, không tác động thư viện IMU):

```c
float yaw, pitch;

if (imu_calibrated == 1U && imu_status == 8U)
{
    yaw   = imu_angle.yaw;
    pitch = imu_angle.pitch;

    // Đóng gói và gửi CAN tại main.c, tùy giao thức của hệ thống.
}
```

Nếu truyền một frame CAN ID `0x101`, DLC 8 như thiết kế đã chốt: byte 0–3 chứa **Yaw float32**; byte 4–7 chứa **Pitch float32**; hai bên thống nhất thứ tự byte little-endian.

## 8. Debug bằng Keil Watch 1

Mở `Debug → Start/Stop Debug Session` → `View → Watch Windows → Watch 1` và thêm:

```text
imu_id
imu_status
imu_count
imu_error
imu_hal_error
imu_cal_count
imu_calibrated
imu_offset
imu_data
imu_angle
imu_dt
imu_angle_count
imu_gz_peak
imu_dt_max
```

| Trạng thái | Giá trị mong đợi |
|---|---|
| WHO_AM_I hợp lệ | `imu_id = 104` (`0x68`) |
| Hiệu chuẩn xong | `imu_cal_count = 500`, `imu_calibrated = 1` |
| Đọc/tính góc | `imu_status = 8`, `imu_angle_count` tăng |
| Không lỗi DMA/I²C | `imu_error = 0` |
| Nằm ngang (Z+ lên) | Roll/Pitch gần 0°, `az` gần +1 g |
| Chu kỳ cập nhật | `imu_dt` gần 0,01 s khi lịch task ổn định |

### `imu_status` trong mã nguồn

| Mã | Ý nghĩa |
|---|---|
| 0 | Chưa chạy |
| 1 | Không tạo được semaphore |
| 2 | Đang khởi tạo MPU6050 |
| 3 | Khởi tạo MPU6050 thất bại |
| 4 | Khởi tạo xong, chuẩn bị đọc |
| 5 | Không khởi động được DMA |
| 6 | Hết thời gian chờ DMA |
| 7 | Callback DMA báo lỗi |
| 8 | Đã hiệu chuẩn, đang cập nhật dữ liệu/góc |
| 9 | Đang lấy 500 mẫu hiệu chuẩn |

## 9. Các hàm trong thư viện

| Hàm | Vai trò | Cách dùng |
|---|---|---|
| `IMU_Test_Run()` | Khởi tạo và chạy liên tục trong task | Gọi **một lần** ở `Task_IMU` |
| `IMU_Angle_Reset()` | Đưa Roll/Pitch/Yaw và bộ đếm góc về ban đầu | Đã được gọi trong `IMU_Test_Run()`; không gọi định kỳ |
| `IMU_Angle_Update(data, dt)` | Cập nhật 3 góc từ dữ liệu bù offset | Đã được gọi tự động trong luồng IMU; không cần gọi trong `main.c` |

Hàm `MPU_Init`, `MPU_Convert` và `MPU_Write` là `static` nội bộ trong file `.c`, không gọi trực tiếp từ `main.c`.

## 10. Checklist trước khi dùng lại trên dự án khác

- [ ] Đúng board STM32F103 và có I²C1 PB6/PB7.
- [ ] SDA/SCL pull-up lên 3,3 V, AD0 nối GND.
- [ ] DMA RX Channel7, IRQ và callback được cấu hình đúng.
- [ ] Có FreeRTOS CMSIS V2 và task gọi `IMU_Test_Run()`.
- [ ] Giữ yên, nằm ngang trong 500 mẫu đầu.
- [ ] Đợi `imu_calibrated == 1` và `imu_status == 8` mới lấy góc để sử dụng.
- [ ] Dùng **Yaw/Pitch** trong `main.c` nếu chỉ cần truyền CAN; không sửa thư viện IMU.
- [ ] Kiểm tra sai số Yaw thực tế nếu robot quay nhanh; không giả định độ chính xác ±0,5°.

---

*Tài liệu mô tả cấu hình từ mã nguồn được cung cấp và các thông số CubeMX đã trao đổi; không phải biên bản xác nhận build/hardware cho phiên bản file này.*
