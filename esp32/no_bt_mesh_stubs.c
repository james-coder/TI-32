#include <stdint.h>

#include "esp_bt.h"
#include "esp_err.h"

__attribute__((weak)) esp_err_t esp_bt_controller_mem_release(esp_bt_mode_t mode) {
  (void)mode;
  return ESP_OK;
}

__attribute__((weak)) uint32_t mesh_sta_auth_expire_time(void) {
  return 0;
}
