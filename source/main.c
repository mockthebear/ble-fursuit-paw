#include "startup.h"

#include <stdint.h>

#include <string.h>

#include "nrf_drv_clock.h"

#include "ble_nus.h"

#include "lsm6.h"

#define PIN_LED_1 2
#define PIN_LED_2 3
#define PIN_LED_3 4

const uint8_t pines[] = {
  30,
  31,
  7,
  6,
  5
};
const uint8_t butts_pines[] = {
  29
};

static int adv_led = 0;
static bool use_led = true;

#define LED_INTERVAL APP_TIMER_TICKS(20)

//BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); 

NRF_BLE_GATT_DEF(m_gatt); /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr); /**< Context for the Queued Write module.*/
APP_TIMER_DEF(m_app_timer_id);
/**< Handle of the current connection. */

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET; /**< Advertising handle used to identify an advertising set. */
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX]; /**< Buffer for storing an encoded advertising set. */
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX]; /**< Buffer for storing an encoded scan data. */
static int32_t advCountToSleep = 0;
static int16_t AccValue[13];

static uint8_t button_states[10];

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data = {
  .adv_data = {
    .p_data = m_enc_advdata,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
  },
  .scan_rsp_data = {
    .p_data = m_enc_scan_response_data,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX

  }
};

void assert_nrf_callback(uint16_t line_num,
  const uint8_t * p_file_name) {
  app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void gap_params_init(void) {
  ret_code_t err_code;
  ble_gap_conn_params_t gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN( & sec_mode);

  err_code = sd_ble_gap_device_name_set( & sec_mode,
    (const uint8_t * ) DEVICE_NAME,
    strlen(DEVICE_NAME));
  APP_ERROR_CHECK(err_code);

  memset( & gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
  gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
  gap_conn_params.slave_latency = SLAVE_LATENCY;
  gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

  err_code = sd_ble_gap_ppcp_set( & gap_conn_params);
  APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void) {
  ret_code_t err_code;
  ble_advdata_t advdata;
  ble_advdata_t srdata;

  ble_uuid_t adv_uuids[] = {
    {
      PAWSUIT_UUID_SERVICE,
      m_our_service.uuid_type
    }
  };

  // Build and set advertising data.
  memset( & advdata, 0, sizeof(advdata));

  advdata.name_type = BLE_ADVDATA_FULL_NAME;
  advdata.include_appearance = true;
  advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

  memset( & srdata, 0, sizeof(srdata));
  srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
  srdata.uuids_complete.p_uuids = adv_uuids;

  err_code = ble_advdata_encode( & advdata, m_adv_data.adv_data.p_data, & m_adv_data.adv_data.len);
  APP_ERROR_CHECK(err_code);

  err_code = ble_advdata_encode( & srdata, m_adv_data.scan_rsp_data.p_data, & m_adv_data.scan_rsp_data.len);
  APP_ERROR_CHECK(err_code);

  ble_gap_adv_params_t adv_params;

  // Set advertising parameters.
  memset( & adv_params, 0, sizeof(adv_params));

  adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
  adv_params.duration = APP_ADV_DURATION;
  adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
  adv_params.p_peer_addr = NULL;
  adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
  adv_params.interval = APP_ADV_INTERVAL;

  err_code = sd_ble_gap_adv_set_configure( & m_adv_handle, & m_adv_data, & adv_params);
  APP_ERROR_CHECK(err_code);
}

static void nrf_qwr_error_handler(uint32_t nrf_error) {
  APP_ERROR_HANDLER(nrf_error);
}

static void sleep_mode_enter(void) {
  uint32_t err_code;

  nrf_gpio_cfg_sense_input(butts_pines[0], NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
  nrf_gpio_pin_set(PIN_LED_1);
  nrf_gpio_pin_clear(PIN_LED_2);
  nrf_gpio_pin_clear(PIN_LED_3);

  LSM6_set_power_mode(false);

  //NRF_LOG_PROCESS();
  nrf_delay_ms(1000);
  //nrf_gpio_pin_clear(PIN_LED_1);
  // Go to system-off mode (this function will not return; wakeup will cause a reset).

  nrf_gpio_pin_clear(PIN_LED_1);
  nrf_gpio_pin_clear(PIN_LED_2);
  nrf_gpio_pin_clear(PIN_LED_3);

  //NRF_LOG_FLUSH();

  sd_power_system_off();
  //APP_ERROR_CHECK(err_code);
}

static void services_init(void) {
  ret_code_t err_code;
  nrf_ble_qwr_init_t qwr_init = {
    0
  };

  qwr_init.error_handler = nrf_qwr_error_handler;

  err_code = nrf_ble_qwr_init( & m_qwr, & qwr_init);
  APP_ERROR_CHECK(err_code);

  ble_uuid_t ble_uuid;

  ble_uuid128_t base_uuida = {
    PAWSUIT_UUID_BASE
  };

  err_code = sd_ble_uuid_vs_add( & base_uuida, & m_our_service.uuid_type);

  APP_ERROR_CHECK(err_code);

  ble_uuid.type = m_our_service.uuid_type;

  ble_uuid.uuid = PAWSUIT_UUID_SERVICE;

  err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &
    ble_uuid, &
    m_our_service.service_handle);

  APP_ERROR_CHECK(err_code);

  ble_add_char_params_t add_char_params;

  /*memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = 0xf00d;
    add_char_params.uuid_type         = m_our_service.uuid_type;
    add_char_params.init_len          = sizeof(int16_t)*10;
    add_char_params.max_len           = sizeof(int16_t)*10;
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.notify = 1;

    add_char_params.read_access       = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

   err_code = characteristic_add(m_our_service.service_handle,
                                      &add_char_params,
                                      &m_our_service.button_handle);*/

  memset( & add_char_params, 0, sizeof(add_char_params));
  add_char_params.uuid = 0xfafaf;
  add_char_params.uuid_type = m_our_service.uuid_type;
  add_char_params.init_len = sizeof(int16_t) * 13;
  add_char_params.max_len = sizeof(int16_t) * 13;
  add_char_params.char_props.read = 1;
  add_char_params.char_props.notify = 1;

  add_char_params.read_access = SEC_OPEN;
  add_char_params.cccd_write_access = SEC_OPEN;

  err_code = characteristic_add(m_our_service.service_handle, &
    add_char_params, &
    m_our_service.mems_handle);

  APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
static void advertising_start(void) {
  ret_code_t err_code;
  err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  APP_ERROR_CHECK(err_code);
  nrf_gpio_pin_clear(PIN_LED_1);

  nrf_gpio_pin_set(adv_led);

}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t
  const * p_ble_evt, void * p_context) {
  ret_code_t err_code;

  switch (p_ble_evt -> header.evt_id) {
  case BLE_GAP_EVT_CONNECTED:
    NRF_LOG_INFO("Connected");
    advCountToSleep = 0;
    nrf_gpio_pin_clear(adv_led);
    //bsp_board_led_on(CONNECTED_LED);

    m_our_service.conn_handle = p_ble_evt -> evt.gap_evt.conn_handle;
    err_code = nrf_ble_qwr_conn_handle_assign( & m_qwr, m_our_service.conn_handle);
    APP_ERROR_CHECK(err_code);

    break;

  case BLE_GAP_EVT_DISCONNECTED:
    NRF_LOG_INFO("Disconnected");
    advCountToSleep = 0;
    //app_timer_stop(m_app_timer_id);
    rebeginCycles = 50;
    m_our_service.conn_handle = BLE_CONN_HANDLE_INVALID;
    //err_code = app_button_disable();
    //APP_ERROR_CHECK(err_code);
    advertising_start();
    break;

  case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    // Pairing not supported
    NRF_LOG_INFO("owo");

    err_code = sd_ble_gap_sec_params_reply(m_our_service.conn_handle,
      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
      NULL,
      NULL);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GAP_EVT_CONN_PARAM_UPDATE: {
    NRF_LOG_DEBUG("Asked UPATE");
    err_code = sd_ble_gap_conn_param_update(p_ble_evt -> evt.gatts_evt.conn_handle, NULL);
    //err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, BLE_GATT_ATT_MTU_DEFAULT);
    //ok
    APP_ERROR_CHECK(err_code);
  }
  break;
  case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
    NRF_LOG_DEBUG("PHY update request.");
    ble_gap_phys_t
    const phys = {
      .rx_phys = BLE_GAP_PHY_AUTO,
      .tx_phys = BLE_GAP_PHY_AUTO,
    };
    err_code = sd_ble_gap_phy_update(p_ble_evt -> evt.gap_evt.conn_handle, & phys);
    APP_ERROR_CHECK(err_code);
  }
  break;

  case BLE_GATTS_EVT_SYS_ATTR_MISSING:
    NRF_LOG_INFO("owo");

    // No system attributes have been stored.
    err_code = sd_ble_gatts_sys_attr_set(m_our_service.conn_handle, NULL, 0, 0);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GATTC_EVT_TIMEOUT:
    // Disconnect on GATT Client timeout event.
    NRF_LOG_DEBUG("GATT Client Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt -> evt.gattc_evt.conn_handle,
      BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GATTS_EVT_TIMEOUT:
    // Disconnect on GATT Server timeout event.
    NRF_LOG_DEBUG("GATT Server Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt -> evt.gatts_evt.conn_handle,
      BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;
  case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
    NRF_LOG_DEBUG("Asked for MTU");
    //err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, BLE_GATT_ATT_MTU_DEFAULT);
    // APP_ERROR_CHECK(err_code);
    //ok
    break;
  case BLE_GATTS_EVT_HVN_TX_COMPLETE:
    //ok
    break;
  default:
    NRF_LOG_INFO("uwu: %d", (int) p_ble_evt -> header.evt_id);
    // No implementation needed.
    break;
  }
}

static void gatt_init(void) {
  ret_code_t err_code = nrf_ble_gatt_init( & m_gatt, NULL);
  APP_ERROR_CHECK(err_code);
}
/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void) {
  ret_code_t err_code;
  NRF_LOG_INFO("EN.");
  NRF_LOG_FLUSH();
  err_code = nrf_sdh_enable_request();

  NRF_LOG_INFO("sd err: %d.", err_code);
  NRF_LOG_FLUSH();
  APP_ERROR_CHECK(err_code);

  // Configure the BLE stack using the default settings.
  // Fetch the start address of the application RAM.
  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, & ram_start);
  APP_ERROR_CHECK(err_code);
  NRF_LOG_INFO("cg.");
  NRF_LOG_FLUSH();

  // Enable BLE stack.
  err_code = nrf_sdh_ble_enable( & ram_start);
  APP_ERROR_CHECK(err_code);
  NRF_LOG_INFO("be.");
  NRF_LOG_FLUSH();

  // Register a handler for BLE events.
  NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
  NRF_LOG_INFO("obs.");
  NRF_LOG_FLUSH();
}


uint32_t send_data(uint16_t conn_handle, int16_t AccValue[13]) {
  ble_gatts_hvx_params_t params;

  uint16_t len = sizeof(int16_t) * 13;

  memset( & params, 0, sizeof(params));
  params.type = BLE_GATT_HVX_NOTIFICATION;
  params.handle = m_our_service.mems_handle.value_handle;
  params.p_data = (uint8_t * ) AccValue;
  params.p_len = & len;
  return sd_ble_gatts_hvx(conn_handle, & params);

}

static void button_event_handler() {

  /*ret_code_t err_code;
  if (m_our_service.conn_handle != BLE_CONN_HANDLE_INVALID){
    err_code = baka(m_our_service.conn_handle);
    if (err_code != NRF_SUCCESS &&  err_code != BLE_ERROR_INVALID_CONN_HANDLE &&
      err_code != NRF_ERROR_INVALID_STATE &&
      err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
      {
              APP_ERROR_CHECK(err_code);
      }
  }*/
}

static void app_timer_handler(void * p_context) {

  if (m_our_service.conn_handle != BLE_CONN_HANDLE_INVALID) {
    nrf_gpio_pin_clear(PIN_LED_3);

    if (advCountToSleep % 50 == 0) {
      nrf_gpio_pin_set(PIN_LED_1);
    } else {
      nrf_gpio_pin_clear(PIN_LED_1);
    }

    advCountToSleep++;

    int16_t x;
    int16_t y;
    int16_t z;
    LSM6_readAcceleration( & AccValue[0], & AccValue[1], & AccValue[2]);

    LSM6_readGyro( & AccValue[3], & AccValue[4], & AccValue[5]);

    AccValue[6] = LSM6_readTemperature();

    uint8_t * aux = (uint8_t * ) & AccValue[7];

    for (int i = 0; i < 5; i++) {
      uint32_t state = nrf_gpio_pin_read(pines[i]);
      if (button_states[i] == state) {
        aux[i] = button_states[i] = !state;
      }
    }
    if (rebeginCycles > 0) {
      rebeginCycles--;
      nrf_gpio_pin_set(PIN_LED_3);
    } else {
      send_data(m_our_service.conn_handle, AccValue);
    }
  } else {
    advCountToSleep++;
    int16_t x;
    int16_t y;
    int16_t z;
    LSM6_readAcceleration( & x, & y, & z);

    if (advCountToSleep >= 400) {
      sleep_mode_enter();
    }
  }

  if (advCountToSleep >= 20) {
    for (int i = 0; i < 2; i++) {
      uint32_t state = nrf_gpio_pin_read(butts_pines[0]);
      if (state == 1) {
        //NRF_LOG_INFO("SLEBt: %d", butts_pines[i]);
        sleep_mode_enter();
      }
    }
  }
}

static void timers_init(void) {

  ret_code_t err_code = nrf_drv_clock_init();
  APP_ERROR_CHECK(err_code);

  nrf_drv_clock_lfclk_request(NULL);

  err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);

  err_code = app_timer_create( & m_app_timer_id, APP_TIMER_MODE_REPEATED, app_timer_handler);
  APP_ERROR_CHECK(err_code);
}

static void power_management_init(void) {
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}

static void idle_state_handle(void) {
  if (NRF_LOG_PROCESS() == false) {
    nrf_pwr_mgmt_run();
  }
}

int main(void) {

  m_our_service.conn_handle = BLE_CONN_HANDLE_INVALID;
  rebeginCycles = 50;

  log_init();
  leds_init();
  timers_init();

  memset(button_states, 0, sizeof(uint8_t) * 10);

  nrf_gpio_cfg_output(PIN_LED_1);
  nrf_gpio_cfg_output(PIN_LED_2);
  nrf_gpio_cfg_output(PIN_LED_3);

  nrf_gpio_pin_clear(PIN_LED_3);
  nrf_gpio_pin_clear(PIN_LED_2);
  nrf_gpio_pin_clear(PIN_LED_1);

  for (int i = 0; i < 5; i++) {
    nrf_gpio_cfg_input(pines[i], NRF_GPIO_PIN_PULLUP);
  }

  nrf_delay_ms(200);
  nrf_gpio_pin_set(PIN_LED_3);
  nrf_delay_ms(200);
  nrf_gpio_pin_set(PIN_LED_2);
  nrf_delay_ms(200);
  nrf_gpio_pin_set(PIN_LED_1);
  nrf_delay_ms(200);

  nrf_gpio_pin_clear(PIN_LED_3);
  nrf_delay_ms(200);
  nrf_gpio_pin_clear(PIN_LED_2);
  nrf_delay_ms(200);
  nrf_gpio_pin_clear(PIN_LED_1);
  nrf_delay_ms(200);

  nrf_gpio_cfg_input(butts_pines[0], NRF_GPIO_PIN_PULLDOWN);

  adv_led = PIN_LED_3;

  power_management_init();

  ble_stack_init();

  gap_params_init();

  gatt_init();

  services_init();

  advertising_init();

  conn_params_init();

  nrf_gpio_pin_set(PIN_LED_3);
  nrf_gpio_pin_set(PIN_LED_2);
  nrf_gpio_pin_clear(PIN_LED_1);
  while (LSM6_init() == false) 
  {
    NRF_LOG_INFO("LSM6 initialization failed!!!"); 
    NRF_LOG_FLUSH();
    while (1) {
      nrf_gpio_pin_clear(PIN_LED_3);
      nrf_gpio_pin_clear(PIN_LED_2);
      nrf_gpio_pin_clear(PIN_LED_1);
      nrf_delay_ms(500);
      nrf_gpio_pin_set(PIN_LED_3);
      nrf_gpio_pin_set(PIN_LED_2);
      nrf_gpio_pin_set(PIN_LED_1);
      nrf_delay_ms(500);
    }
  }

  LSM6_set_power_mode(true);

  nrf_gpio_pin_clear(PIN_LED_2);
  nrf_gpio_pin_clear(PIN_LED_3);
  NRF_LOG_INFO("limao");
  NRF_LOG_FLUSH();

  LSM6_configure();

  app_timer_start(m_app_timer_id, LED_INTERVAL, NULL);
  advertising_start();
  NRF_LOG_INFO("go");

  // Enter main loop.
  for (;;) {
    idle_state_handle();
  }
}