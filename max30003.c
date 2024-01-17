#include "pico/stdlib.h"
#include "hardware/structs/spi.h"
#include "hardware/regs/dreq.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include "pico.h"
#include "hardware/spi.h"
#include <stdio.h>
#include <string.h>
#include "max30003-reg.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"

// SPI communication Pins
#define SCLK 18
#define SDA 19
#define SDI 16
#define CS 17

// define Intruppt pin
#define INTPIN 20
/*  First Seven Bits endicates the address of Register
    Last Bit indicates the operation
    Last Bit 0 indicates Write operation
    Last Bit 1 indicates Read operation
*/

#define WREG 0x00
#define RREG 0x01
/* SPI SPEED */
#define MAX30003_SPI_SPEED 50000000

/* These Vars are used to store last value of */
unsigned int RRinterval;
signed long ecgdata;
unsigned int heartRate;

/*  Make the CS pin high to deselect device for SPI communication */
void cs_deselect()
{
    gpio_put(CS, true);
}

/*  Make the CS pin LOW to select device for SPI communication */
void cs_select()
{
    gpio_put(CS, false);
}
/* This openocd command i used to connect debugger */
// sudo openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s tcl

/* This read_registers function is used to read the perticuler Register
    reg: Register address you want to read
    buf: buffer in which you want to store readed data
    len: len of data you want to read
*/
static void read_registers(uint8_t reg, uint8_t *buf, int len)
{
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.
    /* Shift register address to left to store address on First 7 bits and make last bit 1 for Read operation */
    uint8_t read = (reg << 1) | RREG;
    cs_select();
    spi_write_blocking(spi0, &read, 1);
    spi_read_blocking(spi0, 0, buf, len);
    cs_deselect();
}

/*  max30003RegWrite is used to write values on register
    reg: Register address
    data: data you want to write
*/
static void max30003RegWrite(uint8_t reg, uint32_t data)
{
    uint8_t read[3];
    /*  Shift register address to left to store address on First 7 bits and make last bit 0 for Write operation
        Make buffer of 32 bit(4 Bytes) values in this buffer first values shows address where to do write operation
        and last 3 Bytes are values we want to write on first register
    */
    uint8_t buffer[4] = {(reg << 1) | WREG, data >> 16, data >> 8, data};
    cs_select();
    spi_write_blocking(spi0, buffer, 4);
    cs_deselect();
}

void initiaLizeSPI()
{
    /* initialise the SPI */
    spi_init(spi0, MAX30003_SPI_SPEED);
    spi_set_slave(spi0, false);
    gpio_set_function(SDI, GPIO_FUNC_SPI);
    gpio_set_function(SCLK, GPIO_FUNC_SPI);
    gpio_set_function(SDA, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(SDI, SCLK, SDA, GPIO_FUNC_SPI));
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(CS);
    gpio_set_dir(CS, GPIO_OUT);
    cs_deselect();
    // Make the CS pin available to picotool
    bi_decl(bi_1pin_with_name(CS, "SPI CS"));
    spi_set_format(spi0, 8, 0, 0, SPI_MSB_FIRST);
}

/* max30003SetsamplingRate is used to setup a SAMPLING Rate */
void max30003SetsamplingRate(uint16_t samplingRate)
{
    uint8_t regBuff[4] = {0};
    read_registers(CNFG_ECG, regBuff, 4);

    switch (samplingRate)
    {
    case SAMPLINGRATE_128:
        regBuff[0] = (regBuff[0] | 0x80);
        break;

    case SAMPLINGRATE_256:
        regBuff[0] = (regBuff[0] | 0x40);
        break;

    case SAMPLINGRATE_512:
        regBuff[0] = (regBuff[0] | 0x00);
        break;

    default:
        printf("Wrong samplingRate, please choose between 128, 256 or 512");
        break;
    }

    unsigned long cnfgEcg;
    memcpy(&cnfgEcg, regBuff, 4);
    max30003RegWrite(CNFG_ECG, (cnfgEcg >> 8));
    /* Sync Configuration */
    max30003RegWrite(SYNCH, 0x0000);
    sleep_ms(10);
}

void max30003Begin()
{
    /* Reset MAX30003 to its original default state */
    max30003RegWrite(SW_RST, 0x000000);
    sleep_ms(10);
    /*  Configure generate settings of MAX30003
        Ultra-Low Power Lead-On Detection -> Disable (ULP mode is only active when the ECG channel is powered down/disabled)
        ECG Channel -> Enable
        DC Lead-Off Detection Enable -> Enable
    */
    max30003RegWrite(CNFG_GEN, 0x081007);
    sleep_ms(10);
    /* Configure Calibration */
    max30003RegWrite(CNFG_CAL, 0x720000); // 0x700000
    sleep_ms(10);
    /*  Configures the operation, settings, and functionality of the Input Multiplexer
        associated with the ECG channel */
    max30003RegWrite(CNFG_EMUX, 0x0B0000);
    sleep_ms(10);
    /* Configure ECG Channel
    D[14]    DHPF       1       ECG Channel Digital High-Pass Filter Cutoff Frequency
                                0 = Bypass (DC)
                                1 = 0.50Hz
    D[13:12] DLPF[1:0] 01       ECG Channel Digital Low-Pass Filter Cutoff Frequency
                                00 = Bypass (Decimation only, no FIR filter applied)
                                01 = approximately 40Hz (Except for 125 and 128sps settings) Note: See Table 33.
                                10 = approximately 100Hz (Available for 512, 256, 500, and 250sps ECG Rate selections only)
                                11 = approximately 150Hz (Available for 512 and 500sps ECG Rate selections only)
                                Note: See Table 29. If an unsupported DLPF setting is specified, the 40Hz setting
                                (DLPF[1:0] = 01) will be used internally; the CNFG_ECG register will continue to hold
                                the value as written, but return the effective internal value when read back.
*/
    max30003RegWrite(CNFG_ECG, 0x805000); // d23 - d22 : 10 for 250sps , 00:500 sps
    sleep_ms(10);
    /* Configure R to R Peak Detection Algorithm*/
    max30003RegWrite(CNFG_RTOR1, 0x3fc600);
    sleep_ms(10);
    /* Sync Configuration */
    max30003RegWrite(SYNCH, 0x0000);
    sleep_ms(10);
}

/* Read and Decode RR interval and Heart rate */
void getHRandRR(void)
{
    uint8_t regReadBuff[4];
    read_registers(RTOR, regReadBuff, 4);

    unsigned long RTOR_msb = (unsigned long)(regReadBuff[0]);
    unsigned char RTOR_lsb = (unsigned char)(regReadBuff[1]);
    unsigned long rtor = (RTOR_msb << 8 | RTOR_lsb);
    rtor = ((rtor >> 2) & 0x3fff);

    float hr = 60 / ((float)rtor * 0.0078125);
    heartRate = (unsigned int)hr;

    unsigned int RR = (unsigned int)rtor * (7.8125); // 8ms
    RRinterval = RR;
}

/* Read ECG data samples from MAX30003 */
void getEcgSamples(void)
{
    uint8_t regReadBuff[48];
    read_registers(ECG_FIFO, regReadBuff, 48);
    for (int i = 0; i < 48; i = i + 3)
    {
        int eTag = (int)((regReadBuff[i + 2] >> 3) & 0x7);
        // printf("Buffer: %x ", regReadBuff[i+2]);
        // printf("Shifted Buffer: %x ", regReadBuff[i+2] >> 3);
        if (eTag > 0)
        {
            printf("Value of ETag: %d\n", eTag);
        }
        // printf("ECG: %lu    ", ecgdata);
        if (eTag == 0 || eTag == 2)
        {
            /* This is valid sample */
            unsigned long data0 = (unsigned long)(regReadBuff[0]);
            data0 = data0 << 24;
            unsigned long data1 = (unsigned long)(regReadBuff[1]);
            data1 = data1 << 16;
            unsigned long data2 = (unsigned long)(regReadBuff[2]);
            data2 = data2 >> 6;
            data2 = data2 & 0x03;
            unsigned long data = (unsigned long)(data0 | data1 | data2);
            ecgdata = (signed long)(data);
            if (eTag == 2)
            {
                /*Last Valid Sample (EOF)*/
                return;
            }
        }
        else if (eTag == 1)
        {
            /*  This sample was taken while the ECG
                channel was in a FAST recovery mode.
                The voltage information is not valid,
                but the sample represents a valid time
                step.
                Action:
                Discard, note, or post-process this voltage sample, but increment the time base. Continue to gather data from the ECG FIFO.
            */
            printf("Data is not valid ETAG: 1\n");
        }
        else if (eTag == 3)
        {
            /*  Last Fast Mode Sample (EOF)
                See above (ETAG=001), but in addition, this is the last sample
                currently available in the FIFO (End of File indicator).
                Recommended Action:
                    Discard, note, or post-process this voltage
                    sample, but increment the time base.
                    Suspend read back operations on the ECG
                    FIFO until more samples are available.
            */
            printf("Data is not valid ETAG: 3\n");
            return;
        }
        else if (eTag == 5)
        {
            /*  FIFO Empty (Exception)
                This is an invalid sample provided in
                response to an SPI request to read an
                empty FIFO.
                Recommended Action:
                    Discard this sample, without incrementing
                    the time base.
                    Suspend read back operations on this FIFO
                    until more samples are available.
            */
            printf("There are not ECG samples to read Empty Data ETAG: 5\n");
            return;
        }
        else if (eTag == 7)
        {
            /*  FIFO Overflow (Exception)
                The FIFO has been allowed to overflow - the data is corrupted.
                Recommended Action:
                    issue a FIFO_RST command to clear the
                    FIFOs or re-SYNCH if necessary.
                    Note the corresponding halt and resumption
                    in ECG/BIOZ time/voltage records.
            */
            max30003RegWrite(FIFO_RST, 0x0);
            printf("FIFO is overflowed Data is currepted ETAG: 7\n");
            printf("issued FIFO_RST command\n");
            return;
        }
    }
}

/* Read Revesion Id of MAX30003 to indentify if MAX30003 is connected properly */
void max30003ReadInfo(void)
{
    /*  INFO (0x0F) Register Map
        Revision ID REV_ID[3:0]
        First four bits are 0 1 0 1
        Last four bits shows the Revesion Id
        0 1 0 1 REV_ID[3:0]
    */
    uint8_t readBuff[1];
    read_registers(INFO, readBuff, 1);
    printf("MAX3003 Revesion Id: %d\n", (readBuff[0] & 0x0f));
}

/* Read STATUS Register of MAX30003 to check what is current status of MAX30003 */
void readStatus(uint8_t *readBuff)
{
    read_registers(STATUS, readBuff, 3);
}

/* Setup Intruppts */
void setIntruppts()
{
    /*
        ENINT
        Lead off and Lead on Intrupt
        000100000000100000000011 (0x100803)
    */
    // max30003RegWrite(EN_INT2, 0x100803);
    /*  RR AND SAMPLING INT
        000000000000011000000001 (0x601)
    */
    max30003RegWrite(EN_INT, 0x000601);
    sleep_ms(10);
    /*  Manage Inrupt
        011110000000000000010110
        issu intrupt on every 4th sample instant
        Self-clear SAMP after approximately one-fourth of one data rate cycle
        Clear RRINT on RTOR Register Read Back
    */
    max30003RegWrite(MNGR_INT, 0x780016);
    sleep_ms(10);
    /* Sync Configuration */
    max30003RegWrite(SYNCH, 0x0000);
    sleep_ms(10);
}

/* Read Intruppt Configuration */
void readIntruppt()
{
    uint8_t readBuff[3];
    read_registers(EN_INT, readBuff, 3);
    printf("EN_INT STATUS0: %x  ", readBuff[0]);
    printf("STATUS1: %x  ", readBuff[1]);
    printf("STATUS2: %x\n", readBuff[2]);
    read_registers(EN_INT2, readBuff, 3);
    printf("EN_INT2 STATUS0: %x  ", readBuff[0]);
    printf("STATUS1: %x  ", readBuff[1]);
    printf("STATUS2: %x\n", readBuff[2]);
}

/* This is our callback function which is triggeres when we get Intruppt */
void getDataIntrupptCallback(uint gpio, uint32_t events)
{
    uint8_t status[3];
    /* Read Status to check for what event caused to trigger intrupt */
    readStatus(status);
    if ((int)(status[1] & 0x2) == 2)
    {
        // printf("Intruppt for SAMP\n");
        getEcgSamples();
        // printf("ECG: %lu    ", ecgdata);
    }

    if ((int)(status[1] & 0x4) == 4)
    {
        printf("Intruppt for RR Interval\n");
        getHRandRR();
        printf("RR Interval: %u    ", RRinterval);
        printf("Heart Rate: %u\n\n", heartRate);
    }
}

int main()
{
    /* initialise all stdio for printf */
    stdio_init_all();
    /* initialise SPI interface */
    initiaLizeSPI();
    /* Read MAX30003 Revesion ID */
    max30003ReadInfo();
    /* Configure MAX30003 */
    max30003Begin();
    /* Set Sampling Rate */
    max30003SetsamplingRate(SAMPLINGRATE_512);
    /* Set Intruppts */
    setIntruppts();
    /* Read Intruppt Configuration */
    readIntruppt();
    /* PUll UP Intruppt pin */
    gpio_pull_up(INTPIN);
    /* Setup intrupt on GPIO INTPIN */
    gpio_set_irq_enabled_with_callback(INTPIN, GPIO_IRQ_LEVEL_LOW, true, &getDataIntrupptCallback);
    while (1)
    {
        sleep_ms(1000);
    }
    return 0;
}
