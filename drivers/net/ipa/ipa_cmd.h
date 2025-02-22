/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019-2020 Linaro Ltd.
 */
#ifndef _IPA_CMD_H_
#define _IPA_CMD_H_

#include <linux/types.h>
#include <linux/dma-direction.h>

struct sk_buff;
struct scatterlist;

struct ipa;
struct ipa_mem;
struct gsi_trans;
struct gsi_channel;

/**
 * enum ipa_cmd_opcode:	IPA immediate commands
 *
 * All immediate commands are issued using the AP command TX endpoint.
 * The numeric values here are the opcodes for IPA v3.5.1 hardware.
 *
 * IPA_CMD_NONE is a special (invalid) value that's used to indicate
 * a request is *not* an immediate command.
 */
enum ipa_cmd_opcode {
	IPA_CMD_NONE			= 0,
	IPA_CMD_IP_V4_FILTER_INIT	= 3,
	IPA_CMD_IP_V6_FILTER_INIT	= 4,
	IPA_CMD_IP_V4_ROUTING_INIT	= 7,
	IPA_CMD_IP_V6_ROUTING_INIT	= 8,
	IPA_CMD_HDR_INIT_LOCAL		= 9,
	IPA_CMD_REGISTER_WRITE		= 12,
	IPA_CMD_IP_PACKET_INIT		= 16,
	IPA_CMD_DMA_SHARED_MEM		= 19,
	IPA_CMD_IP_PACKET_TAG_STATUS	= 20,
};

/**
 * struct ipa_cmd_info - information needed for an IPA immediate command
 *
 * @opcode:	The command opcode.
 * @direction:	Direction of data transfer for DMA commands
 */
struct ipa_cmd_info {
	enum ipa_cmd_opcode opcode;
	enum dma_data_direction direction;
};


#ifdef IPA_VALIDATE

/**
 * ipa_cmd_table_valid() - Validate a memory region holding a table
 * @ipa:	- IPA pointer
 * @mem:	- IPA memory region descriptor
 * @route:	- Whether the region holds a route or filter table
 * @ipv6:	- Whether the table is for IPv6 or IPv4
 * @hashed:	- Whether the table is hashed or non-hashed
 *
 * Return:	true if region is valid, false otherwise
 */
bool ipa_cmd_table_valid(struct ipa *ipa, const struct ipa_mem *mem,
			    bool route, bool ipv6, bool hashed);

/**
 * ipa_cmd_data_valid() - Validate command-realted configuration is valid
 * @ipa:	- IPA pointer
 *
 * Return:	true if assumptions required for command are valid
 */
bool ipa_cmd_data_valid(struct ipa *ipa);

#else /* !IPA_VALIDATE */

static inline bool ipa_cmd_table_valid(struct ipa *ipa,
				       const struct ipa_mem *mem, bool route,
				       bool ipv6, bool hashed)
{
	return true;
}

static inline bool ipa_cmd_data_valid(struct ipa *ipa)
{
	return true;
}

#endif /* !IPA_VALIDATE */

/**
 * ipa_cmd_pool_init() - initialize command channel pools
 * @channel:	AP->IPA command TX GSI channel pointer
 * @tre_count:	Number of pool elements to allocate
 *
 * Return:	0 if successful, or a negative error code
 */
int ipa_cmd_pool_init(struct gsi_channel *gsi_channel, u32 tre_count);

/**
 * ipa_cmd_pool_exit() - Inverse of ipa_cmd_pool_init()
 * @channel:	AP->IPA command TX GSI channel pointer
 */
void ipa_cmd_pool_exit(struct gsi_channel *channel);

/**
 * ipa_cmd_table_init_add() - Add table init command to a transaction
 * @trans:	GSI transaction
 * @opcode:	IPA immediate command opcode
 * @size:	Size of non-hashed routing table memory
 * @offset:	Offset in IPA shared memory of non-hashed routing table memory
 * @addr:	DMA address of non-hashed table data to write
 * @hash_size:	Size of hashed routing table memory
 * @hash_offset: Offset in IPA shared memory of hashed routing table memory
 * @hash_addr:	DMA address of hashed table data to write
 *
 * If hash_size is 0, hash_offset and hash_addr are ignored.
 */
void ipa_cmd_table_init_add(struct gsi_trans *trans, enum ipa_cmd_opcode opcode,
			    u16 size, u32 offset, dma_addr_t addr,
			    u16 hash_size, u32 hash_offset,
			    dma_addr_t hash_addr);

/**
 * ipa_cmd_hdr_init_local_add() - Add a header init command to a transaction
 * @ipa:	IPA structure
 * @offset:	Offset of header memory in IPA local space
 * @size:	Size of header memory
 * @addr:	DMA address of buffer to be written from
 *
 * Defines and fills the location in IPA memory to use for headers.
 */
void ipa_cmd_hdr_init_local_add(struct gsi_trans *trans, u32 offset, u16 size,
				dma_addr_t addr);

/**
 * ipa_cmd_register_write_add() - Add a register write command to a transaction
 * @trans:	GSI transaction
 * @offset:	Offset of register to be written
 * @value:	Value to be written
 * @mask:	Mask of bits in register to update with bits from value
 * @clear_full: Pipeline clear option; true means full pipeline clear
 */
void ipa_cmd_register_write_add(struct gsi_trans *trans, u32 offset, u32 value,
				u32 mask, bool clear_full);

/**
 * ipa_cmd_dma_shared_mem_add() - Add a DMA memory command to a transaction
 * @trans:	GSI transaction
 * @offset:	Offset of IPA memory to be read or written
 * @size:	Number of bytes of memory to be transferred
 * @addr:	DMA address of buffer to be read into or written from
 * @toward_ipa:	true means write to IPA memory; false means read
 */
void ipa_cmd_dma_shared_mem_add(struct gsi_trans *trans, u32 offset,
				u16 size, dma_addr_t addr, bool toward_ipa);

/**
 * ipa_cmd_tag_process_add() - Add IPA tag process commands to a transaction
 * @trans:	GSI transaction
 */
void ipa_cmd_tag_process_add(struct gsi_trans *trans);

/**
 * ipa_cmd_tag_process_add_count() - Number of commands in a tag process
 *
 * Return:	The number of elements to allocate in a transaction
 *		to hold tag process commands
 */
u32 ipa_cmd_tag_process_count(void);

/**
 * ipa_cmd_trans_alloc() - Allocate a transaction for the command TX endpoint
 * @ipa:	IPA pointer
 * @tre_count:	Number of elements in the transaction
 *
 * Return:	A GSI transaction structure, or a null pointer if all
 *		available transactions are in use
 */
struct gsi_trans *ipa_cmd_trans_alloc(struct ipa *ipa, u32 tre_count);

#endif /* _IPA_CMD_H_ */
