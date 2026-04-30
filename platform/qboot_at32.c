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

#ifdef CHIP_NAME_AT32F403AVGT7
#include <at32f403a_407.h>
#endif

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

rt_weak void qbt_jump_to_app(void)
{
    qbt_qspi_flash_init();
    
    //waiting realize
}

#else

static void qbt_reset_periph(void)
{
#if (defined(AT32F403Axx) || defined(AT32F407xx))
    CRM->ahbrst  = 0xFFFFFFFF;
    CRM->ahbrst  = 0x00000000;

    CRM->apb1rst = 0xFFFFFFFF;
    CRM->apb1rst = 0x00000000;

    CRM->apb2rst = 0xFFFFFFFF;
    CRM->apb2rst = 0x00000000;
#else
    RCC->AHBRST  = 0xFFFFFFFF;
    RCC->AHBRST  = 0x00000000;
    
    RCC->APB1RST = 0xFFFFFFFF;
    RCC->APB1RST = 0x00000000;
    
    RCC->APB2RST = 0xFFFFFFFF;
    RCC->APB2RST = 0x00000000;
#endif
}

void qbt_jump_to_app(void)
{
    typedef void (*app_func_t)(void);
    rt_uint32_t app_addr = QBOOT_APP_ADDR;
    rt_uint32_t stk_addr = *((volatile uint32_t *)app_addr);
    app_func_t app_func = (app_func_t)(*((volatile uint32_t *)(app_addr + 4)));

    if ((((rt_uint32_t)app_func & 0xff000000) != 0x08000000) || ((stk_addr & 0x2ff00000) != 0x20000000))
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

    #if (defined(AT32F403Axx) || defined(AT32F407xx))
    crm_reset();
    #else
    RCC_Reset();
    #endif
    
    __set_CONTROL(0);
    __set_MSP(stk_addr);
    
    app_func();//Jump to application running
    
    LOG_E("Qboot jump to application fail.");
}
#endif
#endif

