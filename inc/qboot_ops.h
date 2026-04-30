/**
 * @file qboot_ops.h
 * @brief 
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-15
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-01-15 1.0     wdfk-prog   split type definitions
 */
#ifndef __QBOOT_TYPES_H__
#define __QBOOT_TYPES_H__

#include "qboot_cfg.h"
#include <rtthread.h>

/**
 * @brief Backend enable flags and count.
 */
#if defined(QBOOT_PKG_SOURCE_FAL)
#define QBT_BACKEND_FAL_ENABLED 1
#else
#define QBT_BACKEND_FAL_ENABLED 0
#endif

#if defined(QBOOT_PKG_SOURCE_FS)
#define QBT_BACKEND_FS_ENABLED 1
#else
#define QBT_BACKEND_FS_ENABLED 0
#endif

#if defined(QBOOT_PKG_SOURCE_CUSTOM)
#define QBT_BACKEND_CUSTOM_ENABLED 1
#else
#define QBT_BACKEND_CUSTOM_ENABLED 0
#endif

#define QBT_BACKEND_COUNT (QBT_BACKEND_FAL_ENABLED + QBT_BACKEND_FS_ENABLED + QBT_BACKEND_CUSTOM_ENABLED)

/**
 * @brief Firmware package header shared between boot and application.
 */
typedef struct
{
    rt_uint8_t type[4];         /**< Magic string, usually "RBL". */
    rt_uint16_t algo;           /**< Compress/encrypt algorithm bits. */
    rt_uint16_t algo2;          /**< Secondary algorithm flags (e.g. verify). */
    rt_uint32_t time_stamp;     /**< Build timestamp. */
    rt_uint8_t part_name[16];   /**< Target partition/file name. */
    rt_uint8_t fw_ver[24];      /**< Firmware version string. */
    rt_uint8_t prod_code[24];   /**< Product code string. */
    rt_uint32_t pkg_crc;        /**< CRC32 of the package body. */
    rt_uint32_t raw_crc;        /**< CRC32 of the raw firmware after decompress/decrypt. */
    rt_uint32_t raw_size;       /**< Raw firmware size in bytes. */
    rt_uint32_t pkg_size;       /**< Package size (excluding header). */
    rt_uint32_t hdr_crc;        /**< CRC32 of this header (excluding hdr_crc field). */
} fw_info_t;

/**
 * @brief Target list for application/factory/download roles (optional SWAP).
 *
 * Each entry maps a target ID to a logical role name used in headers/logs.
 * Backend storage names (FAL partition/file path/custom region) are filled
 * separately in qboot_store_desc_t::store_name.
 */
#if defined(QBOOT_USING_HPATCHLITE) && defined(QBOOT_HPATCH_USE_STORAGE_SWAP)
#define QBT_TARGET_LIST(X)                \
    X(APP, QBOOT_APP_PART_NAME)           \
    X(DOWNLOAD, QBOOT_DOWNLOAD_PART_NAME) \
    X(FACTORY, QBOOT_FACTORY_PART_NAME)   \
    X(SWAP, QBOOT_HPATCH_SWAP_PART_NAME)
#else
#define QBT_TARGET_LIST(X)                \
    X(APP, QBOOT_APP_PART_NAME)           \
    X(DOWNLOAD, QBOOT_DOWNLOAD_PART_NAME) \
    X(FACTORY, QBOOT_FACTORY_PART_NAME)
#endif

/**
 * @brief Target identifiers derived from QBT_TARGET_LIST.
 *
 * Use these IDs to index g_descs and open targets without string compares.
 */
typedef enum
{
#define QBOOT_TARGET_ENUM(id, name) QBOOT_TARGET_##id,
    QBT_TARGET_LIST(QBOOT_TARGET_ENUM)
#undef QBOOT_TARGET_ENUM
    QBOOT_TARGET_COUNT
} qbt_target_id_t;

/**
 * @brief Target descriptor used by storage backends.
 */
typedef enum
{
    QBT_STORE_BACKEND_FAL = 0,
    QBT_STORE_BACKEND_FS,
    QBT_STORE_BACKEND_CUSTOM,
    QBT_STORE_BACKEND_COUNT,
} qbt_store_backend_t;

typedef struct
{
    qbt_target_id_t id;          /**< Target id (APP/DOWNLOAD/FACTORY). */
    const char *role_name;       /**< Logical role name used in headers/logs. */
    const char *store_name;      /**< Backend store identifier (partition/path). */
    rt_uint32_t flash_addr;      /**< Base address for CUSTOM backend. */
    rt_uint32_t flash_len;       /**< Length in bytes for CUSTOM backend. */
    qbt_store_backend_t backend; /**< Backend selector (FAL/FS/CUSTOM). */
} qboot_store_desc_t;

/**
 * @brief IO control commands for qboot IO backends.
 */
#define QBOOT_IO_CMD_GET_ERASE_ALIGN 1

/**
 * @brief Common IO operations shared by sources and targets. must be non-NULL
 */
typedef struct
{
    rt_err_t (*open)(qbt_target_id_t id,          /**< Target identifier (APP/DOWNLOAD/FACTORY). */
                     void **handle,              /**< [out] Output handle for the opened target. */
                     int flags);                 /**< Open flags (QBT_OPEN_*). */
    rt_err_t (*close)(void *handle);              /**< Handle to close. */
    rt_err_t (*read)(void *handle,                /**< Handle to read from. */
                     rt_uint32_t off,             /**< Byte offset to read. */
                     void *buf,                   /**< Destination buffer. */
                     rt_uint32_t len);            /**< Number of bytes to read. */
    rt_err_t (*erase)(void *handle,               /**< Handle to erase on. */
                      rt_uint32_t off,            /**< Byte offset to erase. */
                      rt_uint32_t len);           /**< Number of bytes to erase. */
    rt_err_t (*write)(void *handle,               /**< Handle to write to. */
                      rt_uint32_t off,            /**< Byte offset to write. */
                      const void *buf,            /**< Source buffer to write. */
                      rt_uint32_t len);           /**< Number of bytes to write. */
    rt_err_t (*size)(void *handle,                /**< Handle to query size for. */
                     rt_uint32_t *out_size);      /**< [out] Total size in bytes. */
    rt_err_t (*ioctl)(void *handle,               /**< Handle to query/set. */
                      int cmd,                    /**< Command selector. */
                      void *arg);                 /**< Command argument. */
} qboot_io_ops_t;

/**
 * @brief Open flags for storage backends.
 */
#define QBT_OPEN_READ   0x01
#define QBT_OPEN_WRITE  0x02
#define QBT_OPEN_CREATE 0x04
#define QBT_OPEN_TRUNC  0x08

/**
 * @brief Header parser and package source operations. must be non-NULL
 */
typedef struct
{
    rt_err_t (*sign_read)(void *handle,                 /**< Handle to read from. */
                          rt_bool_t *released,          /**< [out] RT_TRUE when released sign is present. */
                          const fw_info_t *fw_info);    /**< Firmware header context. */
    rt_err_t (*sign_write)(void *handle,                /**< Handle to write to. */
                           const fw_info_t *fw_info);   /**< Firmware header context. */
    rt_err_t (*sign_clear)(void *handle,                /**< Handle to clear sign from. */
                           const fw_info_t *fw_info);   /**< Firmware header context. */
} qboot_header_parser_ops_t;

rt_err_t qboot_register_storage_ops(void);
rt_err_t qboot_register_header_parser_ops(const qboot_header_parser_ops_t *ops);
rt_err_t qboot_register_header_io_ops(const qboot_io_ops_t *ops);

/**
 * @brief Feed watchdog (weak hook).
 */
void qbt_wdt_feed(void);

rt_bool_t qbt_fw_info_read(void *handle, rt_uint32_t part_len, fw_info_t *fw_info, rt_bool_t from_tail);
rt_bool_t qbt_fw_info_write(void *handle, rt_uint32_t part_len, fw_info_t *fw_info, rt_bool_t to_tail);
rt_bool_t qbt_release_sign_check(void *handle, const char *name, fw_info_t *fw_info);
rt_bool_t qbt_release_sign_write(void *handle, const char *name, fw_info_t *fw_info);
rt_bool_t qbt_release_sign_clear(void *handle, const char *name, fw_info_t *fw_info);

rt_bool_t qbt_target_open(qbt_target_id_t id, void **handle, rt_uint32_t *out_size, int flags);
void qbt_target_close(void *handle);
const qboot_store_desc_t *qbt_target_desc(qbt_target_id_t id);
qbt_target_id_t qbt_name_to_id(const char *name);
rt_err_t qbt_erase_with_feed(void *handle, rt_uint32_t off, rt_uint32_t len);

/* weak function */
rt_bool_t qboot_should_enter_update(void);
void qbt_wdt_feed(void);
rt_bool_t qbt_ops_custom_init(void);
rt_int32_t qboot_src_read_pos(void);
void qboot_notify_update_result(rt_bool_t success);

extern const qboot_io_ops_t *_header_io_ops;

#endif
