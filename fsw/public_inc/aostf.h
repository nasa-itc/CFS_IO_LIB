/*******************************************************************************
 * File: AOStf.h
 *
 * Copyright 2017 United States Government as represented by the Administrator
 * of the National Aeronautics and Space Administration.  No copyright is
 * claimed in the United States under Title 17, U.S. Code.
 * All Other Rights Reserved.
 *
 * Purpose:
 *   Provide the Transfer Frame format API for the AOS_SDLP Service.
 *
 * Reference(s):
 *   - _AOS Space Data Link Protocol_, CCSDS 132.0-B-1_ (Issue 1, Sept. 2003)
 *   - _Space Packet Protocol_, CCSDS 133.0-B-1_ (Issue 1, Sept. 2003)
 *   - _A Painless Guide To CRC Error Detection Algorithm_ (Version 3, 1993),
 *      Ross N. Williams.  http://www.ross.net/crc/download/crc_v3.txt
 *
 * Notes:
 *  -The AOS Transfer Frame is the protocol data unit (PDU) of the Telemetry 
 *   Space Data Link Protocol (AOS-SDLP).
 *
 * History:
 *   04/26/2015, A. Asp, Odyssey Space Research, LLC
 *    -Created
 *   10/22/2015, G. de Carufel, Odyssey Space Research, LLC
 *    -Moved all services to AOS_sdlp.h
 *
 ******************************************************************************/

#ifndef _AOS_TRANSFER_FRAME_H_
#define _AOS_TRANSFER_FRAME_H_

#include "common_types.h"

/*------------------------------------------------------------------------------
 * Items below should not require user updates
 */

/* Return codes */
#define AOSTF_SUCCESS            (0)
#define AOSTF_ERROR             (-1)
#define AOSTF_INVALID_POINTER   (-2)
#define AOSTF_INVALID_SECHDR    (-3)
#define AOSTF_INVALID_LENGTH    (-4)

/* Fixed parameters to compute Frame Error Control Field (FECF) */
#define AOSTF_FECF_INIT_REGISTRY  0xffffU
#define AOSTF_FECF_POLYNOMIAL     0x11021UL

/* Max number of virtual channels */
#define AOSTF_MAX_VC                 63  

/* Fixed values */
#define AOSTF_VERSION                1      /* AOS Version 2 = binary '01' */

#define AOSTF_PRIHDR_LENGTH          6      /* 6 octets (without optional FHEC) */
#define AOSTF_PRIHDR_LENGTH_WITH_FHEC 8     /* 8 octets (with optional FHEC) */
#define AOSTF_OCF_LENGTH             4      /* Operational Control Field */
#define AOSTF_FHEC_LENGTH            2      /* Frame Header Error Control */
#define AOSTF_FECF_LENGTH            2      /* Frame Error Control Field */
#define AOSTF_INSERT_ZONE_MAX_LENGTH 1019   /* Maximum Insert Zone length */

/* AOS-specific constants */
#define AOSTF_IDLE_VCID             63      /* Reserved VCID for idle frames */
#define AOSTF_REPLAY_FLAG_NO_REPLAY  0      /* No replay flag value */
#define AOSTF_REPLAY_FLAG_REPLAY     1      /* Replay flag value */

typedef struct
{
    uint8 Id[2];                    /* MCID (10 bits) + VCID (6 bits) */
    uint8 VcFrameCount[3];          /* Virtual Channel Frame Count (24 bits) */
    uint8 SignalingField;           /* Replay Flag + VC Frame Count Usage Flag + Reserved + VC Frame Count Cycle */
    uint8 FrameHeaderErrorControl[2]; /* Optional Frame Header Error Control (16 bits) */
} AOSTF_PriHdr_t;

/*
 * Function: AOSTF_LibInit
 *
 * Purpose:
 *   Initialize the static AOSTF CRC Table
 *
 * Arguments:
 *
 * Return:
 *   AOSTF_SUCCESS          Always returns success.
 *
 * Notes:
 *   - Called by IO_LibInit()
 */
int32 AOSTF_LibInit(void);



/*
 * Function: AOSTF_SetVersion
 *
 * Purpose:
 *   Sets the version number for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the version number
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 * Notes:
 *   For CCSDS 132.0-B-1, Sept. 2003, the Transfer Frame Version Number
 *   shall be set to binary '00'.
 *
 */
int32 AOSTF_SetVersion(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetScId
 *
 * Purpose:
 *   Set spacecraft ID for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the spacecraft ID
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetScId(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetVcId
 *
 * Purpose:
 *   Set virtual channel ID for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the virtual channel ID
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetVcId(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetOcfFlag
 *
 * Purpose:
 *   Set operational control field flag for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : bool value to set the flag
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetOcfFlag(AOSTF_PriHdr_t *tfPtr, bool val);


/*
 * Function: AOSTF_GeAOScId
 *
 * Purpose:
 *   Get the Master Channel Id (Version Num + SCID) 
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *
 * Return:
 *   Master Channel ID (VersionNumber + SCID) 
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_GetAOSMcId(AOSTF_PriHdr_t *tfPtr);

/*
 * Function: AOSTF_GetGlobalVcId
 *
 * Purpose:
 *   Get the global VC Id (MCID + VCID) (not including OCF)
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *
 * Return:
 *   Global Virtual Channel ID (MCID + VCID) 
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_GetGlobalVcId(AOSTF_PriHdr_t *tfPtr);



/*
 * Function: AOSTF_SeAOScFrameCount
 *
 * Purpose:
 *   Set master channel frame count for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the master channel frame count
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetAOSFrameCount(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetVcFrameCount
 *
 * Purpose:
 *   Set virtual channel frame count for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the virtual channel frame count
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetVcFrameCount(AOSTF_PriHdr_t *tfPtr, uint32 val);


/*
 * Function: AOSTF_IncrVcFrameCount
 *
 * Purpose:
 *   Increment virtual channel frame count for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_IncrVcFrameCount(AOSTF_PriHdr_t *tfPtr);


/*
 * Function: AOSTF_SetSecHdrFlag
 *
 * Purpose:
 *   Set flag indicating the presence or absence of the secondary header for
 *   the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : bool value to set the flag
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetSecHdrFlag(AOSTF_PriHdr_t *tfPtr, bool val);


/*
 * Function: AOSTF_SetSyncFlag
 *
 * Purpose:
 *   Set the sync flag for the transfer frame
 *     false = octet-synchronized and forward ordered packets or Idle Data inserted
 *     true  = VCA_SDU inserted
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : bool value to set the flag
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetSyncFlag(AOSTF_PriHdr_t *tfPtr, bool val);


/*
 * Function: AOSTF_SetPacketOrderFlag
 *
 * Purpose:
 *   Set the packet order flag for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : bool value to set the flag
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetPacketOrderFlag(AOSTF_PriHdr_t *tfPtr, bool val);


/*
 * Function: AOSTF_SetSegLengthId
 *
 * Purpose:
 *   Set the segment length ID for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the segment length ID
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 */
int32 AOSTF_SetSegLengthId(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetFirstHdrPtr
 *
 * Purpose:
 *   Set the location of the first header pointer for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the first header pointer
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *
 * Notes:
 *  -Constants AOSTF_NO_FIRST_HDR_PTR and AOSTF_OID_FIRST_HDR_PTR defined to be used
 *   for 'val' in situations where there's no first header pointer in the current
 *   TF and there's only idle data in the packet, respectively.
 *
 */
int32 AOSTF_SetFirstHdrPtr(AOSTF_PriHdr_t *tfPtr, uint16 val);


/*
 * Function: AOSTF_SetSecHdrLength
 *
 * Purpose:
 *   Set the size of the secondary header for the transfer frame
 *
 * Arguments:
 *   tfPtr: pointer to the transfer frame
 *   val  : value to set the length of the secondary header.  This value is one
 *          octet less than the actual length of the secondary header.
 *
 * Return:
 *   AOSTF_SUCCESS          if the value was set
 *   AOSTF_INVALID_POINTER  if the input pointer is NULL
 *   AOSTF_INVALID_SECHDR   if the secondary header flag is not set
 *   AOSTF_INVALID_LENGTH   if the length is too short (0 octets) or too long (> 63 octets)
 *
 */
int32 AOSTF_SetSecHdrLength(AOSTF_PriHdr_t *tfPtr, uint8 val);


/*
 * Function: AOSTF_SetSecHdrData
 *
 * Purpose:
 *   Set the Data of the secondary header for the transfer frame
 *
 * Arguments:
 *   tfPtr : pointer to the transfer frame
 *   data  : pointer to the data to be copied to the secondary header data field
 *   length: number of octets to copy
 *
 * Return:
 *   AOSTF_SUCCESS          if the data is copied
 *   AOSTF_INVALID_POINTER  if an input pointer is NULL
 *   AOSTF_INVALID_SECHDR   if the secondary header flag is not set
 *   AOSTF_INVALID_LENGTH   if the length of the data being copied is larger than
 *                           the secondary header data field
 *
 * Notes:
 *
 */
int32 AOSTF_SetSecHdrData(AOSTF_PriHdr_t *tfPtr, uint8 *data, uint8 length);


/*
 * Function: AOSTF_SetOcf
 *
 * Purpose:
 *   Copies the input data to the Operational Control Field (OCF) of the TF
 *
 * Arguments:
 *   tfPtr : pointer to the transfer frame
 *   data  : pointer to the data to be copied
 *   offset: number of octets from the start of the frame to the OCF
 *
 * Return:
 *   AOSTF_SUCCESS          if the data is copied
 *   AOSTF_INVALID_POINTER  if an input pointer is NULL
 *   AOSTF_ERROR            if the OCF flag is not set for the frame
 *
 * Notes:
 *
 */
int32 AOSTF_SetOcf(AOSTF_PriHdr_t *tfPtr, uint8 *data, uint16 offset);



/*
 * Function: AOSTF_UpdateErrCtrlField
 *
 * Purpose:
 *   Calculates the value of the error control field and copies it to the TF trailer
 *
 * Arguments:
 *   tfPtr : pointer to the transfer frame
 *   offset: number of octets from the start of the frame to the error control field
 *
 * Return:
 *   AOSTF_SUCCESS          if the value is successfully calculated and copied
 *   AOSTF_INVALID_POINTER  if an input pointer is NULL
 *
 * Notes:
 *
 */
int32 AOSTF_UpdateErrCtrlField(AOSTF_PriHdr_t *tfPtr, uint16 offset);

/*
 * Function: AOSTF_SetReplayFlag
 *
 * Notes:
 *
 */
int32 AOSTF_SetReplayFlag(AOSTF_PriHdr_t *tfPtr, bool val);

/*
 * Function: AOSTF_SetVcFrameCountUsageFlag
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcFrameCountUsageFlag(AOSTF_PriHdr_t *tfPtr, bool val);

/*
 * Function: AOSTF_SetVcFrameCountCycle
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcFrameCountCycle(AOSTF_PriHdr_t *tfPtr, uint16 val);

/*
 * Function: AOSTF_UpdateFrameHeaderErrorControl
 *
 * Notes:
 *
 */
int32 AOSTF_UpdateFrameHeaderErrorControl(AOSTF_PriHdr_t *tfPtr);

/*
 * Function: AOSTF_UpdateFrameHeaderErrorControl
 *
 * Notes:
 *
 */
int32 AOSTF_UpdateFrameErrorControlField(AOSTF_PriHdr_t *tfPtr, uint16 offset);

#endif
