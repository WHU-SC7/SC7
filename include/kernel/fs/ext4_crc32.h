

/**
 * @file  ext4_crc32.h
 * @brief Crc32c routine. Taken from FreeBSD kernel.
 */

#ifndef LWEXT4_EXT4_CRC32C_H_
#define LWEXT4_EXT4_CRC32C_H_


#include "ext4_config.h"

#include "types.h"

/**@brief	CRC32 algorithm.
 * @param	crc input feed
 * @param 	buf input buffer
 * @param	size input buffer length (bytes)
 * @return	updated crc32 value*/
uint32_t ext4_crc32(uint32_t crc, const void *buf, uint32_t size);

/**@brief	CRC32C algorithm.
 * @param	crc input feed
 * @param 	buf input buffer
 * @param	size input buffer length (bytes)
 * @return	updated crc32c value*/
uint32_t ext4_crc32c(uint32_t crc, const void *buf, uint32_t size);


#endif /* LWEXT4_EXT4_CRC32C_H_ */

/**
 * @}
 */
