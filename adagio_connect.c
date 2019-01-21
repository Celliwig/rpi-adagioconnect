/*      Module to connect a Raspberry PI to the soundcard from an Adagio Server System                  *
 *      Written C. Burgoyne 2018                                                                        *
 *                                                                                                      *
 *      GPIO code derived from https://sysprogs.com/VisualKernel/tutorials/raspberry/leddriver/         */

#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include "adagio_connect.h"

MODULE_LICENSE("GPL");

static int debug = 0;
module_param(debug, int, 0660);

/////////////////////////////////////////////////////////////////////////
// GPIO Functions
/////////////////////////////////////////////////////////////////////////
struct GpioRegisters *s_pGpioRegisters;

static void SetGPIOFunction(int GPIO, int functionCode)
{
	unsigned int registerIndex = GPIO / 10;
	unsigned int bit = (GPIO % 10) * 3;

	unsigned oldValue = s_pGpioRegisters->GPFSEL[registerIndex];
	unsigned mask = 0b111 << bit;

	if (debug > 0) printk("AdagioConnect: changing function of GPIO%d from %x to %x\n", GPIO, (oldValue >> bit) & 0b111, functionCode);
	s_pGpioRegisters->GPFSEL[registerIndex] = (oldValue & ~mask) | ((functionCode << bit) & mask);
}

static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
	unsigned int registerIndex = GPIO / 32;
	unsigned int bit = (GPIO % 32);

	unsigned int curValue = s_pGpioRegisters->GPLVL[registerIndex] & (1 << bit);

	unsigned int cmpValue = 0;
	if (outputValue) cmpValue = 1;
	cmpValue = cmpValue << bit;

	if (curValue != cmpValue)
	{
		if (outputValue)
		{
			if (debug > 0) printk("AdagioConnect: GPIO%d: 0 -> 1\n", GPIO);
			s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
		}
		else
		{
			if (debug > 0) printk("AdagioConnect: GPIO%d: 1 -> 0\n", GPIO);
			s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
		}
	}
}

/////////////////////////////////////////////////////////////////////////
// Main Module Functions
/////////////////////////////////////////////////////////////////////////
// Pulses the GPIO pin indicated by AdagioResetGpioPin to reset the WM9770 board
static void AdagioConnect_reset(void)
{
	if (debug > 0) printk("AdagioConnect: resetting board.\n");
	// Bring line low
	SetGPIOOutputValue(AdagioResetGpioPin, false);
	ndelay(AdagioResetHoldPeriod);			// (20ns CE to ResetB hold time + 20ns ResetB to SPI Clock setup time + 10ns just in case)
	// Bring line back high
	SetGPIOOutputValue(AdagioResetGpioPin, true);
}

// Module init
static int __init AdagioConnect_init(void)
{
	printk("AdagioConnect: Adagio soundcard, Raspberry PI connector.\n");

	if (debug > 0) printk("AdagioConnect: configuring GPIOs.\n");
	// Map GPIO function registers
	s_pGpioRegisters = (struct GpioRegisters *)ioremap(GPIO_BASE, sizeof(struct GpioRegisters));

	// Setup GPIO pin connected to reset pin of WM9770 as output
	SetGPIOFunction(AdagioResetGpioPin, 0b001);  	//Configure the pin as output

	// Reset the WM9770 board
	AdagioConnect_reset();

	return 0;
}

// Module remove
static void __exit AdagioConnect_exit(void)
{
	// Reset GPIO pin connected to reset pin of WM9770 as input
	SetGPIOFunction(AdagioResetGpioPin, 0);  	//Configure the pin as input

	// Unmap GPIO function registers
	iounmap(s_pGpioRegisters);

	if (debug > 0) printk("AdagioConnect: removed.\n");
}

module_init(AdagioConnect_init);
module_exit(AdagioConnect_exit);
