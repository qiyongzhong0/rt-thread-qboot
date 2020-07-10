/*
 * qboot_verify.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-07-06     qiyongzhong       first version
 */

#ifndef __QBOOT_VERIFY_H__
#define __QBOOT_VERIFY_H__

#include <qboot.h>

#ifdef QBOOT_USING_VERIFY

void qbt_verify_init(void);
void qbt_verify_cal(const u8 *buf, u32 len);
u32 qbt_verify_rst(void);

#endif

#endif

