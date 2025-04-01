
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <atomic>
#include <cerrno>

#ifdef __linux__
#include <sys/fsuid.h>
#endif

#include "bcm2835.h"

#include "rp1lib.c"
#include "gpiochip_rp1.c"
#include "spi-dw.c"

#define RPI5_RP1_PERI_BASE 0x7c000000

bool isRP1 = false; // Pi5 uses RP1 hardware, otherwise assume Pi3 or Pi4 with bcm hardware

int SPI_num = -1;
int CS_num = -1;
int32_t SPI_freq = -1;

bool checkPiType() {

    //int  memfd;
    FILE *fp;

    if ((fp = fopen("/proc/device-tree/soc/ranges" , "rb")))
    {
        unsigned char buf[16];
        uint32_t base_address;
        uint32_t peri_size;
        if (fread(buf, 1, sizeof(buf), fp) >= 8)
        {
            base_address = (buf[4] << 24) |
                           (buf[5] << 16) |
                           (buf[6] << 8) |
                           (buf[7] << 0);

            peri_size = (buf[8] << 24) |
                        (buf[9] << 16) |
                        (buf[10] << 8) |
                        (buf[11] << 0);

            if (!base_address)
            {
                /* looks like RPI 4 or 5 */
                base_address = (buf[8] << 24) |
                               (buf[9] << 16) |
                               (buf[10] << 8) |
                               (buf[11] << 0);

                peri_size = (buf[12] << 24) |
                            (buf[13] << 16) |
                            (buf[14] << 8) |
                            (buf[15] << 0);
            }

            //printf("\nRPi peripheral: Base address 0x%08x size 0x%08x\n", base_address, peri_size);
        }

        if (base_address == BCM2835_RPI2_PERI_BASE)
        {
            printf( "Raspberry Pi 3, using BCM2835 driver\n");
            //bcm = true;
        }
        else if (base_address == BCM2835_RPI4_PERI_BASE)
        {
            printf("Raspberry Pi 4, using BCM2835 driver\n");
            //bcm = true;
        }
        else if (peri_size == RPI5_RP1_PERI_BASE)
        {
            // on the RPi 5, the base address is in the location of the peripheral size
            printf("Raspberry Pi 5, using RP1 driver\n");
            isRP1 = true;
        }
        else
        {
            printf("Error, compatible RPi not detected\n");
            return false;
        }

        fclose(fp);
    }

    return true;
}

struct WithRoot
{
    WithRoot();
    ~WithRoot();
    static std::atomic<int> level;
};

std::atomic<int> WithRoot::level;
static uid_t euid, ruid;

WithRoot::WithRoot() {
    if(!level++) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

WithRoot::~WithRoot() {
    if(!--level) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

#define WITH_ROOT WithRoot root

int rtapi_open_as_root(const char *filename, int mode) {
    WITH_ROOT;
    int r = open(filename, mode);
    if(r < 0) return -errno;
    return r;
}

// This is the same as the standard bcm2835 library except for the use of
// "rtapi_open_as_root" in place of "open"

bool rt_bcm2835_init()
{
    int  memfd;
    bool  ok;
    FILE *fp;

    /* Figure out the base and size of the peripheral address block
    // using the device-tree. Required for RPi2/3/4, optional for RPi 1
    */
    if ((fp = fopen(BMC2835_RPI2_DT_FILENAME , "rb")))
    {
        unsigned char buf[16];
        uint32_t base_address;
        uint32_t peri_size;
        if (fread(buf, 1, sizeof(buf), fp) >= 8)
        {
            base_address = (buf[4] << 24) |
                           (buf[5] << 16) |
                           (buf[6] << 8) |
                           (buf[7] << 0);

            peri_size = (buf[8] << 24) |
                        (buf[9] << 16) |
                        (buf[10] << 8) |
                        (buf[11] << 0);

            if (!base_address)
            {
                /* looks like RPI 4 */
                base_address = (buf[8] << 24) |
                               (buf[9] << 16) |
                               (buf[10] << 8) |
                               (buf[11] << 0);

                peri_size = (buf[12] << 24) |
                            (buf[13] << 16) |
                            (buf[14] << 8) |
                            (buf[15] << 0);
            }
            /* check for valid known range formats */
            if ((buf[0] == 0x7e) &&
                (buf[1] == 0x00) &&
                (buf[2] == 0x00) &&
                (buf[3] == 0x00) &&
                ((base_address == BCM2835_PERI_BASE) || (base_address == BCM2835_RPI2_PERI_BASE) || (base_address == BCM2835_RPI4_PERI_BASE)))
            {
                bcm2835_peripherals_base = (off_t)base_address;
                bcm2835_peripherals_size = (size_t)peri_size;
                if( base_address == BCM2835_RPI4_PERI_BASE )
                {
                    //pud_type_rpi4 = 1;
                }
            }

        }

        fclose(fp);
    }
    /* else we are prob on RPi 1 with BCM2835, and use the hardwired defaults */

    /* Now get ready to map the peripherals block
     * If we are not root, try for the new /dev/gpiomem interface and accept
     * the fact that we can only access GPIO
     * else try for the /dev/mem interface and get access to everything
     */
    memfd = -1;
    ok = false;
    if (geteuid() == 0)
    {
        /* Open the master /dev/mem device */
        if ((memfd = rtapi_open_as_root("/dev/mem", O_RDWR | O_SYNC) ) < 0)
        {
            fprintf(stderr, "bcm2835_init: Unable to open /dev/mem: %s\n",
                    strerror(errno)) ;
            goto exit;
        }

        /* Base of the peripherals block is mapped to VM */
        bcm2835_peripherals = (uint32_t*)mapmem("gpio", bcm2835_peripherals_size, memfd, bcm2835_peripherals_base);
        if (bcm2835_peripherals == MAP_FAILED) goto exit;

        /* Now compute the base addresses of various peripherals,
      // which are at fixed offsets within the mapped peripherals block
      // Caution: bcm2835_peripherals is uint32_t*, so divide offsets by 4
      */
        bcm2835_gpio = bcm2835_peripherals + BCM2835_GPIO_BASE/4;
        bcm2835_pwm  = bcm2835_peripherals + BCM2835_GPIO_PWM/4;
        bcm2835_clk  = bcm2835_peripherals + BCM2835_CLOCK_BASE/4;
        bcm2835_pads = bcm2835_peripherals + BCM2835_GPIO_PADS/4;
        bcm2835_spi0 = bcm2835_peripherals + BCM2835_SPI0_BASE/4;
        bcm2835_bsc0 = bcm2835_peripherals + BCM2835_BSC0_BASE/4; /* I2C */
        bcm2835_bsc1 = bcm2835_peripherals + BCM2835_BSC1_BASE/4; /* I2C */
        bcm2835_st   = bcm2835_peripherals + BCM2835_ST_BASE/4;
        bcm2835_aux  = bcm2835_peripherals + BCM2835_AUX_BASE/4;
        bcm2835_spi1 = bcm2835_peripherals + BCM2835_SPI1_BASE/4;

        ok = true;
    }
    else
    {
        /* Not root, try /dev/gpiomem */
        /* Open the master /dev/mem device */
        if ((memfd = open("/dev/gpiomem", O_RDWR | O_SYNC) ) < 0)
        {
            fprintf(stderr, "bcm2835_init: Unable to open /dev/gpiomem: %s\n",
                    strerror(errno)) ;
            goto exit;
        }

        /* Base of the peripherals block is mapped to VM */
        bcm2835_peripherals_base = 0;
        bcm2835_peripherals = (uint32_t*)mapmem("gpio", bcm2835_peripherals_size, memfd, bcm2835_peripherals_base);
        if (bcm2835_peripherals == MAP_FAILED) goto exit;
        bcm2835_gpio = bcm2835_peripherals;
        ok = true;
    }

exit:
    if (memfd >= 0)
        close(memfd);

    if ( ! ok )
        bcm2835_close();

    return ok;
}

bool bcm2835_setupSPI() {

    // Set the SPI0 pins to the Alt 0 function to enable SPI0 access, setup CS register
    // and clear TX and RX fifos
    if (!bcm2835_spi_begin())
    {
        printf("bcm2835_spi_begin failed. Are you running with root privlages??\n");
        return false;
    }

    // Configure SPI0
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default

    //bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128);		// 3.125MHz on RPI3
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);		// 6.250MHz on RPI3
    //bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);		// 12.5MHz on RPI3

    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default


    /* RPI_GPIO_P1_19        = 10 		MOSI when SPI0 in use
 * RPI_GPIO_P1_21        =  9 		MISO when SPI0 in use
 * RPI_GPIO_P1_23        = 11 		CLK when SPI0 in use
 * RPI_GPIO_P1_24        =  8 		CE0 when SPI0 in use
 * RPI_GPIO_P1_26        =  7 		CE1 when SPI0 in use
     */

    // Configure pullups on SPI0 pins - source termination and CS high (does this allows for higher clock frequencies??? wiring is more important here)
    bcm2835_gpio_set_pud(RPI_GPIO_P1_19, BCM2835_GPIO_PUD_DOWN);	// MOSI
    bcm2835_gpio_set_pud(RPI_GPIO_P1_21, BCM2835_GPIO_PUD_DOWN);	// MISO
    bcm2835_gpio_set_pud(RPI_GPIO_P1_24, BCM2835_GPIO_PUD_UP);

    return true;

}




int rt_rp1lib_init(void)
{
    uint64_t phys_addr = RP1_BAR1;

    DEBUG_PRINT("Initialising RP1 library: %s\n", __func__);

    // rp1_chip is declared in gpiochip_rp1.c
    chip = &rp1_chip;

    inst = rp1_create_instance(chip, phys_addr, NULL);
    if (!inst)
        return -1;

    inst->phys_addr = phys_addr;

    // map memory
    inst->mem_fd = rtapi_open_as_root("/dev/mem", O_RDWR | O_SYNC);
    if (inst->mem_fd < 0)
        return errno;

    inst->priv = mmap(
        NULL,
        RP1_BAR1_LEN,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        inst->mem_fd,
        inst->phys_addr
        );

    DEBUG_PRINT("Base address: %11lx, size: %x, mapped at address: %p\n", inst->phys_addr, RP1_BAR1_LEN, inst->priv);

    if (inst->priv == MAP_FAILED)
        return errno;

    return 1;
}

bool rpispi_init() {

    if ( isRP1 ) {

        // the following stuff causes segfaults instead of returning nicely if user is not root
        if ( getuid() != 0 ) {
            printf("%s", "Need to be running as root to go further!\n");
            return false;
        }

        if ( ! rt_rp1lib_init() )
        {
            printf("rt_rp1lib_init failed. Are you running as root?\n");
            return false;
        }

        if (SPI_num == -1) SPI_num = 0; // default to SPI0
        if (CS_num == -1) CS_num = 0; // default to CS0
        if (SPI_freq == -1) SPI_freq = 6250000; // default to 20MHz

        if ( ! rp1spi_init(SPI_num, CS_num, SPI_MODE_0, SPI_freq) )  // SPIx, CSx, mode, freq
        {
            printf("rp1spi_init failed.\n");
            return false;
        }

        return true;
    }
    else {
        if (!rt_bcm2835_init())
        {
            printf("rt_bcm2835_init failed. Are you running as root?\n");
            return false;
        }

        if ( ! bcm2835_setupSPI() ) {
            printf("bcm2835_setupSPI failed.\n");
            return false;
        }

        return true;
    }

}

void rpispi_transfernb(char* tbuf, char* rbuf, uint32_t len) {
    if ( isRP1 ) {
        rp1spi_transfer(0, tbuf, rbuf, len);
    }
    else {
        bcm2835_spi_transfernb(tbuf, rbuf, len);
    }
}

bool rpispi_close() {
    if ( isRP1 ) {
        return (rp1lib_deinit() == 0);
    }
    else {
        return bcm2835_close();
    }
}







