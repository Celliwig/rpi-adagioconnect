#define LOG_PREFIX "AdagioConnect: "

#define ADAGIO_DEBUG = 1

#ifdef ADAGIO_DEBUG
#       define printd(...) pr_alert(LOG_PREFIX __VA_ARGS__)
#else
#       define printd(...) do {} while (0)
#endif
#define printe(...) pr_err(LOG_PREFIX __VA_ARGS__)
#define printi(...) pr_info(LOG_PREFIX __VA_ARGS__)
#define printn(...) pr_notice(LOG_PREFIX __VA_ARGS__)

#define PERIPH_BASE 0x3f000000

/////////////////////////////////////////////////////////////////////////////////////
// GPIO raw access (used for clock setup)
/////////////////////////////////////////////////////////////////////////////////////
#define GPIO_BASE (PERIPH_BASE + 0x200000)

#define GPIO_IN 0b000				// GPIO Pin is an input
#define GPIO_OUT 0b001				// GPIO Pin is an output
#define GPIO_ALT0 0b100				// GPIO Pin takes alternate function 0
#define GPIO_ALT1 0b101				// GPIO Pin takes alternate function 1
#define GPIO_ALT2 0b110				// GPIO Pin takes alternate function 2
#define GPIO_ALT3 0b111				// GPIO Pin takes alternate function 3
#define GPIO_ALT4 0b011				// GPIO Pin takes alternate function 4
#define GPIO_ALT5 0b010				// GPIO Pin takes alternate function 5

struct GpioRegisters *s_pGpioRegisters;
struct GpioRegisters
{
	uint32_t GPFSEL[6];
	uint32_t Reserved1;
	uint32_t GPSET[2];
	uint32_t Reserved2;
	uint32_t GPCLR[2];
	uint32_t Reserved3;
	uint32_t GPLVL[2];
};

/////////////////////////////////////////////////////////////////////////////////////
// Clocks
/////////////////////////////////////////////////////////////////////////////////////
#define CLK_BASE (PERIPH_BASE + 0x101070)

#define GPCLK0_PIN 4
#define GPCLK1_PIN 5
#define GPCLK2_PIN 6

#define CLK_CTL_PASSWD  (0x5A<<24)

#define CLK_CTL_MASH(x)((x)<<9)
#define CLK_CTL_FLIP    (1 <<8)
#define CLK_CTL_BUSY    (1 <<7)
#define CLK_CTL_KILL    (1 <<5)
#define CLK_CTL_ENAB    (1 <<4)
#define CLK_CTL_SRC(x) ((x)<<0)

#define CLK_CTL_SRC_OSC  1  /* 19.2 MHz */
#define CLK_CTL_SRC_PLLC 5  /* 1000 MHz */
#define CLK_CTL_SRC_PLLD 6  /*  500 MHz */
#define CLK_CTL_SRC_HDMI 7  /*  216 MHz */

#define CLK_DIV_DIVI(x) ((x)<<12)
#define CLK_DIV_DIVF(x) ((x)<< 0)

struct ClkRegisters *s_pClkRegisters;
struct ClkRegisters
{
	uint32_t CM_GP0CTL;
	uint32_t CM_GP0DIV;
	uint32_t CM_GP1CTL;
	uint32_t CM_GP1DIV;
	uint32_t CM_GP2CTL;
	uint32_t CM_GP2DIV;
};

static const int AdagioMClkGpioPin = 4;			// The GPIO pin to configure as a clock source for WM8770 MCLK
static const int AdagioMClkSrc = CLK_CTL_SRC_PLLC;

//// 12.288 MHz for 48 kHz playback (256fs)
//static const int AdagioMClkDivI = 81;
//static const int AdagioMClkDivF = 1557;
//static const int AdagioMClkMASH = 1;

//// 11.2896 MHz for 44.1 kHz playback (256fs)
//static const int AdagioMClkDivI = 88;
//static const int AdagioMClkDivF = 2363;
//static const int AdagioMClkMASH = 1;

//// 16.9340 MHz for 44.1 kHz playback (384fs)
//// Actual (Approx) 16.949 MHz, BClk 4.237 MHz, giving 96 BClk per frame
//static const int AdagioMClkDivI = 59;
//static const int AdagioMClkDivF = 2048;
//static const int AdagioMClkMASH = 0;

static int clock_settings[4][7] = {
//	{ Sample Rate, MClk DivI, MClk DivF, MClk MASH, MClk Freq, FS Ratio, BClk Ratio }
	{ 32000, 122, 288, 1, 8192000, 256, 64},
	{ 44100, 88, 2363, 1, 11289600, 256, 64 },
	{ 48000, 81, 1557, 1, 12288000, 256, 64 },
	{ 96000, 40, 2826, 1, 24576000, 256, 64 }
};


/////////////////////////////////////////////////////////////////////////////////////
// GPIO kernel access
/////////////////////////////////////////////////////////////////////////////////////

static const int AdagioResetHoldPeriod = 50;	// The time (in ns) to hold ResetB low
						// (20ns CE to ResetB hold time + 20ns ResetB to SPI Clock setup time + 10ns just in case)

static struct gpio_desc *gpio_hw_mute;		// Active low
static struct gpio_desc *gpio_hw_reset;		// Active low

/////////////////////////////////////////////////////////////////////////////////////
// ALSA interfaces
/////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// DAI
////////////////////////////////////////

/* machine stream operations */
static int AdagioConnect_dai_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params);
static struct snd_soc_ops snd_adagioconnect_dai_ops = {
	.hw_params = AdagioConnect_dai_hw_params,
};

static int AdagioConnect_dai_init(struct snd_soc_pcm_runtime *rtd);
static struct snd_soc_dai_link snd_adagioconnect_dai[] = {
	{
		.name		= "AdagioConnect",
		.stream_name	= "AdagioConnect HiFi",
		.cpu_dai_name	= "3f203000.i2s",
		.platform_name	= "3f203000.i2s",
//		.codec_name	= "snd-soc-dummy",
		.codec_name	= "spi0.0",
//		.codec_dai_name	= "snd-soc-dummy-dai",
		.codec_dai_name	= "wm8770-hifi",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.ops		= &snd_adagioconnect_dai_ops,
		.init		= AdagioConnect_dai_init,
	},
};

////////////////////////////////////////
// Driver
////////////////////////////////////////

static int AdagioConnect_set_bias_level(struct snd_soc_card *card, struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level);
static struct snd_soc_card snd_rpi_adagioconnect = {
	.name           = "snd_adagioconnect",
	.owner          = THIS_MODULE,
	.dai_link       = snd_adagioconnect_dai,
	.num_links      = ARRAY_SIZE(snd_adagioconnect_dai),
	.set_bias_level = AdagioConnect_set_bias_level,
};

static const struct of_device_id adagioconnect_dev_match[] = {
	{.compatible = "rpi-adagioconnect"},
	{}
};

static int AdagioConnect_md_probe(struct platform_device *pdev);
static int AdagioConnect_md_remove(struct platform_device *pdev);
static struct platform_driver adagioconnect_driver = {
	.driver = {
		.name = "rpi_adagioconnect",
		.owner          = THIS_MODULE,
		.of_match_table = adagioconnect_dev_match,
	},
	.probe = AdagioConnect_md_probe,
	.remove = AdagioConnect_md_remove,
};
