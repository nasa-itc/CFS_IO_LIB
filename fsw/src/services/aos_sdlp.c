/******************************************************************************/
/** \file  AOS_sdlp.c
*
*   Copyright 2017 United States Government as represented by the Administrator
*   of the National Aeronautics and Space Administration.  No copyright is
*   claimed in the United States under Title 17, U.S. Code.
*   All Other Rights Reserved.
*  
*   \brief Function Definitions for AOS_SDLP
*
*   \par
*     Provides Telemetry Space Data Link Protocol (AOS_SDLP) services
*
*   \par Modification History:
*     - 2015-04-26 | Alan A. Asp | OSR | Code Started (originally in AOStf.c)
*     - 2015-10-22 | Guy de Carufel | OSR | Migrated from AOStf.c. 
*           Major revision: Comments, Structs, idle data, overflow, API.
*******************************************************************************/

#include "aos_sdlp.h"

static int32 AOS_SDLP_AddData(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pData, 
                             uint16 dataLength, bool isPacket);
static int32 AOS_SDLP_CopyToOverflow(AOS_SDLP_OverflowInfo_t *pOverflow, 
                                    uint8 *data, uint16 length, 
                                    bool isPartial);
static int32 AOS_SDLP_CopyFromOverflow(AOS_SDLP_FrameInfo_t *pFrameInfo);



/*****************************************************************************/
/** \brief AOS_SDLP_InitIdlePacket
******************************************************************************/
int32 AOS_SDLP_InitIdlePacket(CFE_MSG_Message_t *pIdlePacket, uint8 *pIdlePattern,
                            uint16 bufferLength, uint32 patternBitLength)
{
    uint8 *pIdleData = NULL;
    uint16 idleDataLength = 0;
    uint16 byte = 0;
    uint32 bit = 0;
    uint16 bitOffset = 0;
    uint16 byteIdx = 0;
    uint16 patternLength = 0;
    int32 iStatus = AOS_SDLP_SUCCESS;
    
    if (pIdlePacket == NULL || pIdlePattern == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitIdlePacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    if (patternBitLength == 0 || bufferLength == 0)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitIdlePacket Error: "
                          "Input length is 0.");
        
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* Idle packet, as specified in CCSDS 133.0-B-1 */
    CFE_MSG_Init(pIdlePacket, CFE_SB_ValueToMsgId(0x7ffU), bufferLength);
    idleDataLength = CFE_SB_GetUserDataLength(pIdlePacket);
    pIdleData = CFE_SB_GetUserData(pIdlePacket);

    patternLength = patternBitLength / 8;
    if (patternBitLength % 8 != 0) 
    {
        patternLength++;
    }

    /* Build the idle data from a provided pattern */
    for (byte = 0; byte < idleDataLength; ++byte)
    {
        bit = (byte * 8) % patternBitLength;
        byteIdx = bit / 8;
        bitOffset = bit % 8;

        pIdleData[byte] = (pIdlePattern[byteIdx] << bitOffset) |
                          (pIdlePattern[(byteIdx + 1) % patternLength] >> 
                           (8-bitOffset));
    }

end_of_function:
    return iStatus;
}


/*****************************************************************************/
/** \brief AOS_SDLP_InitChannel
******************************************************************************/
int32 AOS_SDLP_InitChannel(AOS_SDLP_FrameInfo_t *pFrameInfo, 
                          uint8 *pTfBuffer, uint8 *pOverflowBuffer,
                          AOS_SDLP_GlobalConfig_t *pGlobalConfig, 
                          AOS_SDLP_ChannelConfig_t *pChannelConfig)
{
    int32 iStatus = AOS_SDLP_SUCCESS;
    int32 dataFieldLength;
    uint16 dataFieldOffset;
    uint8  sdlsSecurityHeaderLength = 0;
    uint8  sdlsSecurityTrailerLength = 0;
    char mutName[OS_MAX_API_NAME];
    SecurityAssociation_t* sa_ptr = NULL;

    if (pGlobalConfig == NULL || pChannelConfig == NULL || pFrameInfo == NULL ||
        pOverflowBuffer == NULL || pTfBuffer == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitChannel Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    /* Validate VCID range (0-62, 63 reserved for idle) */
    if (pChannelConfig->vcId > 62)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitChannel Error: "
                          "Invalid VCID:%d (must be 0-62)", pChannelConfig->vcId);
        
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* Validate VC Frame Count Cycle (4-bit value) */
    if (pChannelConfig->vcFrameCountCycle > 15)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitChannel Error: "
                          "Invalid VC Frame Count Cycle:%d (must be 0-15)", 
                          pChannelConfig->vcFrameCountCycle);
        
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    dataFieldLength = (int32) pGlobalConfig->frameLength;
    dataFieldOffset = AOSTF_PRIHDR_LENGTH;

    /* Add Frame Header Error Control if present */
    if (pGlobalConfig->hasFhec == true)
    {
        dataFieldOffset += AOSTF_FHEC_LENGTH;
    }

    /* Add Insert Zone if present */
    if (pGlobalConfig->hasInsertZone == true)
    {
        dataFieldOffset += pGlobalConfig->insertZoneLength;
    }

    // Need SA information for security parameter lengths
    // Query SA DB for active SA / SDLS parameters
    if (sa_if == NULL) // This should not happen, but tested here for safety
    {
        printf(KRED "ERROR: SA DB Not initalized! -- CRYPTO_LIB_ERR_NO_INIT, Will Exit\n" RESET);
        iStatus = CRYPTO_LIB_ERR_NO_INIT;
    }
    else
    {
        iStatus = sa_if->sa_get_operational_sa_from_gvcid(1, (uint16)pGlobalConfig->scId, (uint16)pChannelConfig->vcId, 0, &sa_ptr);
 
        if (iStatus != CRYPTO_LIB_SUCCESS) 
        {   
            printf(KRED "Error retrieving operational SA. Error code %d. scId = %d, vcId = %d \n" RESET, iStatus, pGlobalConfig->scId, pChannelConfig->vcId);
            goto end_of_function;
        }
    }

    // IF using SDLS
    // TODO Review this if_statement
    if (1)
    {
        sdlsSecurityHeaderLength = Crypto_Get_Security_Header_Length(sa_ptr);
        dataFieldOffset += sdlsSecurityHeaderLength;
    }

    // Reduce available field length based on cumulative offset
    dataFieldLength -= dataFieldOffset;

    // IF using SDLS
    if (1)
    {
        sdlsSecurityTrailerLength = Crypto_Get_Security_Trailer_Length(sa_ptr);
        dataFieldLength -= sdlsSecurityTrailerLength;
    }

    if (pChannelConfig->ocfFlag == true)
    {
        dataFieldLength -= AOSTF_OCF_LENGTH;
    }

    if (pGlobalConfig->hasFecf == true)
    {
        dataFieldLength -= AOSTF_FECF_LENGTH;
    }

#ifdef AOS_DEBUG
    printf("AOS_SDLP Initializing channel:\n");
    printf("\t Primary header length: \t%d\n", AOSTF_PRIHDR_LENGTH);
    printf("\t Frame Header Error Control: \t%d\n", pGlobalConfig->hasFhec ? AOSTF_FHEC_LENGTH : 0);
    printf("\t Insert Zone length: \t%d\n", pGlobalConfig->hasInsertZone ? pGlobalConfig->insertZoneLength : 0);
    printf("\t\t SPI Length: 2 bytes\n");
    printf("\t\t IV Length: %d bytes\n", sa_ptr->shivf_len);
    printf("\t\t SNF Length Length: %d bytes\n", sa_ptr->shsnf_len);
    printf("\t\t PLF Length: %d bytes\n", sa_ptr->shplf_len);
    printf("\t Security header length: \t%d\n", sdlsSecurityHeaderLength);
    printf("\t Data field offset: \t%d\n", dataFieldOffset);
    printf("\t Data field length: \t%d\n", dataFieldLength);
    printf("\t Security trailer length: \t%d\n", sdlsSecurityTrailerLength);
    printf("\t OCF Length: \t%d\n", AOSTF_OCF_LENGTH);
    printf("\t FECF length: \t%d\n", AOSTF_FECF_LENGTH);
#endif

    if (dataFieldLength < 0)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_InitChannel Error: "
                          "Invalid Length Configuration.");
        
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* Update the Transfer Frame Info */
    pFrameInfo->dataFieldLength     = (uint16) dataFieldLength;
    pFrameInfo->dataFieldOffset     = dataFieldOffset;
    pFrameInfo->insertZoneOffset    = AOSTF_PRIHDR_LENGTH + 
                                      (pGlobalConfig->hasFhec ? AOSTF_FHEC_LENGTH : 0);
    pFrameInfo->ocfOffset           = dataFieldOffset + (uint16) dataFieldLength;
    pFrameInfo->freeOctets          = pFrameInfo->dataFieldLength;
    pFrameInfo->currentDataOffset   = pFrameInfo->dataFieldOffset;
    pFrameInfo->vcFrameCount        = 0;
    pFrameInfo->globConfig          = pGlobalConfig;
    pFrameInfo->chnlConfig          = pChannelConfig;
    pFrameInfo->frame               = (AOSTF_PriHdr_t *) pTfBuffer; 
    pFrameInfo->isReady             = false;

    if (pChannelConfig->ocfFlag == true)
    {
        pFrameInfo->fecfOffset      = pFrameInfo->ocfOffset + AOSTF_OCF_LENGTH;
    }
    else
    {
        pFrameInfo->fecfOffset      = pFrameInfo->ocfOffset;
    }
    
    /* Set the overflowInfo */
    pFrameInfo->overflowInfo.buffSize      = pChannelConfig->overflowSize;
    pFrameInfo->overflowInfo.freeOctets    = pChannelConfig->overflowSize;
    pFrameInfo->overflowInfo.partialOctets = 0;
    pFrameInfo->overflowInfo.dataStart     = pOverflowBuffer;
    pFrameInfo->overflowInfo.dataEnd       = pOverflowBuffer;
    pFrameInfo->overflowInfo.buffer        = pOverflowBuffer;

    /* Initialize the TF buffer */
    CFE_PSP_MemSet((void *)pTfBuffer, 0, pGlobalConfig->frameLength);
    AOSTF_SetVersion(pFrameInfo->frame, AOSTF_VERSION);
    AOSTF_SetScId(pFrameInfo->frame, pGlobalConfig->scId);
    AOSTF_SetVcId(pFrameInfo->frame, pChannelConfig->vcId);

    /* Set AOS Signaling Field components */
    AOSTF_SetReplayFlag(pFrameInfo->frame, pChannelConfig->replayFlag);
    AOSTF_SetVcFrameCountUsageFlag(pFrameInfo->frame, pChannelConfig->vcFrameCountUsage);
    AOSTF_SetVcFrameCountCycle(pFrameInfo->frame, pChannelConfig->vcFrameCountCycle);

    /* Initialize the Overflow buffer */
    CFE_PSP_MemSet((void *)pOverflowBuffer, 0, pChannelConfig->overflowSize);

    /* Create the Mutex */
    AOSTF_GetGlobalVcId(pFrameInfo->frame);
    OS_MutSemCreate(&pFrameInfo->mutexId, mutName, 0); 

    pFrameInfo->isInitialized = true;

end_of_function:
    return iStatus;
}


/*****************************************************************************/
/** \brief AOS_SDLP_FrameHasData
******************************************************************************/
int32 AOS_SDLP_FrameHasData(AOS_SDLP_FrameInfo_t *pFrameInfo)
{
    int32 hasData = 0;

    if (pFrameInfo == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_FrameHasData Error: "
                          "Input Pointer is Null.");
        
        hasData = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

#ifdef AOS_DEBUG
    printf("*** DATA LENGTH INFO!***\n");
    printf("*** Free Octets: %d\n", pFrameInfo->freeOctets);
    printf("*** dataFieldLength: %d\n", pFrameInfo->dataFieldLength);
#endif
    
    if (pFrameInfo->freeOctets < pFrameInfo->dataFieldLength)
    {
        hasData = 1;
    }

end_of_function:
    return hasData;
}


/******************************************************************************/
/** \brief AOS_SDLP_AddPacket
*******************************************************************************/
int32 AOS_SDLP_AddPacket(AOS_SDLP_FrameInfo_t *pFrameInfo, CFE_MSG_Message_t *pPacket)
{
    size_t length = 0;
    int32 iStatus = AOS_SDLP_SUCCESS;

    if (pFrameInfo == NULL || pPacket == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddPacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddPacket Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    CFE_MSG_GetSize(pPacket, &length);

    OS_MutSemTake(pFrameInfo->mutexId);
    iStatus = AOS_SDLP_AddData(pFrameInfo, (uint8 *) pPacket, length, true);
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}

                           
/******************************************************************************/
/** \brief AOS_SDLP_AddIdlePacket
*******************************************************************************/
int32 AOS_SDLP_AddIdlePacket(AOS_SDLP_FrameInfo_t *pFrameInfo,
                           CFE_MSG_Message_t *pIdlePacket)
{
    int32 iStatus = AOS_SDLP_SUCCESS;
    uint16 lengthToCopy = 0;
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    if (pFrameInfo == NULL || pIdlePacket == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddIdlePacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddIdlePacket Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }

    OS_MutSemTake(pFrameInfo->mutexId);
    
    lengthToCopy = pFrameInfo->freeOctets;

    /* If no free octets, no idle data to add. Done. */
    if (lengthToCopy == 0)
    {
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = AOS_SDLP_SUCCESS;
        goto end_of_function;
    }

    /* Limit copy length to available idle data */
    if (lengthToCopy < 7)
    {
        lengthToCopy = 7;
    }

    CFE_MSG_GetMsgId(pIdlePacket, &MsgId);

    /* The Message ID of the idle buffer should always be 0x7ff (Idle Packet). */
    if (CFE_SB_MsgIdToValue(MsgId) != 0x7ffU)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddIdlePacket Error: "
                          "The IdlePacket has MsgId other than 0x7ff.");
        
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = AOS_SDLP_ERROR;
        goto end_of_function;
    }

    /* Set the idlePacket length in header to lengthToCopy */
    CFE_MSG_SetSize(pIdlePacket,  lengthToCopy);
    
    /* Add the idle data. May spill over to overflow buffer. */
    /* iStatus should return remaining free octets if successful. */
    iStatus = AOS_SDLP_AddData(pFrameInfo, (uint8 *) pIdlePacket, lengthToCopy, true);
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}    


/******************************************************************************/
/** \brief AOS_SDLP_AddBitstreamData
*******************************************************************************/
int32 AOS_SDLP_AddBitstreamData(AOS_SDLP_FrameInfo_t *pFrameInfo, 
                                uint8 *pData, uint16 dataLength)
{
    int32 iStatus = AOS_SDLP_SUCCESS;

    if (pFrameInfo == NULL || pData == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddBitstreamData Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddBitstreamData Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);
    iStatus = AOS_SDLP_AddData(pFrameInfo, pData, dataLength, false);
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SDLP_AddVcaData
*******************************************************************************/
int32 AOS_SDLP_AddVcaData(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pData,
                         uint16 dataLength)
{
    int32 iStatus = AOS_SDLP_SUCCESS;

    if (pFrameInfo == NULL || pData == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddVcaData Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if the frame is initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_AddVcaData Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);
    iStatus = AOS_SDLP_AddData(pFrameInfo, pData, dataLength, false);
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SDLP_StartFrame
*******************************************************************************/
int32 AOS_SDLP_StartFrame(AOS_SDLP_FrameInfo_t *pFrameInfo) 
{
    uint16 lengthToCopy = 0;
    uint16 lengthCopied = 0;
    AOS_SDLP_OverflowInfo_t *pOverflow;
    int32 iStatus = AOS_SDLP_SUCCESS;
    
    if (pFrameInfo == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_StartFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_StartFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);
    
    /* If the frame is already started, issue a warning. */
    if (pFrameInfo->isReady == true)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_INFORMATION,
                          "AOS_SDLP_StartFrame: "
                          "The frame was already started. Will continue.");
    }
    
    /* Set Frame as ready */
    pFrameInfo->isReady = true;
    
    pOverflow = &pFrameInfo->overflowInfo;
    lengthToCopy = pOverflow->buffSize - pOverflow->freeOctets;

    if (lengthToCopy > 0)
    {
        /* Only copy as much as TF allows */
        if (lengthToCopy > pFrameInfo->dataFieldLength)
        {
            lengthToCopy = pFrameInfo->dataFieldLength;
        }
        
        while (lengthToCopy > 0)
        {
            lengthCopied = AOS_SDLP_CopyFromOverflow(pFrameInfo);
            lengthToCopy -= lengthCopied;
        }
    }

    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SDLP_SetIdleFrame
*******************************************************************************/
int32 AOS_SDLP_SetIdleFrame(AOS_SDLP_FrameInfo_t *pFrameInfo, 
                           uint8 *pIdleData, uint16 idleLength)
{
    int32 iStatus = AOS_SDLP_SUCCESS;
    
    if (pFrameInfo == NULL || pIdleData == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetIdleFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetIdleFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }

    OS_MutSemTake(pFrameInfo->mutexId);

    /* If the frame is not empty, This method should not be called. */
    if (pFrameInfo->freeOctets != pFrameInfo->dataFieldLength)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetIdleFrame Error: "
                          "The frame is not empty. VC ID:%u",
                          pFrameInfo->chnlConfig->vcId);
        
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = AOS_SDLP_ERROR;
        goto end_of_function;
    }

    /* Set VCID to 63 (idle frame VCID) if not already set */
    if (pFrameInfo->chnlConfig->vcId != AOSTF_IDLE_VCID)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_INFORMATION,
                          "AOS_SDLP_SetIdleFrame: "
                          "Setting VCID to 63 (idle frame)");
        AOSTF_SetVcId(pFrameInfo->frame, AOSTF_IDLE_VCID);
    }

    /* Ensure we don't try to copy more data than frame can hold */
    uint16 copyLength = (idleLength < pFrameInfo->dataFieldLength) ? idleLength : pFrameInfo->dataFieldLength;
    
    /* Add idle data to fill the frame */
    iStatus = AOS_SDLP_AddData(pFrameInfo, pIdleData, copyLength, false);
    
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SDLP_SetInsertZone
*******************************************************************************/
int32 AOS_SDLP_SetInsertZone(AOS_SDLP_FrameInfo_t *pFrameInfo,
                             uint8 *pInsertData, uint16 dataLength)
{
    int32 iStatus = AOS_SDLP_SUCCESS;
    
    if (pFrameInfo == NULL || pInsertData == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetInsertZone Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetInsertZone Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }

    /* Check if frame has Insert Zone */
    if (pFrameInfo->globConfig->hasInsertZone == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetInsertZone Error: "
                          "Frame does not have Insert Zone configured.");
        
        iStatus = AOS_SDLP_ERROR;
        goto end_of_function;
    }
    
    /* Check if data length exceeds Insert Zone size */
    if (dataLength > pFrameInfo->globConfig->insertZoneLength)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_SetInsertZone Error: "
                          "Data length %u exceeds Insert Zone length %u.",
                          dataLength, pFrameInfo->globConfig->insertZoneLength);
        
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    OS_MutSemTake(pFrameInfo->mutexId);

    /* Copy data to the Insert Zone */
    CFE_PSP_MemCpy((void *)((char*)pFrameInfo->frame + pFrameInfo->insertZoneOffset), 
                  pInsertData, dataLength);
    
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SDLP_CompleteFrame
*******************************************************************************/
int32 AOS_SDLP_CompleteFrame(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pOcf)
{
    int32 iStatus = AOS_SDLP_SUCCESS;

    if (pFrameInfo == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_CompleteFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_CompleteFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = AOS_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    if (pFrameInfo->chnlConfig->ocfFlag == true && pOcf == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP_CompleteFrame Error: "
                          "The Input OCF Pointer is NULL.");
        
        iStatus = AOS_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);

    /* Set VC Frame Count (24-bits) */
    AOSTF_SetVcFrameCount(pFrameInfo->frame, pFrameInfo->vcFrameCount);
    
    /* Increment VC frame count for next frame */
    pFrameInfo->vcFrameCount = (pFrameInfo->vcFrameCount + 1) & 0xFFFFFF;

    /* Set AOS signaling field components */
    AOSTF_SetReplayFlag(pFrameInfo->frame, pFrameInfo->chnlConfig->replayFlag);
    AOSTF_SetVcFrameCountUsageFlag(pFrameInfo->frame, pFrameInfo->chnlConfig->vcFrameCountUsage);
    AOSTF_SetVcFrameCountCycle(pFrameInfo->frame, pFrameInfo->chnlConfig->vcFrameCountCycle);

    /* If an OCF Field is present, set it. */
    if (pFrameInfo->chnlConfig->ocfFlag)
    {
        AOSTF_SetOcf(pFrameInfo->frame, pOcf, pFrameInfo->ocfOffset);
    }
   
    /* If Frame Header Error Control is present, update it */
    if (pFrameInfo->globConfig->hasFhec)
    {
        AOSTF_UpdateFrameHeaderErrorControl(pFrameInfo->frame);
    }

    /* If Frame Error Control Field is present, set it. */
    if (pFrameInfo->globConfig->hasFecf)
    {
        AOSTF_UpdateFrameErrorControlField(pFrameInfo->frame, pFrameInfo->fecfOffset);
    }

    /* Reset frame metadata */
    pFrameInfo->freeOctets          = pFrameInfo->dataFieldLength;
    pFrameInfo->currentDataOffset   = pFrameInfo->dataFieldOffset;
    pFrameInfo->isReady             = false;

    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/*******************************************************************************
** Static Functions
*******************************************************************************/

/******************************************************************************/
/** \brief Add Generic Data to the Transfer Frame
*
*   \par Description/Algorithm
*       Copies a data buffer to the TF data field at the next free octet.
*
*   \par Assumptions, External Events, and Notes:
*       - Lower level function called by AddPacket, AddIdleData, AddBitstreamData, AddVcaData
*       - Data unit will be segmented into overflow buffer if frame is full.
*       - Transfer frame will be populated with overflow data if frame is 
*         empty prior to adding data.
*       - isPacket flag is used for M_PDU processing (multiplex protocol data units)
*
*   \param[in,out] pFrameInfo  Pointer to the Frame info/working struct.
*   \param[in]     pData       Pointer to data buffer
*   \param[in]     dataLength  Length of data to copy
*   \param[in]     isPacket    Data is a packet (M_PDU or similar)
*
*   \return Frame FreeOctets
*   \return AOS_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return AOS_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see
*       #AOS_SDLP_AddPacket
*       #AOS_SDLP_AddIdlePacket
*******************************************************************************/
static int32 AOS_SDLP_AddData(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pData, 
                             uint16 dataLength, bool isPacket)
{
    uint16 lengthToCopy = dataLength;
    int32 iStatus = AOS_SDLP_SUCCESS;
    bool isPartial = false;

    /* Check if the frame is ready to add new data. */
    if (pFrameInfo->isReady == false)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP Error: "
                          "The Frame is not ready. Call StartFrame().");
        
        iStatus = AOS_SDLP_FRAME_NOT_READY;
        goto end_of_function;
    }

    /* If data needs to be segmented, copy extra octets to overflow buffer */
    if (pFrameInfo->freeOctets < lengthToCopy)
    {
        lengthToCopy = pFrameInfo->freeOctets;
        if (lengthToCopy > 0)
        {
            isPartial = true;
        }

        iStatus = AOS_SDLP_CopyToOverflow(&pFrameInfo->overflowInfo,
                                      pData + lengthToCopy, 
                                      dataLength - lengthToCopy, isPartial);
        if (iStatus < 0)
        {
            goto end_of_function;
        }
    }

    CFE_PSP_MemCpy((void *) ((char*)pFrameInfo->frame + pFrameInfo->currentDataOffset), 
                   pData, lengthToCopy);
    pFrameInfo->freeOctets -= lengthToCopy;
    pFrameInfo->currentDataOffset += lengthToCopy;
    iStatus = (int32) pFrameInfo->freeOctets;

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief Copy data to the end of the overflow queue
*
*   \par Description/Algorithm
*       Copy data to overflow queue
*
*   \par Assumptions, External Events, and Notes:
 *      - If overflow buffer is full, message is dropped in AddData
 *      - The overflow queue is implemented as a sliding window buffer.
 *      - Function called by AddData()
 *      - Input pointers are checked by calling function. 
*
*   \param[in,out] pOverflow        Pointer to the Overflow info
*   \param[in]     data             Pointer to the data to copy
*   \param[in]     length           Length of the data to copy
*   \param[in]     isPartial        Is data partial 
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_LENGTH      If an input length is invalid
*   \return AOS_SDLP_OVERFLOW_FULL       If overflow buffer is full
*
*   \see 
*       #AOS_SDLP_AddData
*******************************************************************************/
static int32 AOS_SDLP_CopyToOverflow(AOS_SDLP_OverflowInfo_t *pOverflow, uint8 *data, 
                                    uint16 length, bool isPartial)
{
    int32 iStatus = AOS_SDLP_SUCCESS;
    uint16 lengthToEnd;

    if (length > pOverflow->buffSize)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "AOS_SDLP ERROR: "
                          "Message Length too large for overflow Buffer.");
       
        iStatus = AOS_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* If the buffer is full, drop message. */
    if (length > pOverflow->freeOctets)
    {
        CFE_EVS_SendEvent(IO_LIB_AOS_SDLP_EID, CFE_EVS_EventType_INFORMATION,
                          "AOS_SDLP Warning: "
                          "The Frame's OverflowBuffer is Full. Message Dropped.");
       
       iStatus = AOS_SDLP_OVERFLOW_FULL;
       goto end_of_function;
    }

    /* Get size left in buffer after dataEnd cursor */
    lengthToEnd = pOverflow->buffSize - (pOverflow->dataEnd - pOverflow->buffer);

    /* Copy the data at the dataEnd cursor */
    if (length < lengthToEnd)
    {
        CFE_PSP_MemCpy(pOverflow->dataEnd, data, length);
        pOverflow->dataEnd += length;
    }
    /* Wrap arround data in overflow buffer */
    else
    {
        CFE_PSP_MemCpy(pOverflow->dataEnd, data, lengthToEnd);
        CFE_PSP_MemCpy(pOverflow->buffer, data + lengthToEnd,
                       length - lengthToEnd);
        pOverflow->dataEnd = pOverflow->buffer + (length - lengthToEnd);
    }
    
    /* If we are passing only partial data, set the partialOctets */
    if (isPartial == true)
    {
        pOverflow->partialOctets = length;
    }
    
    pOverflow->freeOctets -= length;

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief Copy one packet or partial octects from overflow queue.
*
*   \par Description/Algorithm
*       Copy one packet or partial octects from the start of the overflow queue
*       to the transfer frame.
*
*   \par Assumptions, External Events, and Notes:
*      - The overflow queue is implemented as a sliding window buffer.
*      - Function called by StartFrame()
*      - Input pointers are checked by calling function. 
*
*   \param[out] pFrameInfo     Pointer to the Frame info/working struct.
*
*   \return lengthCopied    Length of data copied
*
*   \see 
*       #AOS_SDLP_StartFrame
*******************************************************************************/
static int32 AOS_SDLP_CopyFromOverflow(AOS_SDLP_FrameInfo_t *pFrameInfo)
{
    uint8 msgHdr[6];
    CFE_MSG_Message_t* MsgPtr = (CFE_MSG_Message_t*) msgHdr;
    size_t lengthToCopy;
    size_t lengthToEnd;
    bool isPacket;
    size_t freeOctets;
    AOS_SDLP_OverflowInfo_t *pOverflow = &pFrameInfo->overflowInfo;

    lengthToEnd = pOverflow->buffSize - 
                  (pOverflow->dataStart - pOverflow->buffer);

    freeOctets = pFrameInfo->freeOctets;

    /* First Determine how many octets to copy from overflow buffer. */
    
    /* If there are partial octets, copy those. */
    if (pFrameInfo->overflowInfo.partialOctets > 0)
    {
        lengthToCopy = pFrameInfo->overflowInfo.partialOctets;
        isPacket = false;
    }
    /* Otherwise, copy the first packet */
    else 
    {
        /* If the the lengthToEnd is shorter than the Packet Primary Header,
         * Copy the header locally first. */
        if (lengthToEnd < 6)
        {
            CFE_PSP_MemCpy((void *)msgHdr, (void *) pOverflow->dataStart, 
                           lengthToEnd);
            CFE_PSP_MemCpy((void *)(msgHdr + lengthToEnd), 
                           (void *) pOverflow->buffer, 6 - lengthToEnd);
            CFE_MSG_GetSize(MsgPtr, &lengthToCopy);
        }
        else
        {
            CFE_MSG_GetSize((CFE_MSG_Message_t *) pOverflow->dataStart, &lengthToCopy);
        }
        
        isPacket = true;
    }

    /* Only copy as many octets as TF has free octets available */
    if (freeOctets < lengthToCopy)
    {
        /* If we are passing a full packet, set the new value of partialOctets */
        if (isPacket == true)
        {
            pFrameInfo->overflowInfo.partialOctets = lengthToCopy - freeOctets;
        }
        /* If we are passing partial octets, revise the partial octet value */
        else
        {
            pFrameInfo->overflowInfo.partialOctets -= freeOctets;
        }
        
        /* The length to copy is freeOctets */
        lengthToCopy = freeOctets;
    }
    /* If we are copying partial octets and there is enough freeOctets for all
     * partial octets, reset the partialOctets to 0. */
    else if (isPacket == false)
    {
        pFrameInfo->overflowInfo.partialOctets = 0;
    }

    /* If the length to the end is greater than the length to copy, copy it. */
    if (lengthToEnd > lengthToCopy)
    {
        freeOctets = AOS_SDLP_AddData(pFrameInfo, pOverflow->dataStart, 
                                     lengthToCopy, isPacket);
        pOverflow->dataStart += lengthToCopy;
    }
    /* Wrap arround overflow buffer if required */
    else
    {
        freeOctets = AOS_SDLP_AddData(pFrameInfo, pOverflow->dataStart, 
                                  lengthToEnd, isPacket);
        freeOctets = AOS_SDLP_AddData(pFrameInfo, pOverflow->buffer, 
                                  lengthToCopy - lengthToEnd, false);
        pOverflow->dataStart = pOverflow->buffer + 
                                 (lengthToCopy - lengthToEnd);
    }
    
    /* Revise the number of free Octets in overflow buffer */
    pOverflow->freeOctets += lengthToCopy;
    pFrameInfo->freeOctets = freeOctets;

    return lengthToCopy;
}