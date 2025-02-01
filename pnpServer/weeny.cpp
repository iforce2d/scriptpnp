
#include <stdio.h>
#include <cstring>

#include "bcm2835.h"
#include "weeny.h"

#define STEPBIT				22			// bit location in DDS accum
#define STEP_MASK			(1L<<STEPBIT)
#define STEP_OFFSET			(1L<<(STEPBIT-1))



#define PRU_DATA            0x64617461 	// "data" SPI payload
#define PRU_READ            0x72656164  // "read" SPI payload
#define PRU_WRITE           0x77726974  // "writ" SPI payload
#define PRU_ESTOP           0x65737470  // "estp" SPI payload


typedef struct __attribute__((__packed__))
{
    // this data goes from LinuxCNC to PRU

    int32_t header;                 // 4 bytes
    int32_t jointFreqCmd[JOINTS];   // 16 bytes
    uint8_t jointEnable;            // 1 byte
    uint16_t outputs;               // 2 bytes
    uint8_t rgb[6];                 // 6 bytes (3 bits per LED x 16 LEDs = 48 bits)
    uint16_t spindleSpeed;          // 2 bytes

    uint8_t microsteps[JOINTS];     // 4 bytes
    uint8_t rmsCurrent[JOINTS];     // 4 bytes

    uint8_t dummy[9];               // make up to same size as rxData_t
} txData_t;

typedef struct __attribute__((__packed__))
{
    // this data goes from PRU back to LinuxCNC

    int32_t header;                 // 4 bytes
    int32_t jointFeedback[JOINTS];  // 16 bytes
    int32_t jogcounts[4];           // 16 bytes
    uint16_t inputs;                // 2 bytes
    uint16_t adc[2];                // 4 bytes
    uint16_t pressure;              // 2 bytes
    int32_t loadcell;               // 4 bytes
} rxData_t;

typedef union
{
    rxData_t rx;
    txData_t tx;
} commPacket_t;

static rxData_t rxData;
static txData_t txData;

data_t data = {0};


int64_t 		accum[JOINTS] = { 0 };
int32_t 		old_count[JOINTS] = { 0 };
//static int32_t		accum_diff = 0;

void initWeenyData() {
    for (int i = 0; i < JOINTS; i++) {
        data.microsteps[i] = 16; // 4x microstepping
        data.rmsCurrent[i] = 200;
    }
    data.rgb[0] = 0b00010001;
    data.rgb[1] = 0b00000001;
}

void prepareTxData() {

    txData.header = PRU_WRITE;

    // Joint frequency commands
    for (int i = 0; i < JOINTS; i++)
    {
        txData.jointFreqCmd[i] = data.freq[i];
    }

    for (int i = 0; i < JOINTS; i++)
    {
        //if (*(data->stepperEnable[i]) == 1)
        if ( true )
        {
            txData.jointEnable |= (1 << i);
        }
        else
        {
            txData.jointEnable &= ~(1 << i);
        }
    }

//    for (int i = 0; i < DIGITAL_OUTPUTS; i++)
//    {
//        if ( data.outputs[i] == 1)
//        //if ( i == 0 )
//        {
//            txData.outputs |= (1 << i);		// output is high
//        }
//        else
//        {
//            txData.outputs &= ~(1 << i);	// output is low
//        }
//    }
    txData.outputs = data.outputs;
/*
    for (int i = 0; i < RGB_BITS; i++)
    {
        int bytePos = i / 8;
        int bitPos = i % 8;
        if ( data.rgb[i] == 1 )
        //if ( i == 0 || i == 4 || i == 8 )
        {
            txData.rgb[bytePos] |= (1 << bitPos);
        }
        else
        {
            txData.rgb[bytePos] &= ~(1 << bitPos);
        }
    }
*/
    memcpy(txData.rgb, data.rgb, 6);

//    float ssf = data.spindleSpeed;
//    if ( ssf < 0 )
//        ssf = 0;
//    else if ( ssf > 1 )
//        ssf = 1;
    txData.spindleSpeed = data.spindleSpeed;//(uint16_t)(ssf * 65535);

    for (int i = 0; i < JOINTS; i++)
    {
        uint8_t mstepsCode = 0xFF; // signifies no change, only allow values that make sense

        switch( data.microsteps[i] ) {
        //switch( 16 ) {
        case 256: mstepsCode = 0; break;
        case 128: mstepsCode = 1; break;
        case  64: mstepsCode = 2; break;
        case  32: mstepsCode = 3; break;
        case  16: mstepsCode = 4; break;
        case   8: mstepsCode = 5; break;
        case   4: mstepsCode = 6; break;
        case   2: mstepsCode = 7; break;
        case   0: mstepsCode = 8; break;
        default: break;
        }

        txData.microsteps[i] = mstepsCode;


        int32_t c = data.rmsCurrent[i];
        //int32_t c = 200;
        if ( c < 0 )
            c = 0;
        else if ( c > 2000 )
            c = 2000;
        c /= 10; // to fit into one byte
        txData.rmsCurrent[i] = (uint8_t)c;
    }


    // Data header
//    if ( txData.header != PRU_WRITE )
//        txData.header = PRU_READ;
}

void printSPIPacket() {
    printf("  ");
    for (unsigned int i = 0; i < sizeof(rxData); i++) {
        printf("%02X ", ((char*)&rxData)[i] );
    }
    printf("\n");
    printf("    jointFeedback: %d, %d, %d, %d\n", rxData.jointFeedback[0], rxData.jointFeedback[1], rxData.jointFeedback[2], rxData.jointFeedback[3]);
    printf("    jogcounts: %d, %d, %d, %d\n", rxData.jogcounts[0], rxData.jogcounts[1], rxData.jogcounts[2], rxData.jogcounts[3]);
    printf("    inputs: %d\n", rxData.inputs);
    printf("    adc: %d, %d\n", rxData.adc[0], rxData.adc[1]);
    printf("    pressure: %d\n", rxData.pressure);
    printf("    loadcell: %d\n", rxData.loadcell);
}

void processRxData() {

    static unsigned long processRxCount = 0;

    if ( rxData.header == PRU_DATA ) {

        if ( ! data.lastSPIPacketGood ) {// only print when changed from good to bad
            //printf("Good SPI payload: %x (count: %ld)\n", rxData.header, processRxCount);
            //printSPIPacket();
        }
        data.lastSPIPacketGood = true;

        for (int i = 0; i < JOINTS; i++)
        {
            // the PRU DDS accumulator uses 32 bit counter, this code converts that counter into 64 bits
//            int32_t accum_diff = rxData.jointFeedback[i] - old_count[i];
//            old_count[i] = rxData.jointFeedback[i];
//            accum[i] += accum_diff;

            //data.scale_recip[i] = (1.0 / STEP_MASK) / data.pos_scale[i];
//            double curr_pos = (double)(accum[i]-STEP_OFFSET) * (1.0 / STEP_MASK);
//            data.pos_fb[i] = (float)((curr_pos+0.5) / data.pos_scale[i]);

            data.pos_fb[i] = (float)(rxData.jointFeedback[i]) / data.pos_scale[i];
        }

        //printf("jointFeedback: %d\n",rxData.jointFeedback[1]);

        data.inputs = rxData.inputs;
        data.rotary = rxData.jogcounts[0];
        memcpy(data.adc, rxData.adc, 2*sizeof(uint16_t));
        data.pressure = rxData.pressure;
        data.loadcell = rxData.loadcell;
    }
    else {
        if ( (processRxCount == 0) || data.lastSPIPacketGood ) {// only print when changed from good to bad
            printf("Bad SPI payload: %x (count: %ld)\n", rxData.header, processRxCount);
            //printSPIPacket();
        }
        data.lastSPIPacketGood = false;
    }

    processRxCount++;
}

void spi_transfer()
{
    prepareTxData();

    // send and receive data to and from the weeny PRU concurrently
    bcm2835_spi_transfernb( (char*)&txData, (char*)&rxData, sizeof(commPacket_t) );

    processRxData();
}
