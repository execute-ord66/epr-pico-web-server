#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "logic.h"

const char * ssi_tags[] = {"colours", "state", "rot", "tan", "dist", "ang"};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    size_t printed;
    switch (iIndex) {
        case 0: // colours
            {
                printed = snprintf(pcInsert, iInsertLen, "%d%d%d%d%d", 
                    sensor_colours[0], sensor_colours[1], sensor_colours[2], sensor_colours[3], sensor_colours[4]);
            }
            break;
        case 1: // state
            {
                const char* state_strings[] = {"IDLE", "CAL", "MAZE", "SOS"};
                const char* internal_state_strings[] = {"MOVING FORWARD", "DETERMINE COLOUR/ANGLE", "REVERSING", "ROTATING"};
                if (current_state != STATE_MAZE)
                    printed = snprintf(pcInsert, iInsertLen, "%s", state_strings[current_state]);
                else
                    printed = snprintf(pcInsert, iInsertLen, "%s - %s", state_strings[current_state], internal_state_strings[internal_state]);
            }
            break;
        case 2: // rot
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_rotation);
            }
            break;
        case 3: // tan
            {
                printed = snprintf(pcInsert, iInsertLen, "%d %d", last_speedL, last_speedR);
            }
            break;
        case 4: // dist
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_distance);
            }
            break;  
        case 5: // ang
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", last_angle);
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
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}