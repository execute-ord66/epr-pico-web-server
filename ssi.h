#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "logic.h"

const char * ssi_tags[] = {"temp","led","packet", "colours", "state", "rot", "tan", "dist", "ang", "debug"};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    size_t printed;
    switch (iIndex) {
        case 0: // temp
            {
                // const float voltage = adc_read() * 3.3f / (1 << 12);
                // const float tempC = 27.0f - (voltage - 0.706f) / 0.001721f;
                printed = snprintf(pcInsert, iInsertLen, "%.1f", 0);
            }
            break;
        case 1: // led
            {
                bool led_status = cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN);
                printed = snprintf(pcInsert, iInsertLen, "%s", led_status ? "ON" : "OFF");
            }
            break;
        case 2: // packet
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", 0);
            }
            break;
        case 3: // colours
            {
                printed = snprintf(pcInsert, iInsertLen, "%d%d%d%d%d", 
                    sensor_colours[0], sensor_colours[1], sensor_colours[2], sensor_colours[3], sensor_colours[4]);
            }
            break;
        case 4: // state
            {
                const char* state_strings[] = {"IDLE", "CAL", "MAZE", "SOS"};
                printed = snprintf(pcInsert, iInsertLen, "%s", state_strings[current_state]);
            }
            break;
        case 5: // rot
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_rotation);
            }
            break;
        case 6: // tan
            {
                printed = snprintf(pcInsert, iInsertLen, "%d %d", last_speedL, last_speedR);
            }
            break;
        case 7: // dist
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_distance);
            }
            break;  
        case 8: // ang
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_angle);
            }
            break;
        case 9: // debug
            {
                printed = snprintf(pcInsert, iInsertLen, "%s", "");
            }
            break;
        default:
            printed = 0;
            break;
    }
    return (u16_t)printed;
}

// Initialise the SSI handler
void ssi_init() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}