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
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <sound/jack.h>

#include "adagio_connect.h"

static bool cfg_osc = false;
module_param(cfg_osc, bool, 0440);
MODULE_PARM_DESC(cfg_osc, "Controls whether the module configures GPCLKn as a clock source for the board.\n");

static bool cfg_osc_stop_existing = false;
module_param(cfg_osc_stop_existing, bool, 0440);
MODULE_PARM_DESC(cfg_osc_stop_existing, "Stops an existing GPCLK which is currently running (Dangerous, suggests it's already in use by something else).\n");

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

	printd("Changing function of GPIO%d from %x to %x\n", GPIO, (oldValue >> bit) & 0b111, functionCode);
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
			printd("GPIO%d: 0 -> 1\n", GPIO);
			s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
		}
		else
		{
			printd("GPIO%d: 1 -> 0\n", GPIO);
			s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
		}
	}
}

/////////////////////////////////////////////////////////////////////////
// Clock functions
/////////////////////////////////////////////////////////////////////////
struct ClkRegisters *s_pClkRegisters;

static void StopClockSource(uint32_t* clk_reg)
{
	if (clk_reg != NULL)
	{
		// if clock source already running, stop it
		if (*clk_reg & CLK_CTL_BUSY)
		{
			// Try just stopping it
			*clk_reg = (*clk_reg & ~CLK_CTL_ENAB) | CLK_CTL_PASSWD;

			usleep_range(10, 100);

			// If it's still running, just kill it
			if (*clk_reg & CLK_CTL_BUSY)
			{
				printe("Killing clock source.\n");
				*clk_reg = *clk_reg | CLK_CTL_PASSWD | CLK_CTL_KILL;

				// wait for clock to stop
				while (*clk_reg & CLK_CTL_BUSY)
				{
					usleep_range(10, 100);
				}
			}
		}
	}
}

// Setup the GPIO function registers to configure AdagioMClkGpioPin as a clock
static int AdagioConnect_MClk_init(int GPIO)
{
	uint32_t* clk_reg = NULL;

	printn("Configuring GPCLK on pin %d as WM8770 MClk.\n", AdagioMClkGpioPin);

	switch (GPIO)
	{
		case GPCLK0_PIN:
			clk_reg = &s_pClkRegisters->CM_GP0CTL;
			break;
		case GPCLK1_PIN:
			printe("You shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			break;
		default:
			printe("GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if (clk_reg != NULL)
	{
		// Check if the clock is running
		if (*clk_reg & CLK_CTL_BUSY)
		{
			printe("There is a clock already running on GPIO %d (status reg: %d).\n", GPIO, *clk_reg);

			if (cfg_osc_stop_existing)
			{
				printe("Forcing stop of clock source.\n");
				StopClockSource(clk_reg);
			}
			else
			{
				printe("Not changing clock source.\n");
				return -1;
			}
		}

		// Change GPIO pin to Alt0 (GPCLK)
		SetGPIOFunction(GPIO, GPIO_ALT0);

		return 0;
	}

	return -1;
}

// Disables the configuration of AdagioMClkGpioPin as a clock
static int AdagioConnect_MClk_remove(int GPIO)
{
	uint32_t* clk_reg = NULL;

	printn("Removing GPCLK.\n");

	switch (GPIO)
	{
		case GPCLK0_PIN:
			clk_reg = &s_pClkRegisters->CM_GP0CTL;
			break;
		case GPCLK1_PIN:
			printe("You shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			break;
		default:
			printe("GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if (clk_reg != NULL)
	{
		// Change GPIO pin back to an input
		SetGPIOFunction(GPIO, GPIO_IN);

		// if clock source already running, stop it
		StopClockSource(clk_reg);

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
			printe("You shouldn't use GPCLK2 as a clock source, it's used by the system.\n");
			break;
		case GPCLK2_PIN:
			clk_reg = &s_pClkRegisters->CM_GP2CTL;
			clk_div = &s_pClkRegisters->CM_GP2DIV;
			break;
		default:
			printe("GPIO pin %d is not available as a clock source.\n", GPIO);
			break;
	}

	if ((clk_reg != NULL) && (clk_div != NULL))
	{
		if ((clk_src < 0) || (clk_src > 7 ))
		{
			printe("Clock source selection incorrect.\n");
			return -1;
		}
		if ((clk_divI   < 2) || (clk_divI   > 4095))
		{
			printe("DivI value (%d) incorrect.\n", clk_divI);
			return -1;
		}
		if ((clk_divF   < 0) || (clk_divF   > 4095))
		{
			printe("DivF value (%d) incorrect.\n", clk_divF);
			return -1;
		}
		if ((clk_MASH   < 0) || (clk_MASH   > 3))
		{
			printe("MASH value (%d) incorrect.\n", clk_MASH);
			return -1;
		}

		// if clock source already running, stop it
		StopClockSource(clk_reg);

		printd("Clock source - %d.\n", clk_src);
		printd("Clock DIV_I - %d.\n", clk_divI);
		printd("Clock DIV_F - %d.\n", clk_divF);
		printd("Clock MASH - %d.\n", clk_MASH);

		*clk_div = (CLK_CTL_PASSWD | CLK_DIV_DIVI(clk_divI) | CLK_DIV_DIVF(clk_divF));
		usleep_range(10, 100);
		*clk_reg = (CLK_CTL_PASSWD | CLK_CTL_ENAB | CLK_CTL_MASH(clk_MASH) | CLK_CTL_SRC(clk_src));
		usleep_range(10, 100);

		return 0;
	}

	return -1;
}

/////////////////////////////////////////////////////////////////////////
// Machine driver
/////////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Basic device functions
////////////////////////////////////////

// Pulses the GPIO pin indicated by AdagioResetGpioPin to reset the WM8770 board
static void AdagioConnect_reset(void)
{
	printn("Resetting board.\n");
	// Bring line low
	SetGPIOOutputValue(AdagioResetGpioPin, false);
	ndelay(AdagioResetHoldPeriod);			// (20ns CE to ResetB hold time + 20ns ResetB to SPI Clock setup time + 10ns just in case)
	// Bring line back high
	SetGPIOOutputValue(AdagioResetGpioPin, true);
}

static void AdagioConnect_reset_iface(void)
{
	// Disable MClk
	if (cfg_osc && (s_pClkRegisters != NULL))
	{
		AdagioConnect_MClk_remove(AdagioMClkGpioPin);
		iounmap(s_pClkRegisters);
	}

	// Reset GPIO pin connected to reset pin of WM8770 as input
	SetGPIOFunction(AdagioResetGpioPin, GPIO_IN);  	//Configure the pin as input

	// Unmap GPIO function registers
	iounmap(s_pGpioRegisters);
}

////////////////////////////////////////
// DAI
////////////////////////////////////////

static int AdagioConnect_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	printd("AdagioConnect_dai_init\n");

	return 0;
}

static int AdagioConnect_dai_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret, i;

	printd("AdagioConnect_dai_hw_params\n");

	for (i = 0; i < ARRAY_SIZE(clock_settings); ++i)
	{
		if (clock_settings[i][0] == params_rate(params))
		{
			if (cfg_osc)
			{
				AdagioConnect_MClk_cfg(AdagioMClkGpioPin, AdagioMClkSrc, clock_settings[i][1], clock_settings[i][2], clock_settings[i][3]);
			}

			/* Set proto bclk */
			ret = snd_soc_dai_set_bclk_ratio(cpu_dai,clock_settings[i][6]);
			if (ret < 0)
			{
				printe("Failed to set BCLK ratio %d\n", ret);
				return ret;
			}

			/* Set proto sysclk */
			ret = snd_soc_dai_set_sysclk(codec_dai, 0, clock_settings[i][4], 0);
			if (ret < 0)
			{
				printe("Failed to set SYSCLK: %d\n", ret);
				return ret;
			}

			break;
		}
	}

	if (i == ARRAY_SIZE(clock_settings)) printe("Failed to setup MCLK.\n ");

	return 0;
}

////////////////////////////////////////
// Module interface
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
// Driver
//////////////////////////////////////////////////////////////////////////////////////////

static int AdagioConnect_md_probe(struct platform_device *pdev)
{
	int ret = 0;
	const char *mdl_wm8770 = "wm8770";			// module to load

	printi("Adagio soundcard, Raspberry PI connector.\n");

	ret = request_module(mdl_wm8770);
	if (ret)
	{
		printe("Unable to request module load '%s': %d\n", mdl_wm8770, ret);
		goto exit;
	}

// Initial config
	printn("Configuring GPIOs.\n");
	// Map GPIO function registers
	s_pGpioRegisters = (struct GpioRegisters *)ioremap(GPIO_BASE, sizeof(struct GpioRegisters));
	// Setup GPIO pin connected to reset pin of WM8770 as output
	SetGPIOFunction(AdagioResetGpioPin, GPIO_OUT);  	//Configure the pin as output

	// Configure PI based MClk, if selected
	if (cfg_osc)
	{
		s_pClkRegisters = (struct ClkRegisters *)ioremap(CLK_BASE, sizeof(struct ClkRegisters));
		if (!AdagioConnect_MClk_init(AdagioMClkGpioPin))
		{
			// Default to 44.1 kHz settings
			AdagioConnect_MClk_cfg(AdagioMClkGpioPin, AdagioMClkSrc, clock_settings[1][1], clock_settings[1][2], clock_settings[1][3]);
		}
		else
		{
			iounmap(s_pClkRegisters);
			s_pClkRegisters = NULL;
		}
	}

	// Reset the WM8770 board
	AdagioConnect_reset();

// ALSA config
	snd_rpi_adagioconnect.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &snd_adagioconnect_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

		if (i2s_node) {
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}

	ret = snd_soc_register_card(&snd_rpi_adagioconnect);
	if (ret && ret != -EPROBE_DEFER) {
		printe("snd_soc_register_card() failed: %d\n", ret);
		AdagioConnect_reset_iface();
	}

exit:
	return ret;
}

static int AdagioConnect_md_remove(struct platform_device *pdev)
{
	int rtn = snd_soc_unregister_card(&snd_rpi_adagioconnect);
	AdagioConnect_reset_iface();
	printi("Removed.\n");
	return rtn;
}

module_platform_driver(adagioconnect_driver);

MODULE_DEVICE_TABLE(of, adagioconnect_dev_match);
MODULE_ALIAS("platform:rpi-adagioconnect");
MODULE_DESCRIPTION("Adagio WM8770 soundcard driver");
MODULE_AUTHOR("C Burgoyne");
MODULE_LICENSE("GPL");
