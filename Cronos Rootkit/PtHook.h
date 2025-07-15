#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>

// Syscall numbers and constants
#define SYSCALL_NTQUERYSYSTEMINFORMATION 0x55
#define SYSTEMPROCESSINFORMATION 5
#define MSR_LSTAR 0xC0000082

// EPROCESS ImageFileName offset (Windows 10/11)
#define EPROCESS_IMAGEFILENAME_OFFSET 0x5a8

// Intel PT MSR definitions
#define IA32_RTIT_CTL           0x570
#define IA32_RTIT_STATUS        0x571
#define IA32_RTIT_OUTPUT_BASE   0x560
#define IA32_RTIT_OUTPUT_MASK   0x561

// PT Control MSR bits
#define RTIT_CTL_TRACEEN        (1ULL << 0)   // Enable tracing
#define RTIT_CTL_CYCEN          (1ULL << 1)   // Enable cycle counting
#define RTIT_CTL_OS             (1ULL << 2)   // Trace OS
#define RTIT_CTL_USER           (1ULL << 3)   // Trace user
#define RTIT_CTL_PWREVT         (1ULL << 4)   // Power event enable
#define RTIT_CTL_FUPONPTW       (1ULL << 5)   // FUP on PTW
#define RTIT_CTL_FABRICEN       (1ULL << 6)   // Fabric enable
#define RTIT_CTL_CR3FILTER      (1ULL << 7)   // CR3 filter enable
#define RTIT_CTL_TOPA           (1ULL << 8)   // ToPA enable
#define RTIT_CTL_MTCEN          (1ULL << 9)   // MTC enable
#define RTIT_CTL_TSCEN          (1ULL << 10)  // TSC enable
#define RTIT_CTL_DISRETC        (1ULL << 11)  // Disable RET compression
#define RTIT_CTL_PTWEN          (1ULL << 12)  // PTW enable
#define RTIT_CTL_BRANCHEN       (1ULL << 13)  // Branch enable

// PT Status MSR bits
#define RTIT_STATUS_FILTEREN    (1ULL << 0)   // Filter enabled
#define RTIT_STATUS_CONTEXTEN   (1ULL << 1)   // Context enabled
#define RTIT_STATUS_TRIGGEREN   (1ULL << 2)   // Trigger enabled
#define RTIT_STATUS_BUFFOVF     (1ULL << 3)   // Buffer overflow
#define RTIT_STATUS_ERROR       (1ULL << 4)   // Error condition
#define RTIT_STATUS_STOPPED     (1ULL << 5)   // Stopped

// ToPA (Table of Physical Addresses) entry structure
typedef struct _TOPA_ENTRY {
    union {
        struct {
            UINT64 End : 1;         // End of table
            UINT64 Reserved1 : 1;   // Reserved
            UINT64 Interrupt : 1;   // Generate interrupt
            UINT64 Reserved2 : 1;   // Reserved
            UINT64 Stop : 1;        // Stop tracing
            UINT64 Reserved3 : 1;   // Reserved
            UINT64 Size : 4;        // Size (4KB * 2^Size)
            UINT64 Reserved4 : 2;   // Reserved
            UINT64 Base : 40;       // Physical base address (4KB aligned)
            UINT64 Reserved5 : 12;  // Reserved
        };
        UINT64 Value;
    };
} TOPA_ENTRY, *PTOPA_ENTRY;

// PT hooking context structure
typedef struct _PT_HOOK_CONTEXT {
    PVOID TopaBuffer;               // ToPA table buffer
    PHYSICAL_ADDRESS TopaPhysAddr;  // Physical address of ToPA table
    PVOID TraceBuffer;              // PT trace buffer
    PHYSICAL_ADDRESS TracePhysAddr; // Physical address of trace buffer
    ULONG BufferSize;               // Size of trace buffer
    BOOLEAN IsInitialized;          // Initialization flag
    BOOLEAN IsEnabled;              // PT enabled flag
} PT_HOOK_CONTEXT, *PPT_HOOK_CONTEXT;

// Global variables
extern PT_HOOK_CONTEXT g_PtContext;
extern KINTERRUPT* g_PmiInterrupt;
extern ULONG64 g_KiSystemCall64;

// Function declarations
NTSTATUS InitializePtHooking(VOID);
VOID CleanupPtHooking(VOID);
BOOLEAN PtPmiHandler(
    _In_ PKINTERRUPT Interrupt,
    _In_ PVOID ServiceContext
);
NTSTATUS RegisterPmiHandler(VOID);
VOID UnregisterPmiHandler(VOID);
VOID EnablePtTracing(VOID);
VOID DisablePtTracing(VOID);
