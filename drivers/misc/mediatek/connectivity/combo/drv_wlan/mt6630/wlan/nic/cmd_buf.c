/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/cmd_buf.c#1 $
*/

/*! \file   "cmd_buf.c"
    \brief  This file contain the management function of internal Command Buffer
	    for CMD_INFO_T.

	We'll convert the OID into Command Packet and then send to FW. Thus we need
    to copy the OID information to Command Buffer for following reasons.
    1. The data structure of OID information may not equal to the data structure of
       Command, we cannot use the OID buffer directly.
    2. If the Command was not generated by driver we also need a place to store the
       information.
    3. Because the CMD is NOT FIFO when doing memory allocation (CMD will be generated
       from OID or interrupt handler), thus we'll use the Block style of Memory Allocation
       here.
*/



/*
** $Log: cmd_buf.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 18 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Provide cnmMgtPktAlloc() and alloc/free function of msg/buf
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 02 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. clear prPendingCmdInfo properly
 *  * 2. while allocating memory for cmdinfo, no need to add extra 4 bytes.
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-10-13 21:59:08 GMT mtk01084
**  remove un-neceasary spaces
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-05-20 12:24:26 GMT mtk01461
**  Increase CMD Buffer - HIF_RX_HW_APPENDED_LEN when doing CMD_INFO_T allocation
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-04-21 09:41:08 GMT mtk01461
**  Add init of Driver Domain MCR flag and fix lint MTK WARN
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-17 19:51:45 GMT mtk01461
**  allocation function of CMD_INFO_T
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to initial the MGMT memory pool for CMD Packet.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cmdBufInitialize(IN P_ADAPTER_T prAdapter)
{
	P_CMD_INFO_T prCmdInfo;
	UINT_32 i;

	ASSERT(prAdapter);

	QUEUE_INITIALIZE(&prAdapter->rFreeCmdList);

	for (i = 0; i < CFG_TX_MAX_CMD_PKT_NUM; i++) {
		prCmdInfo = &prAdapter->arHifCmdDesc[i];
		QUEUE_INSERT_TAIL(&prAdapter->rFreeCmdList, &prCmdInfo->rQueEntry);
	}

}				/* end of cmdBufInitialize() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Allocate CMD_INFO_T from a free list and MGMT memory pool.
*
* @param[in] prAdapter      Pointer to the Adapter structure.
* @param[in] u4Length       Length of the frame buffer to allocate.
*
* @retval NULL      Pointer to the valid CMD Packet handler
* @retval !NULL     Fail to allocat CMD Packet
*/
/*----------------------------------------------------------------------------*/
P_CMD_INFO_T cmdBufAllocateCmdInfo(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length)
{
	P_CMD_INFO_T prCmdInfo;
	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("cmdBufAllocateCmdInfo");


	ASSERT(prAdapter);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
	QUEUE_REMOVE_HEAD(&prAdapter->rFreeCmdList, prCmdInfo, P_CMD_INFO_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);

	if (prCmdInfo) {
		/* Setup initial value in CMD_INFO_T */
		/* Start address of allocated memory */
		prCmdInfo->pucInfoBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4Length);

		if (prCmdInfo->pucInfoBuffer == NULL) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
			QUEUE_INSERT_TAIL(&prAdapter->rFreeCmdList, &prCmdInfo->rQueEntry);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);

			prCmdInfo = NULL;
		} else {
			prCmdInfo->u2InfoBufLen = 0;
			prCmdInfo->fgIsOid = FALSE;
			prCmdInfo->fgDriverDomainMCR = FALSE;
		}
	}

	if (prCmdInfo) {
		DBGLOG(MEM, INFO,
		       ("CMD[0x%p] allocated! LEN[%04u], Rest[%u]\n", prCmdInfo, u4Length,
			prAdapter->rFreeCmdList.u4NumElem));
	} else {
		DBGLOG(MEM, INFO,
		       ("CMD allocation failed! LEN[%04u], Rest[%u]\n", u4Length,
			prAdapter->rFreeCmdList.u4NumElem));
	}

	return prCmdInfo;

}				/* end of cmdBufAllocateCmdInfo() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to free the CMD Packet to the MGMT memory pool.
*
* @param prAdapter  Pointer to the Adapter structure.
* @param prCmdInfo  CMD Packet handler
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cmdBufFreeCmdInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("cmdBufFreeCmdInfo");

	ASSERT(prAdapter);

	if (prCmdInfo) {
		if (prCmdInfo->pucInfoBuffer) {
			cnmMemFree(prAdapter, prCmdInfo->pucInfoBuffer);
			prCmdInfo->pucInfoBuffer = NULL;
		}

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
		QUEUE_INSERT_TAIL(&prAdapter->rFreeCmdList, &prCmdInfo->rQueEntry);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
	}

	if (prCmdInfo) {
		DBGLOG(MEM, INFO,
		       ("CMD[0x%p] freed! Rest[%u]\n", prCmdInfo,
			prAdapter->rFreeCmdList.u4NumElem));
	}

	return;

}				/* end of cmdBufFreeCmdPacket() */
