#define PERIPH_BASE 0x3f000000
#define GPIO_BASE (PERIPH_BASE + 0x200000)

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
static int AdagioResetHoldPeriod = 50;		// The time (in ns) to hold ResetB low
						// (20ns CE to ResetB hold time + 20ns ResetB to SPI Clock setup time + 10ns just in case)
