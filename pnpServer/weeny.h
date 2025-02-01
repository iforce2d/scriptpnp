
#ifndef WEENY_H
#define WEENY_H

typedef double real_t __attribute__((aligned(8)));
#define hal_float_t volatile real_t

#define JOINTS 4
#define DIGITAL_OUTPUTS		16
#define DIGITAL_INPUTS		16
#define RGB_BITS		48

typedef struct {
    bool                lastSPIPacketGood;

    hal_float_t 	pos_scale[JOINTS];		// param: steps per position unit
    //float 		scale_recip[JOINTS];		// reciprocal value used for scaling
    float 		freq[JOINTS];			// param: frequency command sent to PRU
    uint16_t            outputs;
    uint8_t             rgb[6];
    uint16_t     	spindleSpeed;
    uint16_t            microsteps[JOINTS];
    uint16_t            rmsCurrent[JOINTS];

    //uint8_t             inputs[DIGITAL_INPUTS];
    //hal_float_t 	adc[2]; // raw ADC values
    hal_float_t 	pos_fb[JOINTS];                 // pin: position feedback (position units)

    uint16_t inputs;
    int32_t rotary;
    uint16_t adc[2];
    uint16_t pressure;
    int32_t loadcell;

//    hal_float_t 	pos_cmd[JOINTS];		// pin: position command (position units)
//    hal_float_t 	vel_cmd[JOINTS];		// pin: velocity command (position units/sec)
    //hal_s32_t	count[JOINTS];                          // pin: psition feedback (raw counts)
//    hal_float_t 	freq_cmd[JOINTS];		// pin: frequency command monitoring, available in LinuxCNC
//    hal_float_t 	maxvel[JOINTS];			// param: max velocity, (pos units/sec)
//    hal_float_t 	maxaccel[JOINTS];		// param: max accel (pos units/sec^2)
//    hal_float_t	pgain[JOINTS];
//    hal_float_t	ff1gain[JOINTS];
//    hal_float_t	deadband[JOINTS];
//    float 		old_pos_cmd[JOINTS];		// previous position command (counts)
//    float 		old_pos_cmd_raw[JOINTS];	// previous position command (counts)
//    float 		old_scale[JOINTS];		// stored scale value
//    float		prev_cmd[JOINTS];
//    float		cmd_d[JOINTS];			// command derivative
} data_t;

void initWeenyData();
void spi_transfer();
void printSPIPacket();

extern data_t data;

#endif
