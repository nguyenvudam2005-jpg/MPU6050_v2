
#ifndef IMU_TEST_H
#define IMU_TEST_H

#include "main.h"
#include "cmsis_os2.h"

/* Accelerometer and Gyroscope */
typedef struct
{
    float ax, ay, az;   /* g */
    float gx, gy, gz;   /* deg/s */
} IMU_Data_t;

/* Roll, Pitch, Yaw */
typedef struct
{
    float roll;
    float pitch;
    float yaw;
} IMU_Angle_t;

/* Sensor data */
extern volatile IMU_Data_t imu_data;
extern volatile IMU_Data_t imu_offset;

/* Angle data */
extern volatile IMU_Angle_t imu_angle;
extern volatile float imu_dt;
extern volatile uint32_t imu_angle_count;

/* Debug status */
extern volatile uint8_t imu_id;
extern volatile uint8_t imu_status;
extern volatile uint32_t imu_count;
extern volatile uint32_t imu_error;
extern volatile uint32_t imu_hal_error;

/* Calibration */
extern volatile uint32_t imu_cal_count;
extern volatile uint8_t imu_calibrated;

/* Functions */
void IMU_Test_Run(void);
void IMU_Angle_Reset(void);
void IMU_Angle_Update(
    const IMU_Data_t *data,
    float dt
);

#endif
