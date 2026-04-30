/**
 * @file qboot_update.h
 * @brief 
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-01-23 1.0     wdfk-prog   first version
 */
#ifndef QBOOT_UPDATE_MGR_H
#define QBOOT_UPDATE_MGR_H

#include "qboot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Persistent update request reason.
 *
 * Stored outside qboot (EEPROM/Flash) via ops callbacks.
 */
typedef enum
{
    QBT_UPD_REASON_NONE = 0,        /**< No update requested. */
    QBT_UPD_REASON_REQ = 1,         /**< Update requested by application. */
    QBT_UPD_REASON_IN_PROGRESS = 2, /**< Update is in progress. */
    QBT_UPD_REASON_DONE = 3,        /**< Update finished successfully. */
} qbt_update_reason_t;

/**
 * @brief Runtime update state inside the manager.
 */
typedef enum
{
    QBT_UPD_STATE_IDLE = 0, /**< No update activity. */
    QBT_UPD_STATE_WAIT,     /**< Waiting for download start. */
    QBT_UPD_STATE_RECV,     /**< Receiving firmware data. */
    QBT_UPD_STATE_DONE,     /**< Update finished (reserved). */
    QBT_UPD_STATE_ERROR,    /**< Error state (transient). */
    QBT_UPD_STATE_READY,    /**< Ready to jump to application. */
} qbt_upd_state_t;

/**
 * @brief Callbacks provided by the bootloader layer.
 *
 * All callbacks must be implemented (no NULL checks inside update manager).
 */
typedef struct
{
    /**
     * @brief Check if the current application is valid.
     *
     * @return RT_TRUE if app can be jumped to, RT_FALSE otherwise.
     */
    rt_bool_t (*is_app_valid)(void);
    /**
     * @brief Read persistent update reason (from EEPROM/Flash).
     *
     * @return A value from qbt_update_reason_t.
     */
    rt_uint32_t (*get_reason)(void);
    /**
     * @brief Write persistent update reason (to EEPROM/Flash).
     *
     * @param reason A value from qbt_update_reason_t.
     */
    void (*set_reason)(rt_uint32_t reason);
    /**
     * @brief Called when a download session starts.
     *
     * Typical usage: switch LED blink pattern, pause app tasks, clear stats,
     * or change watchdog feed strategy.
     *
     * Must be implemented (non-NULL).
     */
    void (*enter_download)(void);
    /**
     * @brief Called when a download session ends or aborts.
     *
     * Typical usage: restore LED, resume tasks, or close comm resources.
     *
     * Must be implemented (non-NULL).
     */
    void (*leave_download)(void);
    /**
     * @brief Error notification hook.
     *
     * @param err Error code (implementation-defined).
     *
     * Typical usage: log/report error, increment error counters, or reset.
     *
     * Must be implemented (non-NULL).
     */
    void (*on_error)(int err);
    /**
     * @brief Notify bootloader to allow jump to application.
     */
    void (*on_ready_to_app)(void);
    /**
     * @brief Try to recover app from DOWNLOAD/FACTORY when app is invalid.
     *
     * Return RT_TRUE when recovery resources are valid and bootloader can continue.
     */
    rt_bool_t (*try_recover)(void);
} qbt_update_ops_t;

void qbt_update_mgr_register(const qbt_update_ops_t *ops, rt_uint32_t wait_ms, rt_uint32_t idle_ms);
qbt_upd_state_t qbt_update_mgr_get_state(void);
void qbt_update_mgr_poll(rt_uint32_t poll_delay_ms);

void qbt_update_mgr_on_request(void);
void qbt_update_mgr_on_start(void);
void qbt_update_mgr_on_data(void);

void qbt_update_mgr_set_total(rt_uint32_t total_bytes);
void qbt_update_mgr_on_data_len(rt_uint32_t bytes);

void qbt_update_mgr_on_finish(rt_bool_t ok);
void qbt_update_mgr_on_abort(void);
rt_bool_t qbt_update_mgr_download_begin(void);
rt_bool_t qbt_update_mgr_download_write(rt_uint32_t offset, rt_uint8_t *data, rt_uint32_t size);
rt_bool_t qbt_update_mgr_try_recover(void);

#ifdef __cplusplus
}
#endif

#endif /* QBOOT_UPDATE_MGR_H */
