/*******************************************************************************
** File: AOStf.c
**
** Copyright 2017 United States Government as represented by the Administrator
** of the National Aeronautics and Space Administration.  No copyright is
** claimed in the United States under Title 17, U.S. Code.
** All Other Rights Reserved.
**
** Purpose:
**   Provides functionality to handling telemetry space data link protocol
**   (AOS SDLP) transfer frames (TF).
**
** History:
**   04/26/15, A. Asp, Odyssey Space Research, LLC
**    * Created
 *   10/22/2015, G. de Carufel, Odyssey Space Research, LLC
 *    -AddIdlePacket and CRC computation. Revised overflow buffer as queue.
**
*******************************************************************************/

#include "cfe.h"

#include "../../public_inc/aostf.h"

static uint16 fecfTable[256];    /* CRC Table generated through GenFecfTable */


/*------------------------------------------------------------------------------
 *
 * Macros for reading and writing the fields in a Telemetry Space Data Link
 * Protocol Transfer Frame.  All of the macros are used in a similar way:
 *
 *   AOSTF_RD_xxx(hdr)        -- Read field xxx from TF header.
 *   AOSTF_WR_xxx(hdr,value)  -- Write value to field xxx of TF header.
 *
 * Note that hdr is a reference to the actual TF structure,
 * not to a pointer to the structure.  If using a pointer, one must
 * refer to the structure as *pointer.
 *
 * The AOSTF_WR macros may refer to the 'hdr' more than once; thus
 * the expression for 'hdr' must NOT contain any side effects.
 *
 *----------------------------------------------------------------------------*/
#define AOSTF_WR_TF_VERSION(hdr,val)   ((hdr).Id[0] = ((hdr).Id[0] & 0x3F)    | \
                                                      ((val) << 6))

#define AOSTF_WR_SCID(hdr,val)         ((hdr).Id[0] = (((hdr).Id[0] & 0xC0)   | \
                                                       (((val) >> 2) & 0x3F))), \
                                       ((hdr).Id[1] = (((hdr).Id[1] & 0x3F)   | \
                                                       (((val) & 0x03) << 6)))

#define AOSTF_WR_VCID(hdr,val)         ((hdr).Id[1] = (((hdr).Id[1] & 0xC0)   | \
                                                       ((val) & 0x3F)))

// OCF Flag is not present in AOS frames - OCF is handled differently
// No direct equivalent macro needed

// MCID (Master Channel Identifier) - 10 bits in AOS
#define AOSTF_RD_MCID(hdr)             ((((hdr).Id[0] & 0x3F) << 4) | \
                                        (((hdr).Id[1] & 0xF0) >> 4))

// GVCID equivalent would be MCID + VCID
#define AOSTF_RD_GVCID(hdr)            ((((hdr).Id[0] & 0x3F) << 10) | \
                                        (((hdr).Id[1] & 0xF0) << 2) | \
                                        ((hdr).Id[1] & 0x3F))

// MC Frame Count doesn't exist in AOS - no equivalent

// VC Frame Count - 3 octets (24 bits) in AOS
#define AOSTF_WR_VCFRMCNT(hdr,val)     ((hdr).VcFrameCount[0] = ((val) >> 16) & 0xFF), \
                                       ((hdr).VcFrameCount[1] = ((val) >> 8) & 0xFF),  \
                                       ((hdr).VcFrameCount[2] = (val) & 0xFF)

// Replay Flag - 1 bit in AOS Signaling Field
#define AOSTF_RD_REPLAYFLAG(hdr)       (((hdr).SignalingField & 0x80) >> 7)
#define AOSTF_WR_REPLAYFLAG(hdr,val)   ((hdr).SignalingField = (((val) << 7) | \
                                        ((hdr).SignalingField & 0x7F)))

// VC Frame Count Usage Flag - 1 bit in AOS Signaling Field
#define AOSTF_RD_VCFCNTUSAGEFLAG(hdr)  (((hdr).SignalingField & 0x40) >> 6)
#define AOSTF_WR_VCFCNTUSAGEFLAG(hdr,val) ((hdr).SignalingField = (((val) << 6) | \
                                           ((hdr).SignalingField & 0xBF)))

// Reserved Spare bits - 2 bits in AOS Signaling Field
#define AOSTF_WR_RSVDSPARE(hdr,val)    ((hdr).SignalingField = (((val) << 4) & 0x30) | \
                                        ((hdr).SignalingField & 0xCF))

// VC Frame Count Cycle - 4 bits in AOS Signaling Field
#define AOSTF_WR_VCFCNTCYCLE(hdr,val)  ((hdr).SignalingField = ((val) & 0x0F) | \
                                        ((hdr).SignalingField & 0xF0))

// Frame Header Error Control - 2 octets (optional in AOS)
#define AOSTF_WR_FHEC(hdr,val)         ((hdr).FrameHeaderErrorControl[0] = ((val) >> 8) & 0xFF), \
                                       ((hdr).FrameHeaderErrorControl[1] = (val) & 0xFF)

// CRC equivalent - Frame Error Control Field (optional in AOS)
#define AOSTF_WR_FECF(ptr,val)         (((ptr)[0] = (val >> 8) & 0xFF),\
                                        ((ptr)[1] = (val) & 0xFF))

/* Prototypes for internal functions */
static void   AOSTF_GenFecfTable(uint32 polynomial);


/*
 * Function: AOSTF_LibInit
 *
 * Notes:
 *  -Called by IO_LibInit()
 *
 */
int32 AOSTF_LibInit(void)
{
    AOSTF_GenFecfTable(AOSTF_FECF_POLYNOMIAL);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetVersion
 *
 * Notes:
 *
 */
int32 AOSTF_SetVersion(AOSTF_PriHdr_t *tfPtr, uint16 val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    AOSTF_WR_TF_VERSION(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetScId
 *
 * Notes:
 *
 */
int32 AOSTF_SetScId(AOSTF_PriHdr_t *tfPtr, uint16 val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    AOSTF_WR_SCID(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetVcId
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcId(AOSTF_PriHdr_t *tfPtr, uint16 val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    /* Validate VCID range (0-62, 63 reserved for idle) */
    if (val > 63)
    {
        return AOSTF_INVALID_LENGTH;
    }

    AOSTF_WR_VCID(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetReplayFlag
 *
 * Notes:
 *
 */
int32 AOSTF_SetReplayFlag(AOSTF_PriHdr_t *tfPtr, bool val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    AOSTF_WR_REPLAYFLAG(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetVcFrameCountUsageFlag
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcFrameCountUsageFlag(AOSTF_PriHdr_t *tfPtr, bool val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    AOSTF_WR_VCFCNTUSAGEFLAG(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetVcFrameCountCycle
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcFrameCountCycle(AOSTF_PriHdr_t *tfPtr, uint16 val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    /* Validate 4-bit value */
    if (val > 15)
    {
        return AOSTF_INVALID_LENGTH;
    }

    AOSTF_WR_VCFCNTCYCLE(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_GetMcId
 *
 * Notes:
 *
 */
int32 AOSTF_GetMcId(AOSTF_PriHdr_t *tfPtr)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    return AOSTF_RD_MCID(*tfPtr);
}


/*
 * Function: AOSTF_GetGlobalVcId
 *
 * Notes:
 *
 */
int32 AOSTF_GetGlobalVcId(AOSTF_PriHdr_t *tfPtr)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    return AOSTF_RD_GVCID(*tfPtr);
}


/*
 * Function: AOSTF_SetVcFrameCount
 *
 * Notes:
 *
 */
int32 AOSTF_SetVcFrameCount(AOSTF_PriHdr_t *tfPtr, uint32 val)
{
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    /* Validate 24-bit value */
    if (val > 0xFFFFFF)
    {
        return AOSTF_INVALID_LENGTH;
    }

    AOSTF_WR_VCFRMCNT(*tfPtr, val);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_IncrVcFrameCount
 *
 * Notes:
 *
 */
int32 AOSTF_IncrVcFrameCount(AOSTF_PriHdr_t *tfPtr)
{
    uint32 currentCount = 0;

    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    /* Read current 24-bit counter value */
    currentCount = (tfPtr->VcFrameCount[0] << 16) | 
                   (tfPtr->VcFrameCount[1] << 8) | 
                   tfPtr->VcFrameCount[2];

    /* Increment with 24-bit rollover */
    currentCount = (currentCount + 1) & 0xFFFFFF;

    /* Write back the incremented value */
    tfPtr->VcFrameCount[0] = (currentCount >> 16) & 0xFF;
    tfPtr->VcFrameCount[1] = (currentCount >> 8) & 0xFF;
    tfPtr->VcFrameCount[2] = currentCount & 0xFF;

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetInsertZone
 *
 * Notes:
 *
 */
int32 AOSTF_SetInsertZone(AOSTF_PriHdr_t *tfPtr, uint8 *data, uint16 length)
{
    uint8 *insertZonePtr = NULL;

    if ((tfPtr == NULL) || (data == NULL))
    {
        return AOSTF_INVALID_POINTER;
    }

    if (length > AOSTF_INSERT_ZONE_MAX_LENGTH)
    {
        return AOSTF_INVALID_LENGTH;
    }

    /* Insert zone follows the primary header */
    insertZonePtr = (uint8 *)(tfPtr) + AOSTF_PRIHDR_LENGTH;

    CFE_PSP_MemCpy(insertZonePtr, data, length);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_SetOcf
 *
 * Notes:
 *
 */
int32 AOSTF_SetOcf(AOSTF_PriHdr_t *tfPtr, uint8 *data, uint16 offset)
{
    if ((tfPtr == NULL) || (data == NULL))
    {
        return AOSTF_INVALID_POINTER;
    }

    CFE_PSP_MemCpy((uint8 *)tfPtr + offset, data, AOSTF_OCF_LENGTH);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_UpdateFrameHeaderErrorControl
 *
 * Notes:
 *
 */
int32 AOSTF_UpdateFrameHeaderErrorControl(AOSTF_PriHdr_t *tfPtr)
{
    uint16 reg = 0xffffU;
    uint8 *octPtr = NULL;
    uint8 byte;
    uint32 len = 0;
    
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    /* First clear the FHEC bytes */
    CFE_PSP_MemSet((void *)tfPtr->FrameHeaderErrorControl, 0x00, 2);
    
    octPtr = (uint8 *) tfPtr;

    /* Calculate FHEC over first 6 octets of header */
    len = 6;
    while (len--)
    {
        byte = (reg >> 8) & 0xff;
        reg = (reg << 8) | *octPtr;
        reg ^= fecfTable[byte];
        octPtr++;
    }

    AOSTF_WR_FHEC(*tfPtr, reg);

    return AOSTF_SUCCESS;
}


/*
 * Function: AOSTF_UpdateFrameErrorControlField
 *
 */
int32 AOSTF_UpdateFrameErrorControlField(AOSTF_PriHdr_t *tfPtr, uint16 offset)
{
    uint16 reg = 0xffffU;
    uint8 *octPtr = NULL;
    uint8 byte;
    uint32 len = 0;
    
    if (tfPtr == NULL)
    {
        return AOSTF_INVALID_POINTER;
    }

    if (offset < sizeof(AOSTF_PriHdr_t))
    {
        return AOSTF_INVALID_LENGTH;
    }

    /* First clear the FECF bytes */
    CFE_PSP_MemSet((void *)((uint8 *)tfPtr + offset), 0x00, 2);
    
    octPtr = (uint8 *) tfPtr;

    len = offset + 2;
    while (len--)
    {
        byte = (reg >> 8) & 0xff;
        reg = (reg << 8) | *octPtr;
        reg ^= fecfTable[byte];
        octPtr++;
    }

    AOSTF_WR_FECF((uint8 *)tfPtr + offset, reg);

    return AOSTF_SUCCESS;
}



/* ---------------------------  Helper Functions  ----------------------------- */


/*
 * Function: AOSTF_GenFecfTable
 *
 * Purpose:
 *   Generate the FECF Table for FECF computation
 
 * Arguments:
 *   polynomial  : The polynomial coefficients (eg. 0x1021: x^16 + x^12 + x^5 + 1)
 *
 * Note:
 *   - The highest order coefficient is not required in the polynomial.
 *
 */
void AOSTF_GenFecfTable(uint32 polynomial)
{
    uint32 remainder = 0;
    uint16 topbit = 1 << 15;
    uint16 val = 0;
    uint8 bit = 0;

    for (val = 0; val < 256; ++val)
    {
        /* The first remainder (16-bit) is the divident. */
        remainder = val << 8;
        
        /* Perform modulo-2 division, one bit at a time */
        for (bit = 0; bit < 8; ++bit)
        {
            /* If the remainder has topbit, divide by polynomial */
            if (remainder & topbit)
            {
                remainder = (uint16)(remainder << 1) ^ (uint16)(polynomial);
            }
            else
            {
                remainder = remainder << 1;
            }
        }

        fecfTable[val] = (uint16)remainder;       
    }
}