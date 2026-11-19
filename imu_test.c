
#include "imu_test.h"
#include <math.h>
#include <stddef.h>

/* MPU6050 registers */
#define MPU_ADDR        (0x68U << 1)
#define MPU_WHO_AM_I    0x75U
#define MPU_PWR_MGMT_1  0x6BU
#define MPU_SMPLRT_DIV  0x19U
#define MPU_CONFIG      0x1AU
#define MPU_GYRO_CFG    0x1BU
#define MPU_ACCEL_CFG   0x1CU
#define MPU_DATA_REG    0x3BU

#define IMU_CAL_SAMPLES 500U
#define IMU_RAD_TO_DEG  57.2957795f
#define IMU_ALPHA       0.98f

extern I2C_HandleTypeDef hi2c1;

/* Variables for Keil Watch */
volatile IMU_Data_t imu_data = {0};
volatile IMU_Data_t imu_offset = {0};
volatile IMU_Angle_t imu_angle = {0};

volatile uint8_t imu_id = 0;
volatile uint8_t imu_status = 0;
volatile uint8_t imu_calibrated = 0;

volatile uint32_t imu_count = 0;
volatile uint32_t imu_error = 0;
volatile uint32_t imu_hal_error = 0;
volatile uint32_t imu_cal_count = 0;
volatile uint32_t imu_angle_count = 0;

volatile float imu_dt = 0.0f;
volatile float imu_gz_peak = 0.0f;
volatile float imu_dt_max = 0.0f;


/* Internal variables */
static IMU_Data_t imu_sum = {0};
static uint8_t rx_buffer[14];

static osSemaphoreId_t dma_sem = NULL;
static volatile uint8_t dma_result = 0;
static uint8_t imu_angle_initialized = 0;

/* -------------------------------------------
   MPU6050 - Write Register
------------------------------------------- */
static HAL_StatusTypeDef MPU_Write(
    uint8_t reg,
    uint8_t value)
{
    return HAL_I2C_Mem_Write(
        &hi2c1,
        MPU_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &value,
        1,
        100
    );
}

/* -------------------------------------------
   MPU6050 - Initialization
------------------------------------------- */
static HAL_StatusTypeDef MPU_Init(void)
{
    uint8_t id = 0;

    if (HAL_I2C_IsDeviceReady(
        &hi2c1, MPU_ADDR, 3, 100) != HAL_OK)
        return HAL_ERROR;

    if (HAL_I2C_Mem_Read(
        &hi2c1,
        MPU_ADDR,
        MPU_WHO_AM_I,
        I2C_MEMADD_SIZE_8BIT,
        &id,
        1,
        100
    ) != HAL_OK)
        return HAL_ERROR;

    imu_id = id;

    if (id != 0x68)
        return HAL_ERROR;

    /* Wake up */
    if (MPU_Write(MPU_PWR_MGMT_1, 0x01) != HAL_OK)
        return HAL_ERROR;

    osDelay(100);

    /* 100 Hz sensor sample rate */
    if (MPU_Write(MPU_SMPLRT_DIV, 0x09) != HAL_OK)
        return HAL_ERROR;

    /* DLPF = 3 */
    if (MPU_Write(MPU_CONFIG, 0x03) != HAL_OK)
        return HAL_ERROR;

    /* Gyro +/-250 deg/s */
    if (MPU_Write(MPU_GYRO_CFG, 0x08) != HAL_OK)
        return HAL_ERROR;

    /* Accel +/-2g */
    if (MPU_Write(MPU_ACCEL_CFG, 0x00) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

/* -------------------------------------------
   Convert 2 bytes to int16
------------------------------------------- */
static int16_t MPU_Int16(
    uint8_t high,
    uint8_t low)
{
    uint16_t raw;

    raw = ((uint16_t)high << 8) | low;

    return (int16_t)raw;
}

/* -------------------------------------------
   Convert and calibrate IMU
------------------------------------------- */
static void MPU_Convert(void)
{
    IMU_Data_t raw;

    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    float n;

    ax = MPU_Int16(rx_buffer[0], rx_buffer[1]);
    ay = MPU_Int16(rx_buffer[2], rx_buffer[3]);
    az = MPU_Int16(rx_buffer[4], rx_buffer[5]);

    /* rx_buffer[6] and [7] are temperature:
       intentionally unused */

    gx = MPU_Int16(rx_buffer[8], rx_buffer[9]);
    gy = MPU_Int16(rx_buffer[10], rx_buffer[11]);
    gz = MPU_Int16(rx_buffer[12], rx_buffer[13]);

    raw.ax = ax / 16384.0f;
    raw.ay = ay / 16384.0f;
    raw.az = az / 16384.0f;

    raw.gx = gx / 65.5f;
    raw.gy = gy / 65.5f;
    raw.gz = gz / 65.5f;

    /* Calibration phase */
    if (imu_calibrated == 0)
    {
        imu_sum.ax += raw.ax;
        imu_sum.ay += raw.ay;
        imu_sum.az += raw.az;

        imu_sum.gx += raw.gx;
        imu_sum.gy += raw.gy;
        imu_sum.gz += raw.gz;

        imu_cal_count++;

        if (imu_cal_count >= IMU_CAL_SAMPLES)
        {
            n = (float)imu_cal_count;

            imu_offset.ax = imu_sum.ax / n;
            imu_offset.ay = imu_sum.ay / n;
            imu_offset.az =
                (imu_sum.az / n) - 1.0f;

            imu_offset.gx = imu_sum.gx / n;
            imu_offset.gy = imu_sum.gy / n;
            imu_offset.gz = imu_sum.gz / n;

            imu_calibrated = 1;
        }
        else
        {
            return;
        }
    }

    /* Apply calibration */
    imu_data.ax = raw.ax - imu_offset.ax;
    imu_data.ay = raw.ay - imu_offset.ay;
    imu_data.az = raw.az - imu_offset.az;

    imu_data.gx = raw.gx - imu_offset.gx;
    imu_data.gy = raw.gy - imu_offset.gy;
    imu_data.gz = raw.gz - imu_offset.gz;
}

/* -------------------------------------------
   Reset angles
------------------------------------------- */
void IMU_Angle_Reset(void)
{
    imu_angle.roll = 0.0f;
    imu_angle.pitch = 0.0f;
    imu_angle.yaw = 0.0f;

    imu_dt = 0.0f;
    imu_angle_count = 0;
    imu_angle_initialized = 0;
}

/* -------------------------------------------
   Calculate Roll, Pitch and Yaw
------------------------------------------- */
void IMU_Angle_Update(
    const IMU_Data_t *data,
    float dt)
{
    float ax, ay, az;
    float gx, gy, gz;

    float roll_acc;
    float pitch_acc;

    float roll_gyro;
    float pitch_gyro;

    float acc_norm;

    if (data == NULL || dt <= 0.0f || dt > 0.1f)
        return;

    ax = data->ax;
    ay = data->ay;
    az = data->az;

    gx = data->gx;
    gy = data->gy;
    gz = data->gz;


    /* Maximum measured angular speed */
    if (fabsf(gz) > imu_gz_peak)
    {
        imu_gz_peak = fabsf(gz);
    }

    /* Maximum sampling interval */
    if (dt > imu_dt_max)
    {
        imu_dt_max = dt;
    }


    acc_norm = sqrtf(
        ax*ax + ay*ay + az*az
    );

    if (acc_norm < 0.1f)
        return;

    /* Roll from accelerometer */
    roll_acc =
        atan2f(ay, az) * IMU_RAD_TO_DEG;

    /* Pitch from accelerometer */
    pitch_acc =
        atan2f(
            -ax,
            sqrtf(ay*ay + az*az)
        ) * IMU_RAD_TO_DEG;

    /* Initial orientation */
    if (imu_angle_initialized == 0)
    {
        imu_angle.roll = roll_acc;
        imu_angle.pitch = pitch_acc;
        imu_angle.yaw = 0.0f;

        imu_angle_initialized = 1;
        imu_dt = dt;
        imu_angle_count++;
        return;
    }

    /* Gyroscope integration */
    roll_gyro =
        imu_angle.roll + gx * dt;

    pitch_gyro =
        imu_angle.pitch + gy * dt;

    imu_angle.yaw += gz * dt;

    /* Complementary filter */
    if (acc_norm > 0.85f && acc_norm < 1.15f)
    {
        imu_angle.roll =
            IMU_ALPHA * roll_gyro +
            (1.0f - IMU_ALPHA) * roll_acc;

        imu_angle.pitch =
            IMU_ALPHA * pitch_gyro +
            (1.0f - IMU_ALPHA) * pitch_acc;
    }
    else
    {
        imu_angle.roll = roll_gyro;
        imu_angle.pitch = pitch_gyro;
    }

    /* Normalize Yaw -180 to 180 degrees */
    while (imu_angle.yaw > 180.0f)
        imu_angle.yaw -= 360.0f;

    while (imu_angle.yaw < -180.0f)
        imu_angle.yaw += 360.0f;

    imu_dt = dt;
    imu_angle_count++;
}

/* -------------------------------------------
   I2C DMA Receive Callback
------------------------------------------- */
void HAL_I2C_MemRxCpltCallback(
    I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1 && dma_sem != NULL)
    {
        dma_result = 1;
        osSemaphoreRelease(dma_sem);
    }
}

/* -------------------------------------------
   I2C Error Callback
------------------------------------------- */
void HAL_I2C_ErrorCallback(
    I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1 && dma_sem != NULL)
    {
        imu_hal_error = HAL_I2C_GetError(hi2c);
        dma_result = 2;
        osSemaphoreRelease(dma_sem);
    }
}

/* -------------------------------------------
   Main IMU Task
------------------------------------------- */
void IMU_Test_Run(void)
{
    HAL_StatusTypeDef result;

    uint32_t tick;
    uint32_t angle_last_tick = 0;
    uint32_t angle_now;

    float angle_dt;
    IMU_Data_t angle_input;

    dma_sem = osSemaphoreNew(1, 0, NULL);

    if (dma_sem == NULL)
    {
        imu_status = 1;

        for (;;)
            osDelay(1000);
    }

    imu_status = 2;

    if (MPU_Init() != HAL_OK)
    {
        imu_hal_error = HAL_I2C_GetError(&hi2c1);
        imu_status = 3;

        for (;;)
            osDelay(1000);
    }

    imu_status = 4;

    IMU_Angle_Reset();

    tick = osKernelGetTickCount();

    for (;;)
    {
        dma_result = 0;

        result = HAL_I2C_Mem_Read_DMA(
            &hi2c1,
            MPU_ADDR,
            MPU_DATA_REG,
            I2C_MEMADD_SIZE_8BIT,
            rx_buffer,
            14
        );

        if (result != HAL_OK)
        {
            imu_hal_error = HAL_I2C_GetError(&hi2c1);
            imu_error++;
            imu_status = 5;
            break;
        }

        if (osSemaphoreAcquire(
            dma_sem, 20) != osOK)
        {
            imu_error++;
            imu_status = 6;
            break;
        }

        if (dma_result != 1)
        {
            imu_error++;
            imu_status = 7;
            break;
        }

        MPU_Convert();

        imu_count++;

        if (imu_calibrated == 0)
        {
            imu_status = 9;
        }
        else
        {
            angle_now = osKernelGetTickCount();

            if (angle_last_tick == 0)
            {
                angle_last_tick = angle_now;
            }
            else
            {
                angle_dt =
                    (float)(angle_now - angle_last_tick)
                    / (float)osKernelGetTickFreq();

                angle_last_tick = angle_now;

                angle_input.ax = imu_data.ax;
                angle_input.ay = imu_data.ay;
                angle_input.az = imu_data.az;

                angle_input.gx = imu_data.gx;
                angle_input.gy = imu_data.gy;
                angle_input.gz = imu_data.gz;

                IMU_Angle_Update(
                    &angle_input,
                    angle_dt
                );
            }

            imu_status = 8;
        }

        /* Keep target period ~10ms */
        tick += 10;

        if ((int32_t)(
            tick - osKernelGetTickCount()) > 0)
        {
            osDelayUntil(tick);
        }
        else
        {
            tick = osKernelGetTickCount();
        }
    }

    /* Stop on error */
    for (;;)
    {
        osDelay(1000);
    }
}
