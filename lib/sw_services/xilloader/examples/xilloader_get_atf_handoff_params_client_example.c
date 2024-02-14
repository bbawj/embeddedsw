/**************************************************************************************************
* Copyright (C) 2024 Advanced Micro Devices, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
**************************************************************************************************/

/*************************************************************************************************/
/**
 *
 * @file xilloader_get_atf_handoff_params_client_example.c
 *
 * This file illustrates ATF handoff parameters information.
 *
 * To build this application, xilmailbox library must be included in BSP and
 * xilloader library must be in client mode
 *
 * This example is supported for Versal and Versal Net devices.
 *
 * Procedure to run the example.
 * ------------------------------------------------------------------------------------------------
 * Load the Pdi.
 * Download the partial pdi into ddr.
 * Select the target.
 * Download the example elf into the target.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- ----------------------------------------------------------------------------
 * 1.00  dd   02/13/24 Initial release
 *
 * </pre>
 *
 *************************************************************************************************/

/**
 * @addtogroup xloader_client_example_apis XilLoader Client Example APIs
 * @{
 */

/*************************************** Include Files *******************************************/

#ifdef SDT
#include "xloader_bsp_config.h"
#endif
#include "xloader_client.h"
#include "xil_util.h"
#include "xil_cache.h"
#include "xloader_defs.h"

/************************************ Constant Definitions ***************************************/

/* - Example defines below, update with required values*/
#define     BUFFER_ADDR             (0x1000000U) /**< Buffer lower address */
#define		BUF_TOTAL_SIZE			(0x8) /**< Handoff params size */

/************************************** Type Definitions *****************************************/

/*************************** Macros (Inline Functions) Definitions *******************************/

/************************************ Function Prototypes ****************************************/

static int GetATFHandOffParams(XLoader_ClientInstance *InstancePtr);

/************************************ Variable Definitions ***************************************/

static u32 Buffer[BUF_TOTAL_SIZE]__attribute__ ((aligned(16U)));

/*************************************************************************************************/
/**
 * @brief    Main function to call the get ATF handoff params example function.
 *
 * @return
 *            - XST_SUCCESS on success.
 *            - Error code on failure.
 *
 *************************************************************************************************/
int main(void)
{
    int Status = XST_FAILURE;
    XMailbox MailboxInstance;
    XLoader_ClientInstance LoaderClientInstance;

    xil_printf("\r\nGet ATF HandOff Params client example");

    #ifdef XLOADER_CACHE_DISABLE
        Xil_DCacheDisable();
    #endif

    #ifndef SDT
    Status = XMailbox_Initialize(&MailboxInstance, 0U);
    #else
    Status = XMailbox_Initialize(&MailboxInstance, XPAR_XIPIPSU_0_BASEADDR);
    #endif
    if (Status != XST_SUCCESS) {
        goto END;
    }

    Status = XLoader_ClientInit(&LoaderClientInstance, &MailboxInstance);
    if (Status != XST_SUCCESS) {
        goto END;
    }

    Status = GetATFHandOffParams(&LoaderClientInstance);

END:
    if (Status == XST_SUCCESS) {
        xil_printf("\r\nSuccessfully ran GetATFHandOffParams example....");
    }
    else {
        xil_printf("\r\nGetATFHandOffParams example failed with error code = %08x", Status);
    }

    return Status;
}

/*************************************************************************************************/
/**
 * @brief    This function  gives the handoff parameter information for the current subsystem.
 *
 * @param	InstancePtr is a pointer to the XLoader_ClientInstance instance to be worked on.
 *
 * @return
 *           - XST_SUCCESS on success.
 *           - XLOADER_ERR_INVALID_HANDOFF_PARAM_DEST_ADDR on Invalid destination
 *           address.
 *           - XLOADER_ERR_INVALID_HANDOFF_PARAM_DEST_SIZE on Invalid destination
 *           size.
 *
 *************************************************************************************************/
static int GetATFHandOffParams(XLoader_ClientInstance *InstancePtr)
{
    int Status = XST_FAILURE;
    u32 BufferSize = 0;

    Status = XLoader_GetATFHandOffParams(InstancePtr, (u64)&Buffer[0U], sizeof(Buffer),
                &BufferSize);
    if (Status != XST_SUCCESS) {
        goto END;
    }

    #ifndef	XLOADER_CACHE_DISABLE
    Xil_DCacheInvalidateRange((UINTPTR)Buffer, sizeof(Buffer));
    #endif

    xil_printf("\r\nBuffer Size = %x", BufferSize);

END:
    return Status;
}