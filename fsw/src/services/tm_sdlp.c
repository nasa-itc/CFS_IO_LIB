/******************************************************************************/
/** \file  tm_sdlp.c
*
*   Copyright 2017 United States Government as represented by the Administrator
*   of the National Aeronautics and Space Administration.  No copyright is
*   claimed in the United States under Title 17, U.S. Code.
*   All Other Rights Reserved.
*  
*   \brief Function Definitions for TM_SDLP
*
*   \par
*     Provides Telemetry Space Data Link Protocol (TM_SDLP) services
*
*   \par Modification History:
*     - 2015-04-26 | Alan A. Asp | OSR | Code Started (originally in tmtf.c)
*     - 2015-10-22 | Guy de Carufel | OSR | Migrated from tmtf.c. 
*           Major revision: Comments, Structs, idle data, overflow, API.
*******************************************************************************/

#include "tm_sdlp.h"

static int32 TM_SDLP_AddData(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer, 
                             uint8 *pData, uint16 dataLength, bool isPacket);
static int32 TM_SDLP_CopyToOverflow(TM_SDLP_OverflowInfo_t *pOverflow, 
                                    uint8 *data, uint16 length, 
                                    bool isPartial);
static int32 TM_SDLP_CopyFromOverflow(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer);



/*****************************************************************************/
/** \brief TMTF_SDLP_InitIdlePacket
******************************************************************************/
int32 TM_SDLP_InitIdlePacket(CFE_MSG_Message_t *pIdlePacket, uint8 *pIdlePattern,
                             uint16 bufferLength, uint32 patternBitLength)
{
    uint8 *pIdleData = NULL;
    uint16 idleDataLength = 0;
    uint32 bit = 0;
    uint16 byte = 0;
    uint16 bitOffset = 0;
    uint16 byteIdx = 0;
    uint16 patternLength = 0;
    int32 iStatus = TM_SDLP_SUCCESS;
    
    if (pIdlePacket == NULL || pIdlePattern == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_InitIdlePacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    if (patternBitLength == 0)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_InitIdlePacket Error: "
                          "Input patternBitLength is 0.");
        
        iStatus = TM_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* Idle packet, as specified in CCSDS 133.0-B-2 */
    CFE_MSG_Init(pIdlePacket, CFE_SB_ValueToMsgId(0x7ffU), bufferLength);

    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    printf("sdlp INIT Setting msg id\n");
    CFE_MSG_GetMsgId(pIdlePacket, &MsgId);
    printf("sdlp INIT MsgID Readback: 0x%04X\n", CFE_SB_MsgIdToValue(MsgId));

    printf("Printing Freshly inited idle packet for the...\n\t");
        for (int i=0; i<1786; i++)
        {
            printf("%02X", *((uint8_t *)pIdlePacket + i));
        }
        printf("\n");
        


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

    printf("Printing Freshly FILLED idle packet for the...\n\t");
        for (int i=0; i<1786; i++)
        {
            printf("%02X", *((uint8_t *)pIdlePacket + i));
        }
        printf("\n");

end_of_function:
    return iStatus;
}


/*****************************************************************************/
/** \brief TMTF_SDLP_InitChannel
******************************************************************************/
int32 TM_SDLP_InitChannel(TM_SDLP_FrameInfo_t *pFrameInfo, 
                          uint8_t *pTfBuffer, uint8 *pOverflowBuffer,
                          TM_SDLP_GlobalConfig_t *pGlobalConfig, 
                          TM_SDLP_ChannelConfig_t *pChannelConfig)
{
    int32 iStatus = TM_SDLP_SUCCESS;
    int32 dataFieldLength;
    uint16 dataFieldOffset;
    uint16 secHdrLength;
    uint16 gvcid = 0;
    uint8  sdlsSecurityHeaderLength = 0;
    uint8  sdlsSecurityTrailerLength = 0;
    char mutName[OS_MAX_API_NAME];
    SecurityAssociation_t* sa_ptr = NULL;

    if (pGlobalConfig == NULL || pChannelConfig == NULL || pFrameInfo == NULL ||
        pOverflowBuffer == NULL || pTfBuffer == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_InitChannel Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    secHdrLength = pChannelConfig->secHdrLength;

    /* The secHdr Length must be between 1-63 bytes if present. 
     * (TM_SDLP 4.1.3.1.3) */
    if ((pChannelConfig->fshFlag == true && 
         (secHdrLength > TMTF_SECHDR_MAX_LENGTH || secHdrLength < 1)) ||
        (pChannelConfig->fshFlag == false && secHdrLength != 0))        
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_InitChannel Error: "
                          "Invalid SecHdrLength:%d", secHdrLength);
        
        iStatus = TM_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    dataFieldLength = (int32) pGlobalConfig->frameLength;
    dataFieldOffset = TMTF_PRIHDR_LENGTH;

    if (secHdrLength > 0)
    {
        dataFieldOffset += secHdrLength + 1;
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
        // CODE REVIEW - Use of MAP_IDs seems non-correct. They exist for TC specifically, but somehow overtime
        // we've morphed and have a TYPE_TC and TYPE_TM enum - realistically MAP_IDs are a set of allowable values
        // this might take some figurin'
        iStatus = sa_if->sa_get_operational_sa_from_gvcid(0, (uint16)pGlobalConfig->scId, (uint16)pChannelConfig->vcId, 0, &sa_ptr);
 
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
        dataFieldLength -= TMTF_OCF_LENGTH;
    }

    if (pGlobalConfig->hasErrCtrl == true)
    {
        dataFieldLength -= TMTF_ERR_CTRL_FIELD_LENGTH;
    }

// #ifdef TM_DEBUG
    printf("TM_SDLP Initializing channel:\n");
    printf("\t Total Frame length set to: %d\n", pGlobalConfig->frameLength);
    printf("\t Primary header length: \t%d\n", TMTF_PRIHDR_LENGTH);
    printf("\t Secondary header length: \t%d\n", secHdrLength);
    printf("\t\t SPI Length: 2 bytes\n");
    printf("\t\t IV Length: %d bytes\n", sa_ptr->shivf_len);
    printf("\t\t SNF Length Length: %d bytes\n", sa_ptr->shsnf_len);
    printf("\t\t PLF Length: %d bytes\nEnable", sa_ptr->shplf_len);
    printf("\t Security header length: \t%d\n", sdlsSecurityHeaderLength);
    printf("\t Data field offset: \t%d\n", dataFieldOffset);
    printf("\t Data field length: \t%d\n", dataFieldLength);
    printf("\t Security trailer MAC length: \t%d\n", sdlsSecurityTrailerLength);
    if (pChannelConfig->ocfFlag == true)
    {
        printf("\t OCF Length: \t%d\n", TMTF_OCF_LENGTH);
    }
    else 
    {
        printf("\t OCF Length: \t%d\n", 0);
    }
    if (pGlobalConfig->hasErrCtrl == true)
    {
        printf("\t FECF length: \t%d \n", TMTF_ERR_CTRL_FIELD_LENGTH);
    }
    else
    {
        printf("\t FECF length: \t 0");
    }
// #endif

    if (dataFieldLength < 0)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_InitChannel Error: "
                          "Invalid Length Configuration.");
        
        iStatus = TM_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* Update the Transfer Frame Info */
    pFrameInfo->dataFieldLength     = (uint16) dataFieldLength;
    pFrameInfo->dataFieldOffset     = dataFieldOffset;
    pFrameInfo->ocfOffset           = dataFieldOffset + (uint16) dataFieldLength;
    pFrameInfo->freeOctets          = pFrameInfo->dataFieldLength;
    pFrameInfo->currentDataOffset   = pFrameInfo->dataFieldOffset;
    pFrameInfo->globConfig          = pGlobalConfig;
    pFrameInfo->chnlConfig          = pChannelConfig;
    pFrameInfo->frame               = (TMTF_PriHdr_t *) pTfBuffer; 
    pFrameInfo->isFirstHdrPtrSet    = false;
    pFrameInfo->isReady             = false;

    if (pChannelConfig->ocfFlag == true)
    {
        pFrameInfo->errCtrlOffset   = pFrameInfo->ocfOffset + TMTF_OCF_LENGTH;
    }
    else
    {
        pFrameInfo->errCtrlOffset   = pFrameInfo->ocfOffset;
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
    TMTF_SetScId(pFrameInfo->frame, pGlobalConfig->scId);
    TMTF_SetVcId(pFrameInfo->frame, pChannelConfig->vcId);
    TMTF_SetOcfFlag(pFrameInfo->frame, pChannelConfig->ocfFlag);

    /* Initialize the Overflow buffer */
    CFE_PSP_MemSet((void *)pOverflowBuffer, 0, pChannelConfig->overflowSize);
    
    /* Set secondary header flag and sec hdr length */
    if (pChannelConfig->fshFlag == true)
    {
        TMTF_SetSecHdrFlag(pFrameInfo->frame, 1);
        TMTF_SetSecHdrLength(pFrameInfo->frame, secHdrLength);
    }

    /* If we are using the VCP service (CCSDS packets) [TM_SDLP 4.1.2.7] 
     * - Sync Flag set to 0
     * - Packet Order flag set to 0
     * - Segmentation Length ID: must be binary '11' 
     *   */  
    if (pChannelConfig->dataType == 0)
    {
        TMTF_SetSyncFlag(pFrameInfo->frame, 0);
        TMTF_SetPacketOrderFlag(pFrameInfo->frame, 0);
        TMTF_SetSegLengthId(pFrameInfo->frame, 3); 
        TMTF_SetFirstHdrPtr(pFrameInfo->frame, TMTF_NO_FIRST_HDR_PTR);
    }
    /* If VCA service is used, set sync flag to 1. All other fields are
     * undefined [TM_SDLP 4.1.2.7] */
    else
    {
        TMTF_SetSyncFlag(pFrameInfo->frame, 1);
    }

    /* Create the Mutex */
    gvcid = TMTF_GetGlobalVcId(pFrameInfo->frame);
    sprintf(mutName, "TF Global VC ID %d", gvcid);
    OS_MutSemCreate(&pFrameInfo->mutexId, mutName, 0); 

    pFrameInfo->isInitialized = true;

end_of_function:
    return iStatus;
}


/*****************************************************************************/
/** \brief TMTF_SDLP_FrameHasData
******************************************************************************/
int32 TM_SDLP_FrameHasData(TM_SDLP_FrameInfo_t *pFrameInfo)
{
    int32 hasData = 0;

    if (pFrameInfo == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_FrameHasData Error: "
                          "Input Pointer is Null.");
        
        hasData = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

// #ifdef TM_DEBUG
    printf("*** DATA LENGTH INFO!***\n");
    printf("*** Free Octets: %d\n", pFrameInfo->freeOctets);
    printf("*** dataFieldLength: %d\n", pFrameInfo->dataFieldLength);
// #endif
    
    if (pFrameInfo->freeOctets < pFrameInfo->dataFieldLength)
    {
        hasData = 1;
    }

end_of_function:
    return hasData;
}


/******************************************************************************/
/** \brief TM_SDLP_AddPacket
*******************************************************************************/
int32 TM_SDLP_AddPacket(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer, CFE_MSG_Message_t *pPacket)
{
    size_t length = 0;
    int32 iStatus = TM_SDLP_SUCCESS;

    printf("Inside TM SDLP AddPacket...\n");

    if (pFrameInfo == NULL || pPacket == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddPacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddPacket Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    CFE_MSG_GetSize(pPacket, &length);

    // printf(" \\/ \\/ \\/ \\/ \\/ \n");
    // printf(" Current frame in memory is:\n\t");
    // for (int i=0; i < (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset); i++)
    // {
    //     // printf("%02X", *(uint8 *)(pFrameInfo->frame+i));
    //     printf("%02X", *(((uint8 *)pFrameInfo->frame)+i));
    // }
    // // for (int i=0; i < (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset); i++)
    // // {
    // //     printf("%02X", *(uint8 *)(pFrameInfo->frame+i));
    // // }
    // printf("\n\n");

    // // printf("Current size of frame including header is %d bytes\n",  (pFrameInfo->dataFieldLength - pFrameInfo->freeOctets));
    // printf("Current size of frame including header is %d bytes\n",  (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset));

    // printf("Preparing to copy in the %ld byte following frame:\n\t", length);
    // for (int i=0; i < length; i++)
    // {
    //     printf("%02X", *((uint8_t *)pPacket + i));
    // }
    // printf("\n\n");
    printf("AT: %d\n", __LINE__);
    OS_MutSemTake(pFrameInfo->mutexId);
    iStatus = TM_SDLP_AddData(pFrameInfo, pBuffer, (uint8 *) pPacket, length, true);
    OS_MutSemGive(pFrameInfo->mutexId);

    // // printf(" NEW %d byte frame in memory is:\n\t", (pFrameInfo->dataFieldLength - pFrameInfo->freeOctets));
    // printf(" NEW %d byte frame in memory is:\n\t", (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset));
    // for (int i=0; i < (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset); i++)
    // {
    //     // printf("%02X", *((uint8 *)(pFrameInfo->frame)+i));
    //     printf("%02X", *(((uint8 *)pFrameInfo->frame)+i));
    // }
    // printf("\n /\\ /\\ /\\ /\\ \n");

end_of_function:
    return iStatus;
}

                           
/******************************************************************************/
/** \brief TM_SDLP_AddIdlePacket
*******************************************************************************/
int32 TM_SDLP_AddIdlePacket(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer,
                            CFE_MSG_Message_t *pIdlePacket)
{
    int32 iStatus = TM_SDLP_SUCCESS;
    uint16 lengthToCopy = 0;
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    printf("Inside Idle Packet...\n");

    if (pFrameInfo == NULL || pIdlePacket == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddIdlePacket Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }

    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddIdlePacket Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }

    // printf("Getting mutex\n");
    OS_MutSemTake(pFrameInfo->mutexId);
    // printf("Got mutex\n");
    
    lengthToCopy = pFrameInfo->freeOctets;
    printf("AddIdle length to copy %d\n", lengthToCopy);

    /* If no free octets, no idle data to add. Done. */
    if (lengthToCopy == 0)
    {
        printf("IdlePacket - no free octets!\n");
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = TM_SDLP_SUCCESS;
        goto end_of_function;
    }
    /* Minimum length of idle packet is 7. */
    else if (lengthToCopy < 7)
    {
        printf("tm_sdlp addidle short length\n");
        lengthToCopy = 7;
    }

    /* The Message ID of the idle buffer should always be 0x3ff (Idle Packet). */
    printf("sdlp Getting msg id\n");
    CFE_MSG_GetMsgId(pIdlePacket, &MsgId);
    printf("sdlp Got msg id\n");
    if (CFE_SB_MsgIdToValue(MsgId) != 0x7ffU)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddIdlePacket Error: "
                          "The IdlePacket has MsgId other than 0x3ff.");
        printf("IDLE PACKET MSGID SET TO 0x%04X\n", CFE_SB_MsgIdToValue(MsgId));

        printf("Printing idle packet for the luls...\n\t");
        for (int i=0; i<1786; i++)
        {
            printf("%02X", *((uint8_t *)pIdlePacket + i));
        }
        printf("\n");
        
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = TM_SDLP_ERROR;
        goto end_of_function;
    }

    printf("addidle getting size...\n");
    /* Set the idlePacket length in header to lenghtToCopy */
    CFE_MSG_SetSize(pIdlePacket,  lengthToCopy);
    
    /* Add the idle packet. May spill over to overflow buffer. */
    /* iStatus should always return 0 free-octet if successful. */
    printf("IdlePacket calling AddData!\n");
    printf("AT: %d\n", __LINE__);
    iStatus = TM_SDLP_AddData(pFrameInfo, pBuffer, (uint8 *) pIdlePacket, lengthToCopy, 
                              true);
    // printf("Reprinting idle packet....\n");
    //     for (int i=0; i<1786; i++)
    //     {
    //         printf("%02X", *((uint8_t *)pIdlePacket + i));
    //     }
    //     printf("\n");
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}    


/******************************************************************************/
/** \brief TM_SDLP_AddVcaData
*******************************************************************************/
int32 TM_SDLP_AddVcaData(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer,
                         uint8 *pData, uint16 dataLength)
{
    int32 iStatus = TM_SDLP_SUCCESS;

    if (pFrameInfo == NULL || pData == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddVcaData Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if the frame is initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_AddVcaData Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    printf("AT: %d\n", __LINE__);
    OS_MutSemTake(pFrameInfo->mutexId);
    iStatus = TM_SDLP_AddData(pFrameInfo, pBuffer, pData, dataLength, false);
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief TM_SDLP_StartFrame
*******************************************************************************/
int32 TM_SDLP_StartFrame(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer) 
{
    uint16 lengthToCopy = 0;
    uint16 lengthCopied = 0;
    TM_SDLP_OverflowInfo_t *pOverflow;
    int32 iStatus = TM_SDLP_SUCCESS;
    
    if (pFrameInfo == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_StartFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_StartFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);
    
    /* If the frame is already started, issue a warning. */
    if (pFrameInfo->isReady == true)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_INFORMATION,
                          "TM_SDLP_StartFrame: "
                          "The frame was already started. Will continue.");
    }
    
    /* Set Frame as ready */
    pFrameInfo->isReady = true;

    // Account for the header as part of the frame
    printf("PRINTING FreeOctets before updating! : %d\n", pFrameInfo->freeOctets);
    pFrameInfo->freeOctets = pFrameInfo->freeOctets; // - pFrameInfo->dataFieldOffset;
    printf("NEW FRAME total length is %d, free octets available are %d\n", pFrameInfo->freeOctets+pFrameInfo->dataFieldOffset, pFrameInfo->freeOctets);
    printf("NEW FRAME dataFieldOffset seems to be %d\n", pFrameInfo->dataFieldOffset);
    
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
            lengthCopied = TM_SDLP_CopyFromOverflow(pFrameInfo, pBuffer);
            lengthToCopy -= lengthCopied;
        }
    }

    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief TM_SDLP_SetOidFrame
*******************************************************************************/
int32 TM_SDLP_SetOidFrame(TM_SDLP_FrameInfo_t *pFrameInfo,
                          CFE_MSG_Message_t *pIdlePacket)
{
    int32 iStatus = TM_SDLP_SUCCESS;
    uint8 *pIdleData = NULL;
    
    if (pFrameInfo == NULL || pIdlePacket == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_SetOidFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_SetOidFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }

    OS_MutSemTake(pFrameInfo->mutexId);

    /* If the frame is not empty, This method should not be called. */
    if (pFrameInfo->freeOctets != pFrameInfo->dataFieldLength)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_SetOidFrame Error: "
                          "The frame is not empty. VC ID:%u",
                          pFrameInfo->chnlConfig->vcId);
        
        OS_MutSemGive(pFrameInfo->mutexId);
        iStatus = TM_SDLP_ERROR;
        goto end_of_function;
    }

    pIdleData = CFE_SB_GetUserData(pIdlePacket);    

    // TODO - Consider if accessing header this way is best, ensure we're doing it consistently
    TMTF_SetFirstHdrPtr(pFrameInfo->frame, TMTF_OID_FIRST_HDR_PTR);
    pFrameInfo->isFirstHdrPtrSet = true;
    printf("AT: %d\n", __LINE__);
    iStatus = TM_SDLP_AddData(pFrameInfo, (uint8_t *)pFrameInfo->frame, pIdleData, pFrameInfo->dataFieldLength, 
                              false);
    
    OS_MutSemGive(pFrameInfo->mutexId);

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief TM_SDLP_CompleteFrame
*******************************************************************************/
int32 TM_SDLP_CompleteFrame(TM_SDLP_FrameInfo_t *pFrameInfo,
                            uint8 *pMcFrameCnt, uint8 *pOcf)
{
    //uint8 vcFrameCnt = 0;
    int32 iStatus = TM_SDLP_SUCCESS;

    if (pFrameInfo == NULL || pMcFrameCnt == NULL) 
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_CompleteFrame Error: "
                          "Input Pointer is Null.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    /* Check if frame has been initialized */
    if (pFrameInfo->isInitialized == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_CompleteFrame Error: "
                          "The channel is not initialized.");
        
        iStatus = TM_SDLP_FRAME_NOT_INIT;
        goto end_of_function;
    }
    
    if (pFrameInfo->chnlConfig->ocfFlag == true && pOcf == NULL)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP_CompleteFrame Error: "
                          "The Input OCF Pointer is NULL.");
        
        iStatus = TM_SDLP_INVALID_POINTER;
        goto end_of_function;
    }
    
    OS_MutSemTake(pFrameInfo->mutexId);

    /* Increment the master channel frame count */
    *pMcFrameCnt = *pMcFrameCnt + 1;
    TMTF_SetMcFrameCount(pFrameInfo->frame, *pMcFrameCnt);

    /* Increment VC frame count if it is a virtual channel */
    //if (pFrameInfo->chnlConfig->isMaster == false)
    //{
    //    vcFrameCnt = TMTF_IncrVcFrameCount(pFrameInfo->frame);
    //}

    /* If an OCF Field is present, set it. */
    if (pFrameInfo->chnlConfig->ocfFlag)
    {
        TMTF_SetOcf(pFrameInfo->frame, pOcf, pFrameInfo->ocfOffset);
    }
   
    /* If an ErrCtrl Field is present, set it. */
    if (pFrameInfo->globConfig->hasErrCtrl == 1)
    {
        printf("Updating ErrCtrlField!!!! ****\n");
        TMTF_UpdateErrCtrlField(pFrameInfo->frame, pFrameInfo->errCtrlOffset);
    }
    
    /* This may happen if the frame is filled by a partial Packet */
    if (pFrameInfo->isFirstHdrPtrSet == false)
    {
        TMTF_SetFirstHdrPtr(pFrameInfo->frame, TMTF_NO_FIRST_HDR_PTR);
    }

    /* Reset frame metadata */
    pFrameInfo->freeOctets          = pFrameInfo->dataFieldLength;
    pFrameInfo->currentDataOffset   = pFrameInfo->dataFieldOffset;
    pFrameInfo->isFirstHdrPtrSet    = false;
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
*       - Lower level function called by AddPacket, AddIdlePacket, AddVcaData
*       - Data unit will be segmented into overflow buffer if frame is full.
*       - Transfer frame will be populated with overflow data if frame is 
*         empty prior to adding data.
*       - isPacket flag is used to set the First Header Pointer to the start 
*         of the suplied packet.
*
*   \param[in,out] pFrameInfo  Pointer to the Frame info/working struct.
*   \param[in]     pData       Pointer to data buffer
*   \param[in]     dataLength  Length of data to copy
*   \param[in]     isPacket    Data is a packet (VCP PDU / Idle packet)
*
*   \return Frame FreeOctets
*   \return TM_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return TM_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see
*       #TM_SDLP_AddPacket
*       #TM_SDLP_AddIdlePacket
*******************************************************************************/
static int32 TM_SDLP_AddData(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer, uint8 *pData, 
                             uint16 dataLength, bool isPacket)
{
    uint16 lengthToCopy = dataLength;
    int32 iStatus = TM_SDLP_SUCCESS;
    bool isPartial = false;

    printf("Inside add data!\n");

    /* Check if the frame is ready to add new data. */
    if (pFrameInfo->isReady == false)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP Error: "
                          "The Frame is not ready. Call StartFrame().");
        
        iStatus = TM_SDLP_FRAME_NOT_READY;
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

        iStatus = TM_SDLP_CopyToOverflow(&pFrameInfo->overflowInfo,
                                      pData + lengthToCopy, 
                                      dataLength - lengthToCopy, isPartial);
        if (iStatus < 0)
        {
            goto end_of_function;
        }
    }

    printf(" \\/ \\/ \\/ \\/ \\/ \n");
    printf(" Current frame in memory is:\n\t");
    for (int i=0; i < pFrameInfo->currentDataOffset; i++)
    // for (int i=0; i < (pFrameInfo->freeOctets - pFrameInfo->dataFieldOffset); i++)
    {
        // printf("%02X", *(uint8 *)(pFrameInfo->frame+i));
        printf("%02X", *(((uint8 *)pFrameInfo->frame)+i));
    }
    // for (int i=0; i < (pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset); i++)
    // {
    //     printf("%02X", *(uint8 *)(pFrameInfo->frame+i));
    // }
    printf("\n\n");

    // printf("FREE OCTET METHOD: Current size of frame including header is %d bytes\n",  (pFrameInfo->dataFieldLength - pFrameInfo->freeOctets));
    // printf("FREE OCTET METHOD: Current size of frame including header is %d bytes\n",  (pFrameInfo->freeOctets - pFrameInfo->dataFieldOffset));
    printf("Current size of frame including header is %d bytes\n",  pFrameInfo->currentDataOffset);

    printf("Preparing to copy in the %d byte following frame:\n\t", dataLength);
    for (int i=0; i < dataLength; i++)
    {
        printf("%02X", *((uint8_t *)pData + i));
    }
    printf("\n\n");

    printf("Copying into currentDataOffset of: %d\n", pFrameInfo->currentDataOffset);
    // CFE_PSP_MemCpy((void *) (((uint8 *)pFrameInfo->frame) + pFrameInfo->dataFieldOffset + pFrameInfo->currentDataOffset), 
                //    pData, lengthToCopy);
    CFE_PSP_MemCpy((void *) (((uint8 *)pFrameInfo->frame) + pFrameInfo->currentDataOffset), 
                   pData, lengthToCopy);

    printf("Immediately after memcpy, idle packet starts with:\n\t");
    for (int i=0; i < 20; i++)
    {
        printf("%02X", *((uint8_t *)pData + i));
    }
    printf("\n\n");
    pFrameInfo->freeOctets -= lengthToCopy;

    if ((isPacket == true) && (pFrameInfo->isFirstHdrPtrSet == false))
    {
        printf("prepping to set first header pointer...\n");
        uint16 firstHdrPtr = pFrameInfo->currentDataOffset - 
                             pFrameInfo->dataFieldOffset;
        printf("Setting first header pointer to index %d\n", firstHdrPtr);
        TMTF_SetFirstHdrPtr(pFrameInfo->frame, firstHdrPtr);
        pFrameInfo->isFirstHdrPtrSet = true;
    }
    pFrameInfo->currentDataOffset += lengthToCopy;

    // printf(" NEW %d byte frame in memory is:\n\t", (pFrameInfo->dataFieldLength - pFrameInfo->freeOctets));
    printf(" NEW %d byte frame in memory is:\n\t", pFrameInfo->currentDataOffset);
    for (int i=0; i < pFrameInfo->currentDataOffset; i++)
    {
        // printf("%02X", *((uint8 *)(pFrameInfo->frame)+i));
        printf("%02X", *(((uint8 *)pFrameInfo->frame)+i));
    }
    printf("\n /\\ /\\ /\\ /\\ \n");

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
*   \return TM_SDLP_SUCCESS             If successful.
*   \return TM_SDLP_INVALID_LENGTH      If an input length is invalid
*   \return TM_SDLP_OVERFLOW_FULL       If overflow buffer is full
*
*   \see 
*       #TM_SDLP_AddData
*******************************************************************************/
static int32 TM_SDLP_CopyToOverflow(TM_SDLP_OverflowInfo_t *pOverflow, uint8 *data, 
                                    uint16 length, bool isPartial)
{
    int32 iStatus = TM_SDLP_SUCCESS;
    uint16 lengthToEnd;

    if (length > pOverflow->buffSize)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_ERROR,
                          "TM_SDLP ERROR: "
                          "Message Length too large for overflow Buffer.");
       
        iStatus = TM_SDLP_INVALID_LENGTH;
        goto end_of_function;
    }

    /* If the buffer is full, drop message. */
    if (length > pOverflow->freeOctets)
    {
        CFE_EVS_SendEvent(IO_LIB_TM_SDLP_EID, CFE_EVS_EventType_INFORMATION,
                          "TM_SDLP Warning: "
                          "The Frame's OverflowBuffer is Full. Message Dropped.");
       
       iStatus = TM_SDLP_OVERFLOW_FULL;
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
*       #TM_SDLP_StartFrame
*******************************************************************************/
static int32 TM_SDLP_CopyFromOverflow(TM_SDLP_FrameInfo_t *pFrameInfo, uint8_t *pBuffer)
{
    uint8 msgHdr[6];
    CFE_MSG_Message_t* MsgPtr = (CFE_MSG_Message_t*) msgHdr;
    size_t lengthToCopy;
    size_t lengthToEnd;
    bool setHeader;
    size_t freeOctets;
    TM_SDLP_OverflowInfo_t *pOverflow = &pFrameInfo->overflowInfo;

    lengthToEnd = pOverflow->buffSize - 
                  (pOverflow->dataStart - pOverflow->buffer);

    freeOctets = pFrameInfo->freeOctets;

    /* First Determine how many octets to copy from overflow buffer. */
    
    /* If there are partial octets, copy those. */
    if (pFrameInfo->overflowInfo.partialOctets > 0)
    {
        lengthToCopy = pFrameInfo->overflowInfo.partialOctets;
        setHeader = false;
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
        
        setHeader = true;
    }

    /* Only copy as many octets as TF has free octets available */
    if (freeOctets < lengthToCopy)
    {
        /* If we are passing a full packet, set the new value of partialOctets */
        if (setHeader == true)
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
    else if (setHeader == false)
    {
        printf("AT: %d\n", __LINE__);
        pFrameInfo->overflowInfo.partialOctets = 0;
    }

    /* If the length to the end is greater than the length to copy, copy it. */
    if (lengthToEnd > lengthToCopy)
    {
        printf("AT: %d\n", __LINE__);
        freeOctets = TM_SDLP_AddData(pFrameInfo, pBuffer, pOverflow->dataStart, 
                                     lengthToCopy, setHeader);
        pOverflow->dataStart += lengthToCopy;
    }
    /* Wrap around overflow buffer if required */
    else
    {
        printf("AT: %d\n", __LINE__);
        freeOctets = TM_SDLP_AddData(pFrameInfo, pBuffer, pOverflow->dataStart, 
                                  lengthToEnd, setHeader);
        freeOctets = TM_SDLP_AddData(pFrameInfo, pBuffer, pOverflow->buffer, 
                                  lengthToCopy - lengthToEnd, setHeader);
        pOverflow->dataStart = pOverflow->buffer + 
                                 (lengthToCopy - lengthToEnd);
    }
    
    /* Revise the number of free Octets in overflow buffer */
    pOverflow->freeOctets += lengthToCopy;
    pFrameInfo->freeOctets = freeOctets;

    return lengthToCopy;
}
