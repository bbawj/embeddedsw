/******************************************************************************
* Copyright (c) 2022 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2023 Advanced Micro Devices, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
*******************************************************************************/

/*****************************************************************************/
/**
*
* @file xcert_genx509cert.c
*
* This file contains the implementation of the interface functions for creating
* X.509 certificate for DevIK and DevAK public key.
*
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date       Changes
* ----- ---- ---------- -------------------------------------------------------
* 1.0   har  01/09/2023 Initial release
*
* </pre>
* @note
*
******************************************************************************/

/***************************** Include Files *********************************/
#include "xparameters.h"

#include "xil_util.h"
#ifndef PLM_ECDSA_EXCLUDE
#include "xsecure_elliptic.h"
#include "xsecure_ellipticplat.h"
#endif
#include "xsecure_sha384.h"
#include "xsecure_utils.h"
#include "xcert_genx509cert.h"
#include "xcert_createfield.h"
#include "xplmi.h"
#include "xplmi_plat.h"
#include "xplmi_status.h"
#include "xplmi_tamper.h"
#include "xsecure_plat_kat.h"

/************************** Constant Definitions *****************************/
/** @name Macros for OIDs
 * @{
 */
 /**< Object IDs used in X.509 Certificate and Certificate Signing Request */
#define XCERT_OID_SIGN_ALGO				"06082A8648CE3D040303"
#define XCERT_OID_EC_PUBLIC_KEY				"06072A8648CE3D0201"
#define XCERT_OID_P384					"06052B81040022"
#define XCERT_OID_SUB_KEY_IDENTIFIER			"0603551D0E"
#define XCERT_OID_AUTH_KEY_IDENTIFIER			"0603551D23"
#define XCERT_OID_TCB_INFO_EXTN				"0606678105050401"
#define XCERT_OID_UEID_EXTN				"0606678105050404"
#define XCERT_OID_KEY_USAGE_EXTN			"0603551D0F"
#define XCERT_OID_EKU_EXTN				"0603551D25"
#define XCERT_OID_EKU_CLIENT_AUTH			"06082B06010505070302"
#define XCERT_OID_SHA3_384				"0609608648016503040209"
/** @} */

#define XCERT_SERIAL_FIELD_LEN				(22U)
			/**< Length of Serial Field */
#define XCERT_BIT7_MASK 				(0x80U)
			/**< Mask to get bit 7*/
#define XCERT_LOWER_NIBBLE_MASK				(0xFU)
			/**< Mask to get lower nibble */
#define XCERT_SIGN_AVAILABLE				(0x3U)
			/**< Signature available in SignStore */
#define XCERT_BYTE_MASK					(0xFFU)
			/**< Mask to get byte */
#define XCERT_MAX_CERT_SUPPORT				(4U)
	/**< Number of supported certificates is 4 -> 1 DevIK certificate and 3 DevAK certificates */
#define XCERT_SUB_KEY_ID_VAL_LEN			(20U)
			/**< Length of value of Subject Key ID */
#define XCERT_AUTH_KEY_ID_VAL_LEN			(20U)
			/**< Length of value of Authority Key ID */
#define XCERT_MAX_LEN_OF_KEYUSAGE_VAL			(2U)
			/**< Maximum length of value of key usage */

#define XCERT_WORD_LEN					(0x04U)
			/**< Length of word in bytes */
#define XCERT_LEN_OF_BYTE_IN_BITS			(8U)
			/**< Length of byte in bits */
#define XCERT_AUTH_KEY_ID_OPTIONAL_PARAM		(0x80U)
		/**< Optional parameter in Authority Key Identifier field*/

/** @name Optional parameter tags
 * @{
 */
 /**< Tags for optional parameters */
#define XCERT_OPTIONAL_PARAM_0_TAG			(0xA0U)
#define XCERT_OPTIONAL_PARAM_3_TAG			(0xA3U)
#define XCERT_OPTIONAL_PARAM_6_TAG			(0xA6U)
/** @} */

/** @name DNA
 * @{
 */
 /**< Macros related to Address of DNA and length of DNA in words and bytes */
#define XCERT_DNA_0_ADDRESS				(0xF1250020U)
#define XCERT_DNA_LEN_IN_WORDS				(4U)
#define XCERT_DNA_LEN_IN_BYTES				(XCERT_DNA_LEN_IN_WORDS * XCERT_WORD_LEN)
/** @} */

#define XCert_In32					(XPlmi_In32)
			/**< Alias of XPlmi_In32 to be used in XilCert*/

/************************** Type Definitions ******************************/
typedef enum {
	XCERT_DIGITALSIGNATURE,	/**< Digital Signature */
	XCERT_NONREPUDIATION,	/**< Non Repudiation */
	XCERT_KEYENCIPHERMENT,	/**< Key Encipherment */
	XCERT_DATAENCIPHERMENT,	/**< Data Encipherment */
	XCERT_KEYAGREEMENT,	/**< Key Agreeement */
	XCERT_KEYCERTSIGN,	/**< Key Certificate Sign */
	XCERT_CRLSIGN,		/**< CRL Sign */
	XCERT_ENCIPHERONLY,	/**< Encipher Only */
	XCERT_DECIPHERONLY	/**< Decipher Only */
} XCert_KeyUsageOption;

/************************** Function Prototypes ******************************/
static XCert_InfoStore *XCert_GetCertDB(void);
static u32* XCert_GetNumOfEntriesInUserCfgDB(void);
static int XCert_IsBufferNonZero(u8* Buffer, int BufferLen);
static int XCert_GetUserCfg(u32 SubsystemId, XCert_UserCfg **UserCfg);
static void XCert_GenVersionField(u8* TBSCertBuf, u32 *VersionLen);
static void XCert_GenSerialField(u8* TBSCertBuf, u8* Serial, u32 *SerialLen);
static int XCert_GenSignAlgoField(u8* CertBuf, u32 *SignAlgoLen);
static void XCert_GenIssuerField(u8* TBSCertBuf, u8* Issuer, const u32 IssuerValLen, u32 *IssuerLen);
static void XCert_GenValidityField(u8* TBSCertBuf, u8* Validity, const u32 ValidityValLen, u32 *ValidityLen);
static void XCert_GenSubjectField(u8* TBSCertBuf, u8* Subject, const u32 SubjectValLen, u32 *SubjectLen);
#ifndef PLM_ECDSA_EXCLUDE
static int XCert_GenPubKeyAlgIdentifierField(u8* TBSCertBuf, u32 *Len);
static int XCert_GenPublicKeyInfoField(u8* TBSCertBuf, u8* SubjectPublicKey,u32 *PubKeyInfoLen);
static void XCert_GenSignField(u8* X509CertBuf, u8* Signature, u32 *SignLen);
static int XCert_GetSignStored(u32 SubsystemId, XCert_SignStore **SignStore);
#endif
static int XCert_GenTBSCertificate(u8* X509CertBuf, XCert_Config* Cfg, u32 *DataLen);
static void XCert_GetData(const u32 Size, const u8 *Src, const u64 DstAddr);
static int XCert_GenSubjectKeyIdentifierField(u8* TBSCertBuf, u8* SubjectPublicKey, u32 *SubjectKeyIdentifierLen);
static int XCert_GenAuthorityKeyIdentifierField(u8* TBSCertBuf, u8* IssuerPublicKey, u32 *AuthorityKeyIdentifierLen);
static int XCert_GenTcbInfoExtnField(u8* TBSCertBuf, XCert_Config* Cfg, u32 *TcbInfoExtnLen);
static int XCert_GenUeidExnField(u8* TBSCertBuf, u32 *UeidExnLen);
static void XCert_UpdateKeyUsageVal(u8* KeyUsageVal, XCert_KeyUsageOption KeyUsageOption);
static int XCert_GenKeyUsageField(u8* TBSCertBuf, XCert_Config* Cfg, u32 *KeyUsageExtnLen);
static int XCert_GenExtKeyUsageField(u8* TBSCertBuf,  XCert_Config* Cfg, u32 *EkuLen);
static int XCert_GenX509v3ExtensionsField(u8* TBSCertBuf,  XCert_Config* Cfg, u32 *ExtensionsLen);

/************************** Function Definitions *****************************/
/*****************************************************************************/
/**
 * @brief	This function provides the pointer to the X509 InfoStore database.
 *
 * @return
 *	-	Pointer to the first entry in InfoStore database
 *
 * @note	InfoStore DB is used to store the user configurable fields of
 * 		X.509 certificate, hash and signature of the TBS Ceritificate
 * 		for different subsystems.
 *		Each entry in the DB wil have following fields:
 *		- Subsystem Id
 *		- Issuer
 *		- Subject
 *		- Validity
 *		- Signature
 *		- Hash
 *		- IsSignAvailable
 *
 ******************************************************************************/
static XCert_InfoStore *XCert_GetCertDB(void)
{
	static XCert_InfoStore CertDB[XCERT_MAX_CERT_SUPPORT] = {0U};

	return &CertDB[0];
}

/*****************************************************************************/
/**
 * @brief	This function provides the pointer to the NumOfEntriesInUserCfgDB
*		which indicates the total number of subsystems for which
*		user configuration is stored in UsrCfgDB.
 *
 * @return
 *	-	Pointer to the NumOfEntriesInUserCfgDB
 *
 ******************************************************************************/
static u32* XCert_GetNumOfEntriesInUserCfgDB(void)
{
	static u32 NumOfEntriesInUserCfgDB = 0U;

	return &NumOfEntriesInUserCfgDB;
}

/*****************************************************************************/
/**
 * @brief	This function checks if all the bytes in the provided buffer are
 *		zero.
 *
 * @param	Buffer - Pointer to the buffer
 * @param	BufferLen - Length of the buffer
 *
 * @return
 *		XST_SUCCESS - If buffer is non-empty
 *		XST_FAILURE - If buffer is empty
 *
 ******************************************************************************/
static int XCert_IsBufferNonZero(u8* Buffer, int BufferLen)
{
	int Status = XST_FAILURE;
	int Sum = 0;

	for (int i = 0; i < BufferLen; i++) {
		Sum |= Buffer[i];
	}

	if (Sum == 0) {
		Status = XST_FAILURE;
		goto END;
	}

	Status = XST_SUCCESS;

END:
	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function finds the provided Subsystem ID in UserCfg DB and
 *		returns the pointer to the corresponding entry in DB
 *		if all the other fields are valid.
 *
 * @param	SubsystemID - Subsystem ID for which user configuration is requested
 * @param	UserCfg - Pointer to the entry in DB for the provided Subsystem ID
 *
 * @return
 *		XST_SUCCESS - If subsystem ID is found and other fields are valid
 *		Error Code - Upon any failure
 *
 ******************************************************************************/
static int XCert_GetUserCfg(u32 SubsystemId, XCert_UserCfg **UserCfg)
{
	int Status = XST_FAILURE;
	XCert_InfoStore *CertDB = XCert_GetCertDB();
	u32 *NumOfEntriesInUserCfgDB = XCert_GetNumOfEntriesInUserCfgDB();
	u32 Idx;

	/**
	 * Search for given Subsystem ID in the UserCfg DB
	 */
	for (Idx = 0; Idx < *NumOfEntriesInUserCfgDB; Idx++) {
		if (CertDB[Idx].SubsystemId == SubsystemId) {
			/**
			 * If Subsystem ID is found then check that Subject,
			 * Issuer and Validity for that Subsystem ID is non-zero.
			 */
			Status = XCert_IsBufferNonZero(CertDB[Idx].UserCfg.Issuer,
				CertDB[Idx].UserCfg.IssuerLen);
			if (Status != XST_SUCCESS) {
				Status = XOCP_ERR_X509_INVALID_USER_CFG;
				goto END;
			}

			Status = XCert_IsBufferNonZero(CertDB[Idx].UserCfg.Subject,
				CertDB[Idx].UserCfg.SubjectLen);
			if (Status != XST_SUCCESS) {
				Status = XOCP_ERR_X509_INVALID_USER_CFG;
				goto END;
			}

			Status = XCert_IsBufferNonZero(CertDB[Idx].UserCfg.Validity,
				CertDB[Idx].UserCfg.ValidityLen);
			if (Status != XST_SUCCESS) {
				Status = XOCP_ERR_X509_INVALID_USER_CFG;
				goto END;
			}
			*UserCfg = &CertDB[Idx].UserCfg;
			Status = XST_SUCCESS;
			goto END;
		}
	}

	if (Idx == *NumOfEntriesInUserCfgDB) {
		Status = XOCP_ERR_X509_USR_CFG_NOT_FOUND;
		goto END;
	}

END:
	return Status;
}

#ifndef PLM_ECDSA_EXCLUDE
/*****************************************************************************/
/**
 * @brief	This function finds the provided Subsystem ID in InfoStore DB and
 *		returns the pointer to the corresponding sign entry in DB.
 *
 * @param	SubsystemId - SubsystemId for which stored signature is requested
 * @param	SignStore - Pointer to the entry in DB for the provided Subsystem ID
 *
 * @return
 *		XST_SUCCESS - If subsystem ID is found
 *		Error Code - Upon any failure
 *
 ******************************************************************************/
static int XCert_GetSignStored(u32 SubsystemId, XCert_SignStore **SignStore)
{
	int Status = XST_FAILURE;
	XCert_InfoStore *CertDB = XCert_GetCertDB();
	u32 *NumOfEntriesInCertDB = XCert_GetNumOfEntriesInUserCfgDB();
	u32 Idx;

	/**
	 * Search for given Subsystem ID in the CertDB
	 */
	for (Idx = 0; Idx < *NumOfEntriesInCertDB; Idx++) {
		if (CertDB[Idx].SubsystemId == SubsystemId) {
			*SignStore = &CertDB[Idx].SignStore;
			Status = XST_SUCCESS;
			goto END;
		}
	}

END:
	return Status;
}
#endif

/*****************************************************************************/
/**
 * @brief	This function stores the user provided value for the user configurable
 *		fields in the certificate as per the provided FieldType.
 *
 * @param	SubSystemId is the id of subsystem for which field data is provided
 * @param	FieldType is to identify the field for which input is provided
 * @param	Val is the value of the field provided by the user
 * @param	Len is the length of the value in bytes
 *
 * @return
 *		- XST_SUCCESS - If whole operation is success
 *		- XST_FAILURE - Upon any failure
 *
 ******************************************************************************/
int XCert_StoreCertUserInput(u32 SubSystemId, XCert_UserCfgFields FieldType, u8* Val, u32 Len)
{
	int Status = XST_FAILURE;
	u32 IdxToBeUpdated;
	u8 IsSubsystemIdPresent = FALSE;
	XCert_InfoStore *CertDB = XCert_GetCertDB();
	u32 *NumOfEntriesInUserCfgDB = XCert_GetNumOfEntriesInUserCfgDB();
	u32 Idx;

	if (FieldType > XCERT_VALIDITY) {
		Status = XST_INVALID_PARAM;
		goto END;
	}

	if (((FieldType == XCERT_VALIDITY) && (Len > XCERT_VALIDITY_MAX_SIZE)) ||
		((FieldType == XCERT_ISSUER) && (Len > XCERT_ISSUER_MAX_SIZE)) ||
		((FieldType == XCERT_SUBJECT) && (Len > XCERT_SUBJECT_MAX_SIZE))) {
		Status = XST_INVALID_PARAM;
		goto END;
	}

	/**
	 * Look for the Subsystem Id. If it is there get the index and update the
	 * field of existing subsystem else increment index and add entry for
	 * new subsystem.
	*/
	for (Idx = 0; Idx < *NumOfEntriesInUserCfgDB; Idx++) {
		if (CertDB[Idx].SubsystemId == SubSystemId) {
			IdxToBeUpdated = Idx;
			IsSubsystemIdPresent = TRUE;
			break;
		}
	}

	if (IsSubsystemIdPresent == FALSE) {
		IdxToBeUpdated = *NumOfEntriesInUserCfgDB;
		if (IdxToBeUpdated >= XCERT_MAX_CERT_SUPPORT) {
			Status = XOCP_ERR_X509_USER_CFG_STORE_LIMIT_CROSSED;
			goto END;
		}
		CertDB[IdxToBeUpdated].SubsystemId = SubSystemId;
		*NumOfEntriesInUserCfgDB = (*NumOfEntriesInUserCfgDB) + 1;
	}

	if (FieldType == XCERT_ISSUER) {
		XSecure_MemCpy(CertDB[IdxToBeUpdated].UserCfg.Issuer, Val, Len);
		CertDB[IdxToBeUpdated].UserCfg.IssuerLen = Len;
	}
	else if (FieldType == XCERT_SUBJECT) {
		XSecure_MemCpy(CertDB[IdxToBeUpdated].UserCfg.Subject, Val, Len);
		CertDB[IdxToBeUpdated].UserCfg.SubjectLen = Len;
	}
	else {
		XSecure_MemCpy(CertDB[IdxToBeUpdated].UserCfg.Validity, Val, Len);
		CertDB[IdxToBeUpdated].UserCfg.ValidityLen = Len;
	}

	Status = XST_SUCCESS;
END:
	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function creates the X.509 Certificate.
 *
 * @param	X509CertBuf is the pointer to the X.509 Certificate buffer
 * @param	MaxCertSize is the maximum size of the X.509 Certificate buffer
 * @param	X509CertSize is the size of X.509 Certificate in bytes
 * @param	Cfg is structure which includes configuration for the X.509 Certificate.
 *
 * @note	Certificate  ::=  SEQUENCE  {
 *			tbsCertificate       TBSCertificate,
 *			signatureAlgorithm   AlgorithmIdentifier,
 *			signatureValue       BIT STRING  }
 *
 ******************************************************************************/
int XCert_GenerateX509Cert(u64 X509CertAddr, u32 MaxCertSize, u32* X509CertSize, XCert_Config *Cfg)
{
	int Status = XST_FAILURE;
	u8 X509CertBuf[1024];
	u8* Start = X509CertBuf;
	u8* Curr = Start;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u32 TBSCertLen = 0U;
	u32 SignAlgoLen;
#ifndef PLM_ECDSA_EXCLUDE
	int HashCmpStatus = XST_FAILURE;
	u32 SignLen;
	u8 Sign[XSECURE_ECC_P384_SIZE_IN_BYTES * 2U] = {0U};
	u8 SignTmp[XSECURE_ECC_P384_SIZE_IN_BYTES * 2U] = {0U};
	u8 Hash[XCERT_HASH_SIZE_IN_BYTES] = {0U};
	XCert_SignStore *SignStore;
#endif
	u8 HashTmp[XCERT_HASH_SIZE_IN_BYTES] = {0U};
	u8 *TbsCertStart;
	(void)MaxCertSize;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_GetUserCfg(Cfg->SubSystemId, &(Cfg->UserCfg));
	if (Status != XST_SUCCESS) {
		goto END;
	}

	TbsCertStart = Curr;

	/**
	 * Generate TBS certificate field
	 */
	Status = XCert_GenTBSCertificate(Curr, Cfg, &TBSCertLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + TBSCertLen;
	}

	/**
	 * Generate Sign Algorithm field
	 */
	Status = XCert_GenSignAlgoField(Curr, &SignAlgoLen);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_GEN_SIGN_ALGO_FIELD;
		goto END;
	}
	else {
		Curr = Curr + SignAlgoLen;
	}

	/**
	 * Calcualte SHA2 Digest of the TBS certificate
	 */
	Status = XSecure_Sha384Digest(TbsCertStart, TBSCertLen, HashTmp);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_GEN_TBSCERT_DIGEST;
		goto END;
	}
#ifndef PLM_ECDSA_EXCLUDE

	/**
	 * Get the TBS certificate signature stored in Cert DB
	 */
	Status = XCert_GetSignStored(Cfg->SubSystemId, &SignStore);
	if (Status != XST_SUCCESS) {
		Status = (int)XOCP_ERR_X509_GET_SIGN;
		goto END;
	}
	/**
	 * If the Signature is available, compare the hash stored with
	 * the hash calculated to make sure nothing is changed.
	 * if hash matches, copy the stored signature else generate
	 * the signature again.
	 */
	if (SignStore->IsSignAvailable == XCERT_SIGN_AVAILABLE) {
		HashCmpStatus = Xil_SMemCmp((void *)HashTmp, sizeof(HashTmp),
				(void *)SignStore->Hash, sizeof(SignStore->Hash),
				sizeof(HashTmp));
		if (HashCmpStatus == XST_SUCCESS) {
			Status = Xil_SMemCpy((void *)Sign, sizeof(Sign),
				(void *)SignStore->Sign, sizeof(SignStore->Sign),
				sizeof(Sign));
			if (Status != XST_SUCCESS) {
				goto END;
			}
		}
	}

	/**
	 * If the Signature is not available or Hash comparision from above fails,
	 * regenerate the signature.
	 */
	if ((SignStore->IsSignAvailable != XCERT_SIGN_AVAILABLE) || (HashCmpStatus != XST_SUCCESS)) {

		XSecure_FixEndiannessNCopy(XSECURE_ECC_P384_SIZE_IN_BYTES, (u64)(UINTPTR)Hash,
					(u64)(UINTPTR)HashTmp);
		/**
		 * Calculate signature of the TBS certificate using the private key
		 */
		Status = XSecure_EllipticGenEphemeralNSign(XSECURE_ECC_NIST_P384, (const u8 *)Hash, sizeof(Hash),
				Cfg->AppCfg.IssuerPrvtKey, SignTmp);
		if (Status != XST_SUCCESS) {
			Status = XOCP_ERR_X509_CALC_SIGN;
			goto END;
		}
		XSecure_FixEndiannessNCopy(XSECURE_ECC_P384_SIZE_IN_BYTES,
				(u64)(UINTPTR)Sign, (u64)(UINTPTR)SignTmp);
		XSecure_FixEndiannessNCopy(XSECURE_ECC_P384_SIZE_IN_BYTES,
				(u64)(UINTPTR)(Sign + XSECURE_ECC_P384_SIZE_IN_BYTES),
				(u64)(UINTPTR)(SignTmp + XSECURE_ECC_P384_SIZE_IN_BYTES));
		/**
		 * Copy the generated signature and hash into SignStore
		 */
		Status = Xil_SMemCpy(SignStore->Hash, sizeof(SignStore->Hash),
				HashTmp, sizeof(HashTmp), sizeof(HashTmp));
		if (Status != XST_SUCCESS) {
			goto END;
		}
		Status = Xil_SMemCpy(SignStore->Sign, sizeof(SignStore->Sign),
				Sign, sizeof(Sign), sizeof(Sign));
		if (Status != XST_SUCCESS) {
			goto END;
		}
		SignStore->IsSignAvailable = XCERT_SIGN_AVAILABLE;
	}
	/**
	 * Generate Signature field
	 */
	XCert_GenSignField(Curr, Sign, &SignLen);
	Curr = Curr + SignLen;
#else
	Status = XOCP_ECDSA_NOT_ENABLED_ERR;
	goto END;
#endif

	/**
	 * Update the encoded length in the X.509 certificate SEQUENCE
	 */
	Status = XCert_UpdateEncodedLength(SequenceLenIdx, Curr - SequenceValIdx, SequenceValIdx);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_UPDATE_ENCODED_LEN;
		goto END;
	}
	Curr = Curr + ((*SequenceLenIdx) & XCERT_LOWER_NIBBLE_MASK);
	*X509CertSize = Curr - Start;

	XCert_GetData(*X509CertSize, (u8 *)X509CertBuf, X509CertAddr);

END:
	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function creates the Version field of the TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Version field shall be added.
 * @param	VersionLen is the length of the Version field
 *
 * @note	Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 *		This field describes the version of the encoded certificate.
 *		XilCert library supports X.509 V3 certificates.
 *
 ******************************************************************************/
static void XCert_GenVersionField(u8* TBSCertBuf, u32 *VersionLen)
{
	u8* Curr = TBSCertBuf;

	*(Curr++) = XCERT_ASN1_TAG_INTEGER;
	*(Curr++) = XCERT_LEN_OF_VALUE_OF_VERSION;
	*(Curr++) = XCERT_VERSION_VALUE_V3;

	*VersionLen = Curr - TBSCertBuf;
}

/*****************************************************************************/
/**
 * @brief	This function creates the Serial field of the TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Serial field shall be added.
 * @param	SerialLen is the length of the Serial field
 *
 * @note	CertificateSerialNumber  ::=  INTEGER
 *		The length of the serial must not be more than 20 bytes.
 *		The value of the serial is determined by calculating the
 *		SHA2 hash of the fields in the TBS Certificate except the Version
 *		and Serial Number fields. 20 bytes from LSB of the calculated
 *		hash is updated as the Serial Number
 *
 ******************************************************************************/
static void XCert_GenSerialField(u8* TBSCertBuf, u8* DataHash, u32 *SerialLen)
{
	u8 Serial[XCERT_LEN_OF_VALUE_OF_SERIAL] = {0U};
	u32 LenToBeCopied;

	/**
	 * The value of serial field must be 20 bytes. If the most significant
	 * bit in the first byte of Serial is set, then the value shall be
	 * prepended with 0x00 after DER encoding.
	 * So if MSB is set then 00 followed by 19 bytes of hash wil be the serial
	 * value. If not set then 20 bytes of hash will be used as serial value.
	 */
	if ((*DataHash & XCERT_BIT7_MASK) == XCERT_BIT7_MASK) {
		LenToBeCopied = XCERT_LEN_OF_VALUE_OF_SERIAL - 1U;
	}
	else {
		LenToBeCopied = XCERT_LEN_OF_VALUE_OF_SERIAL;
	}

	XSecure_MemCpy64((u64)(UINTPTR)Serial, (u64)(UINTPTR)DataHash,
			LenToBeCopied);

	XCert_CreateInteger(TBSCertBuf, Serial, LenToBeCopied, SerialLen);
}

/*****************************************************************************/
/**
 * @brief	This function creates the Signature Algorithm field.
 *		This field in present in TBS Certificate as well as the
 *		X509 certificate.
 *
 * @param	CertBuf is the pointer in the Certificate buffer where
 *		the Signature Algorithm field shall be added.
 * @param	SignAlgoLen is the length of the SignAlgo field
 *
 * @note	AlgorithmIdentifier  ::=  SEQUENCE  {
 *		algorithm		OBJECT IDENTIFIER,
 *		parameters		ANY DEFINED BY algorithm OPTIONAL}
 *
 *		This function supports only ECDSA with SHA-384 as
 *		 Signature Algorithm.
 *		The algorithm identifier for ECDSA with SHA-384 signature values is:
 *		ecdsa-with-SHA384 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
 *		us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 3 }
 *		The parameters field MUST be absent.
 *		Hence, the AlgorithmIdentifier shall be a SEQUENCE of
 *		one component: the OID ecdsa-with-SHA384.
 *		The parameter field must be replaced with NULL.
 *
 ******************************************************************************/
static int XCert_GenSignAlgoField(u8* CertBuf, u32 *SignAlgoLen)
{
	int Status = XST_FAILURE;
	u8* Curr = CertBuf;
	u32 OidLen;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_SIGN_ALGO, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	*(Curr++) = XCERT_ASN1_TAG_NULL;
	*(Curr++) = XCERT_NULL_VALUE;

	*SequenceLenIdx = Curr - SequenceValIdx;
	*SignAlgoLen = Curr - CertBuf;

END:
	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function creates the Issuer field in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Issuer field shall be added.
 * @param	Issuer is the DER encoded value of the Issuer field
 * @param	IssuerValLen is the length of the DER encoded value
 * @param	IssuerLen is the length of the Issuer field
 *
 * @note	This function expects the user to provide the Issuer field in DER
 *		encoded format and it will be updated in the TBS Certificate buffer.
 *
 ******************************************************************************/
static void XCert_GenIssuerField(u8* TBSCertBuf, u8* Issuer, const u32 IssuerValLen, u32 *IssuerLen)
{
	XCert_CreateRawDataFromByteArray(TBSCertBuf, Issuer, IssuerValLen, IssuerLen);
}

/*****************************************************************************/
/**
 * @brief	This function creates the Validity field in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Validity field shall be added.
 * @param	Validity is the DER encoded value of the Validity field
 * @param	ValidityValLen is the length of the DER encoded value
 * @param	ValidityLen is the length of the Validity field
 *
 * @note	This function expects the user to provide the Validity field in DER
 *		encoded format and it will be updated in the TBS Certificate buffer.
 *
 ******************************************************************************/
static void XCert_GenValidityField(u8* TBSCertBuf, u8* Validity, const u32 ValidityValLen, u32 *ValidityLen)
{
	XCert_CreateRawDataFromByteArray(TBSCertBuf, Validity, ValidityValLen, ValidityLen);
}

/*****************************************************************************/
/**
 * @brief	This function creates the Subject field in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Subject field shall be added.
 * @param	Subject is the DER encoded value of the Subject field
 * @param	IssuerValLen is the length of the DER encoded value
 * @param	SubjectLen is the length of the Subject field
 *
 * @note	This function expects the user to provide the Subject field in DER
 *		encoded format and it will be updated in the TBS Certificate buffer.
 *
 ******************************************************************************/
static void XCert_GenSubjectField(u8* TBSCertBuf, u8* Subject, const u32 SubjectValLen, u32 *SubjectLen)
{
	XCert_CreateRawDataFromByteArray(TBSCertBuf, Subject, SubjectValLen, SubjectLen);
}

#ifndef PLM_ECDSA_EXCLUDE
/*****************************************************************************/
/**
 * @brief	This function creates the Public Key Algorithm Identifier sub-field.
 *		It is a part of Subject Public Key Info field present in
 *		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Public Key Algorithm Identifier sub-field shall be added.
 * @param	Len is the length of the Public Key Algorithm Identifier sub-field.
 *
 * @note	AlgorithmIdentifier  ::=  SEQUENCE  {
 *		algorithm		OBJECT IDENTIFIER,
 *		parameters		ANY DEFINED BY algorithm OPTIONAL}
 *
 *		This function supports only ECDSA P-384 key
 *		The algorithm identifier for ECDSA P-384 key is:
 *		{iso(1) member-body(2) us(840) ansi-x962(10045) keyType(2) ecPublicKey(1)}
 *		The parameter for ECDSA P-384 key is:
 *		secp384r1 OBJECT IDENTIFIER ::= {
 *		iso(1) identified-organization(3) certicom(132) curve(0) 34 }
 *
 *		Hence, the AlgorithmIdentifier shall be a SEQUENCE of
 *		two components: the OID id-ecPublicKey and OID secp384r1
 *
 ******************************************************************************/
static int XCert_GenPubKeyAlgIdentifierField(u8* TBSCertBuf, u32 *Len)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u32 OidLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_EC_PUBLIC_KEY, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_P384, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	*SequenceLenIdx = Curr - SequenceValIdx;
	*Len = Curr - TBSCertBuf;

END:
	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function creates the Public Key Info field present in
 *		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Public Key Info field shall be added.
 * @param	SubjectPublicKey is the public key of the Subject for which the
  *		certificate is being created.
 * @param	PubKeyInfoLen is the length of the Public Key Info field.
 *
 * @note	SubjectPublicKeyInfo  ::=  SEQUENCE  {
			algorithm            AlgorithmIdentifier,
			subjectPublicKey     BIT STRING  }
 *
 ******************************************************************************/
static int XCert_GenPublicKeyInfoField(u8* TBSCertBuf, u8* SubjectPublicKey, u32 *PubKeyInfoLen)
{
	int Status = XST_FAILURE;
	u32 KeyLen = XSECURE_ECC_P384_SIZE_IN_BYTES + XSECURE_ECC_P384_SIZE_IN_BYTES;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u32 Len;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_GenPubKeyAlgIdentifierField(Curr, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	XCert_CreateBitString(Curr, SubjectPublicKey, KeyLen, &Len);
	Curr = Curr + Len;

	*SequenceLenIdx = Curr - SequenceValIdx;
	*PubKeyInfoLen = Curr - TBSCertBuf;

END:
	return Status;
}
#endif

/******************************************************************************/
/**
 * @brief	This function creates the Subject Key Identifier field present in
 * 		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Subject Key Identifier field shall be added.
 * @param	SubjectPublicKey is the public key whose hash will be used as
 * 		Subject Key Identifier
 * @param	SubjectKeyIdentifierLen is the length of the Subject Key Identifier field.
 *
 * @note	SubjectKeyIdentifierExtension  ::=  SEQUENCE  {
 *		extnID      OBJECT IDENTIFIER,
 *		extnValue   OCTET STRING
 *		}
 *		To calculate value of SubjectKeyIdentifier field, hash is
 *		calculated on the Subject Public Key and 20 bytes from LSB of
 *		the hash is considered as the value for this field.
 *
 ******************************************************************************/
static int XCert_GenSubjectKeyIdentifierField(u8* TBSCertBuf, u8* SubjectPublicKey, u32 *SubjectKeyIdentifierLen)
{
	int Status = XST_FAILURE;
	u8 Hash[XCERT_HASH_SIZE_IN_BYTES] = {0U};
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u32 OidLen;
	u32 FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_SUB_KEY_IDENTIFIER, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	Status = XSecure_Sha384Digest(SubjectPublicKey, XCERT_ECC_P384_PUBLIC_KEY_LEN, Hash);
	if (Status != XST_SUCCESS) {
		goto END;
	}

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;

	XCert_CreateOctetString(Curr, Hash, XCERT_SUB_KEY_ID_VAL_LEN, &FieldLen);
	Curr = Curr + FieldLen;

	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*SubjectKeyIdentifierLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}

/******************************************************************************/
/**
 * @brief	This function creates the Authority Key Identifier field present in
 * 		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Authority Key Identifier field shall be added.
 * @param	IssuerPublicKey is the public key whose hash will be used as
 * 		Authority Key Identifier
 * @param	AuthorityKeyIdentifierLen is the length of the Authority Key Identifier field.
 *
 * @note
 * 		id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 }
 *
 *		AuthorityKeyIdentifier ::= SEQUENCE {
 *		keyIdentifier             [0] KeyIdentifier           OPTIONAL,
 *		authorityCertIssuer       [1] GeneralNames            OPTIONAL,
 *		authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
 *
 *		KeyIdentifier ::= OCTET STRING
 *		To calculate value ofAuthorityKeyIdentifier field, hash is
 *		calculated on the Issuer Public Key and 20 bytes from LSB of
 *		the hash is considered as the value for this field.
 *
 ******************************************************************************/
static int XCert_GenAuthorityKeyIdentifierField(u8* TBSCertBuf, u8* IssuerPublicKey, u32 *AuthorityKeyIdentifierLen)
{
	int Status = XST_FAILURE;
	u8 Hash[XCERT_HASH_SIZE_IN_BYTES] = {0U};
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u8* KeyIdSequenceLenIdx;
	u8* KeyIdSequenceValIdx;
	u32 OidLen;
	u32 FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_AUTH_KEY_IDENTIFIER, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	Status = XSecure_Sha384Digest(IssuerPublicKey, XCERT_ECC_P384_PUBLIC_KEY_LEN, Hash);
	if (Status != XST_SUCCESS) {
		goto END;
	}

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;


	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	KeyIdSequenceLenIdx = Curr++;
	KeyIdSequenceValIdx = Curr;

	XCert_CreateOctetString(Curr, Hash, XCERT_AUTH_KEY_ID_VAL_LEN, &FieldLen);
	Curr = Curr + FieldLen;

	/**
	 * 0x80 indicates that the SEQUENCE contains the optional parameter tagged
	 * as [0] in the AuthorityKeyIdentifier sequence
	 */
	*KeyIdSequenceValIdx = XCERT_AUTH_KEY_ID_OPTIONAL_PARAM;

	*KeyIdSequenceLenIdx = (u8)(Curr - KeyIdSequenceValIdx);
	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*AuthorityKeyIdentifierLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}

/******************************************************************************/
/**
 * @brief	This function creates the TCB Info Extension(2.23.133.5.4.1)
 * 		field present in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the TCB Info Extension field shall be added.
 * @param	TcbInfoExtnLen is the length of the Authority Key Identifier field.
 *
 * @note
 * 		tcg-dice-TcbInfo OBJECT IDENTIFIER ::= {tcg-dice 1}
 *		DiceTcbInfo ::== SEQUENCE {
 *			vendor [0] IMPLICIT UTF8String OPTIONAL,
 *			model [1] IMPLICIT UTF8String OPTIONAL,
 *			version [2] IMPLICIT UTF8String OPTIONAL,
 *			svn [3] IMPLICIT INTEGER OPTIONAL,
 *			layer [4] IMPLICIT INTEGER OPTIONAL,
 *			index [5] IMPLICIT INTEGER OPTIONAL,
 *			fwids [6] IMPLICIT FWIDLIST OPTIONAL,
 *			flags [7] IMPLICIT OperationalFlags OPTIONAL,
 *			vendorInfo [8] IMPLICIT OCTET STRING OPTIONAL,
 *			type [9] IMPLICIT OCTET STRING OPTIONAL
 *		}
 *		FWIDLIST ::== SEQUENCE SIZE (1..MAX) OF FWID
 *		FWID ::== SEQUENCE {
 *			hashAlg OBJECT IDENTIFIER,
 *			digest OCTET STRING
 *		}
 *
 * 		As per requirement, only fwids needs to be included in the extension.
 *		For DevIk certificates, the value of fwid shall be SHA3-384 hash of
 *		PLM and PMC CDO.
 *		For DevAk certificates, the value of fwid shall be SHA3-384 hash of
 *		the application.
 *
 ******************************************************************************/
static int XCert_GenTcbInfoExtnField(u8* TBSCertBuf, XCert_Config* Cfg, u32 *TcbInfoExtnLen)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u8* TcbInfoSequenceLenIdx;
	u8* TcbInfoSequenceValIdx;
	u8* OptionalTagLenIdx;
	u8* OptionalTagValIdx;
	u8* FwIdSequenceLenIdx;
	u8* FwIdSequenceValIdx;
	u32 OidLen;
	u32 FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_TCB_INFO_EXTN, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	TcbInfoSequenceLenIdx = Curr++;
	TcbInfoSequenceValIdx = Curr;

	*(Curr++) = XCERT_OPTIONAL_PARAM_6_TAG;
	OptionalTagLenIdx = Curr++;
	OptionalTagValIdx = Curr;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	FwIdSequenceLenIdx = Curr++;
	FwIdSequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_SHA3_384, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	XCert_CreateOctetString(Curr, Cfg->AppCfg.FwHash, XCERT_HASH_SIZE_IN_BYTES, &FieldLen);
	Curr = Curr + FieldLen;

	*FwIdSequenceLenIdx = (u8)(Curr - FwIdSequenceValIdx);
	*OptionalTagLenIdx = (u8)(Curr - OptionalTagValIdx);
	*TcbInfoSequenceLenIdx = (u8)(Curr - TcbInfoSequenceValIdx);
	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*TcbInfoExtnLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}

/******************************************************************************/
/**
 * @brief	This function creates the UEID extension(2.23.133.5.4.4) field
 * 		present in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the UEID extension field shall be added.
 * @param	UeidExnLen is the length of the UEID Extension field.
 *
 * @note	tcg-dice-Ueid OBJECT IDENTIFIER ::= {tcg-dice 4}
 *		TcgUeid ::== SEQUENCE {
 *			ueid OCTET STRING
 *		}
 *		The content of the UEID extension should contributes to the CDI
 *		which generated the Subject Key. Hence Device DNA is used as the
 *		value of this extension.
 *
 ******************************************************************************/
static int XCert_GenUeidExnField(u8* TBSCertBuf, u32 *UeidExnLen)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u8* UeidSequenceLenIdx;
	u8* UeidSequenceValIdx;
	u32 OidLen;
	u32 FieldLen;
	u32 Dna[XCERT_DNA_LEN_IN_WORDS] = {0U};
	u32 Offset;
	u32 Address;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_UEID_EXTN, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;


	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	UeidSequenceLenIdx = Curr++;
	UeidSequenceValIdx = Curr;

	for (Offset = 0; Offset < XCERT_DNA_LEN_IN_WORDS; Offset++) {
		Address = XCERT_DNA_0_ADDRESS + (Offset * XCERT_WORD_LEN);
		Dna[Offset] = XCert_In32(Address);
	}

	XCert_CreateOctetString(Curr, (u8*)Dna, XCERT_DNA_LEN_IN_BYTES ,&FieldLen);
	Curr = Curr + FieldLen;

	*UeidSequenceLenIdx = (u8)(Curr - UeidSequenceValIdx);
	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*UeidExnLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}


/******************************************************************************/
/**
 * @brief	This function updates the value of Key Usage extension field
 * 		present in TBS Certificate as per the KeyUsageOption.
 *
 * @param	KeyUsageVal is the pointer in the Key Usage value buffer where
 *		the Key Usage option shall be updated.
 * @param	KeyUsageOption is type of key usage which has to be updated
 *
 ******************************************************************************/
static void XCert_UpdateKeyUsageVal(u8* KeyUsageVal, XCert_KeyUsageOption KeyUsageOption)
{
	u8 Idx = KeyUsageOption / XCERT_LEN_OF_BYTE_IN_BITS;
	u8 ShiftVal = (XCERT_LEN_OF_BYTE_IN_BITS * (Idx + 1U)) - (u8)KeyUsageOption - 1U;

	KeyUsageVal[Idx] |= 1U << ShiftVal;
}

/******************************************************************************/
/**
 * @brief	This function creates the Key Usage extension field present in
 * 		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Key Usage extension field shall be added.
 * @param	Cfg is structure which includes configuration for the TBS Certificate.
 * @param	KeyUsageExtnLen is the length of the Key Usage Extension field.
 *
 * @note	id-ce-keyUsage OBJECT IDENTIFIER ::=  { id-ce 15 }
 *
 *		KeyUsage ::= BIT STRING {
 *			digitalSignature        (0),
 *			nonRepudiation          (1),
 *			keyEncipherment         (2),
 *			dataEncipherment        (3),
 *			keyAgreement            (4),
 *			keyCertSign             (5),
 *			cRLSign                 (6),
 *			encipherOnly            (7),
 *			decipherOnly            (8)
 *		}
 *
 ******************************************************************************/
static int XCert_GenKeyUsageField(u8* TBSCertBuf, XCert_Config* Cfg, u32 *KeyUsageExtnLen)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u32 OidLen;
	u32 FieldLen;
	u8 KeyUsageVal[XCERT_MAX_LEN_OF_KEYUSAGE_VAL] = {0U};
	u8 KeyUsageValLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_KEY_USAGE_EXTN, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	XCert_CreateBoolean(Curr, (u8)TRUE, &FieldLen);
	Curr = Curr + FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;

	if (Cfg->AppCfg.IsSelfSigned == TRUE) {
		XCert_UpdateKeyUsageVal(KeyUsageVal, XCERT_KEYCERTSIGN);
	}
	else {
		XCert_UpdateKeyUsageVal(KeyUsageVal, XCERT_DIGITALSIGNATURE);
		XCert_UpdateKeyUsageVal(KeyUsageVal, XCERT_KEYAGREEMENT);
	}

	if ((KeyUsageVal[1U] & XCERT_BYTE_MASK) == 0U) {
		KeyUsageValLen = XCERT_MAX_LEN_OF_KEYUSAGE_VAL - 1U;
	}
	else {
		KeyUsageValLen = XCERT_MAX_LEN_OF_KEYUSAGE_VAL;
	}

	XCert_CreateBitString(Curr, KeyUsageVal, KeyUsageValLen, &FieldLen);
	Curr = Curr + FieldLen;

	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*KeyUsageExtnLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}

/******************************************************************************/
/**
 * @brief	This function creates the Extended Key Usage extension field
 * 		present in TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the Extended Key Usage extension field shall be added.
 * @param	Cfg is structure which includes configuration for the TBS Certificate.
 * @param	EkuLen is the length of the Extended Key Usage Extension field.
 *
 * @note	id-ce-extKeyUsage OBJECT IDENTIFIER ::= { id-ce 37 }
 *		ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *		KeyPurposeId ::= OBJECT IDENTIFIER
 *
 ******************************************************************************/
static int XCert_GenExtKeyUsageField(u8* TBSCertBuf, XCert_Config* Cfg, u32 *EkuLen)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OctetStrLenIdx;
	u8* OctetStrValIdx;
	u8* EkuSequenceLenIdx;
	u8* EkuSequenceValIdx;
	u32 OidLen;
	u32 FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_EKU_EXTN, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	XCert_CreateBoolean(Curr, (u8)TRUE, &FieldLen);
	Curr = Curr + FieldLen;

	*(Curr++) = XCERT_ASN1_TAG_OCTETSTRING;
	OctetStrLenIdx = Curr++;
	OctetStrValIdx = Curr;


	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	EkuSequenceLenIdx = Curr++;
	EkuSequenceValIdx = Curr;

	Status = XCert_CreateRawDataFromStr(Curr, XCERT_OID_EKU_CLIENT_AUTH, &OidLen);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + OidLen;
	}

	*EkuSequenceLenIdx = (u8)(Curr - EkuSequenceValIdx);
	*OctetStrLenIdx = (u8)(Curr - OctetStrValIdx);
	*SequenceLenIdx = (u8)(Curr - SequenceValIdx);
	*EkuLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}

/******************************************************************************/
/**
 * @brief	This function creates the X.509 v3 extensions field present in
 * 		TBS Certificate.
 *
 * @param	TBSCertBuf is the pointer in the TBS Certificate buffer where
 *		the X.509 V3 extensions field shall be added.
 * @param	Cfg is structure which includes configuration for the TBS Certificate.
 * @param	ExtensionsLen is the length of the X.509 V3 Extensions field.
 *
 * @note	Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
 *
 *		Extension  ::=  SEQUENCE  {
 *		extnID      OBJECT IDENTIFIER,
 *		critical    BOOLEAN DEFAULT FALSE,
 *		extnValue   OCTET STRING
 *				-- contains the DER encoding of an ASN.1 value
 *				-- corresponding to the extension type identified
 *				-- by extnID
 *		}
 *
 *		This field is a SEQUENCE of the different extensions which should
 * 		be part of the Version 3 of the X.509 certificate.
 *
 ******************************************************************************/
static int XCert_GenX509v3ExtensionsField(u8* TBSCertBuf,  XCert_Config* Cfg, u32 *ExtensionsLen)
{
	int Status = XST_FAILURE;
	u8* Curr = TBSCertBuf;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* OptionalTagLenIdx;
	u8* OptionalTagValIdx;
	u32 Len;

	*(Curr++) = XCERT_OPTIONAL_PARAM_3_TAG;
	OptionalTagLenIdx = Curr++;
	OptionalTagValIdx = Curr;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	Status = XCert_GenSubjectKeyIdentifierField(Curr, Cfg->AppCfg.SubjectPublicKey, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	Status = XCert_GenAuthorityKeyIdentifierField(Curr, Cfg->AppCfg.IssuerPublicKey, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	Status = XCert_GenTcbInfoExtnField(Curr, Cfg, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	/**
	 * UEID entension (2.23.133.5.4.4) should be added for self-signed
	 * DevIK certificates only
	 */
	if (Cfg->AppCfg.IsSelfSigned == TRUE) {
		Status = XCert_GenUeidExnField(Curr, &Len);
		if (Status != XST_SUCCESS) {
			goto END;
		}
		else {
			Curr = Curr + Len;
		}
	}

	Status = XCert_GenKeyUsageField(Curr, Cfg, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	if (Cfg->AppCfg.IsSelfSigned == TRUE) {
		Status = XCert_GenExtKeyUsageField(Curr, Cfg, &Len);
		if (Status != XST_SUCCESS) {
			goto END;
		}
		else {
			Curr = Curr + Len;
		}
	}

	Status =  XCert_UpdateEncodedLength(SequenceLenIdx, (u32)(Curr - SequenceValIdx), SequenceValIdx);
	if (Status != XST_SUCCESS) {
		Status = (int)XOCP_ERR_X509_UPDATE_ENCODED_LEN;
		goto END;
	}
	Curr = Curr + ((*SequenceLenIdx) & XCERT_LOWER_NIBBLE_MASK);

	Status =  XCert_UpdateEncodedLength(OptionalTagLenIdx, (u32)(Curr - OptionalTagValIdx), OptionalTagValIdx);
	if (Status != XST_SUCCESS) {
		Status = (int)XOCP_ERR_X509_UPDATE_ENCODED_LEN;
		goto END;
	}
	Curr = Curr + ((*OptionalTagLenIdx) & XCERT_LOWER_NIBBLE_MASK);

	*ExtensionsLen = (u32)(Curr - TBSCertBuf);

END:
	return Status;
}



/*****************************************************************************/
/**
 * @brief	This function creates the TBS(To Be Signed) Certificate.
 *
 * @param	TBSCertBuf is the pointer to the TBS Certificate buffer
 * @param	Cfg is structure which includes configuration for the TBS Certificate.
 * @param	TBSCertLen is the length of the TBS Certificate
 *
 * @note	TBSCertificate  ::=  SEQUENCE  {
 *			version         [0]  EXPLICIT Version DEFAULT v1,
 *			serialNumber         CertificateSerialNumber,
 *			signature            AlgorithmIdentifier,
 *			issuer               Name,
 *			validity             Validity,
 *			subject              Name,
 *			subjectPublicKeyInfo SubjectPublicKeyInfo,
 *		}
 *
 ******************************************************************************/
static int XCert_GenTBSCertificate(u8* TBSCertBuf, XCert_Config* Cfg, u32 *TBSCertLen)
{
	volatile int Status = XST_FAILURE;
	volatile int StatusTmp = XST_FAILURE;
	u8* Start = TBSCertBuf;
	u8* Curr  = Start;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u8* SerialStartIdx;
	u8 Hash[XCERT_HASH_SIZE_IN_BYTES] = {0U};
	u32 Len;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	/**
	 * Generate Version field
	 */
	XCert_GenVersionField(Curr, &Len);
	Curr = Curr + Len;

	/**
	 * Store the start index for the Serial field. Once all the remaining
	 * fields are populated then the SHA2 hash is calculated for the
	 * remaining fields in the TBS certificate and Serial is 20 bytes from
	 * LSB of the calculated hash. Hence we need to store the start index of Serial
	 * so that it can be updated later
	 */
	SerialStartIdx = Curr;

	/**
	 * Generate Signature Algorithm field
	 */
	Status = XCert_GenSignAlgoField(Curr, &Len);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_GEN_TBSCERT_SIGN_ALGO_FIELD;
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	/**
	 * Generate Issuer field
	 */
	XCert_GenIssuerField(Curr, Cfg->UserCfg->Issuer, Cfg->UserCfg->IssuerLen, &Len);
	Curr = Curr + Len;

	/**
	 * Generate Validity field
	 */
	XCert_GenValidityField(Curr, Cfg->UserCfg->Validity, Cfg->UserCfg->ValidityLen, &Len);
	Curr = Curr + Len;

	/**
	 * Generate Subject field
	 */
	XCert_GenSubjectField(Curr, Cfg->UserCfg->Subject, Cfg->UserCfg->SubjectLen, &Len);
	Curr = Curr + Len;

#ifndef PLM_ECDSA_EXCLUDE
	/**
	 * Generate Public Key Info field
	 */
	Status = XCert_GenPublicKeyInfoField(Curr, Cfg->AppCfg.SubjectPublicKey, &Len);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_GEN_TBSCERT_PUB_KEY_INFO_FIELD;
		goto END;
	}
	else {
		Curr = Curr + Len;
	}
#else
	Status = XOCP_ECDSA_NOT_ENABLED_ERR;
	goto END;
#endif

	/**
	 * Generate X.509 V3 extensions field
	 *
	 */
	Status = XCert_GenX509v3ExtensionsField(Curr, Cfg, &Len);
	if (Status != XST_SUCCESS) {
		goto END;
	}
	else {
		Curr = Curr + Len;
	}

	if (XPlmi_IsKatRan(XPLMI_SECURE_SHA384_KAT_MASK) != TRUE) {
		XPLMI_HALT_BOOT_SLD_TEMPORAL_CHECK(XOCP_ERR_KAT_FAILED, Status, StatusTmp, XSecure_Sha384Kat);
		if ((Status != XST_SUCCESS) || (StatusTmp != XST_SUCCESS)) {
			goto END;
		}
		XPlmi_SetKatMask(XPLMI_SECURE_SHA384_KAT_MASK);
	}

	/**
	 * Calculate SHA2 Hash for all fields in the TBS certificate except Version and Serial
	 * Please note that currently SerialStartIdx points to the field after Serial.
	 * Hence this is the start pointer for calcualting the hash.
	 */
	Status = XSecure_Sha384Digest((u8* )SerialStartIdx, Curr - SerialStartIdx, Hash);
	if (Status != XST_SUCCESS) {
		goto END;
	}

	/**
	 * The fields after Serial have to be moved to make space for updating the Serial field
	 * Since the length of value of Serial field is 20 bytes and total length of Serial field
	 * (including tyag, length and value) is 22 bytes. So the remaining fields
	 * are moved by 22 bytes
	 */
	Status = Xil_SMemMove(SerialStartIdx + XCERT_SERIAL_FIELD_LEN, Curr - SerialStartIdx,
		SerialStartIdx, Curr - SerialStartIdx, Curr - SerialStartIdx);
	if (Status != XST_SUCCESS) {
		goto END;
	}

	/**
	 * Generate Serial field
	 */
	XCert_GenSerialField(SerialStartIdx, Hash, &Len);
	Curr = Curr + XCERT_SERIAL_FIELD_LEN;

	/**
	 * Update the encoded length in the TBS certificate SEQUENCE
	 */
	Status =  XCert_UpdateEncodedLength(SequenceLenIdx, Curr - SequenceValIdx, SequenceValIdx);
	if (Status != XST_SUCCESS) {
		Status = XOCP_ERR_X509_UPDATE_ENCODED_LEN;
		goto END;
	}
	Curr = Curr + ((*SequenceLenIdx) & XCERT_LOWER_NIBBLE_MASK);

	*TBSCertLen = Curr - Start;

END:
	return Status;
}
#ifndef PLM_ECDSA_EXCLUDE
/*****************************************************************************/
/**
 * @brief	This function creates the Signature field in the X.509 certificate
 *
 * @param	X509CertBuf is the pointer to the X.509 Certificate buffer
 * @param	Signature is value of the Signature field in X.509 certificate
 * @param	SignLen is the length of the Signature field
 *
 * @note	Th signature value is encoded as a BIT STRING and included in the
 *		signature field. When signing, the ECDSA algorithm generates
 *		two values - r and s.
 *		To easily transfer these two values as one signature,
 *		they MUST be DER encoded using the following ASN.1 structure:
 *		Ecdsa-Sig-Value  ::=  SEQUENCE  {
 *			r     INTEGER,
 *			s     INTEGER  }
 *
 ******************************************************************************/
static void XCert_GenSignField(u8* X509CertBuf, u8* Signature, u32 *SignLen)
{
	u8* Curr = X509CertBuf;
	u8* BitStrLenIdx;
	u8* BitStrValIdx;
	u8* SequenceLenIdx;
	u8* SequenceValIdx;
	u32 Len;

	*(Curr++) = XCERT_ASN1_TAG_BITSTRING;
	BitStrLenIdx = Curr++;
	BitStrValIdx = Curr++;
	*BitStrValIdx = 0x00;

	*(Curr++) = XCERT_ASN1_TAG_SEQUENCE;
	SequenceLenIdx = Curr++;
	SequenceValIdx = Curr;

	XCert_CreateInteger(Curr, Signature, XSECURE_ECC_P384_SIZE_IN_BYTES, &Len);
	Curr = Curr + Len;

	XCert_CreateInteger(Curr, Signature + XSECURE_ECC_P384_SIZE_IN_BYTES,
		XSECURE_ECC_P384_SIZE_IN_BYTES, &Len);
	Curr = Curr + Len;

	*SequenceLenIdx = Curr - SequenceValIdx;
	*BitStrLenIdx = Curr - BitStrValIdx;
	*SignLen = Curr - X509CertBuf;
}
#endif

/*****************************************************************************/
/**
 * @brief	This function copies data to 32/64 bit address from
 *		local buffer.
 *
 * @param	Size 	- Length of data in bytes
 * @param	Src     - Pointer to the source buffer
 * @param	DstAddr - Destination address
 *
 *****************************************************************************/
static void XCert_GetData(const u32 Size, const u8 *Src, const u64 DstAddr)
{
	u32 Index = 0U;

	for (Index = 0U; Index < Size; Index++) {
		XSecure_OutByte64((DstAddr + Index), Src[Index]);
	}
}
