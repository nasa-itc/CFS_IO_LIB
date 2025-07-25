/******************************************************************************/
/** \file  tm_sync.c
*
*   Copyright 2017 United States Government as represented by the Administrator
*   of the National Aeronautics and Space Administration.  No copyright is
*   claimed in the United States under Title 17, U.S. Code.
*   All Other Rights Reserved.
*
*   \brief Provides the AOS Channel Synchronization service.
*
*   \par Modification History:
*     - 2015-10-29 | Guy de Carufel | OSR | Code Started 
*******************************************************************************/
#include <stdlib.h>

#include "aos_sync.h"
#include "io_lib_utils.h"

static uint8 prSeq[32]; 

/*****************************************************************************/
/** \brief AOS_SYNC_LibInit
******************************************************************************/
int32 AOS_SYNC_LibInit(void)
{
    IO_LIB_UTIL_GenPseudoRandomSeq(&prSeq[0], 0xa9, 0xff);

    return AOS_SYNC_SUCCESS;
}


/*****************************************************************************/
/** \brief AOS_SYNC_Synchronize
******************************************************************************/
int32 AOS_SYNC_Synchronize(uint8 *pBuff, char *asmStr, uint8 asmSize, 
                          uint16 frameSize, bool randomize)
{
    uint16 byte;
    char *hexchar = asmStr;
    int32 iStatus = AOS_SYNC_SUCCESS;

    if (pBuff == NULL || asmStr == NULL)
    {
        iStatus = AOS_SYNC_INVALID_POINTER;
        goto end_of_function;
    }
    
    if (asmSize % 2 != 0 || asmSize < 4)
    {
        iStatus = AOS_SYNC_INVALID_ASM_SIZE;
        goto end_of_function;
    }


    /* Store the ASM into the buffer based on the fixed ASM String. */
    for (byte = 0; byte < asmSize; ++byte)
    {
        sscanf(hexchar, "%2hhx", (uint8 *) &pBuff[byte]);
        hexchar += 2;
    }

    if (randomize == true)
    {
        AOS_SYNC_PseudoRandomize(&pBuff[asmSize], frameSize);
    }

    /* Return the full size of the CADU. */
    iStatus = asmSize + frameSize;

end_of_function:
    return iStatus;
}


/******************************************************************************/
/** \brief AOS_SYNC_PseudoRandomize
*******************************************************************************/
int32 AOS_SYNC_PseudoRandomize(uint8 *pFrame, uint16 frameSize)
{
    return IO_LIB_UTIL_PseudoRandomize(pFrame, frameSize, prSeq);
}
