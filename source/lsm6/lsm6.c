#include <stdbool.h>

#include <stdint.h>

#include <string.h>

#include "lsm6.h"

#include "nrf_log.h"

#include "nrf_log_ctrl.h"

#include "nrf_log_default_backends.h"

#define TWI_ADDRESSES 127

//Initializing TWI0 instance
#define TWI_INSTANCE_ID 0

static float accRange = 0;

// A flag to indicate the transfer state
static volatile bool m_xfer_done = false;

// Create a Handle for the twi communication
static
const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

void twi_handler(nrf_drv_twi_evt_t
  const * p_event, void * p_context) {
  //Check the event to see what type of event occurred
  switch (p_event -> type) {
    //If data transmission or receiving is finished
  case NRF_DRV_TWI_EVT_DONE:
    m_xfer_done = true; //Set the flag
    break;

  default:
    // do nothing
    break;
  }
}

//Initialize the TWI as Master device
void twi_master_init(void) {
  ret_code_t err_code;

  // Configure the settings for twi communication
  const nrf_drv_twi_config_t twi_config = {
    .scl = TWI_SCL_M, //SCL Pin
    .sda = TWI_SDA_M, //SDA Pin
    .frequency = NRF_DRV_TWI_FREQ_400K, //Communication Speed
    .interrupt_priority = APP_IRQ_PRIORITY_HIGH, //Interrupt Priority(Note: if using Bluetooth then select priority carefully)
    .clear_bus_init = false //automatically clear bus
  };

  //A function to initialize the twi communication
  err_code = nrf_drv_twi_init( & m_twi, & twi_config, twi_handler, NULL);
  APP_ERROR_CHECK(err_code);

  //Enable the TWI Communication
  nrf_drv_twi_enable( & m_twi);

  uint8_t address;
  uint8_t sample_data;
  bool detected_device = false;

  for (address = 1; address <= 126; address++) {
    err_code = nrf_drv_twi_rx( & m_twi, address, & sample_data, sizeof(sample_data));
    if (err_code == NRF_SUCCESS) {
      detected_device = true;
      NRF_LOG_INFO("TWI device detected at address 0x%x.", address);
    }
    NRF_LOG_FLUSH();
  }

}

/*
   A function to write a Single Byte to MPU6050's internal Register
*/
bool write_register(uint8_t register_address, uint8_t value) {
  ret_code_t err_code;
  uint8_t tx_buf[LIS3DHTR_ADDRESS_LEN + 1];

  //Write the register address and data into transmit buffer
  tx_buf[0] = register_address;
  tx_buf[1] = value;

  //Set the flag to false to show the transmission is not yet completed
  m_xfer_done = false;

  //Transmit the data over TWI Bus
  err_code = nrf_drv_twi_tx( & m_twi, LIS3DHTR_ADDRESS_UPDATED, tx_buf, LIS3DHTR_ADDRESS_LEN + 1, false);

  //Wait until the transmission of the data is finished
  while (m_xfer_done == false) {}

  // if there is no error then return true else return false
  if (NRF_SUCCESS != err_code) {
    return false;
  }

  return true;
}

ret_code_t anrf_drv_twi_tx(nrf_drv_twi_t
  const * p_instance,
  uint8_t address,
  uint8_t
  const * p_data,
  uint8_t length,
  bool no_stop) {
  ret_code_t result = 0;
  if (NRF_DRV_TWI_USE_TWIM) {
    result = nrfx_twim_tx( & p_instance -> u.twim,
      address, p_data, length, no_stop);
  } else if (NRF_DRV_TWI_USE_TWI) {
    result = nrfx_twi_tx( & p_instance -> u.twi,
      address, p_data, length, no_stop);
  }
  return result;
}

bool read_registerRegion(uint8_t * destination, uint8_t register_address, uint8_t number_of_bytes) {
  ret_code_t err_code;
  m_xfer_done = false;

  // Send the Register address where we want to write the data

  err_code = anrf_drv_twi_tx( & m_twi, LIS3DHTR_ADDRESS_UPDATED, & register_address, 1, true);

  int tries = 100000;

  //Wait for the transmission to get completed
  while (m_xfer_done == false) {
    tries--;
    if (tries <= 0) {
      err_code = NRF_SUCCESS;
      break;
    }
  }

  // If transmission was not successful, exit the function with false as return value
  if (NRF_SUCCESS != err_code) {
    return false;
  }

  //set the flag again so that we can read data from the MPU6050's internal register
  m_xfer_done = false;

  // Receive the data from the MPU6050
  err_code = nrf_drv_twi_rx( & m_twi, LIS3DHTR_ADDRESS_UPDATED, destination, number_of_bytes);

  //wait until the transmission is completed
  while (m_xfer_done == false) {}

  // if data was successfully read, return true else return false
  if (NRF_SUCCESS != err_code) {
    return false;
  }

  return true;
}

bool LSM6_init(void) {
  twi_master_init();

  if (LSM6_verify_product_id() == false) {
    return false;
  }

  nrf_delay_ms(LIS3DHTR_CONVERSIONDELAY);

  return true;
}

bool LSM6_verify_product_id(void) {
  uint8_t who_am_i; // create a variable to hold the who am i value
  // Note: All the register addresses including WHO_AM_I are declared in 
  // MPU6050.h file, you can check these addresses and values from the
  // datasheet of your slave device.
  if (read_registerRegion( & who_am_i, LSM6DS3_WHO_AM_I_REG, 1)) {
    NRF_LOG_INFO("product id is: %d\n", who_am_i);
    if (who_am_i == 0x6A || who_am_i == 0x69) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

void LSM6_configure() {
  LSM6_set_power_mode(true);
  //set the gyroscope control register to work at 104 Hz, 2000 dps and in bypass mode
  write_register(LSM6DS3_CTRL2_G, 0x4C);
  // Set the Accelerometer control register to work at 104 Hz, 4 g,and in bypass mode and enable ODR/4
  // low pass filter (check figure9 of LSM6DS3's datasheet)
  write_register(LSM6DS3_CTRL1_XL, 0x4A);
  // set gyroscope power mode to high performance and bandwidth to 16 MHz
  write_register(LSM6DS3_CTRL7_G, 0x00);
  // Set the ODR config register to ODR/4
  write_register(LSM6DS3_CTRL8_XL, 0x09);
}

void LSM6_readAcceleration(int16_t * x, int16_t * y, int16_t * z) {
  int16_t data[3];
  read_registerRegion((uint8_t * ) data, LSM6DS3_OUTX_L_XL, sizeof(data));
  ( * x) = data[0];
  ( * y) = data[1];
  ( * z) = data[2];
}

void LSM6_readGyro(int16_t * x, int16_t * y, int16_t * z) {
  int16_t data[3];
  read_registerRegion((uint8_t * ) data, LSM6DS3_OUTX_L_G, sizeof(data));
  ( * x) = data[0];
  ( * y) = data[1];
  ( * z) = data[2];
}

int16_t LSM6_readTemperature() {
  int16_t data[1];
  read_registerRegion((uint8_t * ) data, LSM6DS3_OUT_TEMP_L, sizeof(data));
  return data[0];
}

void LSM6_set_power_mode(bool on) {
  if (on) {
    write_register(LSM6DS3_CTRL2_G, 0x4C);
    write_register(LSM6DS3_CTRL1_XL, 0x4A);
    write_register(LSM6DS3_CTRL7_G, 0x00);
    write_register(LSM6DS3_CTRL8_XL, 0x09);

    //write_register(LSM6DS3_WAKE_UP_DUR, 0x00);
    //write_register(LSM6DS3_WAKE_UP_THS, 0x02);
    //write_register(LSM6DS3_TAP_CFG, 0b00010010);
    //write_register(LSM6DS3_CTRL1_XL, 0x70);
    //nrf_delay_ms(4);
    //write_register(LSM6DS3_CTRL1_XL, 0x10);
    //write_register(LSM6DS3_MD1_CFG, 0x20);
  } else {
    write_register(LSM6DS3_CTRL2_G, 0x00);
    write_register(LSM6DS3_CTRL1_XL, 0x00);
  }
}