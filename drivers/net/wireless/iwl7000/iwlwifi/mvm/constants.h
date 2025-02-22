/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2015        Intel Deutschland GmbH
 * Copyright(c) 2013 - 2014, 2018 - 2020 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2015        Intel Deutschland GmbH
 * Copyright(c) 2013 - 2014, 2018 - 2020 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef __MVM_CONSTANTS_H
#define __MVM_CONSTANTS_H

#include <linux/ieee80211.h>
#include "fw-api.h"

#define IWL_MVM_UAPSD_NOAGG_BSSIDS_NUM		20

#ifndef CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES
#define IWL_MVM_DEFAULT_PS_TX_DATA_TIMEOUT	(100 * USEC_PER_MSEC)
#define IWL_MVM_DEFAULT_PS_RX_DATA_TIMEOUT	(100 * USEC_PER_MSEC)
#define IWL_MVM_WOWLAN_PS_TX_DATA_TIMEOUT	(10 * USEC_PER_MSEC)
#define IWL_MVM_WOWLAN_PS_RX_DATA_TIMEOUT	(10 * USEC_PER_MSEC)
#define IWL_MVM_SHORT_PS_TX_DATA_TIMEOUT	(2 * 1024) /* defined in TU */
#define IWL_MVM_SHORT_PS_RX_DATA_TIMEOUT	(40 * 1024) /* defined in TU */
#define IWL_MVM_P2P_LOWLATENCY_PS_ENABLE	1
#define IWL_MVM_UAPSD_RX_DATA_TIMEOUT		(50 * USEC_PER_MSEC)
#define IWL_MVM_UAPSD_TX_DATA_TIMEOUT		(50 * USEC_PER_MSEC)
#define IWL_MVM_UAPSD_QUEUES		(IEEE80211_WMM_IE_STA_QOSINFO_AC_VO |\
					 IEEE80211_WMM_IE_STA_QOSINFO_AC_VI |\
					 IEEE80211_WMM_IE_STA_QOSINFO_AC_BK |\
					 IEEE80211_WMM_IE_STA_QOSINFO_AC_BE)
#define IWL_MVM_PS_HEAVY_TX_THLD_PACKETS	20
#define IWL_MVM_PS_HEAVY_RX_THLD_PACKETS	8
#define IWL_MVM_PS_SNOOZE_HEAVY_TX_THLD_PACKETS	30
#define IWL_MVM_PS_SNOOZE_HEAVY_RX_THLD_PACKETS	20
#define IWL_MVM_PS_HEAVY_TX_THLD_PERCENT	50
#define IWL_MVM_PS_HEAVY_RX_THLD_PERCENT	50
#define IWL_MVM_PS_SNOOZE_INTERVAL		25
#define IWL_MVM_PS_SNOOZE_WINDOW		50
#define IWL_MVM_WOWLAN_PS_SNOOZE_WINDOW		25
#define IWL_MVM_LOWLAT_QUOTA_MIN_PERCENT	64
#define IWL_MVM_BT_COEX_EN_RED_TXP_THRESH	62
#define IWL_MVM_BT_COEX_DIS_RED_TXP_THRESH	65
#define IWL_MVM_BT_COEX_SYNC2SCO		1
#define IWL_MVM_BT_COEX_MPLUT			1
#define IWL_MVM_BT_COEX_RRC			1
#define IWL_MVM_BT_COEX_TTC			1
#define IWL_MVM_BT_COEX_MPLUT_REG0		0x22002200
#define IWL_MVM_BT_COEX_MPLUT_REG1		0x11118451
#define IWL_MVM_BT_COEX_ANTENNA_COUPLING_THRS	30
#define IWL_MVM_FW_MCAST_FILTER_PASS_ALL	0
#define IWL_MVM_FW_BCAST_FILTER_PASS_ALL	0
#define IWL_MVM_QUOTA_THRESHOLD			4
#define IWL_MVM_RS_RSSI_BASED_INIT_RATE         0
#define IWL_MVM_RS_80_20_FAR_RANGE_TWEAK	1
#define IWL_MVM_TOF_IS_RESPONDER		0
#define IWL_MVM_SW_TX_CSUM_OFFLOAD		0
#define IWL_MVM_HW_CSUM_DISABLE			0
#define IWL_MVM_PARSE_NVM			0
#define IWL_MVM_ADWELL_ENABLE			1
#define IWL_MVM_ADWELL_MAX_BUDGET		0
#define IWL_MVM_TCM_LOAD_MEDIUM_THRESH		10 /* percentage */
#define IWL_MVM_TCM_LOAD_HIGH_THRESH		50 /* percentage */
#define IWL_MVM_TCM_LOWLAT_ENABLE_THRESH	100 /* packets/10 seconds */
#define IWL_MVM_UAPSD_NONAGG_PERIOD		5000 /* msecs */
#define IWL_MVM_UAPSD_NOAGG_LIST_LEN		IWL_MVM_UAPSD_NOAGG_BSSIDS_NUM
#define IWL_MVM_NON_TRANSMITTING_AP		0
#define IWL_MVM_RS_NUM_TRY_BEFORE_ANT_TOGGLE    1
#define IWL_MVM_RS_HT_VHT_RETRIES_PER_RATE      2
#define IWL_MVM_RS_HT_VHT_RETRIES_PER_RATE_TW   1
#define IWL_MVM_RS_INITIAL_MIMO_NUM_RATES       3
#define IWL_MVM_RS_INITIAL_SISO_NUM_RATES       3
#define IWL_MVM_RS_INITIAL_LEGACY_NUM_RATES     2
#define IWL_MVM_RS_INITIAL_LEGACY_RETRIES       2
#define IWL_MVM_RS_SECONDARY_LEGACY_RETRIES	1
#define IWL_MVM_RS_SECONDARY_LEGACY_NUM_RATES   16
#define IWL_MVM_RS_SECONDARY_SISO_NUM_RATES     3
#define IWL_MVM_RS_SECONDARY_SISO_RETRIES       1
#define IWL_MVM_RS_RATE_MIN_FAILURE_TH		3
#define IWL_MVM_RS_RATE_MIN_SUCCESS_TH		8
#define IWL_MVM_RS_STAY_IN_COLUMN_TIMEOUT	5	/* Seconds */
#define IWL_MVM_RS_IDLE_TIMEOUT			5	/* Seconds */
#define IWL_MVM_RS_MISSED_RATE_MAX		15
#define IWL_MVM_RS_LEGACY_FAILURE_LIMIT		160
#define IWL_MVM_RS_LEGACY_SUCCESS_LIMIT		480
#define IWL_MVM_RS_LEGACY_TABLE_COUNT		160
#define IWL_MVM_RS_NON_LEGACY_FAILURE_LIMIT	400
#define IWL_MVM_RS_NON_LEGACY_SUCCESS_LIMIT	4500
#define IWL_MVM_RS_NON_LEGACY_TABLE_COUNT	1500
#define IWL_MVM_RS_SR_FORCE_DECREASE		15	/* percent */
#define IWL_MVM_RS_SR_NO_DECREASE		85	/* percent */
#define IWL_MVM_RS_AGG_TIME_LIMIT	        4000    /* 4 msecs. valid 100-8000 */
#define IWL_MVM_RS_AGG_DISABLE_START	        3
#define IWL_MVM_RS_AGG_START_THRESHOLD	        10	/* num frames per second */
#define IWL_MVM_RS_TPC_SR_FORCE_INCREASE	75	/* percent */
#define IWL_MVM_RS_TPC_SR_NO_INCREASE		85	/* percent */
#define IWL_MVM_RS_TPC_TX_POWER_STEP		3
#define IWL_MVM_ENABLE_EBS			1
#define IWL_MVM_FTM_INITIATOR_ALGO		IWL_TOF_ALGO_TYPE_MAX_LIKE
#define IWL_MVM_FTM_INITIATOR_DYNACK		true
#define IWL_MVM_D3_DEBUG			false
#define IWL_MVM_USE_TWT				true
#define IWL_MVM_TWT_TESTMODE			false
#define IWL_MVM_AMPDU_CONSEC_DROPS_DELBA	10
#define IWL_MVM_FTM_INITIATOR_ENABLE_SMOOTH     false
#define IWL_MVM_FTM_INITIATOR_SMOOTH_ALPHA      40
/*  20016 pSec is 6 meter RTT, meaning 3 meter range */
#define IWL_MVM_FTM_INITIATOR_SMOOTH_UNDERSHOOT 20016
#define IWL_MVM_FTM_INITIATOR_SMOOTH_OVERSHOOT  20016
#define IWL_MVM_FTM_INITIATOR_SMOOTH_AGE_SEC    2
#define IWL_MVM_PHY_FILTER_CHAIN_A		0
#define IWL_MVM_PHY_FILTER_CHAIN_B		0
#define IWL_MVM_PHY_FILTER_CHAIN_C		0
#define IWL_MVM_PHY_FILTER_CHAIN_D		0
#define IWL_MVM_DISABLE_AP_FILS			false
#else /* CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES */
#define IWL_MVM_DEFAULT_PS_TX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_DEFAULT_PS_TX_DATA_TIMEOUT)
#define IWL_MVM_DEFAULT_PS_RX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_DEFAULT_PS_RX_DATA_TIMEOUT)
#define IWL_MVM_WOWLAN_PS_TX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_WOWLAN_PS_TX_DATA_TIMEOUT)
#define IWL_MVM_WOWLAN_PS_RX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_WOWLAN_PS_RX_DATA_TIMEOUT)
#define IWL_MVM_SHORT_PS_TX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_SHORT_PS_TX_DATA_TIMEOUT)
#define IWL_MVM_SHORT_PS_RX_DATA_TIMEOUT	(mvm->trans->dbg_cfg.MVM_SHORT_PS_RX_DATA_TIMEOUT)
#define IWL_MVM_P2P_LOWLATENCY_PS_ENABLE	(mvm->trans->dbg_cfg.MVM_P2P_LOWLATENCY_PS_ENABLE)
#define IWL_MVM_UAPSD_TX_DATA_TIMEOUT		(mvm->trans->dbg_cfg.MVM_UAPSD_TX_DATA_TIMEOUT)
#define IWL_MVM_UAPSD_RX_DATA_TIMEOUT		(mvm->trans->dbg_cfg.MVM_UAPSD_RX_DATA_TIMEOUT)
#define IWL_MVM_UAPSD_QUEUES			(mvm->trans->dbg_cfg.MVM_UAPSD_QUEUES)
#define IWL_MVM_PS_HEAVY_TX_THLD_PACKETS	(mvm->trans->dbg_cfg.MVM_PS_HEAVY_TX_THLD_PACKETS)
#define IWL_MVM_PS_HEAVY_RX_THLD_PACKETS	(mvm->trans->dbg_cfg.MVM_PS_HEAVY_RX_THLD_PACKETS)
#define IWL_MVM_PS_SNOOZE_HEAVY_TX_THLD_PACKETS	(mvm->trans->dbg_cfg.MVM_PS_SNOOZE_HEAVY_TX_THLD_PACKETS)
#define IWL_MVM_PS_SNOOZE_HEAVY_RX_THLD_PACKETS	(mvm->trans->dbg_cfg.MVM_PS_SNOOZE_HEAVY_RX_THLD_PACKETS)
#define IWL_MVM_PS_HEAVY_TX_THLD_PERCENT	(mvm->trans->dbg_cfg.MVM_PS_HEAVY_TX_THLD_PERCENT)
#define IWL_MVM_PS_HEAVY_RX_THLD_PERCENT	(mvm->trans->dbg_cfg.MVM_PS_HEAVY_RX_THLD_PERCENT)
#define IWL_MVM_PS_SNOOZE_INTERVAL		(mvm->trans->dbg_cfg.MVM_PS_SNOOZE_INTERVAL)
#define IWL_MVM_PS_SNOOZE_WINDOW		(mvm->trans->dbg_cfg.MVM_PS_SNOOZE_WINDOW)
#define IWL_MVM_WOWLAN_PS_SNOOZE_WINDOW		(mvm->trans->dbg_cfg.MVM_WOWLAN_PS_SNOOZE_WINDOW)
#define IWL_MVM_LOWLAT_QUOTA_MIN_PERCENT	(mvm->trans->dbg_cfg.MVM_LOWLAT_QUOTA_MIN_PERCENT)
#define IWL_MVM_BT_COEX_EN_RED_TXP_THRESH	(mvm->trans->dbg_cfg.MVM_BT_COEX_EN_RED_TXP_THRESH)
#define IWL_MVM_BT_COEX_DIS_RED_TXP_THRESH	(mvm->trans->dbg_cfg.MVM_BT_COEX_DIS_RED_TXP_THRESH)
#define IWL_MVM_BT_COEX_SYNC2SCO		(mvm->trans->dbg_cfg.MVM_BT_COEX_SYNC2SCO)
#define IWL_MVM_BT_COEX_MPLUT			(mvm->trans->dbg_cfg.MVM_BT_COEX_MPLUT)
#define IWL_MVM_BT_COEX_RRC			(mvm->trans->dbg_cfg.MVM_BT_COEX_RRC)
#define IWL_MVM_BT_COEX_TTC			(mvm->trans->dbg_cfg.MVM_BT_COEX_TTC)
#define IWL_MVM_BT_COEX_MPLUT_REG0		(mvm->trans->dbg_cfg.MVM_BT_COEX_MPLUT_REG0)
#define IWL_MVM_BT_COEX_MPLUT_REG1		(mvm->trans->dbg_cfg.MVM_BT_COEX_MPLUT_REG1)
#define IWL_MVM_BT_COEX_ANTENNA_COUPLING_THRS	(mvm->trans->dbg_cfg.MVM_BT_COEX_ANTENNA_COUPLING_THRS)
#define IWL_MVM_FW_MCAST_FILTER_PASS_ALL	(mvm->trans->dbg_cfg.MVM_FW_MCAST_FILTER_PASS_ALL)
#define IWL_MVM_FW_BCAST_FILTER_PASS_ALL	(mvm->trans->dbg_cfg.MVM_FW_BCAST_FILTER_PASS_ALL)
#define IWL_MVM_TOF_IS_RESPONDER		(mvm->trans->dbg_cfg.MVM_TOF_IS_RESPONDER)
#define IWL_MVM_SW_TX_CSUM_OFFLOAD		(mvm->trans->dbg_cfg.MVM_SW_TX_CSUM_OFFLOAD)
#define IWL_MVM_HW_CSUM_DISABLE			(mvm->trans->dbg_cfg.MVM_HW_CSUM_DISABLE)
#define IWL_MVM_PARSE_NVM			(mvm->trans->dbg_cfg.MVM_PARSE_NVM)
#define IWL_MVM_ADWELL_ENABLE			(mvm->trans->dbg_cfg.MVM_ADWELL_ENABLE)
#define IWL_MVM_ADWELL_MAX_BUDGET		(mvm->trans->dbg_cfg.MVM_ADWELL_MAX_BUDGET)
#define IWL_MVM_TCM_LOAD_MEDIUM_THRESH		(mvm->trans->dbg_cfg.MVM_TCM_LOAD_MEDIUM_THRESH)
#define IWL_MVM_TCM_LOAD_HIGH_THRESH		(mvm->trans->dbg_cfg.MVM_TCM_LOAD_HIGH_THRESH)
#define IWL_MVM_TCM_LOWLAT_ENABLE_THRESH	(mvm->trans->dbg_cfg.MVM_TCM_LOWLAT_ENABLE_THRESH)
#define IWL_MVM_UAPSD_NONAGG_PERIOD		(mvm->trans->dbg_cfg.MVM_UAPSD_NONAGG_PERIOD)
#define IWL_MVM_UAPSD_NOAGG_LIST_LEN		(mvm->trans->dbg_cfg.MVM_UAPSD_NOAGG_LIST_LEN)
#define IWL_MVM_NON_TRANSMITTING_AP		(mvm->trans->dbg_cfg.MVM_NON_TRANSMITTING_AP)
#define IWL_MVM_QUOTA_THRESHOLD			(mvm->trans->dbg_cfg.MVM_QUOTA_THRESHOLD)
#define IWL_MVM_RS_RSSI_BASED_INIT_RATE         (mvm->trans->dbg_cfg.MVM_RS_RSSI_BASED_INIT_RATE)
#define IWL_MVM_RS_80_20_FAR_RANGE_TWEAK	(mvm->trans->dbg_cfg.MVM_RS_80_20_FAR_RANGE_TWEAK)
#define IWL_MVM_RS_NUM_TRY_BEFORE_ANT_TOGGLE    (mvm->trans->dbg_cfg.MVM_RS_NUM_TRY_BEFORE_ANT_TOGGLE)
#define IWL_MVM_RS_HT_VHT_RETRIES_PER_RATE      (mvm->trans->dbg_cfg.MVM_RS_HT_VHT_RETRIES_PER_RATE)
#define IWL_MVM_RS_HT_VHT_RETRIES_PER_RATE_TW   (mvm->trans->dbg_cfg.MVM_RS_HT_VHT_RETRIES_PER_RATE_TW)
#define IWL_MVM_RS_INITIAL_MIMO_NUM_RATES       (mvm->trans->dbg_cfg.MVM_RS_INITIAL_MIMO_NUM_RATES)
#define IWL_MVM_RS_INITIAL_SISO_NUM_RATES       (mvm->trans->dbg_cfg.MVM_RS_INITIAL_SISO_NUM_RATES)
#define IWL_MVM_RS_INITIAL_LEGACY_NUM_RATES     (mvm->trans->dbg_cfg.MVM_RS_INITIAL_LEGACY_NUM_RATES)
#define IWL_MVM_RS_INITIAL_LEGACY_RETRIES       (mvm->trans->dbg_cfg.MVM_RS_INITIAL_LEGACY_RETRIES)
#define IWL_MVM_RS_SECONDARY_LEGACY_RETRIES     (mvm->trans->dbg_cfg.MVM_RS_SECONDARY_LEGACY_RETRIES)
#define IWL_MVM_RS_SECONDARY_LEGACY_NUM_RATES   (mvm->trans->dbg_cfg.MVM_RS_SECONDARY_LEGACY_NUM_RATES)
#define IWL_MVM_RS_SECONDARY_SISO_NUM_RATES     (mvm->trans->dbg_cfg.MVM_RS_SECONDARY_SISO_NUM_RATES)
#define IWL_MVM_RS_SECONDARY_SISO_RETRIES       (mvm->trans->dbg_cfg.MVM_RS_SECONDARY_SISO_RETRIES)
#define IWL_MVM_RS_RATE_MIN_FAILURE_TH		(mvm->trans->dbg_cfg.MVM_RS_RATE_MIN_FAILURE_TH)
#define IWL_MVM_RS_RATE_MIN_SUCCESS_TH		(mvm->trans->dbg_cfg.MVM_RS_RATE_MIN_SUCCESS_TH)
#define IWL_MVM_RS_STAY_IN_COLUMN_TIMEOUT       (mvm->trans->dbg_cfg.MVM_RS_STAY_IN_COLUMN_TIMEOUT)
#define IWL_MVM_RS_IDLE_TIMEOUT                 (mvm->trans->dbg_cfg.MVM_RS_IDLE_TIMEOUT)
#define IWL_MVM_RS_MISSED_RATE_MAX		(mvm->trans->dbg_cfg.MVM_RS_MISSED_RATE_MAX)
#define IWL_MVM_RS_LEGACY_FAILURE_LIMIT		(mvm->trans->dbg_cfg.MVM_RS_LEGACY_FAILURE_LIMIT)
#define IWL_MVM_RS_LEGACY_SUCCESS_LIMIT		(mvm->trans->dbg_cfg.MVM_RS_LEGACY_SUCCESS_LIMIT)
#define IWL_MVM_RS_LEGACY_TABLE_COUNT		(mvm->trans->dbg_cfg.MVM_RS_LEGACY_TABLE_COUNT)
#define IWL_MVM_RS_NON_LEGACY_FAILURE_LIMIT	(mvm->trans->dbg_cfg.MVM_RS_NON_LEGACY_FAILURE_LIMIT)
#define IWL_MVM_RS_NON_LEGACY_SUCCESS_LIMIT	(mvm->trans->dbg_cfg.MVM_RS_NON_LEGACY_SUCCESS_LIMIT)
#define IWL_MVM_RS_NON_LEGACY_TABLE_COUNT	(mvm->trans->dbg_cfg.MVM_RS_NON_LEGACY_TABLE_COUNT)
#define IWL_MVM_RS_SR_FORCE_DECREASE		(mvm->trans->dbg_cfg.MVM_RS_SR_FORCE_DECREASE)
#define IWL_MVM_RS_SR_NO_DECREASE		(mvm->trans->dbg_cfg.MVM_RS_SR_NO_DECREASE)
#define IWL_MVM_RS_AGG_TIME_LIMIT	        (mvm->trans->dbg_cfg.MVM_RS_AGG_TIME_LIMIT)
#define IWL_MVM_RS_AGG_DISABLE_START	        (mvm->trans->dbg_cfg.MVM_RS_AGG_DISABLE_START)
#define IWL_MVM_RS_AGG_START_THRESHOLD	        (mvm->trans->dbg_cfg.MVM_RS_AGG_START_THRESHOLD)
#define IWL_MVM_RS_TPC_SR_FORCE_INCREASE	(mvm->trans->dbg_cfg.MVM_RS_TPC_SR_FORCE_INCREASE)
#define IWL_MVM_RS_TPC_SR_NO_INCREASE		(mvm->trans->dbg_cfg.MVM_RS_TPC_SR_NO_INCREASE)
#define IWL_MVM_RS_TPC_TX_POWER_STEP		(mvm->trans->dbg_cfg.MVM_RS_TPC_TX_POWER_STEP)
#define IWL_MVM_ENABLE_EBS			(mvm->trans->dbg_cfg.MVM_ENABLE_EBS)
#define IWL_MVM_FTM_RESP_TOA_OFFSET		(mvm->trans->dbg_cfg.MVM_FTM_RESP_TOA_OFFSET)
#define IWL_MVM_FTM_RESP_VALID			(mvm->trans->dbg_cfg.MVM_FTM_RESP_VALID)
#define IWL_MVM_FTM_RESP_FLAGS			(mvm->trans->dbg_cfg.MVM_FTM_RESP_FLAGS)
#define IWL_MVM_FTM_INITIATOR_ALGO		(mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_ALGO)
#define IWL_MVM_FTM_INITIATOR_DYNACK		(mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_DYNACK)
#define IWL_MVM_FTM_INITIATOR_MCSI_ENABLED	(mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_MCSI_ENABLED)
#define IWL_MVM_FTM_INITIATOR_COMMON_CALIB	(mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_COMMON_CALIB)
#define IWL_MVM_FTM_INITIATOR_FAST_ALGO_DISABLE (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_FAST_ALGO_DISABLE)
#define IWL_MVM_D3_DEBUG			(((struct iwl_mvm *)ctx)->trans->dbg_cfg.MVM_D3_DEBUG)
#define IWL_MVM_USE_TWT				(mvm->trans->dbg_cfg.MVM_USE_TWT)
#define IWL_MVM_TWT_TESTMODE			(mvm->trans->dbg_cfg.MVM_TWT_TESTMODE)
#define IWL_MVM_AMPDU_CONSEC_DROPS_DELBA	(mvm->trans->dbg_cfg.MVM_AMPDU_CONSEC_DROPS_DELBA)
#define IWL_MVM_FTM_INITIATOR_ENABLE_SMOOTH     (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_ENABLE_SMOOTH)
#define IWL_MVM_FTM_INITIATOR_SMOOTH_ALPHA      (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_SMOOTH_ALPHA)
#define IWL_MVM_FTM_INITIATOR_SMOOTH_UNDERSHOOT (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_SMOOTH_UNDERSHOOT)
#define IWL_MVM_FTM_INITIATOR_SMOOTH_OVERSHOOT  (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_SMOOTH_OVERSHOOT)
#define IWL_MVM_FTM_INITIATOR_SMOOTH_AGE_SEC    (mvm->trans->dbg_cfg.MVM_FTM_INITIATOR_SMOOTH_AGE_SEC)
#define IWL_MVM_PHY_FILTER_CHAIN_A		(mvm->trans->dbg_cfg.MVM_PHY_FILTER_CHAIN_A)
#define IWL_MVM_PHY_FILTER_CHAIN_B		(mvm->trans->dbg_cfg.MVM_PHY_FILTER_CHAIN_B)
#define IWL_MVM_PHY_FILTER_CHAIN_C		(mvm->trans->dbg_cfg.MVM_PHY_FILTER_CHAIN_C)
#define IWL_MVM_PHY_FILTER_CHAIN_D		(mvm->trans->dbg_cfg.MVM_PHY_FILTER_CHAIN_D)
#define IWL_MVM_DISABLE_AP_FILS			(mvm->trans->dbg_cfg.MVM_DISABLE_AP_FILS)

#endif /* CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES */

#endif /* __MVM_CONSTANTS_H */
