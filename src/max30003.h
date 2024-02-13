/*
    @author Premchand Gat
    Github: PremchandGat
    Email: Premchandg278@gmail.com
*/

/*------------------------------Register Map-------------------------------------*/
#pragma once

#define NO_OP 0x00
#define STATUS 0x01
#define EN_INT 0x02
#define EN_INT2 0x03
#define MNGR_INT 0x04
#define MNGR_DYN 0x05

/*  SW_RST (Software Reset) is a write-only register/
    command that resets the MAX30003 to its original default
    conditions at the end of the SPI SW_RST transaction
    (i.e. the 32nd SCLK rising edge). Execution occurs only
    if DIN[23:0] = 0x000000. The effect of a SW_RST is
    identical to power-cycling the device.
*/
#define SW_RST 0x08
/* SYNCH is used to Sync the configuration applied */
#define SYNCH 0x09
#define FIFO_RST 0x0A
#define INFO 0x0F
/*  CNFG_GEN is a read/write register which governs
    general settings, most significantly the master clock rate
    for all internal timing operations. Anytime a change to
    CNFG_GEN is made, there may be discontinuities in
    the ECG record and possibly changes to the size of the
    time steps recorded in the FIFOs. The SYNCH command
    can be used to restore internal synchronization resulting
    from configuration changes. Note when EN_ECG is logic-
    low, the device is in one of two ultra-low power modes
    (determined by EN_ULP_LON).
*/
#define CNFG_GEN 0x10
/*  CNFG_CAL is a read/write register that configures the operation, settings, and function of the Internal Calibration Voltage
    Sources (VCALP and VCALN). The output of the voltage sources can be routed to the ECG inputs through the channel
    input MUXes to facilitate end-to-end testing operations. Note if a VCAL source is applied to a connected device, it is
    recommended that the appropriate channel MUX switches be placed in the OPEN position.
*/
#define CNFG_CAL 0x12
/*  CNFG_EMUX is a read/write register which configures the operation, settings, and functionality of the Input Multiplexer
    associated with the ECG channel.
*/
#define CNFG_EMUX 0x14
/*  CNFG_ECG is a read/write register which configures the operation, settings, and functionality of the ECG channel.
    Anytime a change to CNFG_ECG is made, there may be discontinuities in the ECG record and possibly changes to the
    size of the time steps recorded in the ECG FIFO. The SYNCH command can be used to restore internal synchronization
    resulting from configuration changes.
*/
#define CNFG_ECG 0x15
/*  CNFG_RTOR is a two-part read/write register that configures the operation, settings, and function of the RTOR heart
    rate detection block. The first register contains algorithmic voltage gain and threshold parameters, the second contains
    algorithmic timing parameters.
*/
#define CNFG_RTOR1 0x1D
#define CNFG_RTOR2 0x1E
#define ECG_FIFO_BURST 0x20
#define ECG_FIFO 0x21
#define RTOR 0x25
#define NO_OP_MAX 0x7F

#define RTOR_INTR_MASK 0x04

typedef enum
{
    SAMPLINGRATE_128 = 128,
    SAMPLINGRATE_256 = 256,
    SAMPLINGRATE_512 = 512
} sampRate;
/*--------------------------------------------END---------------------------------------------------------------------------*/

/*  First Seven Bits endicates the address of Register
    Last Bit indicates the operation
    Last Bit 0 indicates Write operation
    Last Bit 1 indicates Read operation
*/

typedef enum
{
    ECGDATA,
    RRINTERVAL
} MAX30003CallBackType;

#define WREG 0x00
#define RREG 0x01

#include "hardware/spi.h"
#include <stdio.h>
#include <vector>
using namespace std;

class MAX30003
{
public:
    MAX30003(int cs, spi_inst_t *spiId, void (*callBack)(signed int, MAX30003CallBackType));
    void max30003SetsamplingRate(uint16_t samplingRate);
    void max30003Begin();
    void getHRandRR(void);
    void getEcgSamples(void);
    void max30003ReadInfo(void);
    void setIntruppts();
    void readIntruppt();
    void getDataIntrupptCallback();
    /* These Vars are used to store last value of */
    unsigned int RRinterval;
    signed long ecgdata;
    unsigned int heartRate;

private:
    void cs_deselect();
    void cs_select();
    void readStatus(uint8_t *readBuff);
    void read_registers(uint8_t reg, uint8_t *buf, int len);
    void max30003RegWrite(uint8_t reg, uint32_t data);
    int _cs;
    spi_inst_t *_spiId;
    void (*_callBack)(signed int, MAX30003CallBackType);
};