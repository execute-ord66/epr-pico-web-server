#include "logic.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "hardware/adc.h"


void process_packet(const uint8_t *buffer) {
    UARTpacket_t *packet = (UARTpacket_t*)buffer;

    // printf("Received: (%d-%d-%d)| %02x | %02x | %02x |\n", sys, sub, ist, packet.DAT1, packet.DAT0, packet.DEC);
    switch (packet->Control) {
        case 0:
            blMyTurn = true;
            blEOM = false;
            break;
        case 1:
            blReset = false;
            current_state = STATE_IDLE;
        case 179:
            blEOM = true;
            current_state = STATE_IDLE;
            blMyTurn = !UseHub;
            break;
        case 177:
            process_colors_packet(packet);
            break;
        case 178:
            process_incidence_angle_packet(packet);
            blMyTurn = true;
            break;
        case 164:
            last_distance = (packet->DAT1 << 8) | packet->DAT0;
            break;
        case 163:
            last_speedR = packet->DAT1;
            last_speedL = packet->DAT0;
            break;
        case 162:
            last_rotation = (packet->DAT1 << 8) | packet->DAT0;
            if (packet->DEC == 3) last_rotation = -last_rotation;
            break;
        case 113:
            process_colors_packet(packet);
            blMyTurn = true;
            break;
        case 228:
            blMyTurn = true;
            break;
        default:
            break;
    }
}

void process_sensor_packet(uint8_t sys, uint8_t ist, const UARTpacket_t *packet) {
    if (ist == 1) {
        process_colors_packet(packet);
        blMyTurn = sys == 1;
    } else if (ist == 2) {
        process_incidence_angle_packet(packet);
        blMyTurn = true;
    }

    blReset = (sys == 3 && ist == 3);
}

void process_colors_packet(const UARTpacket_t *packet) {
    
    sensor_colours[4] = (packet->DAT0) & 0x07;
    sensor_colours[3] = (packet->DAT0 >> 3) & 0x07;
    sensor_colours[2] = (((packet->DAT1) & 0x01) << 2) | ((packet->DAT0 >> 6) & 0x03);
    sensor_colours[1] = (packet->DAT1 >> 1) & 0x07;
    sensor_colours[0] = (packet->DAT1 >> 4) & 0x07;
    if (packet->Control != 113)
        printf("Colours: %d %d %d %d %d\n", sensor_colours[0], sensor_colours[1], sensor_colours[2], sensor_colours[3], sensor_colours[4]);

    // if ((sensor_colours[4] != sensor_colours[3]) || (sensor_colours[0] != sensor_colours[1])) {
    //     // distance_traveled_after_first_sensor = last_distance;
    //     // cap_distance = 69;
    //     blDirection = (sensor_colours[4] != sensor_colours[3]); 
    //     // printf("Colour change detected on %s side\n", blDirection ? "right" : "left");
    // }
}

void process_incidence_angle_packet(const UARTpacket_t *packet) {
    if (last_angle != packet->DAT1) {
        blDirection = (sensor_colours[4] != COLOUR_WHITE) || (sensor_colours[3] != COLOUR_WHITE);
    }
    if (last_angle == packet->DAT1) return;
    printf("New angle: %d\n", last_angle);
    last_angle = packet->DAT1;
}

// void process_mdps_packet(uint8_t sys, uint8_t ist, const UARTpacket_t *packet) {
//     blMyTurn = (sys == 3 && ist == 4);
//     if (sys == 2 && ist == 2) {
//         last_rotation = (packet->DAT1 << 8) | packet->DAT0;
//         if (packet->DEC == 3) last_rotation = -last_rotation;
//     } else if (sys == 2 && ist == 3) {
//         last_speedR = packet->DAT1;
//         last_speedL = packet->DAT0;
//     } else if (sys == 2 && ist == 4) {
//         last_distance = (packet->DAT1 << 8) | packet->DAT0;
//     }
// }

void send_packet(uint8_t control, uint8_t dat1, uint8_t dat0, uint8_t dec) {
    uart_putc_raw(UART_ID, control);
    uart_putc_raw(UART_ID, dat1);
    uart_putc_raw(UART_ID, dat0);
    uart_putc_raw(UART_ID, dec);
    // printf("Sent: %d, %d, %d, %d\n", control, dat1, dat0, dec);
}



bool check_peak_and_touch() {
    bool blPeak =  false;
    float_t max = 0;
    float_t min = 3.3;
    int count = 0;
    while(count < 1000 && !blPeak) {
        float_t voltage = adc_read() * conversion_factor;
        max = fmax(max, voltage);
        min = fmin(min, voltage);
        count++;
    }
    blPeak = (max - min > PeakAmplitude);
    bool Temp = blPeak;
    // printf("Checked Peaking %d\n", Temp);
    if (check_condition(PACKET_SNC, 1, &Temp)) {
        current_state = STATE_SOS;
        blPeak = false;
        return true;
    }


    Temp = blTouched;
    // printf("Checked Touching %d\n", Temp);
    if (check_condition(PACKET_SNC, 2, &Temp)) {
        current_state = STATE_IDLE;
        blReset = true;
        blTouched = false;
        return true;
    }
    return false;
}

bool check_condition(uint8_t packet_type, uint8_t condition_number, bool *condition) {
    bool temp_condition = *condition;
    send_packet(STATE_MAZE << 6 | packet_type << 4 | condition_number, temp_condition, 0, 0);
    if (temp_condition) {
        *condition = false;
        return true;
    }
    return false;
}

void rotate(uint16_t angle) {
    printf("Rotate by %d in direction %d\n", angle, blDirection);
    uint8_t dec = (!blDirection) ? 2 : 3;
    send_packet(0x93, angle >> 8, angle & 0xFF, dec);
    blMyTurn = false;
}

void move_forward(uint8_t speed) {
    printf("Moving forward at %d mm/s\n", speed);
    send_packet(0x93, speed, speed, 0);
    blMyTurn = false;
}

void move_backward(uint8_t speed) {
    printf("Moving backwards at %d mm/s\n", speed);
    send_packet(0x93, speed, speed, 1);
    blMyTurn = false;
}

void calculate_reverse_distance(uint8_t angle) {
    reverse_distance = angle_to_reverse[angle];
}

void stop() {
    printf("Stopping\n");
    send_packet(0x93, 0, 0, 0);
    blMyTurn = false;
}