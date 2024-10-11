#define UART_ID uart0
#define BAUD_RATE 19200
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define BUFFER_SIZE 4
#define PeakAmplitude 1.1f
#include <stdlib.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#include "lwipopts.h"
#include "logic.h"
#include "cgi.h"
#include "ssi.h"
#include "navigation.c"

#if LWIP_MDNS_RESPONDER
static void srv_txt(struct mdns_service *service, void *txt_userdata)
{
  err_t res;
  LWIP_UNUSED_ARG(txt_userdata);

  res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}
#endif

static size_t get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest_in) {
    static const char hexchr[16] = "0123456789ABCDEF";
    uint8_t mac[6];
    char *dest = dest_in;
    assert(chr_off + chr_len <= (2 * sizeof(mac)));
    cyw43_hal_get_mac(idx, mac);
    for (; chr_len && (chr_off >> 1) < sizeof(mac); ++chr_off, --chr_len) {
        *dest++ = hexchr[mac[chr_off >> 1] >> (4 * (1 - (chr_off & 1))) & 0xf];
    }
    return dest - dest_in;
}

bool blTouched = false;
bool blMyTurn = false;
bool blPeak = false;
bool blReset = false;
bool blDirection = false; //False = -Angle incidence; True = +Angle incidence
bool blSecondBK = false;
bool blQTP2_instantDistance = false;
bool blQTP2_instant5deg = false;
bool blQTP3_FirstReverse = false;
bool blAngleChanged = false;
bool blHasStopped = false;
bool blEOM = false;
bool tempBool = false;
uint8_t current_state = STATE_IDLE;
uint8_t internal_state = STATE_IDLE;
uint8_t colour = COLOUR_WHITE;
u16_t angle = 0;
u16_t prev_angle = 0;
u16_t target_angle = 0;
u16_t last_rotation = 0;
u16_t last_speedL = 0;
u16_t last_speedR = 0;
u16_t last_distance = 0;
u16_t last_angle = 0;
// const float conversion_factor = 3.3f / (1 << 12);
uint8_t sensor_colours[5] = {0};
u16_t reverse_distance = 0;
uint8_t distance_traveled_after_first_sensor;
u16_t distance = 0;

// void process_packet(uint8_t* buffer) {
//     printf("Received packet: %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
// }

void init_uart() {
    // uart_init(UART_ID, 2400);
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    // uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

}

void uart_rx_interrupt() {
    while (uart_is_readable(UART_ID)) {
        static uint8_t buffer[4];
        static uint8_t buffer_index = 0;

        buffer[buffer_index++] = uart_getc(UART_ID);

        if (buffer_index == 4) {
            // printf("Received packet: %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
            process_packet(buffer);
            buffer_index = 0;
        }
    }
}
void WaitForTurn() {
    while(!blMyTurn) {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
        sleep_ms(30);
    }
    blMyTurn = false;
}

void WaitForTurnWithTimeout() {
    static const uint32_t timeout_ms = 1000;
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while(!blMyTurn && absolute_time_diff_us(get_absolute_time(), start_time) < timeout_ms) {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
        sleep_ms(30);
    }
    blMyTurn = false;
}

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    //Networking
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    char hostname[sizeof(CYW43_HOST_NAME) + 4];
    memcpy(&hostname[0], CYW43_HOST_NAME, sizeof(CYW43_HOST_NAME) - 1);
    get_mac_ascii(CYW43_HAL_MAC_WLAN0, 8, 4, &hostname[sizeof(CYW43_HOST_NAME) - 1]);
    hostname[sizeof(hostname) - 1] = '\0';
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);

    init_uart();
    irq_set_exclusive_handler(UART0_IRQ, uart_rx_interrupt);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);

    while(cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 3000) != 0) {
        printf("Failed to connect to WiFi network\n");
        // uart_puts(UART_ID, "Failed to connect to WiFi network\n");
    }

    printf("\nReady, running httpd at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

#if LWIP_MDNS_RESPONDER
    // Setup mdns
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    printf("mdns host name %s.local\n", hostname);

    #if LWIP_VERSION_MAJOR >= 2 && LWIP_VERSION_MINOR >= 2
        mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
        mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    #else
        mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname, 60);
        mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, 60, srv_txt, NULL);
    #endif
    cyw43_arch_lwip_end();
#endif
    cyw43_arch_lwip_begin();
    httpd_init();
    cgi_init();
    ssi_init(); 
    cyw43_arch_lwip_end();
    sensor_colours[0] = COLOUR_WHITE;
    sensor_colours[1] = COLOUR_WHITE;
    sensor_colours[2] = COLOUR_WHITE;
    sensor_colours[3] = COLOUR_WHITE;
    sensor_colours[4] = COLOUR_WHITE;

    // send_packet(STATE_IDLE << 6 | PACKET_SNC << 4 | 0, tempBool, 0, 0);

    WaitForTurn();
    blEOM = false;
    while(true) {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
        switch (current_state) {
            case STATE_IDLE:
                while(blEOM && UseHub) {
                    tight_loop_contents();
                }
                blEOM = false;

                tempBool = blTouched;
                send_packet(STATE_IDLE << 6 | PACKET_SNC << 4 | 0, tempBool, 0, 0);
                if (tempBool) {
                    current_state = STATE_CAL;
                    blTouched = false;
                    blMyTurn = false;
                }
                break;
            case STATE_CAL:
                WaitForTurn();
                tempBool = blTouched;
                    send_packet(STATE_CAL << 6 | PACKET_SNC << 4 | 0, tempBool, 0, 0);
                    if (tempBool) {
                        current_state = STATE_MAZE;
                        blTouched = false;
                        blSecondBK = false;
                        if (UseHub)
                            sleep_ms(100);
                        send_packet(STATE_MAZE << 6 | PACKET_SNC << 4 | 1, 0, 0, 0);
                        if (UseHub)
                            sleep_ms(100);
                        send_packet(STATE_MAZE << 6 | PACKET_SNC << 4 | 2, 0, 0, 0);
                        blMyTurn = false;
                        if (UseHub)
                            sleep_ms(100);
                        move_forward(fast_speed);
                        internal_state = MOVING_FORWARD;
                        break;
                    }
                 else {
                    break;
                }
                send_packet(STATE_CAL << 6 | PACKET_SNC << 4 | 0, 0, 0, 0);
                break;
            case STATE_MAZE:
                WaitForTurn();

                if (blEOM) {
                    current_state = STATE_IDLE;
                    continue;
                };

                if (check_peak_and_touch()) continue;

                switch (internal_state)
                {
                case MOVING_FORWARD:
                    
                    if (sensor_colours[1] != COLOUR_WHITE) {
                        colour = sensor_colours[1];
                        if (last_angle <=5 && (colour == COLOUR_RED || colour == COLOUR_GREEN)) {
                            move_forward(fast_speed);
                            blSecondBK = false;
                            continue;
                        }
                        
                        stop();
                        blHasStopped = false;
                        blAngleChanged = false;
                        angle = last_angle;
                        internal_state = DETERMINE_COLOUR;
                        
                    } else if (sensor_colours[3] != COLOUR_WHITE) {
                        colour = sensor_colours[3];
                        if (last_angle <=5 && (colour == COLOUR_RED || colour == COLOUR_GREEN)) {
                            move_forward(fast_speed);
                            blSecondBK = false;
                            continue;
                        }
                        stop();
                        blHasStopped = false;
                        blAngleChanged = false;
                        angle = last_angle;
                        internal_state = DETERMINE_COLOUR;
                    } else
                    {
                        if (sensor_colours[0] != COLOUR_WHITE) {
                            colour = sensor_colours[0];
                        } else if (sensor_colours[4] != COLOUR_WHITE) {
                            colour = sensor_colours[4];
                        } else if (sensor_colours[2] != COLOUR_WHITE) {
                            colour = sensor_colours[2];
                        } else {
                            colour = COLOUR_WHITE;
                            move_forward(fast_speed);
                            continue;
                        }


                        if (blHasStopped) {
                            if (distance >= 39 || (last_distance >= 119 && blQTP2_instantDistance && blQTP3_FirstReverse)) {
                               stop();
                               angle = 46;
                               internal_state = DETERMINE_COLOUR;
                               blHasStopped = false; 
                               continue;
                            
                            }

                            move_forward(slow_speed);
                            blQTP2_instantDistance = false;
                            distance = last_distance;

                        } else {
                            if (blAngleChanged && last_angle <= 5 && (colour == COLOUR_RED || colour == COLOUR_GREEN)) {
                                move_forward(fast_speed);
                                continue;
                            };

                            stop();
                            distance = 0;
                            blAngleChanged = false;
                            blQTP3_FirstReverse = false;
                            blHasStopped = true;
                            blQTP2_instantDistance = true;
                        };
                    }                    
                    
                    break;
                //Move forward till Next sensor hits a colour.
                case DETERMINE_COLOUR:
                    blDirection = !(sensor_colours[0] != COLOUR_WHITE || sensor_colours[1] != COLOUR_WHITE);

                    if (colour == COLOUR_BLACK || colour == COLOUR_BLUE) {
                        if (angle <= 45) {
                            calculate_reverse_distance(angle);
                            internal_state = REVERSING;
                            move_backward(fast_speed);

                            if (blDirection) {
                                target_angle = 90+angle;
                            } else {
                                target_angle = 90-angle;
                            }

                            blDirection = true;

                            if (blSecondBK) {
                                target_angle += 90;
                                blSecondBK = false;
                                blDirection = false;
                            } else {
                                blSecondBK = true;
                            }
                        } else {
                            calculate_reverse_distance(45);
                            internal_state = REVERSING;
                            move_backward(fast_speed);
                            target_angle = 5;
                            blDirection = !blDirection;
                        }
                        blQTP3_FirstReverse = false;
                    } else if (colour == COLOUR_RED || colour == COLOUR_GREEN) {
                        if (angle <= 5) {
                            internal_state = MOVING_FORWARD;
                            blSecondBK = false;
                            move_forward(slow_speed);
                            break;
                        } else if (angle <= 45) {
                            calculate_reverse_distance(angle);
                            move_backward(fast_speed);
                            internal_state = REVERSING;
                            blQTP3_FirstReverse = false;
                            target_angle = angle;
                            break;
                        } else {
                            calculate_reverse_distance(45);
                            move_backward(fast_speed);
                            internal_state = REVERSING;
                            blQTP3_FirstReverse = false;
                            target_angle = 5;
                        }
                    }

                    break;
                case REVERSING:
                    if (distance >= reverse_distance || (blQTP3_FirstReverse && last_distance >= reverse_distance)) {
                        stop();
                        blQTP2_instant5deg = false;
                        internal_state = ROTATING;
                        angle = 0;
                    } else {
                        move_backward(fast_speed);
                    }
                    distance = last_distance;
                    blQTP3_FirstReverse = true;


                    break;
                case ROTATING:
                    if (angle >= target_angle || (blQTP2_instant5deg && target_angle <= 5) || (blQTP2_instant5deg && last_rotation >= target_angle)) {
                        move_forward(fast_speed);
                        internal_state = MOVING_FORWARD;
                    } else {
                        rotate(target_angle);
                        angle = last_rotation;
                        blQTP2_instant5deg = true;
                    }

                    
                    break;
                default:
                    break;
                }
              break;   
            case STATE_SOS:
                WaitForTurnWithTimeout();
                tempBool = false;
                while(true) {
                    sleep_ms(1000);
                    bool blPeak =  false;
                    float_t max = 0;
                    float_t min = 3.3;
                    uint16_t count = 0;
                    while(count < 1000 && !blPeak) {
                        float_t voltage = adc_read() * conversion_factor;
                        max = fmax(max, voltage);
                        min = fmin(min, voltage);
                        count++;
                    }
                    blPeak = (max - min > PeakAmplitude);
                    send_packet(STATE_SOS << 6 | PACKET_SNC << 4 | 0, tempBool, 0, 0);
                    if (!tempBool) {
                        current_state = STATE_MAZE;
                        blPeak = false;
                        blMyTurn = true;
                        break;
                    }
                }
                break;
        }
        sleep_ms(350);

        if (blReset) {
            blReset = false || (current_state != STATE_IDLE);
            current_state = STATE_IDLE;
            blTouched = false;
            blPeak = false;
            blSecondBK = false;
            angle = 0;
            target_angle = 0;
            distance = 0;
            colour = COLOUR_WHITE;
            UseHub = true;
        }
    }
#if LWIP_MDNS_RESPONDER
    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
#endif
    // cyw43_arch_deinit();
}