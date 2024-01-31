#include "pico/stdlib.h"
#include "hardware/structs/spi.h"
#include "hardware/regs/dreq.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include "pico.h"
#include "hardware/spi.h"
#include <stdio.h>
#include <string.h>
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "src/max30003.cpp"
// SPI communication Pins
#define SCLK 18
#define SDA 19
#define SDI 16
#define CS 17

// define Intruppt pin
#define INTPIN 20

/* SPI SPEED */
#define MAX30003_SPI_SPEED 2000000
void max3003CallBack(signed long data, MAX30003CallBackType type);
MAX30003 max30003(CS, spi0, max3003CallBack);

/* This openocd command i used to connect debugger */
// sudo openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s tcl

void initiaLizeMAX30003SPI()
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
    gpio_put(CS, true);
    // Make the CS pin avaimax3003CalBacklable to picotool
    bi_decl(bi_1pin_with_name(CS, "SPI CS"));
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void max30003Intruppt(uint gpio, uint32_t events)
{
    max30003.getDataIntrupptCallback();
}

void max3003CallBack(signed long data, MAX30003CallBackType type)
{
    if (type == ECGDATA)
    {
        printf("%ld\n", data);
    }
    else
    {
        printf("RR Interval: %ld\n", data);
    }
}

int main()
{
    /* initialise all stdio for printf */
    stdio_init_all();
    /* initialise SPI interface */
    initiaLizeMAX30003SPI();
    /* Read MAX30003 Revesion ID */
    max30003.max30003ReadInfo();
    /* Configure MAX30003 */
    max30003.max30003Begin();
    /* Set Sampling Rate */
    max30003.max30003SetsamplingRate(SAMPLINGRATE_512);
    /* Set Intruppts */
    max30003.setIntruppts();
    /* Read Intruppt Configuration */
    max30003.readIntruppt();
    /* PUll UP Intruppt pin */
    gpio_pull_up(INTPIN);
    /* Setup intrupt on GPIO INTPIN */
    gpio_set_irq_enabled_with_callback(INTPIN, GPIO_IRQ_EDGE_FALL, true, &max30003Intruppt);
    while (1)
    {
        sleep_ms(1000);
    }
    return 0;
}
