
#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"


#define FIFOTHR 		64
#define BUF_SIZE		240

volatile bool g_bPDMDataReady = false;
int16_t i16PDMBuf[2][BUF_SIZE] = {{0},{0}};
uint32_t u32PDMPingpong = 0;
uint16_t PDMBuf_idx = 0;

//*****************************************************************************
//
// PDM Interrupt Service Routine (ISR)
//
//*****************************************************************************
void am_pdm0_isr(void)
{
	//uint32_t ui32FIFOCount;
	//uint32_t ui32temp;
	uint16_t idx = 0;
	static uint32_t u32isrType;

	u32isrType = AM_REG(PDM, INTSTAT);

	//
	// Grab the FIFO depth
	//
	//ui32FIFOCount = am_hal_pdm_fifo_depth_read();

	//
	// When Threshold causes ISR, Grab samples from packed FIFO and make sure we don't underflow
	//
	if((u32isrType == AM_HAL_PDM_INT_FIFO))
	{
		//for (idx = 0; idx < ui32FIFOCount; idx ++)
		for (idx = 0; idx < FIFOTHR; idx ++)
		{
			*((uint32_t *)(&(i16PDMBuf[(u32PDMPingpong)%2][PDMBuf_idx])))=am_hal_pdm_fifo_data_read();	
			PDMBuf_idx += 2;
			
		}
		if(PDMBuf_idx >=  BUF_SIZE)
		{
			PDMBuf_idx = 0;
			u32PDMPingpong ++;
			g_bPDMDataReady = true;
			
		}
		//
		// Clear PDM Interrupt (write to clear).
		//
		AM_REG(PDM, INTCLR) = AM_HAL_PDM_INT_FIFO | AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF;     

	}
	else if(u32isrType == AM_HAL_PDM_INT_UNDFL || AM_HAL_PDM_INT_OVF)
	{
		AM_REG(PDM, FLUSH) = 0x1;
	} 
}


//*****************************************************************************
//
// PDM Configuration
//
//*****************************************************************************
void PDMregConfig(void)
{
	am_hal_pdm_config_t g_sPDM_Cfg =
	{
		//
		// uint32_t ui32PDMConfigReg
		// PDM Configuration (PCFG, PDMCFG) register
		// Notes:
		//  Choose from AM_HAL_PDM_PCFG macros.
		//  For completeness, all PCFG fields should be referenced here.
		//
		(
			AM_HAL_PDM_PCFG_LRSWAP_DISABLE      |
			AM_HAL_PDM_PCFG_RIGHT_PGA_P105DB    |	// +10.5dB gain
			AM_HAL_PDM_PCFG_LEFT_PGA_P105DB     |	// +10.5dB gain
			//AM_HAL_PDM_PCFG_RIGHT_PGA_0DB    |	// +10.5dB gain
			//AM_HAL_PDM_PCFG_LEFT_PGA_0DB     |	// +10.5dB gain
			AM_HAL_PDM_PCFG_MCLKDIV_DIV1        |	
			AM_HAL_PDM_PCFG_SINC_RATE(24)       |   // over sample rate (decimation)
			AM_HAL_PDM_PCFG_HPCUTOFF(0xB)       |
			AM_HAL_PDM_PCFG_SOFTMUTE_DISABLE    |
			AM_HAL_PDM_PCFG_PDMCORE_DISABLE
		),

		//
		// uint32_t ui32VoiceConfigReg
		// PDM Voice Config (VCFG, VOICECFG) register
		// Notes:
		//  Choose from AM_HAL_PDM_VCFG macros.
		//  For completeness, all VCFG fields should be referenced here.
		//  AM_HAL_PDM_IOCLK_xxx also sets AM_REG_PDM_VCFG_IOCLKEN_EN
		//  RSTB is set to NORMAL by am_hal_pdm_enable.
		//
		(
			AM_HAL_PDM_IOCLK_750KHZ             |   // AM_REG_PDM_VCFG_IOCLKEN_EN
			AM_HAL_PDM_VCFG_RSTB_RESET          |
			AM_HAL_PDM_VCFG_PDMCLK_ENABLE       |
			AM_HAL_PDM_VCFG_I2SMODE_DISABLE     |
			AM_HAL_PDM_VCFG_BCLKINV_DISABLE     |
			AM_HAL_PDM_VCFG_DMICDEL_0CYC        |
			AM_HAL_PDM_VCFG_SELAP_INTERNAL      |
			AM_HAL_PDM_VCFG_PACK_ENABLE         |
			//AM_HAL_PDM_VCFG_CHANNEL_STEREO
			AM_HAL_PDM_VCFG_CHANNEL_LEFT
			//AM_HAL_PDM_VCFG_CHANNEL_RIGHT
		),

		//
		// uint32_t ui32FIFOThreshold
		// The PDM controller will generate a processor interrupt when the number
		// of entries in the FIFO goes *above* this number.
		//
		(FIFOTHR - 1)
	};

    am_hal_pdm_config(&g_sPDM_Cfg);		
	
}

void PDMinit(void)
{	
	//
	// Enable power to PDM module and configure PDM
	//
	am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PDM);	

	//
	// Configure the PDM microphone pins
	//
	am_hal_gpio_pin_config(12, AM_HAL_PIN_12_PDM_CLK); // Configure GP12 as PDM clock pin output
	am_hal_gpio_pin_config(11, AM_HAL_PIN_11_PDM_DATA);  // Configure GP11 as PDM data pin using WDS	
	//am_hal_gpio_pin_config(23, AM_HAL_PIN_23_PDM_DATA);// Configure GP23 as PDM data pin for unmodified Shield
	am_hal_gpio_out_bit_set(8);
	am_hal_gpio_pin_config(8, AM_HAL_PIN_OUTPUT);


	//
	// Configure the PDM module
	//	
	PDMregConfig();

	//
	// Make sure interrupts are clear
	//
	AM_REG(PDM, INTCLR) = 0x7;
	AM_REG(PDM, FLUSH) = 0x1;                       // Reset FIFO pointers

	//
	// Enable interrupts PDM
	//
	am_hal_interrupt_enable(AM_HAL_INTERRUPT_PDM);  //NVIC setting

	am_hal_pdm_int_enable(AM_HAL_PDM_INT_FIFO | AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF);

	am_hal_interrupt_priority_set(AM_HAL_INTERRUPT_PDM, AM_HAL_INTERRUPT_PRIORITY(4));	

	// enable all interrupts
	//am_hal_interrupt_master_enable();	

	//
	// enable the PDM.
	//
	am_hal_pdm_enable();	

} 


void PDMdeinit(void)
{	

	//
	// Disable the PDM.
	//
	am_hal_pdm_disable();

	//
	// Disable interrupts PDM
	//
	am_hal_pdm_int_disable(AM_HAL_PDM_INT_FIFO | AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF);
	am_hal_interrupt_disable(AM_HAL_INTERRUPT_PDM);  //NVIC setting

	//
	// Clear PDM Interrupt (write to clear).
	//
	AM_REG(PDM, INTCLR) = AM_HAL_PDM_INT_FIFO | AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF;

	//
	// Configure the PDM microphone pins as HiZ
	//
	am_hal_gpio_pin_config(12, AM_HAL_PIN_DISABLE); // Configure GP12 as disable
	am_hal_gpio_pin_config(11, AM_HAL_PIN_DISABLE);  // Configure GP11 as disable	

	//
	// Disable power to PDM module 
	//
	am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PDM);	
	

}

