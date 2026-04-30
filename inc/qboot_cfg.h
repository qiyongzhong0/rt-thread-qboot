/**
 * @file qboot_cfg.h
 * @brief 
 * @author qiyongzhong
 * @version 1.0
 * @date 2020-07-06
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2020-07-06 qiyongzhong       first version
 * 2026-01-15 wdfk-prog         split configuration header
 */
#ifndef __QBOOT_CFG_H__
#define __QBOOT_CFG_H__

#include <rtconfig.h>

//#define QBOOT_DEBUG
#define QBOOT_USING_LOG
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

//#define QBOOT_USING_PRODUCT_CODE
//#define QBOOT_USING_AES
//#define QBOOT_USING_GZIP
//#define QBOOT_USING_QUICKLZ
//#define QBOOT_USING_FASTLZ
//#define QBOOT_USING_SHELL
//#define QBOOT_USING_SYSWATCH
//#define QBOOT_USING_OTA_DOWNLOAD
//#define QBOOT_USING_PRODUCT_INFO
//#define QBOOT_USING_STATUS_LED
//#define QBOOT_USING_FACTORY_KEY
#define QBOOT_USING_APP_CHECK

#define QBOOT_RELEASE_SIGN_ALIGN_SIZE 8//can is 4, 8, 16
#define QBOOT_RELEASE_SIGN_WORD       0x5555AAAA

#define QBOOT_BUF_SIZE 4096//must is 4096
#if (defined(QBOOT_USING_QUICKLZ) || defined(QBOOT_USING_FASTLZ))
#define QBOOT_CMPRS_READ_SIZE 4096 //it can is 512, 1024, 2048, 4096,
#define QBOOT_CMPRS_BUF_SIZE  (QBOOT_BUF_SIZE + QBOOT_CMPRS_READ_SIZE + 32)
#else
#define QBOOT_CMPRS_READ_SIZE QBOOT_BUF_SIZE
#define QBOOT_CMPRS_BUF_SIZE  QBOOT_BUF_SIZE
#endif

#ifdef QBOOT_USING_STATUS_LED
#ifndef QBOOT_STATUS_LED_PIN
#define QBOOT_STATUS_LED_PIN 0
#endif
#ifndef QBOOT_STATUS_LED_LEVEL
#define QBOOT_STATUS_LED_LEVEL 1 //led on level, 0--low, 1--high
#endif
#endif

#ifdef QBOOT_USING_FACTORY_KEY
#ifndef QBOOT_FACTORY_KEY_PIN
#define QBOOT_FACTORY_KEY_PIN -1
#endif
#ifndef QBOOT_FACTORY_KEY_LEVEL
#define QBOOT_FACTORY_KEY_LEVEL 0 //key press level, 0--low, 1--high
#endif
#ifndef QBOOT_FACTORY_KEY_CHK_TMO
#define QBOOT_FACTORY_KEY_CHK_TMO 10
#endif
#endif

#ifdef QBOOT_USING_SHELL
#ifndef QBOOT_SHELL_KEY_CHK_TMO
#define QBOOT_SHELL_KEY_CHK_TMO 5
#endif
#endif

#ifdef RT_APP_PART_ADDR
#define QBOOT_APP_ADDR RT_APP_PART_ADDR
#else
#define QBOOT_APP_ADDR 0x08020000
#endif

#ifdef QBOOT_USING_PRODUCT_CODE
#ifndef QBOOT_PRODUCT_CODE
#define QBOOT_PRODUCT_CODE "00010203040506070809"
#endif
#endif

/* The character name is used for fw_info.part_name matching and log output, 
 * and should remain stable to avoid compatibility issues. */
#define QBOOT_APP_PART_NAME         "app"
#define QBOOT_DOWNLOAD_PART_NAME    "download"
#define QBOOT_FACTORY_PART_NAME     "factory"
#if defined(QBOOT_USING_HPATCHLITE)
#define QBOOT_HPATCH_SWAP_PART_NAME "swap"
#endif /* QBOOT_USING_HPATCHLITE */

#if defined(QBOOT_APP_STORE_FAL) && !defined(QBOOT_APP_FAL_PART_NAME)
#error "QBOOT_APP_FAL_PART_NAME must be defined when QBOOT_APP_STORE_FAL is enabled."
#endif
#if defined(QBOOT_APP_STORE_FS) && !defined(QBOOT_APP_FILE_PATH)
#error "QBOOT_APP_FILE_PATH must be defined when QBOOT_APP_STORE_FS is enabled."
#endif
#if defined(QBOOT_APP_STORE_FS) && !defined(QBOOT_APP_SIGN_FILE_PATH)
#error "QBOOT_APP_SIGN_FILE_PATH must be defined when QBOOT_APP_STORE_FS is enabled."
#endif
#if defined(QBOOT_APP_STORE_CUSTOM) && !defined(QBOOT_APP_FLASH_ADDR)
#error "QBOOT_APP_FLASH_ADDR must be defined when QBOOT_APP_STORE_CUSTOM is enabled."
#endif
#if defined(QBOOT_APP_STORE_CUSTOM) && !defined(QBOOT_APP_FLASH_LEN)
#error "QBOOT_APP_FLASH_LEN must be defined when QBOOT_APP_STORE_CUSTOM is enabled."
#endif

#if defined(QBOOT_DOWNLOAD_STORE_FAL) && !defined(QBOOT_DOWNLOAD_FAL_PART_NAME)
#error "QBOOT_DOWNLOAD_FAL_PART_NAME must be defined when QBOOT_DOWNLOAD_STORE_FAL is enabled."
#endif
#if defined(QBOOT_DOWNLOAD_STORE_FS) && !defined(QBOOT_DOWNLOAD_FILE_PATH)
#error "QBOOT_DOWNLOAD_FILE_PATH must be defined when QBOOT_DOWNLOAD_STORE_FS is enabled."
#endif
#if defined(QBOOT_DOWNLOAD_STORE_FS) && !defined(QBOOT_DOWNLOAD_SIGN_FILE_PATH)
#error "QBOOT_DOWNLOAD_SIGN_FILE_PATH must be defined when QBOOT_DOWNLOAD_STORE_FS is enabled."
#endif
#if defined(QBOOT_DOWNLOAD_STORE_CUSTOM) && !defined(QBOOT_DOWNLOAD_FLASH_ADDR)
#error "QBOOT_DOWNLOAD_FLASH_ADDR must be defined when QBOOT_DOWNLOAD_STORE_CUSTOM is enabled."
#endif
#if defined(QBOOT_DOWNLOAD_STORE_CUSTOM) && !defined(QBOOT_DOWNLOAD_FLASH_LEN)
#error "QBOOT_DOWNLOAD_FLASH_LEN must be defined when QBOOT_DOWNLOAD_STORE_CUSTOM is enabled."
#endif

#if defined(QBOOT_FACTORY_STORE_FAL) && !defined(QBOOT_FACTORY_FAL_PART_NAME)
#error "QBOOT_FACTORY_FAL_PART_NAME must be defined when QBOOT_FACTORY_STORE_FAL is enabled."
#endif
#if defined(QBOOT_FACTORY_STORE_FS) && !defined(QBOOT_FACTORY_FILE_PATH)
#error "QBOOT_FACTORY_FILE_PATH must be defined when QBOOT_FACTORY_STORE_FS is enabled."
#endif
#if defined(QBOOT_FACTORY_STORE_CUSTOM) && !defined(QBOOT_FACTORY_FLASH_ADDR)
#error "QBOOT_FACTORY_FLASH_ADDR must be defined when QBOOT_FACTORY_STORE_CUSTOM is enabled."
#endif
#if defined(QBOOT_FACTORY_STORE_CUSTOM) && !defined(QBOOT_FACTORY_FLASH_LEN)
#error "QBOOT_FACTORY_FLASH_LEN must be defined when QBOOT_FACTORY_STORE_CUSTOM is enabled."
#endif

#if defined(QBOOT_USING_HPATCHLITE) && defined(QBOOT_HPATCH_USE_STORAGE_SWAP)
#if defined(QBOOT_HPATCH_SWAP_STORE_FAL) && !defined(QBOOT_HPATCH_SWAP_PART_NAME)
#error "QBOOT_HPATCH_SWAP_PART_NAME must be defined when HPatchLite swap uses FAL."
#endif
#if defined(QBOOT_HPATCH_SWAP_STORE_CUSTOM) && !defined(QBOOT_HPATCH_SWAP_FLASH_LEN)
#error "QBOOT_HPATCH_SWAP_FLASH_LEN must be defined when HPatchLite swap uses custom flash."
#endif
#if defined(QBOOT_HPATCH_SWAP_STORE_FS) && !defined(QBOOT_HPATCH_SWAP_FILE_PATH)
#error "QBOOT_HPATCH_SWAP_FILE_PATH must be defined when HPatchLite swap uses filesystem."
#endif
#if defined(QBOOT_HPATCH_SWAP_STORE_FS) && !defined(QBOOT_HPATCH_SWAP_FILE_SIZE)
#error "QBOOT_HPATCH_SWAP_FILE_SIZE must be defined when HPatchLite swap uses filesystem."
#endif
#endif

#ifdef QBOOT_USING_AES
#ifndef QBOOT_AES_IV
#define QBOOT_AES_IV "0123456789ABCDEF"
#endif
#ifndef QBOOT_AES_KEY
#define QBOOT_AES_KEY "0123456789ABCDEF0123456789ABCDEF"
#endif
#endif

#ifdef QBOOT_USING_PRODUCT_INFO
#ifndef QBOOT_PRODUCT_NAME
#define QBOOT_PRODUCT_NAME "Qboot test device"
#endif
#ifndef QBOOT_PRODUCT_VER
#define QBOOT_PRODUCT_VER "v1.10 2026.01.01"
#endif
#ifndef QBOOT_PRODUCT_MCU
#define QBOOT_PRODUCT_MCU "stm32l4r5zi"
#endif
#endif

#ifndef QBOOT_THREAD_STACK_SIZE
#define QBOOT_THREAD_STACK_SIZE 4096
#endif

#ifndef QBOOT_THREAD_PRIO
#define QBOOT_THREAD_PRIO 5
#endif

#if (defined(QBOOT_USING_AES) || defined(QBOOT_USING_GZIP) || defined(QBOOT_USING_QUICKLZ) || defined(QBOOT_USING_FASTLZ))
#define QBOOT_USING_COMPRESSION
#endif

#ifndef QBOOT_POLL_DELAY_MS
#define QBOOT_POLL_DELAY_MS 20
#endif

#endif
