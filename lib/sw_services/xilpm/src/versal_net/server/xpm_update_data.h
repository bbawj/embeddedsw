/******************************************************************************
 * Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef XPM_UPDATE_DATA_H_
#define XPM_UPDATE_DATA_H_
#include "xpm_debug.h"
#include "xpm_common.h"
#ifdef __cplusplus
extern "C" {
#endif
/****************** Data struct Version **************/
#define XPM_DATA_STRUCT_VERSION 0x1
#define XPM_DATA_STRUCT_LCVERSION 0x1
/****************** Data struct IDs **************/
#define XPM_BYTEBUFFER_DS_ID		1U
#define XPM_BYTEBUFFER_ADDR_DS_ID 	2U
#define XPM_ALLNODES_DS_ID 		3U
#define XPM_NUMNODES_DS_ID 		4U

#ifdef __cplusplus
}
#endif

#endif /* XPM_UPDATE_DATA_H_ */