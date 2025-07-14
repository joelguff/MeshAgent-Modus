/*
 *  MinHook - The Minimalistic API Hooking Library for x64/x86
 *  Copyright (C) 2009-2017 Tsuda Kageyu.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <windows.h>
#include "trampoline.h"
#include "buffer.h"

#ifndef ARRAYSIZE
    #define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

// Maximum size of a trampoline function.
#if defined(_M_X64) || defined(__x86_64__)
    #define TRAMPOLINE_MAX_SIZE (64 - sizeof(JMP_ABS))
#else
    #define TRAMPOLINE_MAX_SIZE (64 - sizeof(JMP_REL))
#endif

//-------------------------------------------------------------------------
static BOOL IsCodePadding(LPBYTE pInst, UINT size)
{
    UINT i;

    if (pInst[0] != 0x00 && pInst[0] != 0x90 && pInst[0] != 0xCC)
        return FALSE;

    for (i = 1; i < size; ++i)
    {
        if (pInst[i] != pInst[0])
            return FALSE;
    }
    return TRUE;
}

//-------------------------------------------------------------------------
BOOL CreateTrampolineFunction(PTRAMPOLINE ct)
{
    LPBYTE pTarget    = (LPBYTE)ct->pTarget;
    LPBYTE pTrampoline = (LPBYTE)ct->pTrampoline;
    SIZE_T oldPos     = 0;
    SIZE_T newPos     = 0;
    ULONG_PTR jmpDest = 0;     // Destination address of an internal jump.
    BOOL   finished   = FALSE; // Is the function completed?
#if defined(_M_X64) || defined(__x86_64__)
    LPBYTE pRelay = pTrampoline + TRAMPOLINE_MAX_SIZE;
#endif

    ct->patchAbove = FALSE;
    ct->nIP = 0;

    do
    {
        LPBYTE pCopySrc = pTarget + oldPos;
        UINT   copySize = 1;

        if ((newPos + copySize) > TRAMPOLINE_MAX_SIZE)
            return FALSE;

        // Simple instruction length calculation - this is a basic implementation
        // A full implementation would need a complete x86/x64 disassembler
        if (pCopySrc[0] == 0xE9) // JMP rel32
        {
            copySize = 5;
            jmpDest = (ULONG_PTR)pCopySrc + copySize + *(LONG*)(pCopySrc + 1);
            finished = TRUE;
        }
        else if (pCopySrc[0] == 0xEB) // JMP rel8
        {
            copySize = 2;
            jmpDest = (ULONG_PTR)pCopySrc + copySize + *(char*)(pCopySrc + 1);
            finished = TRUE;
        }
        else if (pCopySrc[0] == 0xE8) // CALL rel32
        {
            copySize = 5;
        }
        else if (pCopySrc[0] == 0xFF && (pCopySrc[1] & 0xF8) == 0x20) // JMP [mem]
        {
            copySize = 6;
            finished = TRUE;
        }
        else if (pCopySrc[0] == 0xFF && (pCopySrc[1] & 0xF8) == 0x10) // CALL [mem]
        {
            copySize = 6;
        }
        else if (pCopySrc[0] == 0xC3 || pCopySrc[0] == 0xC2) // RET
        {
            copySize = (pCopySrc[0] == 0xC2) ? 3 : 1;
            finished = TRUE;
        }
        else
        {
            // Basic instruction size detection - simplified
            copySize = 1;
            if (pCopySrc[0] == 0x0F) // Two-byte opcode
                copySize = 2;
            
            // Add a minimum safe copy size
            if (oldPos < sizeof(JMP_REL) && !finished)
                copySize = 1;
        }

        if ((newPos + copySize) > TRAMPOLINE_MAX_SIZE)
            return FALSE;

        if (ct->nIP >= ARRAYSIZE(ct->oldIPs))
            return FALSE;

        ct->oldIPs[ct->nIP] = (UINT8)oldPos;
        ct->newIPs[ct->nIP] = (UINT8)newPos;
        ct->nIP++;

        // Copy the instruction
        memcpy(pTrampoline + newPos, pCopySrc, copySize);

        newPos += copySize;
        oldPos += copySize;
    }
    while (!finished && oldPos < sizeof(JMP_REL));

    // If we haven't captured enough bytes, continue until we have at least 5 bytes
    while (oldPos < sizeof(JMP_REL) && !finished)
    {
        LPBYTE pCopySrc = pTarget + oldPos;
        UINT copySize = 1;

        if ((newPos + copySize) > TRAMPOLINE_MAX_SIZE)
            return FALSE;

        if (ct->nIP >= ARRAYSIZE(ct->oldIPs))
            return FALSE;

        ct->oldIPs[ct->nIP] = (UINT8)oldPos;
        ct->newIPs[ct->nIP] = (UINT8)newPos;
        ct->nIP++;

        memcpy(pTrampoline + newPos, pCopySrc, copySize);
        newPos += copySize;
        oldPos += copySize;
    }

    // Write the jump back to the original function
    if (jmpDest != 0)
    {
        // The original function ended with a jump, so we need to redirect that jump
#if defined(_M_X64) || defined(__x86_64__)
        PJMP_ABS pJmp = (PJMP_ABS)(pTrampoline + newPos);
        pJmp->opcode0 = 0xFF;
        pJmp->opcode1 = 0x25;
        pJmp->dummy = 0x00000000;
        pJmp->address = jmpDest;
#else
        PJMP_REL pJmp = (PJMP_REL)(pTrampoline + newPos);
        pJmp->opcode = 0xE9;
        pJmp->operand = (UINT32)(jmpDest - (ULONG_PTR)(pJmp + 1));
#endif
    }
    else
    {
        // Add a jump back to the original function after the copied bytes
#if defined(_M_X64) || defined(__x86_64__)
        PJMP_ABS pJmp = (PJMP_ABS)(pTrampoline + newPos);
        pJmp->opcode0 = 0xFF;
        pJmp->opcode1 = 0x25;
        pJmp->dummy = 0x00000000;
        pJmp->address = (ULONG_PTR)pTarget + oldPos;
        
        // Create relay function for x64
        ct->pRelay = pRelay;
        PJMP_ABS pRelay64 = (PJMP_ABS)pRelay;
        pRelay64->opcode0 = 0xFF;
        pRelay64->opcode1 = 0x25;
        pRelay64->dummy = 0x00000000;
        pRelay64->address = (ULONG_PTR)ct->pDetour;
#else
        PJMP_REL pJmp = (PJMP_REL)(pTrampoline + newPos);
        pJmp->opcode = 0xE9;
        pJmp->operand = (UINT32)((ULONG_PTR)pTarget + oldPos - (ULONG_PTR)(pJmp + 1));
#endif
    }

    return TRUE;
}
