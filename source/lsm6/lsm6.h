#ifndef AT24C02_H__
#define AT24C02_H__#include "nrf_delay.h"

#include "nrf_drv_twi.h"
 //I2C Pins Settings, you change them to any other pins
#define TWI_SCL_M 26 //I2C SCL Pin
#define TWI_SDA_M 25 //I2C SDA Pin

#define LSM6DS3_ADDRESS 0x6A

#define LSM6DS3_WHO_AM_I_REG 0X0F
#define LSM6DS3_CTRL1_XL 0X10
#define LSM6DS3_CTRL2_G 0X11
#define LSM6DS3_OUT_TEMP_L 0X20
#define LSM6DS3_STATUS_REG 0X1E

#define LSM6DS3_CTRL6_C 0X15
#define LSM6DS3_CTRL7_G 0X16
#define LSM6DS3_CTRL8_XL 0X17

#define LSM6DS3_OUTX_L_G 0X22
#define LSM6DS3_OUTX_H_G 0X23
#define LSM6DS3_OUTY_L_G 0X24
#define LSM6DS3_OUTY_H_G 0X25
#define LSM6DS3_OUTZ_L_G 0X26
#define LSM6DS3_OUTZ_H_G 0X27

#define LSM6DS3_OUTX_L_XL 0X28
#define LSM6DS3_OUTX_H_XL 0X29
#define LSM6DS3_OUTY_L_XL 0X2A
#define LSM6DS3_OUTY_H_XL 0X2B
#define LSM6DS3_OUTZ_L_XL 0X2C
#define LSM6DS3_OUTZ_H_XL 0X2D

#define LSM6DS3_TAP_CFG 0X58
#define LSM6DS3_WAKE_UP_DUR 0X5C
#define LSM6DS3_WAKE_UP_THS 0X5B
#define LSM6DS3_MD1_CFG 0X5E

static
const nrf_drv_twi_t m_twi;

void twi_master_init(void); // initialize the twi communication

bool write_register(uint8_t reg, uint8_t val);

bool LSM6_init();
bool LSM6_verify_product_id();
void LSM6_configure();

void LSM6_readAcceleration(int16_t * , int16_t * , int16_t * );
void LSM6_readGyro(int16_t * x, int16_t * y, int16_t * z);
int16_t LSM6_readTemperature();
void LSM6_set_power_mode(bool on);

#endif