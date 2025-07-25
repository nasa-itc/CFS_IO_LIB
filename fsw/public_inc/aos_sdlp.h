/******************************************************************************/
/** \file  aos_sdlp.h
*
*   Copyright 2017 United States Government as represented by the Administrator
*   of the National Aeronautics and Space Administration.  No copyright is
*   claimed in the United States under Title 17, U.S. Code.
*   All Other Rights Reserved.
*
*   \author Alan A Asp, Guy de Carufel (Odyssey Space Research), NASA, JSC, ER6
*
*   \brief Header file for aos_SDLP Protocol
*
*   \par Limitations, Assumptions, External Events, and Notes:
*       - This library provides a service interface to the aosTF protocol.
*       - This implementation is based on Chapter 4 - without SDLS option.
*       - The VCA service is user defined
*       - The maximum frame length is user defined, and is dependent on the
*         channel coding startegy used.
*       - User may use the IO_LIB_UTIL_GenPseudoRandomSeq to generate an idle 
*         data sequence.
*
*   \par Modification History:
*     - 2015-04-26 | Alan A. Asp | OSR | Code Started (originally in aostf.h)
*     - 2015-10-22 | Guy de Carufel | OSR | Migrated from aostf.h. 
*           Major revision: structs, idle data, overflow, API.
*******************************************************************************/

#ifndef _AOS_SDLP_H_
#define _AOS_SDLP_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
** Includes
*******************************************************************************/
#include "io_lib.h"
#include "aostf.h"
#include "crypto.h"


/*******************************************************************************
** Macro Definitions
*******************************************************************************/
#define AOS_SDLP_SUCCESS                (0)
#define AOS_SDLP_ERROR                 (-1)
#define AOS_SDLP_INVALID_POINTER       (-2)
#define AOS_SDLP_INVALID_LENGTH        (-3)
#define AOS_SDLP_FRAME_NOT_INIT        (-4)
#define AOS_SDLP_FRAME_NOT_READY       (-5)
#define AOS_SDLP_OVERFLOW_FULL         (-6)

/*******************************************************************************
** Structure definitions
*******************************************************************************/
/** Following Structure is the user defined managed / configuration parameters 
 *  for all Transfer Frames over the physical channel. */
typedef struct
{
    uint16  scId;                /* Spacecraft ID (8 bits)                    */
    uint16  frameLength;         /* The length of the frame                   */
    uint8   hasFecf;             /* Has the Frame Error Control Field         */
    uint8   hasFhec;             /* Has the Frame Header Error Control        */
    uint8   hasInsertZone;       /* Has the Insert Zone                       */
    uint16  insertZoneLength;    /* Length of Insert Zone if present          */
} AOS_SDLP_GlobalConfig_t;
    

/** Following Structure is the user defined managed / configuration parameters  
    for a a specific Virtual Channel */
typedef struct
{
    uint8   vcId;                /* Virtual channel ID (6 bits, 0-62)         */
    uint8   dataType;            /* Type-0: M_PDU(packet), Type-1: B_PDU, Type-2: VCA_SDU */
    uint8   ocfFlag;             /* The value of the OCF flag (0/1)           */
    uint8   replayFlag;          /* Replay flag setting                       */
    uint8   vcFrameCountUsage;   /* VC Frame Count Usage Flag                 */
    uint8   vcFrameCountCycle;   /* VC Frame Count Cycle (4 bits)             */
    uint16  overflowSize;        /* Size of overflow buffer                   */
} AOS_SDLP_ChannelConfig_t;


/** Working parameters for overflow buffer */
typedef struct
{
    uint16  buffSize;            /* Overflow buffer size                      */
    uint16  freeOctets;          /* number of free octets remaining in buffer */
    uint16  partialOctets;       /* Partial octets at the start of the 
                                    overflow queue                            */
    uint8 *dataStart;            /* Pointer to start of data (queue start)    */
    uint8 *dataEnd;              /* Pointer to end of data (queue end)        */
    uint8 *buffer;               /* Pointer to overflow buffer                */
} AOS_SDLP_OverflowInfo_t;


/** Working paramters of frame */
typedef struct
{
    uint16  dataFieldLength;     /* Length of the data field                  */
    uint16  dataFieldOffset;     /* offset in octets from the start of frame 
                                    to the data field                         */
    uint16  insertZoneOffset;    /* Offset in octets from the start of frame
                                    to the Insert Zone                        */
    uint16  ocfOffset;           /* Offset in octets from the start of frame 
                                    to the OCF                                */
    uint16  fecfOffset;          /* Offset in octets from the start of frame 
                                    to the Frame Error Control Field          */
    uint16  freeOctets;          /* Number of free octets remaining in the 
                                    data field                                */
    uint16  currentDataOffset;   /* Offset to next free data field octet from 
                                    start of frame                            */
    uint32  mutexId;             /* The mutex ID to protect the TF buffer 
                                    and overflow buffer                       */
    uint32  vcFrameCount;        /* 24-bit VC Frame Counter                   */
    bool isReady;             /* Indicates the TF is ready to add data     */
    bool isInitialized;       /* Indicates the TF is initialized           */
    AOS_SDLP_OverflowInfo_t  overflowInfo;   /* Overflow Info Structure        */
    AOS_SDLP_GlobalConfig_t  *globConfig;    /* Pointer to global config       */
    AOS_SDLP_ChannelConfig_t *chnlConfig;    /* Pointer to channel config      */
    AOSTF_PriHdr_t           *frame;         /* Pointer to Transfer frame      */
} AOS_SDLP_FrameInfo_t;



/*******************************************************************************
** Function Declarations
*******************************************************************************/
/******************************************************************************/
/** \brief Initialize the Idle Data
*
*   \par Description/Algorithm
*       Initializes an Idle Data Buffer with a repeating pattern sequence. 
*
*   \par Assumptions, External Events, and Notes:
*       - The Idle Data is used for filling unused portions of AOS frames
*       - User may use the IO_LIB_UTIL_GenPseudoRandomSeq function to generate 
*         idle data pattern. Pattern should be "sufficiently" random.
*       - The Idle data buffer length must be at least as large as the 
*         frameLength.
*       - The IdleData is used in both AddIdleData and SetIdleFrame
*
*   \param[in,out] pIdleData         Pointer to the Idle Data Buffer.
*   \param[in]     pIdlePattern      A bit pattern to repeat in idle data
*   \param[in]     bufferLength      Length of the Idle Buffer in bytes.
*   \param[in]     patternBitLength  Length of the repeating pattern in bits.
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*   \return AOS_SDLP_INVALID_LENGTH      If an input length is invalid
*
*   \see 
*       #AOS_SDLP_AddData
*       #AOS_SDLP_AddIdlePacket
*       #AOS_SDLP_SetIdleFrame
*       #IO_LIB_UTIL_GenPseudoRandomSeq
*******************************************************************************/
int32 AOS_SDLP_InitIdlePacket(CFE_MSG_Message_t *pIdleData, uint8 *pIdlePattern,
                            uint16 bufferLength, uint32 patternBitLength);

/******************************************************************************/
/** \brief Initialize a specific Virtual Channel
*
*   \par Description/Algorithm
*       This function will populate the Channel and overflow info structures 
*       based on provided configuration data and buffer pointers.
*
*   \par Assumptions, External Events, and Notes:
*       - The idle data sequence must be >= frame data field length.
*       - Insert Zone is optional and configured via global config
*
*   \param[out] pFrameInfo     Pointer to the Frame info/working struct.
*   \param[out] pTfBuffer      Pointer to the Transfer Frame buffer 
*   \param[out] pOfBuffer      Pointer to the Overflow buffer
*   \param[in] pGlobalConfig   Pointer to the Global configuration struct.
*   \param[in] pChannelConfig  Pointer to the Channel configuration struct.
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*   \return AOS_SDLP_INVALID_LENGTH      If frame length is too short
*
*   \see
*       #AOSTF_SetReplayFlag
*       #AOSTF_SetVcFrameCountUsageFlag
*       #AOSTF_SetVcFrameCountCycle
*******************************************************************************/
int32 AOS_SDLP_InitChannel(AOS_SDLP_FrameInfo_t *pFrameInfo, 
                           uint8 *pTfBuffer, uint8 *pOfBuffer,
                           AOS_SDLP_GlobalConfig_t *pGlobalConfig, 
                           AOS_SDLP_ChannelConfig_t *pChannelConfig);

                          
/******************************************************************************/
/** \brief Check if frame currently has data
*
*   \par Description/Algorithm
*       This function will return whether the frame has data or not
*
*   \par Assumptions, External Events, and Notes:
*       - Function does not check if frame has been initalized or started.
*
*   \param[in] pFrameInfo  Pointer to the Frame info/working struct.
*
*   \return Frame has Data (0=no, 1=yes)
*   \return AOS_SDLP_INVALID_POINTER    If a input pointer is NULL
*
*   \see 
*       #AOS_SDLP_AddData
*******************************************************************************/
int32 AOS_SDLP_FrameHasData(AOS_SDLP_FrameInfo_t *pFrameInfo);


/******************************************************************************/
/** \brief Add a Packet to Transfer Frame (M_PDU)
*
*   \par Description/Algorithm
*       This function will add a CFE packet to a provided transfer frame as
*       a Multiplexing Protocol Data Unit (M_PDU).
*
*   \par Assumptions, External Events, and Notes:
*       - A CFE packet becomes an M_PDU in AOS terminology
*       - This function calls AOS_SDLP_AddData
*       - Multiversion multiplexing is not-implemented
*
*   \param[in,out] pFrameInfo  Pointer to the Frame info/working struct.
*   \param[in]     pPacket     Pointer to the CFE Packet (M_PDU)
*
*   \return Frame FreeOctets
*   \return AOS_SDLP_INVALID_POINTER    If a input pointer is NULL
*   \return AOS_SDLP_FRAME_NOT_INIT     If frame has not been initialized
*   \return AOS_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return AOS_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see 
*       #AOS_SDLP_AddData
*******************************************************************************/
int32 AOS_SDLP_AddPacket(AOS_SDLP_FrameInfo_t *pFrameInfo,
                         CFE_MSG_Message_t *pPacket);
                           

/******************************************************************************/
/** \brief Add Bitstream Data to Transfer Frame (B_PDU)
*
*   \par Description/Algorithm
*       This function will add bitstream data to a provided transfer frame as
*       a Bitstream Protocol Data Unit (B_PDU).
*
*   \par Assumptions, External Events, and Notes:
*       - B_PDU is used for bitstream data that doesn't follow packet structure
*       - This function calls AOS_SDLP_AddData
*
*   \param[in,out] pFrameInfo  Pointer to the Frame info/working struct.
*   \param[in]     pData       Pointer to bitstream data
*   \param[in]     dataLength  Length of bitstream data
*
*   \return Frame FreeOctets
*   \return AOS_SDLP_INVALID_POINTER    If a input pointer is NULL
*   \return AOS_SDLP_FRAME_NOT_INIT     If frame has not been initialized
*   \return AOS_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return AOS_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see 
*       #AOS_SDLP_AddData
*******************************************************************************/
int32 AOS_SDLP_AddBitstreamData(AOS_SDLP_FrameInfo_t *pFrameInfo, 
                                uint8 *pData, uint16 dataLength);


/******************************************************************************/
/** \brief Add Idle Data to transfer frame
*
*   \par Description/Algorithm
*       Copies idle data to fill all free octets in TF data field. The data 
*       is based on the supplied idle pattern.
*
*   \par Assumptions, External Events, and Notes:
*       - The Idle data may be segmented if the TF data field does not have 
*         enough space left by saving the extra octets to the overflow buffer.
*       - This function calls AOS_SDLP_AddData
*       - User may use InitIdleData to initialize the Idle Data with a 
*         user specified repeating pattern.
*
*   \param[in,out] pFrameInfo    Pointer to the Frame info/working struct.
*   \param[in]     pIdleData     Pointer to the idle data buffer
*   \param[in]     idleLength    Length of available idle data
*
*   \return AOS_SDLP_SUCCESS            If successful (no free octets).
*   \return AOS_SDLP_INVALID_POINTER    If a input pointer is NULL
*   \return AOS_SDLP_FRAME_NOT_INIT     If frame has not been initialized
*   \return AOS_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return AOS_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see 
*       #AOS_SDLP_AddData
*       #AOS_SDLP_GenPseudoRandomSeq
*******************************************************************************/
int32 AOS_SDLP_AddIdlePacket(AOS_SDLP_FrameInfo_t *pFrameInfo,
                           CFE_MSG_Message_t *pIdleData);


/******************************************************************************/
/** \brief Add a Virtual Channel Access (VCA) PDU to the Transfer Frame
*
*   \par Description/Algorithm
*       Copies a VCA data buffer to the TF data field at the next free octet.
*
*   \par Assumptions, External Events, and Notes:
*       - VCA_SDU is used for virtual channel access service data
*       - The dataLength should be the same for all PDUs added to a specific TF
*
*   \param[in,out] pFrameInfo  Pointer to the Frame info/working struct.
*   \param[in]     pData       Pointer to data buffer
*   \param[in]     dataLength  Length of data to copy
*
*   \return Frame FreeOctets
*   \return AOS_SDLP_INVALID_POINTER    If a input pointer is NULL
*   \return AOS_SDLP_FRAME_NOT_INIT     If frame has not been initialized
*   \return AOS_SDLP_FRAME_NOT_READY    If frame has not been started
*   \return AOS_SDLP_OVERFLOW_FULL      Data dropped. The overflow buffer is full
*
*   \see
*       #AOS_SDLP_AddData
*******************************************************************************/
int32 AOS_SDLP_AddVcaData(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pData, 
                          uint16 dataLength);


/******************************************************************************/
/** \brief Start a transfer frame  
*
*   \par Description/Algorithm
*       Start a new transfer frame by copying any data from the overflow buffer
*       into the empty transfer frame buffer. This readies the frame to accept
*       new data.
*
*   \par Assumptions, External Events, and Notes:
*       - The transfer frame has no data in it's data field prior to call. 
*
*   \param[in,out] pFrameInfo   Pointer to the Frame info/working struct.
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*   \return AOS_SDLP_ERROR               Data field is not empty
*
*   \see 
*       #AOS_SDLP_AddIdlePacket
*******************************************************************************/
int32 AOS_SDLP_StartFrame(AOS_SDLP_FrameInfo_t *pFrameInfo);


/******************************************************************************/
/** \brief Set a Frame with Only Idle Data
*
*   \par Description/Algorithm
*       Copies the Idle Pattern into the frame data field for idle frames.
*
*   \par Assumptions, External Events, and Notes:
*       - The user is responsible for providing an idle buffer with
*         sufficient randomness.
*       - It is recommended that VCID 63 be used for idle frames.
*
*   \param[in,out] pFrameInfo    Pointer to the Frame info/working struct.
*   \param[in]     pIdleData     Pointer to the idle data buffer.
*   \param[in]     idleLength    Length of idle data available
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*
*   \see 
*******************************************************************************/
int32 AOS_SDLP_SetIdleFrame(AOS_SDLP_FrameInfo_t *pFrameInfo,
                            uint8 *pIdleData, uint16 idleLength);


/******************************************************************************/
/** \brief Set Insert Zone Data
*
*   \par Description/Algorithm
*       Copies data to the Insert Zone field of the AOS frame if present.
*
*   \par Assumptions, External Events, and Notes:
*       - Insert Zone is optional in AOS frames
*       - Insert Zone length is configured during initialization
*
*   \param[in,out] pFrameInfo   Pointer to the Frame info/working struct.
*   \param[in]     pInsertData  Pointer to insert zone data
*   \param[in]     dataLength   Length of insert zone data
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*   \return AOS_SDLP_INVALID_LENGTH      If data length exceeds insert zone size
*
*   \see 
*******************************************************************************/
int32 AOS_SDLP_SetInsertZone(AOS_SDLP_FrameInfo_t *pFrameInfo,
                             uint8 *pInsertData, uint16 dataLength);



/******************************************************************************/
/** \brief Complete a Frame to ready for transmission
*
*   \par Description/Algorithm
*       Fills out the final TF information including the frame counters, adds the
*       OCF if available, updates the signaling field, and executes the frame 
*       error control field computation if included.
*
*   \par Assumptions, External Events, and Notes:
*       - This function represents the Virtual Channel Frame (VCF) Service
*       - User is responsible for filling frame with idle data if it is
*         incomplete prior to call. Call AddIdleData or SetIdleFrame.
*
*   \param[in,out] pFrameInfo   Pointer to the Frame info/working struct.
*   \param[in]     pOcf         Pointer to Operational Control Field
*
*   \return AOS_SDLP_SUCCESS             If successful.
*   \return AOS_SDLP_INVALID_POINTER     If a input pointer is NULL
*
*   \see 
*       #AOS_SDLP_AddIdlePacket
*       #AOS_SDLP_SetIdleFrame
*******************************************************************************/
int32 AOS_SDLP_CompleteFrame(AOS_SDLP_FrameInfo_t *pFrameInfo, uint8 *pOcf);


#ifdef __cplusplus
}
#endif

#endif /* _AOS_SDLP_H_ */

/*==============================================================================
** End of file aos_sdlp.h
**============================================================================*/