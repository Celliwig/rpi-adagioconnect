#define PERIPH_BASE 0x3f000000

/////////////////////////////////////////////////////////////////////////////////////
// GPIO
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

static const int AdagioResetGpioPin = 5;	// The GPIO pin WM8770(ResetB) connected to
static const int AdagioResetHoldPeriod = 50;	// The time (in ns) to hold ResetB low
						// (20ns CE to ResetB hold time + 20ns ResetB to SPI Clock setup time + 10ns just in case)
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
static const int AdagioMClkDivI = 81;
static const int AdagioMClkDivF = 1557;
static const int AdagioMClkMASH = 1;
