/*      Module to connect a Raspberry PI to the soundcard from an Adagio Server System                  *
 *      Written C. Burgoyne 2018                                                                        *
 *                                                                                                      *
 *      GPIO code derived from https://sysprogs.com/VisualKernel/tutorials/raspberry/leddriver/         *
 *	Clock code hints from http://abyz.me.uk/rpi/pigpio/examples.html (minimal_clk)			*
 *													*
 * 	TODO												*
 *													*
 *	Shouldn't cast pointer returned from ioremap/should use ioread32/iowrite32 for __iomem		*/

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
MODULE_PARM_DESC(debug, "Print additional debugging information.\n");

static bool cfg_osc = false;
module_param(cfg_osc, bool, 0440);
MODULE_PARM_DESC(cfg_osc, "Controls whether the module configures GPCLKn as a clock source for the board.\n");

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
// Clock functions
/////////////////////////////////////////////////////////////////////////
struct ClkRegisters *s_pClkRegisters;

// Setup the GPIO function registers to configure AdagioMClkGpioPin as a clock
static int AdagioConnect_MClk_init(int GPIO)
{
	uint32_t* clk_reg = NULL;
	switch (GPIO)
	{
		case GPCLK0_PIN:
			clk_reg = &s_pClkRegisters->CM_GP0CTL;
			break;
		case GPCLK1_PIN:
			printk("AdagioConnect: you shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			break;
		default:
			printk("AdagioConnect: GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if (clk_reg != NULL)
	{
		// if clock source already running, stop it
		if (*clk_reg & CLK_CTL_BUSY)
		{
			// Try just stopping it
			*clk_reg = CLK_CTL_PASSWD | CLK_CTL_ENAB;

			usleep_range(10, 100);

			// If it's still running, just kill it
			if (*clk_reg & CLK_CTL_BUSY)
			{
				if (debug > 0) printk("AdagioConnect: killing clock source.\n");
				*clk_reg = CLK_CTL_PASSWD | CLK_CTL_KILL;

				// wait for clock to stop
				while (*clk_reg & CLK_CTL_BUSY)
				{
					usleep_range(10, 100);
				}
			}
		}

		SetGPIOFunction(GPIO, GPIO_ALT0);

		return 0;
	}

	return -1;
}

// Disables the configuration of AdagioMClkGpioPin as a clock
static int AdagioConnect_MClk_remove(int GPIO)
{
	uint32_t* clk_reg = NULL;
	switch (GPIO)
	{
		case GPCLK0_PIN:
			clk_reg = &s_pClkRegisters->CM_GP0CTL;
			break;
		case GPCLK1_PIN:
			printk("AdagioConnect: you shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			break;
		default:
			printk("AdagioConnect: GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if (clk_reg != NULL)
	{
		SetGPIOFunction(GPIO, GPIO_IN);

		// if clock source already running, stop it
		if (*clk_reg & CLK_CTL_BUSY)
		{
			// Try just stopping it
			*clk_reg = CLK_CTL_PASSWD | CLK_CTL_ENAB;

			usleep_range(10, 100);

			// If it's still running, just kill it
			if (*clk_reg & CLK_CTL_BUSY)
			{
				if (debug > 0) printk("AdagioConnect: killing clock source.\n");
				*clk_reg = CLK_CTL_PASSWD | CLK_CTL_KILL;

				// wait for clock to stop
				while (*clk_reg & CLK_CTL_BUSY)
				{
					usleep_range(10, 100);
				}
			}
		}

		return 0;
	}

	return -1;
}

// Setup the actual clock frequency
static int AdagioConnect_MClk_cfg(int GPIO, int clk_src, int clk_divI, int clk_divF, int clk_MASH)
{
	uint32_t* clk_reg = NULL;
	uint32_t* clk_div = NULL;
	switch (GPIO)
	{
		case GPCLK0_PIN:
			clk_reg = &s_pClkRegisters->CM_GP0CTL;
			clk_div = &s_pClkRegisters->CM_GP0DIV;
			break;
		case GPCLK1_PIN:
			printk("AdagioConnect: you shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			clk_div = &s_pClkRegisters->CM_GP2DIV;
			break;
		default:
			printk("AdagioConnect: GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if ((clk_reg != NULL) && (clk_div != NULL))
	{
		if ((clk_src < 0) || (clk_src > 7 ))
		{
			printk("AdagioConnect: clock source selection incorrect.\n");
			return -1;
		}
		if ((clk_divI   < 2) || (clk_divI   > 4095))
		{
			printk("AdagioConnect: divI value (%d) incorrect.\n", clk_divI);
			return -1;
		}
		if ((clk_divF   < 0) || (clk_divF   > 4095))
		{
			printk("AdagioConnect: divF value (%d) incorrect.\n", clk_divF);
			return -1;
		}
		if ((clk_MASH   < 0) || (clk_MASH   > 3))
		{
			printk("AdagioConnect: MASH value (%d) incorrect.\n", clk_MASH);
			return -1;
		}

		// if clock source already running, stop it
		if (*clk_reg & CLK_CTL_BUSY)
		{
			// Try just stopping it
			*clk_reg = CLK_CTL_PASSWD | CLK_CTL_ENAB;

			usleep_range(10, 100);

			// If it's still running, just kill it
			if (*clk_reg & CLK_CTL_BUSY)
			{
				if (debug > 0) printk("AdagioConnect: killing clock source.\n");
				*clk_reg = CLK_CTL_PASSWD | CLK_CTL_KILL;

				// wait for clock to stop
				while (*clk_reg & CLK_CTL_BUSY)
				{
					usleep_range(10, 100);
				}
			}
		}

		*clk_div = (CLK_CTL_PASSWD | CLK_DIV_DIVI(clk_divI) | CLK_DIV_DIVF(clk_divF));
		usleep_range(10, 20);
		*clk_reg = (CLK_CTL_PASSWD | CLK_CTL_MASH(clk_MASH) | CLK_CTL_SRC(clk_src));
		usleep_range(10, 20);
		*clk_reg = (CLK_CTL_PASSWD | CLK_CTL_ENAB);

		return 0;
	}

	return -1;
}

/////////////////////////////////////////////////////////////////////////
// Main Module Functions
/////////////////////////////////////////////////////////////////////////
// Pulses the GPIO pin indicated by AdagioResetGpioPin to reset the WM8770 board
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

	// Setup GPIO pin connected to reset pin of WM8770 as output
	SetGPIOFunction(AdagioResetGpioPin, GPIO_OUT);  	//Configure the pin as output

	if (cfg_osc)
	{
		if (debug > 0) printk("AdagioConnect: configuring GPCLK on pin %d as WM8770 MClk.\n", AdagioMClkGpioPin);
		s_pClkRegisters = (struct ClkRegisters *)ioremap(CLK_BASE, sizeof(struct ClkRegisters));
		AdagioConnect_MClk_init(AdagioMClkGpioPin);
		AdagioConnect_MClk_cfg(AdagioMClkGpioPin, AdagioMClkSrc, AdagioMClkDivI, AdagioMClkDivF, AdagioMClkMASH);
	}

	// Reset the WM8770 board
	AdagioConnect_reset();

	return 0;
}

// Module remove
static void __exit AdagioConnect_exit(void)
{
	if (cfg_osc)
	{
		if (debug > 0) printk("AdagioConnect: removing GPCLK.\n");
		AdagioConnect_MClk_remove(AdagioMClkGpioPin);
		iounmap(s_pClkRegisters);
	}

	// Reset GPIO pin connected to reset pin of WM8770 as input
	SetGPIOFunction(AdagioResetGpioPin, GPIO_IN);  	//Configure the pin as input

	// Unmap GPIO function registers
	iounmap(s_pGpioRegisters);

	if (debug > 0) printk("AdagioConnect: removed.\n");
}

module_init(AdagioConnect_init);
module_exit(AdagioConnect_exit);
