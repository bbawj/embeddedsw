/**************************************************************************************************
* Copyright (C) 2024 Advanced Micro Devices, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
**************************************************************************************************/
/**
 *
 * @file xplmi_get_device_id_client_example.c
 *
 * This file illustrates the board information to the user.
 *
 * To build this application, xilmailbox library must be included in BSP and
 * xilloader library must be in client mode
 *
 * This example is supported for Versal and Versal Net devices.
 *
 * Procedure to run the example.
 * ------------------------------------------------------------------------------------------------
 * Load the Pdi.
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
 * @addtogroup xplmi_client_example_apis XilPlmi Client Example APIs
 * @{
 */

/*************************************** Include Files *******************************************/

#ifdef SDT
#include "xplmi_bsp_config.h"
#endif
#include "xplmi_client.h"
#include "xil_util.h"
#include "xil_cache.h"
#include "xplmi_defs.h"

/************************************ Constant Definitions ***************************************/

/************************************** Type Definitions *****************************************/

/*************************** Macros (Inline Functions) Definitions *******************************/

/************************************ Function Prototypes ****************************************/

static int GetDeviceID(XPlmi_ClientInstance *InstancePtr);

/************************************ Variable Definitions ***************************************/

/*************************************************************************************************/
/**
 * @brief	Main function to call the Load DDR Copy Image example function.
 *
 * @return
 *			 - XST_SUCCESS on success.
 *			 - Error code on failure.
 *
 *************************************************************************************************/
int main(void)
{
	int Status = XST_FAILURE;
	XMailbox MailboxInstance;
	XPlmi_ClientInstance PlmiClientInstance;

	xil_printf("\r\nGet DeviceID client example for Versal");

	#ifdef XPLMI_CACHE_DISABLE
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

	Status = XPlmi_ClientInit(&PlmiClientInstance, &MailboxInstance);
	if (Status != XST_SUCCESS) {
		goto END;
	}

    Status = GetDeviceID(&PlmiClientInstance);

END:
	if (Status == XST_SUCCESS) {
		xil_printf("\r\nSuccessfully ran Get DeviceID client example....");
	}
	else {
		xil_printf("\r\nGet DeviceID failed with error code = %08x", Status);
	}

	return Status;
}
/*************************************************************************************************/
/**
 * @brief    This function  will initialize IPI interface for Get device id
 *
 * @param	InstancePtr is a pointer to the XPlmi_ClientInstance instance to be worked on.
 *
 * @return
 *			 - XST_SUCCESS on success
 *			 - XST_FAILURE on failure
 *
 *************************************************************************************************/
static int GetDeviceID(XPlmi_ClientInstance *InstancePtr)
{
	int Status = XST_FAILURE;
	XLoader_DeviceIdCode DeviceIdCode;

	Status = XPlmi_GetDeviceID(InstancePtr, &DeviceIdCode);
	if (Status != XST_SUCCESS) {
		goto END;
	}

	xil_printf("\r\nIDCODE = %x", DeviceIdCode.IdCode);
	xil_printf("\r\nExtIdCode = %x", DeviceIdCode.ExtIdCode);

END:
	return Status;
}