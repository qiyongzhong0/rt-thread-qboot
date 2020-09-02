/*
 * qboot_stm32.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-08-31     qiyongzhong       first version
 */

#include <board.h>

#ifdef CHIP_FAMILY_AT32

#include <rtthread.h>
#include <rtdevice.h>
#include <qboot.h>

//#define QBOOT_APP_RUN_IN_QSPI_FLASH
//#define QBOOT_DEBUG
#define QBOOT_USING_LOG
#define DBG_TAG "Qboot"

#ifdef QBOOT_DEBUG
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_INFO
#endif

#ifdef QBOOT_USING_LOG
#ifndef DBG_ENABLE
#define DBG_ENABLE
#endif
#ifndef DBG_COLOR
#define DBG_COLOR
#endif
#endif

#include <rtdbg.h>

#ifdef QBOOT_APP_RUN_IN_QSPI_FLASH

static void qbt_qspi_flash_init(void)
{
    //waiting realize
}

void qbt_jump_to_app(void)
{
    qbt_qspi_flash_init();
    
    //waiting realize
}

#else

static void qbt_reset_periph(void)
{
    #if defined (AT32F403xx)
    #define RCC_AHB_PERIPH_ALL          0xFFFFF2A8
    #define RCC_APB2_PERIPH_ALL         0xFFC60002
    #define RCC_APB1_PERIPH_ALL         0xC1003600
    #elif defined (AT32F413xx)
    #define RCC_AHB_PERIPH_ALL          0xFFFFFBA8
    #define RCC_APB2_PERIPH_ALL         0xFF878142
    #define RCC_APB1_PERIPH_ALL         0x6501B7F0
    #elif defined (AT32F415xx)
    #define RCC_AHB_PERIPH_ALL          0xFFFFEBA8
    #define RCC_APB2_PERIPH_ALL         0xFF078142
    #define RCC_APB1_PERIPH_ALL         0xED81B3F0
    #elif defined (AT32F403Axx) || defined (AT32F407xx)
    #define RCC_AHB_PERIPH_ALL          0xEFFE32A8
    #define RCC_APB2_PERIPH_ALL         0xF8070182
    #define RCC_APB1_PERIPH_ALL         0xC1003600
    #endif

    #if defined (AT32F403Axx) || defined (AT32F407xx)
    RCC_AHBPeriphResetCmd(RCC_AHB_PERIPH_ALL, ENABLE);
    RCC_AHBPeriphResetCmd(RCC_AHB_PERIPH_ALL, DISABLE);
    #endif

    RCC_APB2PeriphResetCmd(RCC_APB2_PERIPH_ALL, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2_PERIPH_ALL, DISABLE);

    RCC_APB1PeriphResetCmd(RCC_APB1_PERIPH_ALL, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1_PERIPH_ALL, DISABLE);
}

void qbt_jump_to_app(void)
{
    typedef void (*app_func_t)(void);
	u32 app_addr = QBOOT_APP_ADDR;
    u32 stk_addr = *((__IO uint32_t *)app_addr);
    app_func_t app_func = (app_func_t)(*((__IO uint32_t *)(app_addr + 4)));

    if ((((u32)app_func & 0xff000000) != 0x08000000) || ((stk_addr & 0x2ff00000) != 0x20000000))
	{
        LOG_E("No legitimate application.");
		return;
	}

	rt_kprintf("Jump to application running ... \n");
    rt_thread_mdelay(200);
	
    __disable_irq();
    qbt_reset_periph();

    for(int i=0; i<128; i++)
    {
        __NVIC_DisableIRQ(i);
        __NVIC_ClearPendingIRQ(i);
    }
    
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    RCC_Reset();
    
    __set_CONTROL(0);
	__set_MSP(stk_addr);
	
	app_func();//Jump to application running
	
    LOG_E("Qboot jump to application fail.");
}
#endif
#endif

