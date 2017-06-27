/*
 * dgram_header.h
 *
 *  Created on: Jun 23, 2017
 *      Author: tim
 */

#ifndef IBRCOMMON_IBRCOMMON_NET_DGRAMHEADER_H_
#define IBRCOMMON_IBRCOMMON_NET_DGRAMHEADER_H_

/**
 * CL Header Types
 */
#define CONVERGENCE_LAYER_TYPE_DATA 		0x10
#define CONVERGENCE_LAYER_TYPE_DISCOVERY	0x20
#define CONVERGENCE_LAYER_TYPE_ACK 			0x30
#define CONVERGENCE_LAYER_TYPE_NACK 		0x00
#define CONVERGENCE_LAYER_TYPE_TEMP_NACK	0x40
#define CONVERGENCE_LAYER_TYPE_COMMAND		0x50

/**
 * CL Packet Flags
 */
#define CONVERGENCE_LAYER_FLAGS_FIRST		0x02
#define CONVERGENCE_LAYER_FLAGS_LAST		0x01

/**
 * CL Field Masks
 */
#define CONVERGENCE_LAYER_MASK_COMPAT		0xC0
#define CONVERGENCE_LAYER_MASK_TYPE			0x30
#define CONVERGENCE_LAYER_MASK_SEQNO		0x0C
#define CONVERGENCE_LAYER_MASK_FLAGS		0x03

typedef struct
{
	uint8_t compat : 2;
	uint8_t type : 2;
	uint8_t seqnr : 2;
	uint8_t flags : 2;
} dgram_header __attribute__ ((packed));

#endif /* IBRCOMMON_IBRCOMMON_NET_DGRAMHEADER_H_ */
