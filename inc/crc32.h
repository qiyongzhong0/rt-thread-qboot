/*
 * crc32.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-07-02     qiyongzhong       first version
 */

#ifndef __CRC32_H__
#define __CRC32_H__

#include "typedef.h"

//using const table in flash memory, nodefined using table in ram
#define CRC32_USING_CONST_TABLE 

//crc32 polynome, Poly = x32+x26+x23+...+x2+x+1 (ZIP,RAR,IEEE,LAN/FDDI,PPP-FCS)
#define CRC32_POLY  0xEDB88320 

/* 
 * @brief   cyclic calculation crc check value
 * @param   init_val    - initial value
 * @param   pdata       - datas pointer
 * @param   len         - datas len
 * @retval  calculated result 
 */
u32 crc32_cyc_cal(u32 init_val, u8 *pdata, u32 len);

/* 
 * @brief   calculation crc check value, initial is 0xFFFFFFFF
 * @param   pdata       - datas pointer
 * @param   len         - datas len
 * @retval  calculated result 
 */
u32 crc32_cal(u8 *pdata, u32 len);

#endif

