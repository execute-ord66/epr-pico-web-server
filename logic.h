#ifndef logic_h
#define logic_h

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Settings
const char WIFI_SSID[] = "Onsen";
const char WIFI_PASSWORD[] = "karapara";
const float conversion_factor = 3.3f / (1 << 12);
bool UseHub = false;

const uint8_t fast_speed = 40; //mm/s
const uint8_t slow_speed = 30; //mm/s
const uint8_t angle_to_reverse[46] = {45, 45, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
       44, 44, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 50, 50, 51, 52,
       53, 54, 55, 56, 58, 59, 60, 62, 63, 65, 67, 69};

//Essential variables
extern bool blDirection;
extern uint8_t current_state;
extern uint8_t internal_state;
extern u16_t last_rotation;
extern u16_t last_speedL;
extern u16_t last_speedR;
extern u16_t last_distance;
extern u16_t distance;
extern u16_t last_angle;
extern u16_t prev_angle;
extern u16_t angle;
extern u16_t target_angle;

extern uint8_t sensor_colours[5];
extern uint8_t colour;
extern u16_t reverse_distance;

extern bool blTouched;
extern bool blMyTurn;
extern bool blSecondBK;
extern bool blQTP2_instantDistance;
extern bool blQTP2_instant5deg;
extern bool blQTP3_FirstReverse;
extern bool blPeak;
extern bool blReset;
extern bool blAngleChanged;
extern bool blHasStopped;
extern bool blEOM;

// State definitions
#define STATE_IDLE 0
#define STATE_CAL 1
#define STATE_MAZE 2
#define STATE_SOS 3

//Internal state definitions
#define MOVING_FORWARD 0
#define DETERMINE_COLOUR 1
#define REVERSING 2
#define ROTATING 3

// Color definitions
#define COLOUR_WHITE 0
#define COLOUR_RED 1
#define COLOUR_GREEN 2
#define COLOUR_BLUE 3
#define COLOUR_BLACK 4

// Packet types
#define PACKET_HUB 0x00
#define PACKET_SNC 0x01
#define PACKET_MDPS 0x02
#define PACKET_SENS 0x03

// Struct for UART packets
typedef struct {
    uint8_t Control;
    uint8_t DAT1;
    uint8_t DAT0;
    uint8_t DEC;
} UARTpacket_t;


// Function prototypes
void init_uart();
void process_packet(const uint8_t *buffer);
void send_packet(uint8_t control, uint8_t dat1, uint8_t dat0, uint8_t dec);
void handle_colour_detection(uint8_t colour);
void rotate(uint16_t angle);
void move_forward(uint8_t speed);
void move_backward(uint8_t speed);
void stop();

// Process different types of packets
void process_sensor_packet(uint8_t sys, uint8_t ist, const UARTpacket_t *packet);
void process_colors_packet(const UARTpacket_t *packet);
void process_incidence_angle_packet(const UARTpacket_t *packet);
void process_mdps_packet(uint8_t sys, uint8_t ist, const UARTpacket_t *packet);

// Navigation and movement functions
bool check_peak_and_touch();
bool check_condition(uint8_t packet_type, uint8_t condition_number, bool *condition);
bool handle_rotation_interrupts();
bool wait_if_necessary();
bool handle_backward_movement_interrupts();

// Helper functions for movement
void rotate(uint16_t angle);
void move_forward(uint8_t speed);
void move_backward(uint8_t speed);
void stop();


#endif /* logic_h */