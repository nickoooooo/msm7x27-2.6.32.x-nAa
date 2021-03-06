/*
 * TWDriver.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.
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
 *  * Neither the name Texas Instruments nor the names of its
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
 */


/** \file  TWDriver.c
 *  \brief TI WLAN Hardware Access Driver
 *
 *  \see   TWDriver.h
 */

#define __FILE_ID__  FILE_ID_117
#include "report.h"
#include "TWDriver.h"
#include "MacServices_api.h"
#include "txCtrlBlk_api.h"
#include "txHwQueue_api.h"
#include "txXfer_api.h"
#include "txResult_api.h"
#include "rxXfer_api.h"
#include "TwIf.h"
#include "FwEvent_api.h"
#include "CmdMBox_api.h"
#include "CmdQueue_api.h"
#include "eventMbox_api.h"
#include "fwDebug_api.h"
#include "osApi.h"
#include "TWDriverInternal.h"
#include "HwInit_api.h"
#include "CmdBld.h"
#include "RxQueue_api.h"

void TWD_CheckSRConfigParams(TTwd  *pTWD, ACXSmartReflexConfigParams_t *tSmartReflexParams);


#define TWD_CB_MODULE_OWNER_MASK    0xff00
#define TWD_CB_TYPE_MASK            0x00ff




TI_HANDLE TWD_Create (TI_HANDLE hOs)
{
	TTwd *pTWD;

	/* Allocate the TNETW_Driver module */
	pTWD = (TTwd *)os_memoryAlloc (hOs, sizeof(TTwd));
	if (pTWD == NULL) {
		return NULL;
	}

	os_memoryZero (hOs, pTWD, sizeof(TTwd));

	pTWD->hOs = hOs;

	/* Create TwIf module */
	pTWD->hTwIf = twIf_Create (hOs);
	if (pTWD->hTwIf == NULL) {
		WLAN_OS_REPORT(("twIf_Create failed\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	/* Create command builder module */
	pTWD->hCmdBld = cmdBld_Create (hOs);
	if (pTWD->hCmdBld == NULL) {
		WLAN_OS_REPORT(("cmdBld_Create failed\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	/* Create the MAC Services module */
	pTWD->hMacServices = MacServices_create (hOs);
	if (pTWD->hMacServices == NULL) {
		WLAN_OS_REPORT(("TWD MacServices_create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	/* Create the Ctrl module */
	pTWD->hCmdQueue = cmdQueue_Create (hOs);
	if (pTWD->hCmdQueue == NULL) {
		WLAN_OS_REPORT(("TWD cmdQueue_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	/*
	 * Create the FW-Transfer modules:
	 */

	pTWD->hTxXfer = txXfer_Create (hOs);
	if (pTWD->hTxXfer == NULL) {
		WLAN_OS_REPORT(("TWD txXfer_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hTxResult = txResult_Create (hOs);
	if (pTWD->hTxResult == NULL) {
		WLAN_OS_REPORT(("TWD txResult_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hRxXfer = rxXfer_Create (hOs);
	if (pTWD->hRxXfer == NULL) {
		WLAN_OS_REPORT(("TWD rxXfer_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hFwEvent = fwEvent_Create (hOs);
	if (pTWD->hFwEvent == NULL) {
		WLAN_OS_REPORT(("TWD fwEvent_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hEventMbox = eventMbox_Create (hOs);
	if (pTWD->hEventMbox == NULL) {
		WLAN_OS_REPORT(("TWD eventMbox_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

#ifdef TI_DBG
	pTWD->hFwDbg = fwDbg_Create (hOs);
	if (pTWD->hFwDbg == NULL) {
		WLAN_OS_REPORT(("TWD fwDbg_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}
#endif /* TI_DBG */

	pTWD->hCmdMbox = cmdMbox_Create (hOs);
	if (pTWD->hCmdMbox == NULL) {
		WLAN_OS_REPORT(("TWD cmdMbox_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hRxQueue = RxQueue_Create (hOs);
	if (pTWD->hRxQueue == NULL) {
		WLAN_OS_REPORT(("TWD RxQueue_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}


	/*
	 * Create the Data-Services modules:
	 */

	pTWD->hTxCtrlBlk = txCtrlBlk_Create (hOs);
	if (pTWD->hTxCtrlBlk == NULL) {
		WLAN_OS_REPORT(("TWD txCtrlBlk_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hTxHwQueue = txHwQueue_Create (hOs);
	if (pTWD->hTxHwQueue == NULL) {
		WLAN_OS_REPORT(("TWD txHwQueue_Create failed!!!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	pTWD->hHwInit = hwInit_Create (hOs);
	if (pTWD->hHwInit == NULL) {
		WLAN_OS_REPORT (("wInit_Create failed!\n"));
		TWD_Destroy ((TI_HANDLE)pTWD);
		return NULL;
	}

	WLAN_INIT_REPORT (("TWD_Create: CREATED !!!\n"));

	return (TI_HANDLE)pTWD;
}

TI_STATUS TWD_Destroy (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;

	WLAN_INIT_REPORT(("TWD_Destroy: called\n"));
	if (pTWD == NULL) {
		return TI_NOK;
	}

	if (pTWD->hTwIf != NULL) {
		twIf_Destroy (pTWD->hTwIf);
		pTWD->hTwIf = NULL;
	}

	/* Free the Command Builder */
	if (pTWD->hCmdBld != NULL) {
		cmdBld_Destroy (pTWD->hCmdBld);
		pTWD->hCmdBld = NULL;
	}
	WLAN_INIT_REPORT(("TWD_Destroy: Command Builder released\n"));

	/* Free the MAC Services */
	if (pTWD->hMacServices != NULL) {
		MacServices_destroy(pTWD->hMacServices);
		pTWD->hMacServices = NULL;
	}
	WLAN_INIT_REPORT(("TWD_Destroy: Mac Services released\n"));

	/*
	 * Free the Ctrl modules
	 */
	if (pTWD->hCmdQueue != NULL) {
		cmdQueue_Destroy(pTWD->hCmdQueue);
		pTWD->hCmdQueue = NULL;
	}

	/*
	 * Free the FW-Transfer modules:
	 */
	if (pTWD->hTxXfer != NULL) {
		txXfer_Destroy (pTWD->hTxXfer);
		pTWD->hTxXfer = NULL;
	}

	if (pTWD->hTxResult != NULL) {
		txResult_Destroy (pTWD->hTxResult);
		pTWD->hTxResult = NULL;
	}

	if (pTWD->hRxXfer != NULL) {
		rxXfer_Destroy (pTWD->hRxXfer);
		pTWD->hRxXfer = NULL;
	}

	if (pTWD->hEventMbox != NULL) {
		eventMbox_Destroy (pTWD->hEventMbox);
		pTWD->hEventMbox = NULL;
	}

#ifdef TI_DBG
	if (pTWD->hFwDbg != NULL) {
		fwDbg_Destroy (pTWD->hFwDbg);
		pTWD->hFwDbg = NULL;
	}
#endif /* TI_DBG */

	if (pTWD->hFwEvent != NULL) {
		fwEvent_Destroy (pTWD->hFwEvent);
		pTWD->hFwEvent = NULL;
	}

	if (pTWD->hCmdMbox != NULL) {
		cmdMbox_Destroy (pTWD->hCmdMbox);
		pTWD->hCmdMbox = NULL;
	}

	if (pTWD->hRxQueue != NULL) {
		RxQueue_Destroy (pTWD->hRxQueue);
		pTWD->hRxQueue = NULL;
	}

	/*
	 * Free the Data-Services modules:
	 */

	if (pTWD->hTxCtrlBlk != NULL) {
		txCtrlBlk_Destroy (pTWD->hTxCtrlBlk);
		pTWD->hTxCtrlBlk = NULL;
	}

	if (pTWD->hTxHwQueue != NULL) {
		txHwQueue_Destroy (pTWD->hTxHwQueue);
		pTWD->hTxHwQueue = NULL;
	}

	if (pTWD->hHwInit != NULL) {
		hwInit_Destroy (pTWD->hHwInit);
		pTWD->hHwInit = NULL;
	}

	os_memoryFree (pTWD->hOs, (TI_HANDLE)pTWD, sizeof(TTwd));

	WLAN_INIT_REPORT(("TWD_Destroy pTNETW_Driver released!!!\n"));

	return TI_OK;
}


/**
 * \brief HW Init Callback
 *
 * \param  hTWD         - TWD module object handle
 * \return void
 *
 * \par Description
 * Static CB function
 * Called during TWD Module Init by hwInit_Init in order to complete the HW Configuration init
 *
 * \sa     TWD_InitHw
 */
static void TWD_InitHwCb (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	hwInit_InitPolarity(pTWD->hHwInit);

}

void TWD_Init (TI_HANDLE    hTWD,
               TI_HANDLE 	hReport,
               TI_HANDLE 	hUser,
               TI_HANDLE 	hTimer,
               TI_HANDLE 	hContext,
               TI_HANDLE 	hTxnQ,
               TTwdCallback fInitHwCb,
               TTwdCallback fInitFwCb,
               TTwdCallback fConfigFwCb,
               TTwdCallback	fStopCb,
               TTwdCallback fInitFailCb)
{
	TTwd *pTWD 				= (TTwd *)hTWD;
	pTWD->bInitSuccess 		= TI_FALSE;
	pTWD->bRecoveryEnabled 	= TI_FALSE;
	pTWD->hReport           = hReport;
	pTWD->hUser 			= hUser;
	pTWD->hTimer            = hTimer;
	pTWD->hContext          = hContext;
	pTWD->hTxnQ             = hTxnQ;
	pTWD->fInitHwCb 		= fInitHwCb;
	pTWD->fInitFwCb 		= fInitFwCb;
	pTWD->fConfigFwCb 		= fConfigFwCb;
	pTWD->fStopCb 			= fStopCb;
	pTWD->fInitFailCb       = fInitFailCb;


	/* FwEvent should be configured first */
	fwEvent_Init (pTWD->hFwEvent, hTWD);

	eventMbox_Config (pTWD->hEventMbox, pTWD->hTwIf, pTWD->hReport, pTWD->hFwEvent, pTWD->hCmdBld);

	cmdQueue_Init (pTWD->hCmdQueue,
	               pTWD->hCmdMbox,
	               pTWD->hReport,
	               pTWD->hTwIf,
	               pTWD->hTimer);

	/* Configure Command Builder */
	cmdBld_Config (pTWD->hCmdBld,
	               pTWD->hReport,
	               (void *)TWD_FinalizeDownload,
	               hTWD,
	               pTWD->hEventMbox,
	               pTWD->hCmdQueue,
	               pTWD->hTwIf);

	hwInit_Init (pTWD->hHwInit,
	             pTWD->hReport,
	             hTWD,
	             hTWD,
	             (TFinalizeCb)TWD_FinalizeDownload,
	             TWD_InitHwCb);

	/*
	 * Initialize the FW-Transfer modules
	 */
	txXfer_Init (pTWD->hTxXfer, pTWD->hReport, pTWD->hTwIf);

	txResult_Init (pTWD->hTxResult, pTWD->hReport, pTWD->hTwIf);

	rxXfer_Init (pTWD->hRxXfer, pTWD->hFwEvent, pTWD->hReport, pTWD->hTwIf, pTWD->hRxQueue);

	RxQueue_Init (pTWD->hRxQueue, pTWD->hReport);

#ifdef TI_DBG
	fwDbg_Init (pTWD->hFwDbg, pTWD->hReport, pTWD->hTwIf);
#endif /* TI_DBG */

	/* Initialize the MAC Services */
	MacServices_init (pTWD->hMacServices,
	                  pTWD->hReport,
	                  hTWD,
	                  pTWD->hCmdBld,
	                  pTWD->hEventMbox,
	                  pTWD->hTimer);

	/*
	 * Initialize the Data-Services modules
	 */
	txCtrlBlk_Init (pTWD->hTxCtrlBlk, pTWD->hReport, pTWD->hContext);
	txHwQueue_Init (pTWD->hTxHwQueue, pTWD->hReport);

	/* Initialize the TwIf module */
	twIf_Init (pTWD->hTwIf,
	           pTWD->hReport,
	           pTWD->hContext,
	           pTWD->hTimer,
	           pTWD->hTxnQ,
	           (TRecoveryCb)TWD_StopComplete,
	           hTWD);
}

TI_STATUS TWD_InitHw (TI_HANDLE hTWD,
                      TI_UINT8  *pbuf,
                      TI_UINT32 length,
                      TI_UINT32 uRxDmaBufLen,
                      TI_UINT32 uTxDmaBufLen)
{
	TTwd *pTWD = (TTwd *)hTWD;
	TI_STATUS eStatus;


	/* Provide bus related parameters to Xfer modules before any usage of the bus! */
	rxXfer_SetBusParams (pTWD->hRxXfer, uRxDmaBufLen);
	txXfer_SetBusParams (pTWD->hTxXfer, uTxDmaBufLen);

	hwInit_SetNvsImage (pTWD->hHwInit, pbuf, length);

	/*
	 * Update the TwIf that the HW is awake
	 * This will protect the initialization process from going to sleep
	 * After the firmware initializations completed (TWD_EnableExternalEvents), the sleep will be enabled
	 */
	twIf_Awake (pTWD->hTwIf);
	twIf_HwAvailable (pTWD->hTwIf);

	/* This initiates the HW init sequence */
	eStatus = hwInit_Boot(pTWD->hHwInit);
	if (eStatus == TXN_STATUS_ERROR) {
		return TI_NOK;
	}

	return TI_OK;
}

TI_STATUS TWD_BusOpen (TI_HANDLE hTWD, void* pParams)
{
	TI_STATUS uStatus;


	/*uStatus = TNETWIF_Open(pTWD->hTNETWIF, pParams);*/
	uStatus = TI_OK;

	return uStatus;
}

TI_STATUS TWD_BusClose (TI_HANDLE hTWD)
{
	TI_STATUS uStatus;


	/*uStatus = TNETWIF_Close(pTWD->hTNETWIF);*/
	uStatus = TI_OK;

	return uStatus;
}

TI_STATUS TWD_InitFw (TI_HANDLE hTWD, TFileInfo *pFileInfo)
{
	TTwd *pTWD = (TTwd *)hTWD;
	TI_STATUS status;

	/* check Parameters */
	if (( pTWD == NULL ) || ( pFileInfo == NULL )) {
		return (TI_NOK);
	}


	hwInit_SetFwImage (pTWD->hHwInit, pFileInfo);

	/* This will initiate the download to the FW */
	status = hwInit_LoadFw(pTWD->hHwInit);

	if (status == TXN_STATUS_ERROR) {

		return TI_NOK;
	}

	return TI_OK;
}

/**
 * \brief  Propagate interrogate results
 *
 * \param  hTWD         - TWD module object handle
 * \param  status       - callback status
 * \return TI_OK on success or TI_NOK on failure
 *
 * \par Description
 * Static CB function
 * Propagate interrogate results between TX and RX modules
 * Called by TWD_ConfigFw
 *
 * \sa
 */
static TI_STATUS TWD_ConfigFwCb (TI_HANDLE hTWD, TI_STATUS status)
{
	TTwd        *pTWD = (TTwd *)hTWD;
	TDmaParams  *pDmaParams = &DB_DMA(pTWD->hCmdBld);

	/*
	 * Store the addresses of the cyclic buffer (Rx/Tx)
	 * and the path status and control (Tx/Rx) in the corresponding modules
	 */
	txResult_setHwInfo (pTWD->hTxResult, pDmaParams);

	rxXfer_Restart (pTWD->hRxXfer);
	txXfer_Restart (pTWD->hTxXfer);

	rxXfer_SetRxDirectAccessParams (pTWD->hRxXfer, pDmaParams);

	/* Provide number of HW Tx-blocks and descriptors to Tx-HW-Queue module */
	txHwQueue_SetHwInfo (pTWD->hTxHwQueue, pDmaParams);

	/* If the configure complete function was registered, we call it here - end of TWD_Configure stage */
	if (pTWD->fConfigFwCb) {

		pTWD->fConfigFwCb (pTWD->hUser, TI_OK);
	}

	return TI_OK;
}


TI_STATUS TWD_SetDefaults (TI_HANDLE hTWD, TTwdInitParams *pInitParams)
{
	TTwd         *pTWD = (TTwd *)hTWD;

	TWlanParams         *pWlanParams = &DB_WLAN(pTWD->hCmdBld);
	TKeepAliveList      *pKlvParams = &DB_KLV(pTWD->hCmdBld);
	IniFileRadioParam   *pRadioParams = &DB_RADIO(pTWD->hCmdBld);
	IniFileGeneralParam *pGenParams = &DB_GEN(pTWD->hCmdBld);
	TRateMngParams      *pRateMngParams = &DB_RM(pTWD->hCmdBld);
	TDmaParams          *pDmaParams = &DB_DMA(pTWD->hCmdBld);

	TI_UINT32            k, uIndex;
	int iParam;

	pTWD->bRecoveryEnabled = pInitParams->tGeneral.halCtrlRecoveryEnable;

	pWlanParams->PacketDetectionThreshold   = pInitParams->tGeneral.packetDetectionThreshold;
	pWlanParams->qosNullDataTemplateSize    = pInitParams->tGeneral.qosNullDataTemplateSize;
	pWlanParams->PsPollTemplateSize         = pInitParams->tGeneral.PsPollTemplateSize;
	pWlanParams->probeResponseTemplateSize  = pInitParams->tGeneral.probeResponseTemplateSize;
	pWlanParams->probeRequestTemplateSize   = pInitParams->tGeneral.probeRequestTemplateSize;
	pWlanParams->beaconTemplateSize         = pInitParams->tGeneral.beaconTemplateSize;
	pWlanParams->nullTemplateSize           = pInitParams->tGeneral.nullTemplateSize;
	pWlanParams->disconnTemplateSize        = pInitParams->tGeneral.disconnTemplateSize;
	pWlanParams->ArpRspTemplateSize         = pInitParams->tGeneral.ArpRspTemplateSize;


	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_PD_THRESHOLD)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_PROBE_REQ)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_NULL_DATA)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_DISCONN)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_PS_POLL)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_QOS_NULL_DATA)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_PROBE_RESP)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_BEACON)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_DEAUTH_STA)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_KEEP_ALIVE_TMPL)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CMD_ARP_RSP)


#ifndef AP_MODE_ENABLED
	/* Beacon broadcast options */
	pWlanParams->BcnBrcOptions.BeaconRxTimeout      = pInitParams->tGeneral.BeaconRxTimeout;
	pWlanParams->BcnBrcOptions.BroadcastRxTimeout   = pInitParams->tGeneral.BroadcastRxTimeout;
	pWlanParams->BcnBrcOptions.RxBroadcastInPs      = pInitParams->tGeneral.RxBroadcastInPs;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_BCN_BRC_OPTIONS)
#endif

	pWlanParams->ConsecutivePsPollDeliveryFailureThreshold = pInitParams->tGeneral.ConsecutivePsPollDeliveryFailureThreshold;

	pWlanParams->RxDisableBroadcast         = pInitParams->tGeneral.halCtrlRxDisableBroadcast;
	pWlanParams->calibrationChannel2_4      = pInitParams->tGeneral.halCtrlCalibrationChannel2_4;
	pWlanParams->calibrationChannel5_0      = pInitParams->tGeneral.halCtrlCalibrationChannel5_0;

	/* Not used but need by Palau */
	pWlanParams->RtsThreshold               = pInitParams->tGeneral.halCtrlRtsThreshold;
	pWlanParams->CtsToSelf                  = CTS_TO_SELF_DISABLE;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_CTS_PROTECTION)



#ifndef AP_MODE_ENABLED
	pWlanParams->WiFiWmmPS                  = pInitParams->tGeneral.WiFiWmmPS;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_PS_WMM)
#endif

	pWlanParams->MaxTxMsduLifetime          = pInitParams->tGeneral.halCtrlMaxTxMsduLifetime;
	pWlanParams->MaxRxMsduLifetime          = pInitParams->tGeneral.halCtrlMaxRxMsduLifetime;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_RX_MSDU_LIFE_TIME)


	pWlanParams->rxTimeOut.psPoll           = pInitParams->tGeneral.rxTimeOut.psPoll;
	pWlanParams->rxTimeOut.UPSD             = pInitParams->tGeneral.rxTimeOut.UPSD;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_SERVICE_PERIOD_TIMEOUT)


#ifndef AP_MODE_ENABLED
	/* RSSI/SNR Weights for Average calculations */
	pWlanParams->tRssiSnrWeights.rssiBeaconAverageWeight = pInitParams->tGeneral.uRssiBeaconAverageWeight;
	pWlanParams->tRssiSnrWeights.rssiPacketAverageWeight = pInitParams->tGeneral.uRssiPacketAverageWeight;
	pWlanParams->tRssiSnrWeights.snrBeaconAverageWeight  = pInitParams->tGeneral.uSnrBeaconAverageWeight ;
	pWlanParams->tRssiSnrWeights.snrPacketAverageWeight  = pInitParams->tGeneral.uSnrPacketAverageWeight ;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_RSSI_SNR_WEIGHTS)
#endif

	/* PM config params */
	pWlanParams->uHostClkSettlingTime       = pInitParams->tGeneral.uHostClkSettlingTime;
	pWlanParams->uHostFastWakeupSupport     = pInitParams->tGeneral.uHostFastWakeupSupport;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_PM_CONFIG)


	/* No used */
	pWlanParams->FragmentThreshold          = pInitParams->tGeneral.halCtrlFragThreshold;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_FRAGMENT_THRESHOLD)

	pWlanParams->ListenInterval             = (TI_UINT8)pInitParams->tGeneral.halCtrlListenInterval;
	pWlanParams->RateFallback               = pInitParams->tGeneral.halCtrlRateFallbackRetry;
	pWlanParams->MacClock                   = pInitParams->tGeneral.halCtrlMacClock;
	pWlanParams->ArmClock                   = pInitParams->tGeneral.halCtrlArmClock;

	pWlanParams->ch14TelecCca = pInitParams->tGeneral.halCtrlCh14TelecCca;

	/* Data interrupts pacing */
	pWlanParams->TxCompletePacingThreshold  = pInitParams->tGeneral.TxCompletePacingThreshold;
	pWlanParams->TxCompletePacingTimeout    = pInitParams->tGeneral.TxCompletePacingTimeout;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_TX_CMPLT_PACING)

	pWlanParams->RxIntrPacingThreshold      = pInitParams->tGeneral.RxIntrPacingThreshold;
	pWlanParams->RxIntrPacingTimeout        = pInitParams->tGeneral.RxIntrPacingTimeout;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_RX_INTR_PACING)


	/* Number of Rx mem-blocks to allocate in FW */
	pDmaParams->NumRxBlocks                 = pInitParams->tGeneral.uRxMemBlksNum;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_MEM)
	/* GenFwCmd */
	for (uIndex=0; uIndex<GEN_FW_CMD_SIZE; uIndex++)
		pWlanParams->GenFwCmd[uIndex]      = pInitParams->tGeneral.GenFwCmd[uIndex];
	/* HostIfCfgBitmap */
	pWlanParams->HostIfCfgBitmap        = pInitParams->tGeneral.HostIfCfgBitmap;

#ifndef AP_MODE_ENABLED
	/* Configure ARP IP */
	pWlanParams->arpFilterType    = pInitParams->tArpIpFilter.filterType;
	IP_COPY (pWlanParams->arp_IP_addr, pInitParams->tArpIpFilter.addr);
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_ARP_IP_FILTER)

	/* Configure address group */
	pWlanParams->numGroupAddrs = pInitParams->tMacAddrFilter.numOfMacAddresses;
	pWlanParams->isMacAddrFilteringnabled = pInitParams->tMacAddrFilter.isFilterEnabled;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_GROUP_ADDRESS_TABLE)

#endif


	for (k = 0; k < pWlanParams->numGroupAddrs; k++) {
		MAC_COPY (pWlanParams->aGroupAddr[k], pInitParams->tMacAddrFilter.macAddrTable[k]);
	}



	pWlanParams->tWlanParamsCoexActivityTable.numOfElements = 0;
	for (iParam=0; iParam < (int)pInitParams->tGeneral.halCoexActivityTable.numOfElements; iParam++) {
		TCoexActivity *pSaveCoex = &pWlanParams->tWlanParamsCoexActivityTable.entry[0];
		TCoexActivity *pParmCoex = &pInitParams->tGeneral.halCoexActivityTable.entry[0];
		int i, saveIndex;

		/* Check if to overwrite existing entry or put on last index */
		for (i=0; i<iParam; i++) {
			if ((pSaveCoex[i].activityId == pParmCoex[iParam].activityId) && (pSaveCoex[i].coexIp == pParmCoex[iParam].coexIp)) {
				break;
			}
		}

		if (i == iParam) {
			/* new entry */
			saveIndex = pWlanParams->tWlanParamsCoexActivityTable.numOfElements;
			pWlanParams->tWlanParamsCoexActivityTable.numOfElements++;
		} else {
			/* overwrite existing */
			saveIndex = i;
		}


		pSaveCoex[saveIndex].coexIp          = pParmCoex[iParam].coexIp;
		pSaveCoex[saveIndex].activityId      = pParmCoex[iParam].activityId;
		pSaveCoex[saveIndex].defaultPriority = pParmCoex[iParam].defaultPriority;
		pSaveCoex[saveIndex].raisedPriority  = pParmCoex[iParam].raisedPriority;
		pSaveCoex[saveIndex].minService      = pParmCoex[iParam].minService;
		pSaveCoex[saveIndex].maxService      = pParmCoex[iParam].maxService;
	}

	/* configure keep-alive default mode to enabled */
	pKlvParams->enaDisFlag = TI_TRUE;
	for (uIndex = 0; uIndex < KLV_MAX_TMPL_NUM; uIndex++) {
		pKlvParams->keepAliveParams[ uIndex ].enaDisFlag = TI_FALSE;
	}

	/* Configure the TWD modules */
	rxXfer_SetDefaults (pTWD->hRxXfer, pInitParams);
	txXfer_SetDefaults (pTWD->hTxXfer, pInitParams);
	txHwQueue_Config (pTWD->hTxHwQueue, pInitParams);
	/* Configure the rxXfer module */
	rxXfer_Config (pTWD->hRxXfer, pInitParams);
	MacServices_config (pTWD->hMacServices, pInitParams);

	/*
	 * 802.11n
	 */
	pWlanParams->tTwdHtCapabilities.b11nEnable =            pInitParams->tGeneral.b11nEnable;
	pWlanParams->tTwdHtCapabilities.uChannelWidth = CHANNEL_WIDTH_20MHZ;
	pWlanParams->tTwdHtCapabilities.uRxSTBC       = RXSTBC_NOT_SUPPORTED;
	pWlanParams->tTwdHtCapabilities.uMaxAMSDU     = MAX_MSDU_3839_OCTETS;
	pWlanParams->tTwdHtCapabilities.uMaxAMPDU     = pInitParams->tGeneral.uMaxAMPDU;

	pWlanParams->tTwdHtCapabilities.uAMPDUSpacing =         AMPDU_SPC_8_MICROSECONDS;
	pWlanParams->tTwdHtCapabilities.aRxMCS[0] =             (MCS_SUPPORT_MCS_0 |
	        MCS_SUPPORT_MCS_1 |
	        MCS_SUPPORT_MCS_2 |
	        MCS_SUPPORT_MCS_3 |
	        MCS_SUPPORT_MCS_4 |
	        MCS_SUPPORT_MCS_5 |
	        MCS_SUPPORT_MCS_6 |
	        MCS_SUPPORT_MCS_7);
	os_memoryZero (pTWD->hOs, pWlanParams->tTwdHtCapabilities.aRxMCS + 1, RX_TX_MCS_BITMASK_SIZE - 1);
	pWlanParams->tTwdHtCapabilities.aTxMCS[0]  =             (MCS_SUPPORT_MCS_0 |
	        MCS_SUPPORT_MCS_1 |
	        MCS_SUPPORT_MCS_2 |
	        MCS_SUPPORT_MCS_3 |
	        MCS_SUPPORT_MCS_4 |
	        MCS_SUPPORT_MCS_5 |
	        MCS_SUPPORT_MCS_6 |
	        MCS_SUPPORT_MCS_7);
	os_memoryZero (pTWD->hOs, pWlanParams->tTwdHtCapabilities.aTxMCS + 1, RX_TX_MCS_BITMASK_SIZE - 1);
	pWlanParams->tTwdHtCapabilities.uRxMaxDataRate =         MCS_HIGHEST_SUPPORTED_RECEPTION_DATA_RATE_IN_MBIT_S;
	pWlanParams->tTwdHtCapabilities.uPCOTransTime =          PCO_TRANS_TIME_NO_TRANSITION;
	pWlanParams->tTwdHtCapabilities.uHTCapabilitiesBitMask = (CAP_BIT_MASK_GREENFIELD_FRAME_FORMAT |
	        CAP_BIT_MASK_SHORT_GI_FOR_20MHZ_PACKETS);
	pWlanParams->tTwdHtCapabilities.uMCSFeedback =           MCS_FEEDBACK_NO;

	os_memoryCopy(pTWD->hOs, (void*)pRadioParams, (void*)&pInitParams->tIniFileRadioParams, sizeof(IniFileRadioParam));
	os_memoryCopy(pTWD->hOs, (void*)pGenParams, (void*)&pInitParams->tPlatformGenParams, sizeof(IniFileGeneralParam));

	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_PLATFORM_PARAMS)
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_RADIO_PARAMS)


#ifndef AP_MODE_ENABLED
	os_memoryCopy (pTWD->hOs,
	               (void*)&(pWlanParams->tFmCoexParams),
	               (void*)&(pInitParams->tGeneral.tFmCoexParams),
	               sizeof(TFmCoexParams));

	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_FM_COEX)
#endif


	/* Rate management params */
	pRateMngParams->rateMngParams.InverseCuriosityFactor = pInitParams->tRateMngParams.InverseCuriosityFactor;
	pRateMngParams->rateMngParams.MaxPer = pInitParams->tRateMngParams.MaxPer;
	pRateMngParams->rateMngParams.PerAdd = pInitParams->tRateMngParams.PerAdd;
	pRateMngParams->rateMngParams.PerAddShift = pInitParams->tRateMngParams.PerAddShift;
	pRateMngParams->rateMngParams.PerAlphaShift = pInitParams->tRateMngParams.PerAlphaShift;
	pRateMngParams->rateMngParams.PerBeta1Shift = pInitParams->tRateMngParams.PerBeta1Shift;
	pRateMngParams->rateMngParams.PerBeta2Shift = pInitParams->tRateMngParams.PerBeta2Shift;
	pRateMngParams->rateMngParams.PerTh1 = pInitParams->tRateMngParams.PerTh1;
	pRateMngParams->rateMngParams.PerTh2 = pInitParams->tRateMngParams.PerTh2;
	pRateMngParams->rateMngParams.RateCheckDown = pInitParams->tRateMngParams.RateCheckDown;
	pRateMngParams->rateMngParams.RateCheckUp = pInitParams->tRateMngParams.RateCheckUp;
	pRateMngParams->rateMngParams.RateRetryScore = pInitParams->tRateMngParams.RateRetryScore;
	pRateMngParams->rateMngParams.TxFailHighTh = pInitParams->tRateMngParams.TxFailHighTh;
	pRateMngParams->rateMngParams.TxFailLowTh = pInitParams->tRateMngParams.TxFailLowTh;

	/* RATE_MNG_MAX_RETRY_POLICY_PARAMS_LEN */
	for (uIndex = 0; uIndex < 13; uIndex++) {
		pRateMngParams->rateMngParams.RateRetryPolicy[uIndex] = pInitParams->tRateMngParams.RateRetryPolicy[uIndex];
	}

	/* DCO Itrim params */
	pWlanParams->dcoItrimEnabled = pInitParams->tDcoItrimParams.enable;
	pWlanParams->dcoItrimModerationTimeoutUsec = pInitParams->tDcoItrimParams.moderationTimeoutUsec;
	CMD_BLD_MARK_INIT_SEQUENCE_CMD_AS_VALID(pTWD->hCmdBld, __CFG_DCO_ITRIM_PARAMS)

	return TI_OK;
}

/*
#define MAX_SR_PARAM_LEN  14

void TWD_CheckSRConfigParams(TTwd  *pTWD, ACXSmartReflexConfigParams_t *tSmartReflexParams)
{
  int i,j;
  TI_UINT8 len;
  TI_BOOL flag = TI_FALSE;


  for (i = 0; i<3; i++)
  {
    len = tSmartReflexParams->errorTable[i].len;
     if (len > MAX_SR_PARAM_LEN)
          flag = TI_TRUE;
     for (j=0;j<len-1;j++)
     {
      if (tSmartReflexParams->errorTable[i].values[j] == 0)
         flag = TI_TRUE;
     }
     if (flag == TI_TRUE)
      {
        tSmartReflexParams->errorTable[i].len= 0 ;
        tSmartReflexParams->errorTable[i].upperLimit = 0;
        os_memoryZero(pTWD->hOs,tSmartReflexParams->errorTable[i].values,MAX_SR_PARAM_LEN) ;
      }

  }



} */


TI_STATUS TWD_ConfigFw (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	/*
	 * Configure the WLAN firmware after config all the hardware objects
	 */
	if (cmdBld_ConfigFw (pTWD->hCmdBld, (void *)TWD_ConfigFwCb, hTWD) != TI_OK) {
		return TI_NOK;
	}

	return TI_OK;
}

void TWD_FinalizeDownload (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	if ( pTWD == NULL ) {
		return;
	}

	/* Sign that init has succeeded */
	pTWD->bInitSuccess = TI_TRUE;

	/* Call user application configuration callback */
	if (pTWD->fInitFwCb) {

		(*pTWD->fInitFwCb) (pTWD->hUser, TI_OK);
	}
}

void TWD_FinalizeOnFailure (TI_HANDLE hTWD)
{
	TTwd  *pTWD = (TTwd *)hTWD;


	/* Call the upper layer callback for init failure case */
	if (pTWD->fInitFailCb) {

		pTWD->fInitFailCb (pTWD->hUser, TI_OK);
	}
}

TI_STATUS TWD_CheckMailboxCb (TI_HANDLE hTWD, TI_UINT16 uMboxStatus, void *pItrParamBuf)
{
	TTwd *pTWD = (TTwd *)hTWD;


	if (uMboxStatus != TI_OK) {
		++pTWD->uNumMboxFailures;

		/* Indicating Upper Layer about Mbox Error */
		pTWD->fFailureEventCb (pTWD->hFailureEventCb, MBOX_FAILURE);
	}

	return TI_OK;
}
#ifdef RS_OVER_TWD
extern	void (*gBusTxn_ErrorCb)(TI_HANDLE , int);
extern  void *gBusTxn_ErrorHndle;
#endif

/**
 * \brief Registers TWD Module Callbacks
 *
 * \param  hTWD         - TWD module object handle
 * \param  uCallBackID  - Registered Callback ID
 * \param  fCb 	        - Pointer to Input Registered CB function
 * \param  hCb 	        - Handle to Input Registered CB parameters
 * \return void
 *
 * \par Description
 * Static CB function
 * This CB Registers TWD CB functions for future use:
 * CB which handles failure to the CMD Queue, MAC Service and TwIf
 * CB which handles Command Complete for the CMD Queue
 * Called by TWD_RegisterCb
 *
 * \sa TWD_RegisterCb
 */
static void TWD_RegisterOwnCb (TI_HANDLE hTWD, TI_UINT32 uCallBackID, void *fCb, TI_HANDLE hCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	switch (uCallBackID) {
	case TWD_INT_EVENT_FAILURE:
		/* Save Health-Moitor callback */
		pTWD->fFailureEventCb = (TFailureEventCb)fCb;
		pTWD->hFailureEventCb = hCb;

		/* Register For Error Of Mailbox in case of timeout */
		cmdQueue_RegisterForErrorCb (pTWD->hCmdQueue, (void *)TWD_CheckMailboxCb, hTWD);

		/* Forward the Health-Moitor callback to the MAC-Services modules */
		MacServices_registerFailureEventCB(pTWD->hMacServices, fCb, hCb);

		/* Forward the Health-Moitor callback to the TwIf for bus errors */
		twIf_RegisterErrCb (pTWD->hTwIf, fCb, hCb);

		/* Forward the Health-Moitor callback to the RxXfer for Rx packet errors */
		rxXfer_RegisterErrCb (pTWD->hRxXfer, fCb, hCb);
		break;

	case TWD_INT_COMMAND_COMPLETE:
		cmdQueue_RegisterCmdCompleteGenericCb (pTWD->hCmdQueue, fCb, hCb);
		break;

	default:{}
	}
}

TI_STATUS TWD_RegisterCb (TI_HANDLE hTWD, TI_UINT32 event, TTwdCB *fCb, void *pData)
{
	TTwd *pTWD = (TTwd *)hTWD;
	TI_UINT32 uModuleId    = event & TWD_CB_MODULE_OWNER_MASK;
	TI_UINT32 uCallbackId  = event & TWD_CB_TYPE_MASK;

	/* First detect which module is the owner */

	switch (uModuleId) {
	case TWD_OWNER_TX_HW_QUEUE:
		txHwQueue_RegisterCb (pTWD->hTxHwQueue, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_DRIVER_TX_XFER:
		txXfer_RegisterCb (pTWD->hTxXfer, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_TX_RESULT:
		txResult_RegisterCb (pTWD->hTxResult, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_RX_XFER:
		rxXfer_Register_CB(pTWD->hRxXfer, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_RX_QUEUE:
		RxQueue_Register_CB(pTWD->hRxQueue, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_SELF:
		TWD_RegisterOwnCb (hTWD, uCallbackId, fCb, pData);
		break;

	case TWD_OWNER_MAC_SERVICES:
		switch (uCallbackId) {
		case TWD_OWN_EVENT_SCAN_CMPLT:
			MacServices_scanSRV_registerScanCompleteCB (pTWD->hMacServices, (TScanSrvCompleteCb)fCb, pData);
			break;
		default:{}
		}
		break;

	case TWD_OWNER_SELF_CONFIG:
		pTWD->fConfigFwCb  = (TTwdCallback)fCb;
		break;

	default: {}
	}

	return TI_OK;
}

TI_STATUS TWD_ExitFromInitMode (TI_HANDLE hTWD)
{
	TTwd    *pTWD = (TTwd *)hTWD;


	/* Notify Event MailBox about init complete */
	eventMbox_InitComplete (pTWD->hEventMbox);

	/* Enable Mailbox */
	cmdQueue_EnableMbox (pTWD->hCmdQueue);

	return TI_OK;
}


#ifdef TI_DBG
TI_STATUS TWD_PrintTxInfo (TI_HANDLE hTWD, ETwdPrintInfoType ePrintInfo)
{
	TTwd *pTWD = (TTwd *)hTWD;


	switch (ePrintInfo) {
	case TWD_PRINT_TX_CTRL_BLK_TBL:
		txCtrlBlk_PrintTable (pTWD->hTxCtrlBlk);
		break;

	case TWD_PRINT_TX_HW_QUEUE_INFO:
		txHwQueue_PrintInfo (pTWD->hTxHwQueue);
		break;

	case TWD_PRINT_TX_XFER_INFO:
		txXfer_PrintStats (pTWD->hTxXfer);
		break;

	case TWD_PRINT_TX_RESULT_INFO:
		txResult_PrintInfo (pTWD->hTxResult);
		break;

	case TWD_CLEAR_TX_RESULT_INFO:
		txResult_ClearInfo (pTWD->hTxResult);
		break;

	case TWD_CLEAR_TX_XFER_INFO:
		txXfer_ClearStats (pTWD->hTxXfer);
		break;

	default: {}
	}

	return TI_OK;
}

#endif /* TI_DBG */

TI_STATUS TWD_InterruptRequest (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	fwEvent_InterruptRequest (pTWD->hFwEvent);

	return TI_OK;
}

TI_STATUS TWD_RegisterEvent (TI_HANDLE hTWD, TI_UINT32 event, void *fCb, TI_HANDLE hCb)
{
	TTwd  *pTWD = (TTwd *)hTWD;


	return eventMbox_RegisterEvent (pTWD->hEventMbox, event, fCb, hCb);
}

TI_STATUS TWD_DisableEvent (TI_HANDLE hTWD, TI_UINT32 event)
{
	TTwd  *pTWD = (TTwd *)hTWD;


	return eventMbox_MaskEvent (pTWD->hEventMbox, event, NULL, NULL);
}

TI_STATUS TWD_EnableEvent (TI_HANDLE hTWD, TI_UINT32 event)
{
	TTwd  *pTWD = (TTwd *)hTWD;


	return eventMbox_UnMaskEvent (pTWD->hEventMbox, event, NULL, NULL);
}

void TWD_StopComplete (TI_HANDLE hTWD)
{
	TTwd  *pTWD = (TTwd *)hTWD;



	/* reinit last ELP mode flag in recovery */
	cmdBld_Restart(pTWD->hCmdBld);

	/* Call upper layer callback */
	if (pTWD->fStopCb) {

		(*pTWD->fStopCb) (pTWD->hUser, TI_OK);
	}
}

TI_STATUS TWD_Stop (TI_HANDLE hTWD)
{
	TTwd        *pTWD = (TTwd *)hTWD;
	ETxnStatus   status;


	fwEvent_Stop (pTWD->hFwEvent);


	/* close all BA sessions */
	TWD_CloseAllBaSessions(hTWD);

	cmdMbox_Restart (pTWD->hCmdMbox);
	cmdQueue_Restart (pTWD->hCmdQueue);
	cmdQueue_DisableMbox (pTWD->hCmdQueue);
	eventMbox_Stop (pTWD->hEventMbox);
	MacServices_restart (pTWD->hMacServices);

	status = twIf_Restart(pTWD->hTwIf);

	/* Call user stop callback */
	if (status != TXN_STATUS_PENDING) {
		TWD_StopComplete (hTWD);
	}

	return TI_OK;
}

void TWD_EnableExternalEvents (TI_HANDLE hTWD)
{
	TTwd        *pTWD = (TTwd *)hTWD;


	/*
	 * Enable sleep after all firmware initializations completed
	 * The awake was in the TWD_initHw phase
	 */

	fwEvent_EnableExternalEvents (pTWD->hFwEvent);

	twIf_Sleep (pTWD->hTwIf);
}

TI_BOOL TWD_RecoveryEnabled (TI_HANDLE hTWD)
{
	TTwd  *pTWD = (TTwd *)hTWD;


	return pTWD->bRecoveryEnabled;
}

TI_UINT32 TWD_GetMaxNumberOfCommandsInQueue (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return cmdQueue_GetMaxNumberOfCommands (pTWD->hCmdQueue);
}

TI_STATUS TWD_SetPsMode (TI_HANDLE                   hTWD,
                         E80211PsMode ePsMode,
                         TI_BOOL bSendNullDataOnExit,
                         TI_HANDLE                   hPowerSaveCompleteCb,
                         TPowerSaveCompleteCb        fPowerSaveCompleteCb,
                         TPowerSaveResponseCb        fPowerSaveResponseCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_powerSrv_SetPsMode (pTWD->hMacServices,
	                                       ePsMode,
	                                       bSendNullDataOnExit,
	                                       hPowerSaveCompleteCb,
	                                       fPowerSaveCompleteCb,
	                                       fPowerSaveResponseCb);
}

TI_BOOL TWD_GetPsStatus (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_powerSrv_getPsStatus (pTWD->hMacServices);
}

TI_STATUS TWD_SetNullRateModulation (TI_HANDLE hTWD, TI_UINT16 rate)
{
	TTwd *pTWD = (TTwd *)hTWD;


	MacServices_powerSrv_SetRateModulation (pTWD->hMacServices, rate);

	return TI_OK;
}

void TWD_UpdateDtimTbtt (TI_HANDLE hTWD, TI_UINT8 uDtimPeriod, TI_UINT16 uBeaconInterval)
{
	TTwd *pTWD = (TTwd *)hTWD;


	MacServices_scanSrv_UpdateDtimTbtt (pTWD->hMacServices, uDtimPeriod, uBeaconInterval);
}

TI_STATUS TWD_StartMeasurement (TI_HANDLE                   hTWD,
                                TMeasurementRequest        *pMsrRequest,
                                TI_UINT32                   uTimeToRequestExpiryMs,
                                TCmdResponseCb              fResponseCb,
                                TI_HANDLE                   hResponseCb,
                                TMeasurementSrvCompleteCb   fCompleteCb,
                                TI_HANDLE                   hCompleteCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_measurementSRV_startMeasurement (pTWD->hMacServices,
	        pMsrRequest,
	        uTimeToRequestExpiryMs,
	        fResponseCb,
	        hResponseCb,
	        fCompleteCb,
	        hCompleteCb);
}

TI_STATUS TWD_StopMeasurement (TI_HANDLE       	hTWD,
                               TI_BOOL        	bSendNullData,
                               TCmdResponseCb  	fResponseCb,
                               TI_HANDLE       	hResponseCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_measurementSRV_stopMeasurement (pTWD->hMacServices,
	        bSendNullData,
	        fResponseCb,
	        hResponseCb);
}

TI_STATUS TWD_RegisterScanCompleteCb (TI_HANDLE            hTWD,
                                      TScanSrvCompleteCb   fScanCompleteCb,
                                      TI_HANDLE            hScanCompleteCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	MacServices_scanSRV_registerScanCompleteCB (pTWD->hMacServices,
	        fScanCompleteCb,
	        hScanCompleteCb);

	return TI_OK;
}

#ifdef TI_DBG
TI_STATUS TWD_PrintMacServDebugStatus (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	MacServices_scanSrv_printDebugStatus (pTWD->hMacServices);

	return TI_OK;
}
#endif

TI_STATUS TWD_Scan (TI_HANDLE       hTWD,
                    TScanParams    	*pScanParams,
                    EScanResultTag 	eScanTag,
                    TI_BOOL        	bHighPriority,
                    TI_BOOL        	bDriverMode,
                    TI_BOOL        	bScanOnDriverModeError,
                    E80211PsMode   	ePsRequest,
                    TI_BOOL        	bSendNullData,
                    TCmdResponseCb 	fResponseCb,
                    TI_HANDLE      	hResponseCb)
{
	TTwd *pTWD = (TTwd *)hTWD;

	return MacServices_scanSRV_scan (pTWD->hMacServices,
	                                 pScanParams,
	                                 eScanTag,
	                                 bHighPriority,
	                                 bDriverMode,
	                                 bScanOnDriverModeError,
	                                 ePsRequest,
	                                 bSendNullData,
	                                 fResponseCb,
	                                 hResponseCb);
}

TI_STATUS TWD_StopScan (TI_HANDLE       hTWD,
                        EScanResultTag  eScanTag,
                        TI_BOOL         bSendNullData,
                        TCmdResponseCb  fScanCommandResponseCb,
                        TI_HANDLE       hCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_scanSRV_stopScan (pTWD->hMacServices,
	                                     eScanTag,
	                                     bSendNullData,
	                                     fScanCommandResponseCb,
	                                     hCb);
}

TI_STATUS TWD_StopScanOnFWReset (TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return MacServices_scanSRV_stopOnFWReset (pTWD->hMacServices);
}

TI_STATUS TWD_StartConnectionScan (TI_HANDLE              hTWD,
                                   TPeriodicScanParams    *pPeriodicScanParams,
                                   EScanResultTag         eScanTag,
                                   TI_UINT32              uPassiveScanDfsDwellTimeMs,
                                   TCmdResponseCb         fResponseCb,
                                   TI_HANDLE              hResponseCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return cmdBld_StartPeriodicScan (pTWD->hCmdBld, pPeriodicScanParams, eScanTag, uPassiveScanDfsDwellTimeMs,
	                                 (void*)fResponseCb, hResponseCb);
}

TI_STATUS TWD_StopPeriodicScan  (TI_HANDLE              hTWD,
                                 EScanResultTag         eScanTag,
                                 TCmdResponseCb         fResponseCb,
                                 TI_HANDLE              hResponseCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return cmdBld_StopPeriodicScan (pTWD->hCmdBld, eScanTag, (void*)fResponseCb, hResponseCb);
}

TI_STATUS TWD_readMem (TI_HANDLE hTWD, TFwDebugParams* pMemDebug, void* fCb, TI_HANDLE hCb)
{
	if (hTWD == NULL || pMemDebug == NULL) {
		return (TI_NOK);
	}


	if (fwDbg_ReadAddr(((TTwd *)hTWD)->hFwDbg,pMemDebug->addr,pMemDebug->length,pMemDebug->UBuf.buf8,(TFwDubCallback)fCb,hCb) == TI_NOK) {
		WLAN_OS_REPORT(("TWD_readMem Error: fwDbg_handleCommand failed\n"));
		return TI_NOK;
	}

	return (TI_OK);
}

TI_STATUS TWD_writeMem (TI_HANDLE hTWD, TFwDebugParams* pMemDebug, void* fCb, TI_HANDLE hCb)
{
	if (hTWD == NULL || pMemDebug == NULL) {
		return (TI_NOK);
	}

	if (fwDbg_WriteAddr(((TTwd *)hTWD)->hFwDbg,pMemDebug->addr,pMemDebug->length,pMemDebug->UBuf.buf8,(TFwDubCallback)fCb,hCb) == TI_NOK) {
		WLAN_OS_REPORT(("TWD_writeMem Error: fwDbg_handleCommand failed\n"));
		return TI_NOK;
	}

	return(TI_OK);
}

TI_BOOL TWD_isValidMemoryAddr (TI_HANDLE hTWD, TFwDebugParams* pMemDebug)
{
	if (hTWD == NULL || pMemDebug == NULL) {
		return TI_FALSE;
	}


	return fwDbg_isValidMemoryAddr(((TTwd *)hTWD)->hFwDbg,pMemDebug->addr,pMemDebug->length);
}

TI_BOOL TWD_isValidRegAddr (TI_HANDLE hTWD, TFwDebugParams* pMemDebug)
{
	if (hTWD == NULL || pMemDebug == NULL ) {
		return TI_FALSE;
	}


	return fwDbg_isValidRegAddr(((TTwd *)hTWD)->hFwDbg,pMemDebug->addr,pMemDebug->length);
}

/**
 * \brief Set Template Frame
 *
 * \param hTWD 				- TWD module object handle
 * \param pMib      		- Pointer to Input MIB Structure
 * \return TI_OK on success or TI_NOK on failure
 *
 * \par Description
 * Static function
 * Configure/Interrogate/Modulate the Frame Rate if needed (according to Templete Type)
 * and then write the MIB TemplateFrame to the FW
 *
 * \sa
 */
static TI_STATUS TWD_WriteMibTemplateFrame (TI_HANDLE hTWD, TMib* pMib)
{
	TTwd  *pTWD = (TTwd *)hTWD;
	TSetTemplate  tSetTemplate;
	TI_UINT32  uRateMask = RATE_TO_MASK(pMib->aData.TemplateFrame.Rate);

	/*
	 * Construct the template MIB element
	 */
	switch(pMib->aData.TemplateFrame.FrameType) {
	case TEMPLATE_TYPE_BEACON:
		tSetTemplate.type = BEACON_TEMPLATE;
		break;

	case TEMPLATE_TYPE_PROBE_REQUEST:
		tSetTemplate.type = PROBE_REQUEST_TEMPLATE;
		tSetTemplate.eBand = RADIO_BAND_2_4_GHZ; /* needed for GWSI, if so band must also be passed to choose correct template (G or A) */
		break;

	case TEMPLATE_TYPE_NULL_FRAME:
		tSetTemplate.type = NULL_DATA_TEMPLATE;
		MacServices_powerSrv_SetRateModulation (pTWD->hMacServices, (TI_UINT16)uRateMask);
		break;

	case TEMPLATE_TYPE_PROBE_RESPONSE:
		tSetTemplate.type = PROBE_RESPONSE_TEMPLATE;
		break;

	case TEMPLATE_TYPE_QOS_NULL_FRAME:
		tSetTemplate.type = QOS_NULL_DATA_TEMPLATE;
		break;

	case TEMPLATE_TYPE_PS_POLL:
		tSetTemplate.type = PS_POLL_TEMPLATE;
		break;

	default:
		return PARAM_NOT_SUPPORTED;
	}

	tSetTemplate.len = pMib->aData.TemplateFrame.Length;
	tSetTemplate.ptr = (TI_UINT8 *) &(pMib->aData.TemplateFrame.Data);
	tSetTemplate.uRateMask = uRateMask;

	return TWD_CmdTemplate (hTWD, &tSetTemplate, NULL, NULL);
}

/**
 * \brief Set Beacon Filter IE Table
 *
 * \param hTWD 				- TWD module object handle
 * \param pMib      		- Pointer to Input MIB Structure
 * \return TI_OK on success or TI_NOK on failure
 *
 * \par Description
 * Static function
 * Configure the MIB Beacon Filter IE table
 *
 * \sa
 */
static TI_STATUS TWD_WriteMibBeaconFilterIETable (TI_HANDLE hTWD, TMib *pMib)
{
	TI_UINT8 numOf221IE = 0;
	TI_UINT8 i = 0;
	TI_UINT8 IETableLen = 0;
	TI_UINT8 numOfIEs = 0;
	TI_UINT8 *IETable = NULL;

	numOfIEs = pMib->aData.BeaconFilter.iNumberOfIEs;
	IETable = pMib->aData.BeaconFilter.iIETable;
	/* Find the actual IETableLen */
	for (i = 0; i < numOfIEs; i++) {
		if (IETable[IETableLen] == 0xdd) {
			IETableLen += 8;
			numOf221IE++;
		} else {
			IETableLen += 2;
		}
	}


	return TWD_CfgBeaconFilterTable (hTWD, numOfIEs, IETable, IETableLen);
}

/**
 * \brief Set Tx Rate Policy
 *
 * \param hTWD 				- TWD module object handle
 * \param pMib      		- Pointer to Input MIB Structure
 * \return TI_OK on success or TI_NOK on failure
 *
 * \par Description
 * Static function
 * Coordinates between legacy TxRatePolicy implementation and the MIB format:
 * Converts the pGwsi_txRatePolicy back to command builder commands.
 * Activates the TWD_set function.
 *
 * \sa
 */
static TI_STATUS TWD_WriteMibTxRatePolicy (TI_HANDLE hTWD, TMib* pMib)
{
	TTwd   *pTWD = (TTwd *)hTWD;

#ifdef TI_DBG
	if (NULL == pMib) {
		return TI_NOK;
	}
#endif /* TI_DBG */

	return cmdBld_CfgTxRatePolicy (pTWD->hCmdBld, &pMib->aData.txRatePolicy, NULL, NULL);
}

TI_STATUS TWD_WriteMib (TI_HANDLE hTWD, TMib *pMib)
{
	TTwd *pTWD = (TTwd *)hTWD;


	if (NULL == pMib) {
		return PARAM_VALUE_NOT_VALID;
	}

	switch (pMib->aMib) {
	case MIB_dot11MaxReceiveLifetime:
		return cmdBld_CfgRxMsduLifeTime (pTWD->hCmdBld, pMib->aData.MaxReceiveLifeTime * 1024, (void *)NULL, (void *)NULL);

	case MIB_ctsToSelf:
		return cmdBld_CfgCtsProtection (pTWD->hCmdBld, (TI_UINT8)pMib->aData.CTSToSelfEnable, (void *)NULL, (TI_HANDLE)NULL);

	case MIB_dot11GroupAddressesTable: {
		if (NULL == pMib->aData.GroupAddressTable.aGroupTable) {
			return PARAM_VALUE_NOT_VALID;
		}

		return TWD_CfgGroupAddressTable (hTWD,
		                                 pMib->aData.GroupAddressTable.nNumberOfAddresses,
		                                 pMib->aData.GroupAddressTable.aGroupTable,
		                                 pMib->aData.GroupAddressTable.bFilteringEnable);
	}

	case MIB_arpIpAddressesTable: {
		TIpAddr IpAddress;

		IP_COPY (IpAddress, pMib->aData.ArpIpAddressesTable.addr);


		return cmdBld_CfgArpIpAddrTable (pTWD->hCmdBld,
		                                 IpAddress,
		                                 (TI_BOOL)pMib->aData.ArpIpAddressesTable.FilteringEnable,
		                                 IP_VER_4,
		                                 NULL,
		                                 NULL);
	}

	case MIB_templateFrame:
		return TWD_WriteMibTemplateFrame (hTWD, pMib);

	case MIB_beaconFilterIETable:
		return TWD_WriteMibBeaconFilterIETable (hTWD, pMib);

	case MIB_rxFilter: {
		TI_UINT32  uRxFilter = 0;
		TI_UINT8   uMibRxFilter = pMib->aData.RxFilter;

		if (uMibRxFilter & MIB_RX_FILTER_PROMISCOUS_SET) {
			uRxFilter = RX_CFG_ENABLE_ANY_DEST_MAC;
		} else {
			uRxFilter = RX_CFG_ENABLE_ONLY_MY_DEST_MAC;
		}

		if ((uMibRxFilter & MIB_RX_FILTER_BSSID_SET) != 0) {
			uRxFilter = uRxFilter | RX_CFG_ENABLE_ONLY_MY_BSSID;
		} else {
			uRxFilter = uRxFilter | RX_CFG_ENABLE_ANY_BSSID;
		}

		/*
		 * Activates the TWD_setRxFilters function
		 */
		return TWD_CfgRx (hTWD, uRxFilter, RX_FILTER_OPTION_DEF);
	}

	case MIB_txRatePolicy:
		return TWD_WriteMibTxRatePolicy (hTWD, pMib);

	default:

		return TI_NOK;

	} /* switch */
}

TI_STATUS TWD_ReadMib (TI_HANDLE hTWD, TI_HANDLE hCb, void* fCb, void* pCb)
{
	TTwd *pTWD = (TTwd *)hTWD;


	return cmdBld_ReadMib (pTWD->hCmdBld, hCb, fCb, pCb);
}

void TWD_DisableInterrupts(TI_HANDLE hTWD)
{
	TTwd    *pTWD = (TTwd *)hTWD;


	fwEvent_DisableInterrupts(pTWD->hFwEvent);
}

void TWD_EnableInterrupts(TI_HANDLE hTWD)
{
	TTwd    *pTWD = (TTwd *)hTWD;


	fwEvent_EnableInterrupts(pTWD->hFwEvent);
}

TI_UINT32 TWD_TranslateToFwTime (TI_HANDLE hTWD, TI_UINT32 uHostTime)
{
	TTwd    *pTWD = (TTwd *)hTWD;


	return fwEvent_TranslateToFwTime (pTWD->hFwEvent, uHostTime);
}

void TWD_GetTwdHtCapabilities (TI_HANDLE hTWD, TTwdHtCapabilities **pTwdHtCapabilities)
{
	TTwd        *pTWD        = (TTwd *)hTWD;
	TWlanParams *pWlanParams = &DB_WLAN(pTWD->hCmdBld);

	*pTwdHtCapabilities = &(pWlanParams->tTwdHtCapabilities);
}

/**
 *  \brief TWD get FEM type
 *  *
 * \param  Handle        	- handle to object
 * \return uint8
 *
 * \par Description
 * The function return the Front end module that was read frm FW register *
 * \sa
 */
TI_UINT8 TWD_GetFEMType (TI_HANDLE hTWD)
{
	TTwd        *pTWD        = (TTwd *)hTWD;
	IniFileGeneralParam *pGenParams = &DB_GEN(pTWD->hCmdBld);

	return pGenParams->TXBiPFEMManufacturer;

}

/**
 *  \brief TWD end function of read radio state machine
 *  *  *
 * \param  Handle        	- handle to object
 * \return void
 *
 * \par Description
 * The function calling to HwInit call back function, after finish reading FEM registers *
 * \sa
 */
void TWD_FinalizeFEMRead(TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;

	(*pTWD->fInitHwCb) (pTWD->hUser, TI_OK);
}




void TWD_FinalizePolarityRead(TI_HANDLE hTWD)
{
	TTwd *pTWD = (TTwd *)hTWD;
	/*  allways read FEM type from Radio Registers */
	hwInit_ReadRadioParams(pTWD->hHwInit);
}

void TWD_SetInitMask(TI_HANDLE hTWD, TI_BOOL bInit)
{
	TTwd *pTWD = (TTwd *)hTWD;
	if (bInit)
		fwEvent_SetInitMask (pTWD->hFwEvent);
	else
		fwEvent_EnableExternalEvents(pTWD->hFwEvent);
}

