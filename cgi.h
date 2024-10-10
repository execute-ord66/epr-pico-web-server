#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "logic.h"

// CGI handler for LED control
const char * cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (strcmp(pcParam[0], "led") == 0) {
        if (strcmp(pcValue[0], "0") == 0)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        else if (strcmp(pcValue[0], "1") == 0)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }
    return "/content.shtml";
}

// CGI handler for peak detection in SOS state
const char * cgi_peak_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (current_state == STATE_SOS || current_state == STATE_MAZE) {
        if (strcmp(pcParam[0], "peak") == 0) {
            if (strcmp(pcValue[0], "0") == 0) {
                blPeak = false;
            }
            else if (strcmp(pcValue[0], "1") == 0) {
                blPeak = true;
            }
        }
    }
    return "/content.shtml";
}

// CGI handler for starting the system (simulating touch)
const char * cgi_start_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (current_state != STATE_SOS) {
        blTouched = true;
    }    
    return "/content.shtml";
}

// CGI handler for resetting the system
const char * cgi_reset_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (strcmp(pcParam[0], "reset") == 0) {
        blReset = false;
        current_state = STATE_IDLE;
        // debug_string = "RESET pressed";
        // count_packet = 0;
    }    
    return "/content.shtml";
}

// New CGI handler for manual control (for testing purposes)
const char * cgi_manual_control_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (strcmp(pcParam[0], "action") == 0) {
        if (strcmp(pcValue[0], "forward") == 0) {
            move_forward(fast_speed);
        } else if (strcmp(pcValue[0], "backward") == 0) {
            move_backward(slow_speed);
        } else if (strcmp(pcValue[0], "left") == 0) {
            rotate(-90);
        } else if (strcmp(pcValue[0], "right") == 0) {
            rotate(90);
        } else if (strcmp(pcValue[0], "stop") == 0) {
            stop();
        }
    }
    return "/content.shtml";
}

// New CGI handler for forcing state changes (for testing purposes)
const char * cgi_force_state_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    if (strcmp(pcParam[0], "state") == 0) {
        int new_state = atoi(pcValue[0]);
        if (new_state >= STATE_IDLE && new_state <= STATE_SOS) {
            current_state = new_state;
            // debug_string = "State changed manually";
        }
    }
    return "/content.shtml";
}

// tCGI Struct
static const tCGI cgi_handlers[] = {
    {"/led.cgi", cgi_led_handler},
    {"/peak.cgi", cgi_peak_handler},
    {"/start.cgi", cgi_start_handler},
    {"/reset.cgi", cgi_reset_handler},
    {"/manual.cgi", cgi_manual_control_handler},
    {"/force_state.cgi", cgi_force_state_handler}
};

void cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, 6);
}