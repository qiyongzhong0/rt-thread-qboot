/**
 * @file qboot.c
 * @brief 
 * @author qiyongzhong
 * @version 1.8
 * @date 2026-01-15
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author        Description
 * 2020-07-06     qiyongzhong       first version
 * 2020-08-31     qiyongzhong       fix qbt_jump_to_app type from static to weak
 * 2020-09-01     qiyongzhong       add app verify when checking firmware
 * 2020-09-18     qiyongzhong       fix bug of gzip decompression
 * 2020-09-22     qiyongzhong       add erase firmware function, update version to v1.04
 * 2020-10-05     qiyongzhong       fix to support stm32h7xx, update version to v1.05
 * 2026-01-15     wdfk-prog         split to qboot.c
 * 2026-04-30     wdfk-prog         avoid continuing after closed shell or init failure
 */

#include <qboot.h>
#include "qboot_update.h"

#ifdef QBOOT_USING_SHELL
#include "shell.h"
static void qbt_close_sys_shell(void);
static void qbt_open_sys_shell(void);
static rt_err_t qbt_shell_rx_ind(rt_device_t dev, rt_size_t size);
static rt_bool_t qbt_shell_init(const char *shell_dev_name);
static rt_bool_t qbt_shell_key_check(void);
static rt_bool_t qbt_startup_shell(rt_bool_t wait_press_key);
static rt_bool_t qbt_fw_clone(void *dst_handle, const char *dst_name, void *src_handle, const char *src_name, rt_uint32_t fw_pkg_size);
static void qbt_fw_info_show(qbt_target_id_t part_id);
static rt_bool_t qbt_fw_delete(void *handle, const char *name, rt_uint32_t part_size);
static void qbt_shell_cmd(rt_uint8_t argc, char **argv);
#endif
#ifdef QBOOT_USING_STATUS_LED
#include <qled.h>
#endif

#define DBG_TAG "Qboot"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define QBOOT_VER_MSG      "V1.1.0 2026.01.01"
#define QBOOT_SHELL_PROMPT "Qboot>"

#define QBOOT_ALGO2_VERIFY_NONE 0
#define QBOOT_ALGO2_VERIFY_CRC  1
#define QBOOT_ALGO2_VERIFY_MASK 0x0F

static fw_info_t fw_info;
static rt_uint8_t g_cmprs_buf[QBOOT_CMPRS_BUF_SIZE]; /* Decompression buffer. */
#ifdef QBOOT_USING_COMPRESSION
static rt_uint8_t g_crypt_buf[QBOOT_BUF_SIZE]; /* Decryption buffer. */
#else
#define g_crypt_buf g_cmprs_buf
#endif
/* Share a buffer to reduce memory allocation */
#define g_decmprs_buf g_crypt_buf /* Decompression output buffer. */

/**
 * @brief Validate firmware header magic and header CRC.
 *
 * @param fw_info Firmware header to verify.
 * @return RT_TRUE when header is valid, RT_FALSE otherwise.
 */
static rt_bool_t qbt_fw_info_check(fw_info_t *fw_info)
{
    if (rt_strcmp((const char *)(fw_info->type), "RBL") != 0)
    {
        return RT_FALSE;
    }

    return (crc32_cal((rt_uint8_t *)fw_info, (qboot_src_read_pos() - sizeof(rt_uint32_t))) == fw_info->hdr_crc);
}

/**
 * @brief Calculate CRC32 of a storage region and compare to expected value.
 *
 * @param handle Storage handle.
 * @param name   Storage name for logs.
 * @param addr   Start offset in bytes.
 * @param size   Length in bytes.
 * @param crc    Expected CRC32 value.
 *
 * @return RT_TRUE when CRC matches, RT_FALSE otherwise.
 */
static rt_bool_t qbt_fw_crc_check(void *handle, const char *name, rt_uint32_t addr, rt_uint32_t size, rt_uint32_t crc)
{
    rt_uint32_t pos = 0;
    rt_uint32_t crc32 = 0xFFFFFFFF;
    /* Stream CRC to avoid large memory usage. */
    while (pos < size)
    {
        int read_len = QBOOT_BUF_SIZE;
        int gzip_remain_len = size - pos;
        if (read_len > gzip_remain_len)
        {
            read_len = gzip_remain_len;
        }
        if (_header_io_ops->read(handle, addr + pos, g_cmprs_buf, read_len) != RT_EOK)
        {
            LOG_E("Qboot read firmware datas fail. part = %s, addr = %08X, length = %d", name, pos, read_len);
            return RT_FALSE;
        }
        crc32 = crc32_cyc_cal(crc32, g_cmprs_buf, read_len);
        pos += read_len;
    }
    crc32 ^= 0xFFFFFFFF;

    if (crc32 != crc)
    {
        LOG_E("Qboot verify CRC32 error, cal.crc: %08X != body.crc: %08X", crc32, crc);
        return RT_FALSE;
    }

    return RT_TRUE;
}

#ifdef QBOOT_USING_APP_CHECK
/**
 * @brief Stream and verify application CRC using the selected algorithm.
 *
 * @param src_handle Package source handle.
 * @param src_name   Source name (unused, retained for signature compatibility).
 * @param fw_info    Firmware info header.
 *
 * @return RT_TRUE on success, RT_FALSE on error or CRC mismatch.
 */
static rt_bool_t qbt_app_crc_check(void *src_handle, const char *src_name, fw_info_t *fw_info)
{
    rt_bool_t ret = RT_TRUE;
    rt_uint32_t crc32 = 0xFFFFFFFF;
    qbt_algo_context_t algo_ops = {0};
    RT_UNUSED(src_name);

    if (qbt_fw_get_algo_context(fw_info, &algo_ops) == RT_FALSE)
    {
        LOG_E("Qboot release firmware fail. algo 0x%04X not registered.", fw_info->algo);
        return RT_FALSE;
    }

    if (!qbt_fw_algo_init(&algo_ops))
    {
        LOG_E("Qboot app crc check fail. algo init fail");
        return RT_FALSE;
    }

    qbt_stream_cfg_t stream_cfg = {
        .src_handle = src_handle,
        .dst_handle = RT_NULL,
        .fw_info = fw_info,
        .algo_ops = &algo_ops,
        .cmprs_buf = g_cmprs_buf,
        .out_buf = g_decmprs_buf,
        .crypt_buf = g_crypt_buf,
    };

    ret = qbt_fw_stream_process(&stream_cfg, QBT_STREAM_CRC, qbt_stream_crc_proc, &crc32);
    if (!ret)
    {
        LOG_E("Qboot app crc check fail. decompress error.");
    }
    else
    {
        crc32 ^= 0xFFFFFFFF;
        if (crc32 != fw_info->raw_crc)
        {
            LOG_E("Qboot app crc check fail. cal.crc: %08X != raw.crc: %08X", crc32, fw_info->raw_crc);
            ret = RT_FALSE;
        }
        else
        {
            ret = RT_TRUE;
        }
    }

    qbt_fw_algo_deinit(&algo_ops);
    return ret;
}
#endif

/**
 * @brief Release firmware package to the destination partition.
 *
 * @param dst_handle Destination handle (partition/file).
 * @param dst_size   Destination total size.
 * @param dst_name   Destination name.
 * @param src_handle Source package handle.
 * @param src_name   Source name (unused, retained for signature compatibility).
 * @param fw_info    Firmware info header.
 *
 * @return RT_TRUE on success, RT_FALSE on error.
 */
static rt_bool_t qbt_fw_release(void *dst_handle, rt_uint32_t dst_size, const char *dst_name, void *src_handle, const char *src_name, fw_info_t *fw_info)
{
    qbt_algo_context_t algo_ops = {0};

    if (qbt_fw_get_algo_context(fw_info, &algo_ops) == RT_FALSE)
    {
        LOG_E("Qboot release firmware fail. algo 0x%04X not registered.", fw_info->algo);
        return RT_FALSE;
    }

    if (!qbt_fw_algo_init(&algo_ops))
    {
        LOG_E("Qboot release firmware fail. algo init fail");
        return RT_FALSE;
    }

#ifdef QBOOT_USING_HPATCHLITE
    if (algo_ops.cmprs_ops->cmprs_id == QBOOT_ALGO_CMPRS_HPATCHLITE)
    {
        extern int qbt_hpatchlite_release_from_part(void *patch_part, void *old_part, const char *patch_name, const char *old_name, int patch_file_len, int newer_file_len, int patch_file_offset);
        if (qbt_hpatchlite_release_from_part(src_handle, dst_handle, src_name, dst_name, fw_info->pkg_size, fw_info->raw_size, qboot_src_read_pos()) == RT_TRUE)
        {
            goto done;
        }
        else
        {
            return RT_FALSE;
        }
    }
#endif
    rt_kprintf("Start erase partition %s ...\n", dst_name);
    if ((qbt_erase_with_feed(dst_handle, 0, fw_info->raw_size) != RT_EOK) || (qbt_erase_with_feed(dst_handle, dst_size - qboot_src_read_pos(), qboot_src_read_pos()) != RT_EOK))
    {
        qbt_fw_algo_deinit(&algo_ops);
        LOG_E("Qboot release firmware fail. erase %s error.", dst_name);
        return RT_FALSE;
    }

    rt_kprintf("Start release firmware to %s ...     \n", dst_name);
    qbt_stream_cfg_t stream_cfg = {
        .src_handle = src_handle,
        .dst_handle = dst_handle,
        .fw_info = fw_info,
        .algo_ops = &algo_ops,
        .cmprs_buf = g_cmprs_buf,
        .out_buf = g_decmprs_buf,
        .crypt_buf = g_crypt_buf,
    };
    qbt_stream_state_t stream_state = {
        .dst_handle = dst_handle,
        .raw_pos = 0,
        .raw_size = fw_info->raw_size,
    };

    if (!qbt_fw_stream_process(&stream_cfg, QBT_STREAM_WRITE, qbt_stream_write_proc, &stream_state))
    {
        qbt_fw_algo_deinit(&algo_ops);
        LOG_E("Qboot release firmware fail. stream process to %s fail.", dst_name);
        return RT_FALSE;
    }
    rt_kprintf("\n");

#ifdef QBOOT_USING_HPATCHLITE
done:
#endif
    qbt_fw_algo_deinit(&algo_ops);
    if (!qbt_fw_info_write(dst_handle, dst_size, fw_info, RT_TRUE))
    {
        LOG_E("Qboot release firmware fail. write firmware to %s fail.", dst_name);
        return RT_FALSE;
    }

    return RT_TRUE;
}

/**
 * @brief Verify released firmware in destination partition.
 *
 * @param handle   Destination handle.
 * @param part_len Destination size in bytes.
 * @param name     Destination name for logs.
 *
 * @return RT_TRUE when firmware is valid, RT_FALSE otherwise.
 */
rt_weak rt_bool_t qbt_dest_part_verify(void *handle, rt_uint32_t part_len, const char *name)
{
    if (!qbt_fw_info_read(handle, part_len, &fw_info, RT_TRUE))
    {
        LOG_E("Qboot verify fail, read firmware from %s partition", name);
        return RT_FALSE;
    }

    if (!qbt_fw_info_check(&fw_info))
    {
        LOG_E("Qboot verify fail. firmware infomation check fail.");
        return RT_FALSE;
    }

    switch (fw_info.algo2 & QBOOT_ALGO2_VERIFY_MASK)
    {
    case QBOOT_ALGO2_VERIFY_CRC:
        if (!qbt_fw_crc_check(handle, name, 0, fw_info.raw_size, fw_info.raw_crc))
        {
            return RT_FALSE;
        }
        break;

    default:
        break;
    }

    return RT_TRUE;
}

/**
 * @brief Check firmware package header and body CRC.
 *
 * @param fw_handle Firmware package handle.
 * @param part_len  Package length in bytes.
 * @param name      Package name for logs.
 * @param fw_info   Output firmware header.
 *
 * @return RT_TRUE when package is valid, RT_FALSE otherwise.
 */
rt_weak rt_bool_t qbt_fw_check(void *fw_handle, rt_uint32_t part_len, const char *name, fw_info_t *fw_info)
{
    if(fw_info == RT_NULL)
    {
        return RT_FALSE;
    }

    if (!qbt_fw_info_read(fw_handle, part_len, fw_info, RT_FALSE))
    {
        LOG_E("Qboot firmware check fail. partition \"%s\" read fail.", name);
        return RT_FALSE;
    }

    if (!qbt_fw_info_check(fw_info))
    {
        LOG_E("Qboot firmware check fail. partition \"%s\" infomation check fail.", name);
        return RT_FALSE;
    }

    if (!qbt_fw_crc_check(fw_handle, name, qboot_src_read_pos(), fw_info->pkg_size, fw_info->pkg_crc))
    {
        LOG_E("Qboot firmware check fail. partition \"%s\" body check fail.", name);
        return RT_FALSE;
    }

#ifdef QBOOT_USING_APP_CHECK
    /*
    * The difference algorithm does not support streaming decompression. Therefore, 
    * it cannot support the CRC check of the app and can only rely on the CRC of the package.
    */
    if((fw_info->algo & QBOOT_ALGO_CMPRS_MASK) != QBOOT_ALGO_CMPRS_HPATCHLITE)
    {
        if ((fw_info->algo2 & QBOOT_ALGO2_VERIFY_MASK) == QBOOT_ALGO2_VERIFY_CRC)
        {
            if (!qbt_app_crc_check(fw_handle, name, fw_info))
            {
                LOG_E("Qboot firmware check fail. partition \"%s\" app check fail.", name);
                return RT_FALSE;
            }
        }
    }
#endif

    LOG_D("Qboot partition \"%s\" firmware check success.", name);
    return RT_TRUE;
}

/**
 * @brief Release firmware and verify destination partition.
 *
 * @param dst_handle Destination handle.
 * @param dst_size   Destination size in bytes.
 * @param dst_name   Destination name.
 * @param src_handle Source package handle.
 * @param src_name   Source name.
 * @param fw_info    Firmware header.
 *
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_fw_update(void *dst_handle, rt_uint32_t dst_size, const char *dst_name, void *src_handle, const char *src_name, fw_info_t *fw_info)
{
    rt_bool_t rst;

#ifdef QBOOT_USING_STATUS_LED
    qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 50);
#endif

    rst = qbt_fw_release(dst_handle, dst_size, dst_name, src_handle, src_name, fw_info);

#ifdef QBOOT_USING_STATUS_LED
    qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 450);
#endif

    if (!rst)
    {
        LOG_E("Qboot firmware update fail. firmware release fail.");
        return RT_FALSE;
    }

    if (!qbt_dest_part_verify(dst_handle, dst_size, dst_name))
    {
        LOG_E("Qboot firmware update fail. destination partition verify fail.");
        return RT_FALSE;
    }

    LOG_I("Qboot firmware update success.");
    return RT_TRUE;
}

#if 0
__WEAK void qbt_jump_to_app(void)
{
    typedef void (*app_func_t)(void);
    rt_uint32_t app_addr = QBOOT_APP_ADDR;
    rt_uint32_t stk_addr = *((__IO uint32_t *)app_addr);
    app_func_t app_func = (app_func_t)(*((__IO uint32_t *)(app_addr + 4)));

    if ((((rt_uint32_t)app_func & 0xff000000) != 0x08000000) || ((stk_addr & 0x2ff00000) != 0x20000000))
    {
        LOG_E("No legitimate application.");
        return;
    }

    rt_kprintf("Jump to application running ... \n");
    rt_thread_mdelay(200);
    
    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();
    
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    for(int i=0; i<128; i++)
    {
        HAL_NVIC_DisableIRQ(i);
        HAL_NVIC_ClearPendingIRQ(i);
    }
    
    __set_CONTROL(0);
    __set_MSP(stk_addr);
    
    app_func();//Jump to application running
    
    LOG_E("Qboot jump to application fail.");
}
#else
extern void qbt_jump_to_app(void);
#endif

/**
 * @brief Feed watchdog and jump to application.
 *
 * @return None.
 */
static void qbt_jump_to_app_with_feed(void)
{
    qbt_wdt_feed();
    qbt_jump_to_app();
}

#ifdef QBOOT_USING_STATUS_LED
/**
 * @brief Initialize status LED pattern.
 *
 * @return None.
 */
static void qbt_status_led_init(void)
{
    qled_add(QBOOT_STATUS_LED_PIN, QBOOT_STATUS_LED_LEVEL);
    qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 450);
}
#endif

#ifdef QBOOT_USING_FACTORY_KEY
/**
 * @brief Check factory key press duration.
 *
 * @return RT_TRUE when key press is confirmed, RT_FALSE otherwise.
 */
static rt_bool_t qbt_factory_key_check(void)
{
    rt_bool_t rst = RT_TRUE;

#if (QBOOT_FACTORY_KEY_LEVEL)//press level is high
    rt_pin_mode(QBOOT_FACTORY_KEY_PIN, PIN_MODE_INPUT_PULLDOWN);
#else
    rt_pin_mode(QBOOT_FACTORY_KEY_PIN, PIN_MODE_INPUT_PULLUP);
#endif

    rt_thread_mdelay(500);

#ifdef QBOOT_USING_STATUS_LED
    qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 200);
#endif

    for (int i = 0; i < ((QBOOT_FACTORY_KEY_CHK_TMO * 10) + 1); i++)
    {
        rt_thread_mdelay(100);
        if (rt_pin_read(QBOOT_FACTORY_KEY_PIN) != QBOOT_FACTORY_KEY_LEVEL)
        {
            rst = RT_FALSE;
            break;
        }
    }

#ifdef QBOOT_USING_STATUS_LED
    qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 450);
#endif

    return (rst);
}
#endif

#ifdef QBOOT_USING_SHELL
static rt_sem_t qbt_shell_sem = NULL;
static rt_device_t qbt_shell_dev = NULL;

/**
 * @brief Stop the system shell thread and RX semaphore.
 *
 * @return None.
 */
static void qbt_close_sys_shell(void)
{
    rt_thread_t thread = rt_thread_find(FINSH_THREAD_NAME);
    if (thread == NULL)
    {
        return;
    }
    if (rt_object_is_systemobject((rt_object_t)thread))
    {
        rt_thread_detach(thread);
    }
    else
    {
        rt_thread_delete(thread);
    }

    rt_sem_t sem = (rt_sem_t)rt_object_find("shrx", RT_Object_Class_Semaphore);
    if (sem == NULL)
    {
        return;
    }
    if (rt_object_is_systemobject((rt_object_t)sem))
    {
        rt_sem_detach(sem);
    }
    else
    {
        rt_sem_delete(sem);
    }
}

/**
 * @brief Start system shell if not running.
 *
 * @return None.
 */
static void qbt_open_sys_shell(void)
{
    rt_thread_t thread = rt_thread_find(FINSH_THREAD_NAME);
    if (thread == NULL || RT_SCHED_CTX(thread).stat == RT_THREAD_CLOSE)
    {
#ifdef QBOOT_USING_STATUS_LED
        qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 950);
#endif
        finsh_set_prompt(QBOOT_SHELL_PROMPT);
        extern int finsh_system_init(void);
        finsh_system_init();
    }
}

/**
 * @brief RX indication for shell device.
 *
 * @param dev  Shell device.
 * @param size Received byte count.
 * @return RT_EOK always.
 */
static rt_err_t qbt_shell_rx_ind(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(qbt_shell_sem);
    return (RT_EOK);
}

/**
 * @brief Initialize shell device and RX callback.
 *
 * @param shell_dev_name Device name.
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_shell_init(const char *shell_dev_name)
{
    rt_device_t dev = rt_device_find(shell_dev_name);
    if (dev == NULL)
    {
        LOG_E("Qboot shell initialize fail. no find device: %s.", shell_dev_name);
        return RT_FALSE;
    }

    if (qbt_shell_sem == NULL)
    {
        qbt_shell_sem = rt_sem_create("qboot_shell", 0, RT_IPC_FLAG_FIFO);
        if (qbt_shell_sem == NULL)
        {
            LOG_E("Qboot shell initialize fail. sem create fail.");
            return RT_FALSE;
        }
    }

    if (dev == qbt_shell_dev)
    {
        return RT_TRUE;
    }

    if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_STREAM) != RT_EOK)
    {
        LOG_E("Qboot shell initialize fail. device %s open failed.", shell_dev_name);
        return RT_FALSE;
    }

    if (qbt_shell_dev != RT_NULL)
    {
        rt_device_close(qbt_shell_dev);
        rt_device_set_rx_indicate(qbt_shell_dev, NULL);
    }

    qbt_shell_dev = dev;
    rt_device_set_rx_indicate(dev, qbt_shell_rx_ind);

    LOG_D("shell device %s open success.", shell_dev_name);

    return RT_TRUE;
}

/**
 * @brief Wait for Enter key within timeout.
 *
 * @return RT_TRUE when Enter is received, RT_FALSE otherwise.
 */
static rt_bool_t qbt_shell_key_check(void)
{
    char ch;
    rt_tick_t tick_start = rt_tick_get();
    rt_tick_t tmo = rt_tick_from_millisecond(QBOOT_SHELL_KEY_CHK_TMO * 1000);

    while (rt_tick_get() - tick_start < tmo)
    {
        if (rt_sem_take(qbt_shell_sem, 100) != RT_EOK)
        {
            continue;
        }
        if (rt_device_read(qbt_shell_dev, -1, &ch, 1) > 0)
        {
            if (ch == 0x0d)
            {
                return RT_TRUE;
            }
            continue;
        }
    }

    return RT_FALSE;
}

/**
 * @brief Start shell and optionally wait for Enter.
 *
 * @param wait_press_key RT_TRUE to wait for Enter, RT_FALSE to start immediately.
 * @return RT_TRUE when shell is started, RT_FALSE otherwise.
 */
static rt_bool_t qbt_startup_shell(rt_bool_t wait_press_key)
{
    if (!qbt_shell_init(RT_CONSOLE_DEVICE_NAME))
    {
        LOG_E("Qboot initialize shell fail.");
        return RT_FALSE;
    }

    if (wait_press_key)
    {
        rt_bool_t rst;
        rt_kprintf("Press [Enter] key into shell in %d s : ", QBOOT_SHELL_KEY_CHK_TMO);
        rst = qbt_shell_key_check();
        rt_kprintf("\r\n");
        if (!rst)
        {
            return RT_FALSE;
        }
    }

    qbt_open_sys_shell();

    return RT_TRUE;
}
#endif

/**
 * @brief Print startup banner and product info.
 *
 * @return None.
 */
static void qbt_show_msg(void)
{
    //rt_kprintf("\033[2J\033[0;0H"); //clear screen
    rt_kprintf("\n");
    rt_kprintf("Qboot startup ... \n");
    rt_kprintf("Qboot version   : %s \n", QBOOT_VER_MSG);

#ifdef QBOOT_USING_PRODUCT_INFO
    rt_kprintf("Product name    : %s \n", QBOOT_PRODUCT_NAME);
    rt_kprintf("Product version : %s \n", QBOOT_PRODUCT_VER);
    rt_kprintf("Product mcu     : %s \n", QBOOT_PRODUCT_MCU);
#endif
}

/**
 * @brief Resume application from a specified target partition.
 *
 * @param src_id Source target ID.
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_app_resume_from(qbt_target_id_t src_id)
{
    void *src_handle = RT_NULL;
    void *dst_handle = RT_NULL;
    rt_uint32_t src_size = 0;
    rt_uint32_t dst_size = 0;
    rt_bool_t rst = RT_FALSE;
    const qboot_store_desc_t *src_desc = qbt_target_desc(src_id);

    if (src_desc == RT_NULL || !qbt_target_open(src_id, &src_handle, &src_size, QBT_OPEN_READ))
    {
        LOG_E("Qboot resume fail. target id %d is not exist.", src_id);
        return RT_FALSE;
    }

    const qboot_store_desc_t *app_desc = qbt_target_desc(QBOOT_TARGET_APP);
    if (app_desc == RT_NULL || !qbt_target_open(QBOOT_TARGET_APP, &dst_handle, &dst_size, QBT_OPEN_WRITE | QBT_OPEN_CREATE))
    {
        LOG_E("Qboot resume fail from %s.", src_desc->role_name);
        LOG_E("Destination partition %s is not exist.", app_desc ? app_desc->role_name : "app");
        qbt_target_close(src_handle);
        return RT_FALSE;
    }

    if (!qbt_fw_check(src_handle, src_size, src_desc->role_name, &fw_info))
    {
        goto exit;
    }

#ifdef QBOOT_USING_PRODUCT_CODE
    if (rt_strcmp((char *)fw_info.prod_code, QBOOT_PRODUCT_CODE) != 0)
    {
        LOG_E("Qboot resume fail from %s.", src_desc->role_name);
        LOG_E("The product code error. ");
        goto exit;
    }
#endif

    if (rt_strcmp((char *)fw_info.part_name, app_desc->role_name) != 0)
    {
        LOG_E("Qboot resume fail from %s.", src_desc->role_name);
        LOG_E("The firmware of %s partition is not application. fw_info.part_name(%s) != %s", src_desc->role_name, fw_info.part_name, app_desc->role_name);
        goto exit;
    }

    if (!qbt_fw_update(dst_handle, dst_size, app_desc->role_name, src_handle, src_desc->role_name, &fw_info))
    {
        goto exit;
    }

    LOG_I("Qboot resume success from %s.", src_desc->role_name);
    rst = RT_TRUE;

exit:
    qbt_target_close(src_handle);
    qbt_target_close(dst_handle);
    return rst;
}

/**
 * @brief Release firmware from a source partition to APP.
 *
 * @param src_id     Source target ID.
 * @param check_sign RT_TRUE to check release sign, RT_FALSE otherwise.
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_release_from_part(qbt_target_id_t src_id, rt_bool_t check_sign)
{
    qbt_target_id_t dst_id = QBOOT_TARGET_COUNT;
    void *src_handle = RT_NULL;
    void *dst_handle = RT_NULL;
    rt_uint32_t src_size = 0;
    rt_uint32_t dst_size = 0;
    rt_bool_t rst = RT_FALSE;
    const qboot_store_desc_t *src_desc = qbt_target_desc(src_id);

    if (src_desc == RT_NULL || !qbt_target_open(src_id, &src_handle, &src_size, QBT_OPEN_READ))
    {
        LOG_E("Qboot release fail. target id %d is not exist.", src_id);
        return RT_FALSE;
    }

    if (!qbt_fw_check(src_handle, src_size, src_desc->role_name, &fw_info))
    {
        goto exit;
    }

#ifdef QBOOT_USING_PRODUCT_CODE
    if (rt_strcmp((char *)fw_info.prod_code, QBOOT_PRODUCT_CODE) != 0)
    {
        LOG_E("The product code error.");
        goto exit;
    }
#endif

    if (check_sign)
    {
        if (qbt_release_sign_check(src_handle, src_desc->role_name, &fw_info))//not need release
        {
            rst = RT_TRUE;
            goto exit;
        }
    }

    dst_id = qbt_name_to_id((char *)fw_info.part_name);
    if (dst_id >= QBOOT_TARGET_COUNT || !qbt_target_open(dst_id, &dst_handle, &dst_size, QBT_OPEN_WRITE | QBT_OPEN_CREATE))
    {
        LOG_E("The destination %s partition is not exist.", fw_info.part_name);
        goto exit;
    }

    if (!qbt_fw_update(dst_handle, dst_size, (char *)fw_info.part_name, src_handle, src_desc->role_name, &fw_info))
    {
        goto exit;
    }

    if (!qbt_release_sign_check(src_handle, src_desc->role_name, &fw_info))
    {
        qbt_release_sign_write(src_handle, src_desc->role_name, &fw_info);
    }

    LOG_I("Release firmware success from %s to %s.", src_desc->role_name, fw_info.part_name);
    rst = RT_TRUE;

exit:
    if (dst_handle != RT_NULL)
    {
        qbt_target_close(dst_handle);
    }
    if (src_handle != RT_NULL)
    {
        qbt_target_close(src_handle);
    }
    return rst;
}

/**
 * @brief Main qboot thread entry.
 *
 * @param params Thread parameter (unused).
 * @return None.
 */
static void qbt_thread_entry(void *params)
{
#define QBOOT_REBOOT_DELAY_MS 5000

#ifdef QBOOT_USING_SHELL
    rt_thread_mdelay(2);
    qbt_close_sys_shell();
#endif

    qbt_show_msg();

#ifdef QBOOT_USING_STATUS_LED
    qbt_status_led_init();
#endif

    if (qboot_register_storage_ops() != RT_EOK)
    {
        LOG_E("Qboot register storage ops fail.");
#ifdef QBOOT_USING_SHELL
        if (qbt_startup_shell(RT_FALSE))
        {
            return;
        }
#endif
        LOG_I("Qboot will reboot after %d ms.", QBOOT_REBOOT_DELAY_MS);
        rt_thread_mdelay(QBOOT_REBOOT_DELAY_MS);
        rt_hw_cpu_reset();
    }

#ifdef QBOOT_USING_FACTORY_KEY
    if (qbt_factory_key_check())
    {
        if (qbt_app_resume_from(QBOOT_TARGET_FACTORY))
        {
            qbt_jump_to_app_with_feed();
        }
    }
#endif

#ifdef QBOOT_USING_SHELL
    if (qbt_startup_shell(RT_TRUE))
    {
        return;
    }
#endif

#ifdef QBOOT_USING_UPDATE_MGR
    qbt_update_mgr_poll(QBOOT_POLL_DELAY_MS);
    qboot_notify_update_result(RT_TRUE);
#endif

    const qboot_store_desc_t *download_desc = qbt_target_desc(QBOOT_TARGET_DOWNLOAD);
    qbt_release_from_part(QBOOT_TARGET_DOWNLOAD, RT_TRUE);
    qbt_jump_to_app_with_feed();

    LOG_I("Try resume application from %s", download_desc->role_name);
    if (qbt_app_resume_from(QBOOT_TARGET_DOWNLOAD))
    {
        qbt_jump_to_app_with_feed();
    }

    const qboot_store_desc_t *factory_desc = qbt_target_desc(QBOOT_TARGET_FACTORY);
    LOG_I("Try resume application from %s", factory_desc->role_name);
    if (qbt_app_resume_from(QBOOT_TARGET_FACTORY))
    {
        qbt_jump_to_app_with_feed();
    }

    qboot_notify_update_result(RT_FALSE);
#ifdef QBOOT_USING_SHELL
    if (qbt_startup_shell(RT_FALSE))
    {
        return;
    }
#endif

    LOG_I("Qboot will reboot after %d ms.", QBOOT_REBOOT_DELAY_MS);
    rt_thread_mdelay(QBOOT_REBOOT_DELAY_MS);
    rt_hw_cpu_reset();
}

/**
 * @brief Create and start qboot thread.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static int qbt_startup(void)
{
    if(qbot_algo_startup() != RT_EOK)
    {
        LOG_E("qbot_algo_startup fail.");
        return -RT_ERROR;
    }

    rt_thread_t tid = rt_thread_create("Qboot", qbt_thread_entry, NULL, QBOOT_THREAD_STACK_SIZE, QBOOT_THREAD_PRIO, 20);
    if (tid == NULL)
    {
        LOG_E("Qboot thread create fail.");
        return -RT_ERROR;
    }

    rt_thread_startup(tid);

    return RT_EOK;
}
INIT_APP_EXPORT(qbt_startup);

#ifdef QBOOT_USING_SHELL
/**
 * @brief Clone firmware from source to destination.
 *
 * @param dst_handle Destination handle.
 * @param dst_name   Destination name.
 * @param src_handle Source handle.
 * @param src_name   Source name.
 * @param fw_pkg_size Package size in bytes.
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_fw_clone(void *dst_handle, const char *dst_name, void *src_handle, const char *src_name, rt_uint32_t fw_pkg_size)
{
    rt_uint32_t pos = 0;

    rt_kprintf("Erasing %s partition ... \n", dst_name);
    if (qbt_erase_with_feed(dst_handle, 0, fw_pkg_size) != RT_EOK)
    {
        LOG_E("Qboot clone firmware fail. erase %s error.", dst_name);
        return RT_FALSE;
    }

    rt_kprintf("Cloning firmware from %s to %s ...    ", src_name, dst_name);
    while (pos < fw_pkg_size)
    {
        int read_len = QBOOT_BUF_SIZE;
        int remain_len = fw_pkg_size - pos;
        if (read_len > remain_len)
        {
            read_len = remain_len;
        }
        if (_header_io_ops->read(src_handle, pos, g_cmprs_buf, read_len) != RT_EOK)
        {
            LOG_E("Qboot clone firmware fail. read error, part = %s, addr = %08X, length = %d", src_name, pos, read_len);
            return RT_FALSE;
        }
        if (_header_io_ops->write(dst_handle, pos, g_cmprs_buf, read_len) != RT_EOK)
        {
            LOG_E("Qboot clone firmware fail. write error, part = %s, addr = %08X, length = %d", dst_name, pos, read_len);
            return RT_FALSE;
        }
        pos += read_len;
        rt_kprintf("\b\b\b%02d%%", (pos * 100 / fw_pkg_size));
    }
    rt_kprintf("\n");

    return RT_TRUE;
}
/**
 * @brief Show firmware information of a target partition.
 *
 * @param part_id Target partition ID.
 * @return None.
 */
static void qbt_fw_info_show(qbt_target_id_t part_id)
{
    void *handle = RT_NULL;
    rt_uint32_t part_size = 0;
    const qboot_store_desc_t *desc = qbt_target_desc(part_id);

    if (desc == RT_NULL)
    {
        rt_kprintf("The target id %d is not exist.\n", part_id);
        return;
    }
    if (!qbt_target_open(part_id, &handle, &part_size, QBT_OPEN_READ))
    {
        rt_kprintf("The %s partition is not exist.\n", desc->role_name);
        return;
    }

    if (!qbt_fw_check(handle, part_size, desc->role_name, &fw_info))
    {
        qbt_target_close(handle);
        return;
    }

    qbt_algo_context_t algo_ops = {0};
    qbt_fw_get_algo_context(&fw_info, &algo_ops);
    rt_kprintf("==== Firmware infomation of %s partition ====\n", desc->role_name);
    rt_kprintf("| Product code          | %*.s |\n", 20, fw_info.prod_code);
    rt_kprintf("| Crypt Algorithm       | %*.s |\n", 20, algo_ops.crypt_ops->crypto_name);
    rt_kprintf("| Cmprs Algorithm       | %*.s |\n", 20, algo_ops.cmprs_ops->cmprs_name);
    rt_kprintf("| Destition partition   | %*.s |\n", 20, fw_info.part_name);
    rt_kprintf("| Version               | %*.s |\n", 20, fw_info.fw_ver);
    rt_kprintf("| Package size          | %20d |\n", fw_info.pkg_size);
    rt_kprintf("| Raw code size         | %20d |\n", fw_info.raw_size);
    rt_kprintf("| Package crc           | %20X |\n", fw_info.pkg_crc);
    rt_kprintf("| Raw code verify       | %20X |\n", fw_info.raw_crc);
    rt_kprintf("| Header crc            | %20X |\n", fw_info.hdr_crc);
    rt_kprintf("| Build timestamp       | %20d |\n", fw_info.time_stamp);
    rt_kprintf("\n");
    qbt_target_close(handle);
}
/**
 * @brief Erase firmware in a target partition.
 *
 * @param handle    Target handle.
 * @param name      Target name.
 * @param part_size Partition size in bytes.
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
static rt_bool_t qbt_fw_delete(void *handle, const char *name, rt_uint32_t part_size)
{
    rt_kprintf("Erasing %s partition ... \n", name);
    if (qbt_erase_with_feed(handle, 0, part_size) != RT_EOK)
    {
        rt_kprintf("Qboot delete firmware fail. erase %s error.\n", name);
        return RT_FALSE;
    }

    rt_kprintf("Qboot delete firmware success.\n");

    return RT_TRUE;
}
/**
 * @brief Handle qboot shell commands.
 *
 * @param argc Argument count.
 * @param argv Argument list.
 * @return None.
 */
static void qbt_shell_cmd(rt_uint8_t argc, char **argv)
{
    const char *cmd_info[] = {
        "Usage:\n",
        "qboot probe                    - probe firmware packages\n",
        "qboot resume src_part          - resume application from src partiton\n",
        "qboot clone src_part dst_part  - clone src partition to dst partiton\n",
        "qboot release part             - release firmware from partiton\n",
        "qboot verify part              - verify released code of partition\n",
        "qboot del part                 - delete firmware of partiton\n",
        "qboot jump                     - jump to application\n",
        "\n"
    };

    if (argc < 2)
    {
        for (int i = 0; i < sizeof(cmd_info) / sizeof(char *); i++)
        {
            rt_kprintf(cmd_info[i]);
        }
        return;
    }

    if (rt_strcmp(argv[1], "probe") == 0)
    {
        qbt_fw_info_show(QBOOT_TARGET_DOWNLOAD);
        qbt_fw_info_show(QBOOT_TARGET_FACTORY);
        return;
    }

    if (rt_strcmp(argv[1], "resume") == 0)
    {
        qbt_target_id_t src_id = QBOOT_TARGET_COUNT;
        if (argc < 3)
        {
            rt_kprintf(cmd_info[2]);
            return;
        }
        src_id = qbt_name_to_id(argv[2]);
        if (src_id < QBOOT_TARGET_COUNT)
        {
            qbt_app_resume_from(src_id);
        }
        else
        {
            rt_kprintf("The %s partition is not exist.\n", argv[2]);
        }

#ifdef QBOOT_USING_STATUS_LED
        qled_set_blink(QBOOT_STATUS_LED_PIN, 50, 950);
#endif
        return;
    }

    if (rt_strcmp(argv[1], "clone") == 0)
    {
        char *src, *dst;
        qbt_target_id_t src_id = QBOOT_TARGET_COUNT;
        qbt_target_id_t dst_id = QBOOT_TARGET_COUNT;
        void *src_handle = RT_NULL;
        void *dst_handle = RT_NULL;
        rt_uint32_t src_size = 0;
        if (argc < 4)
        {
            rt_kprintf(cmd_info[3]);
            return;
        }
        src = argv[2];
        dst = argv[3];
        dst_id = qbt_name_to_id(dst);
        if (dst_id >= QBOOT_TARGET_COUNT || !qbt_target_open(dst_id, &dst_handle, NULL, QBT_OPEN_WRITE | QBT_OPEN_CREATE))
        {
            rt_kprintf("Desttition %s partition is not exist.\n", dst);
            return;
        }
        src_id = qbt_name_to_id(src);
        if (src_id >= QBOOT_TARGET_COUNT || !qbt_target_open(src_id, &src_handle, &src_size, QBT_OPEN_READ))
        {
            rt_kprintf("Soure %s partition is not exist.\n", src);
            qbt_target_close(dst_handle);
            return;
        }

        if (!qbt_fw_check(src_handle, src_size, src, &fw_info))
        {
            rt_kprintf("Soure %s partition firmware error.\n", src);
            qbt_target_close(src_handle);
            qbt_target_close(dst_handle);
            return;
        }
        if (qbt_fw_clone(dst_handle, dst, src_handle, src, qboot_src_read_pos() + fw_info.pkg_size))
        {
            rt_kprintf("Clone firmware success from %s to %s.\n", src, dst);
        }
        qbt_target_close(src_handle);
        qbt_target_close(dst_handle);
        return;
    }

    if (rt_strcmp(argv[1], "release") == 0)
    {
        char *part_name;
        qbt_target_id_t part_id = QBOOT_TARGET_COUNT;
        if (argc < 3)
        {
            rt_kprintf(cmd_info[4]);
            return;
        }
        part_name = argv[2];
        part_id = qbt_name_to_id(part_name);
        if (part_id < QBOOT_TARGET_COUNT && qbt_release_from_part(part_id, RT_FALSE))
        {
            rt_kprintf("Release firmware success from %s partition.\n", part_name);
        }
        else
        {
            rt_kprintf("Release firmware fail from %s partition.\n", part_name);
        }
        return;
    }

    if (rt_strcmp(argv[1], "verify") == 0)
    {
        char *part_name;
        qbt_target_id_t part_id = QBOOT_TARGET_COUNT;
        void *handle = RT_NULL;
        rt_uint32_t part_size = 0;
        if (argc < 3)
        {
            rt_kprintf(cmd_info[5]);
            return;
        }
        part_name = argv[2];
        part_id = qbt_name_to_id(part_name);
        if (part_id >= QBOOT_TARGET_COUNT || !qbt_target_open(part_id, &handle, &part_size, QBT_OPEN_READ))
        {
            rt_kprintf("%s partition is not exist.\n", part_name);
            return;
        }

        if (qbt_dest_part_verify(handle, part_size, part_name))
        {
            rt_kprintf("%s partition code verify ok.\n", part_name);
        }
        else
        {
            rt_kprintf("%s partition code verify error.\n", part_name);
        }
        qbt_target_close(handle);
        return;
    }

    if (rt_strcmp(argv[1], "del") == 0)
    {
        char *part_name;
        qbt_target_id_t part_id = QBOOT_TARGET_COUNT;
        void *handle = RT_NULL;
        rt_uint32_t part_size = 0;

        if (argc < 3)
        {
            rt_kprintf(cmd_info[6]);
            return;
        }

        part_name = argv[2];
        part_id = qbt_name_to_id(part_name);
        if (part_id >= QBOOT_TARGET_COUNT || !qbt_target_open(part_id, &handle, &part_size, QBT_OPEN_WRITE))
        {
            rt_kprintf("%s partition is not exist.\n", part_name);
            return;
        }

        qbt_fw_delete(handle, part_name, part_size);
        qbt_target_close(handle);

        return;
    }

    if (rt_strcmp(argv[1], "jump") == 0)
    {
        qbt_jump_to_app_with_feed();
        return;
    }

    rt_kprintf("No supported command.\n");
}
MSH_CMD_EXPORT_ALIAS(qbt_shell_cmd, qboot, Quick bootloader test commands);
#endif
