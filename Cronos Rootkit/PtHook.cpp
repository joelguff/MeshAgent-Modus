#include "PtHook.h"
#include "Driver.h"

// Global PT context
PT_HOOK_CONTEXT g_PtContext = { 0 };
KINTERRUPT* g_PmiInterrupt = NULL;

// PT buffer size (64KB)
#define PT_BUFFER_SIZE (64 * 1024)

NTSTATUS InitializePtHooking(VOID)
{
    NTSTATUS status = STATUS_SUCCESS;
    
    KdPrint(("[+] Initializing PT hooking infrastructure\n"));
    
    // Check if already initialized
    if (g_PtContext.IsInitialized) {
        KdPrint(("[!] PT hooking already initialized\n"));
        return STATUS_ALREADY_INITIALIZED;
    }
    
    // Verify PT support (should be checked in DriverEntry)
    if (!g_HasPtWrite) {
        KdPrint(("[-] PT support not available\n"));
        return STATUS_NOT_SUPPORTED;
    }
    
    // Initialize context structure
    RtlZeroMemory(&g_PtContext, sizeof(PT_HOOK_CONTEXT));
    g_PtContext.BufferSize = PT_BUFFER_SIZE;
    
    // Allocate ToPA table (must be 4KB aligned)
    g_PtContext.TopaBuffer = MmAllocateContiguousMemory(PAGE_SIZE, (PHYSICAL_ADDRESS){ .QuadPart = MAXULONG64 });
    if (!g_PtContext.TopaBuffer) {
        KdPrint(("[-] Failed to allocate ToPA buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Get physical address of ToPA table
    g_PtContext.TopaPhysAddr = MmGetPhysicalAddress(g_PtContext.TopaBuffer);
    KdPrint(("[+] ToPA buffer allocated at 0x%p (PA: 0x%llx)\n", 
             g_PtContext.TopaBuffer, g_PtContext.TopaPhysAddr.QuadPart));
    
    // Allocate trace buffer (must be page-aligned)
    g_PtContext.TraceBuffer = MmAllocateContiguousMemory(g_PtContext.BufferSize, (PHYSICAL_ADDRESS){ .QuadPart = MAXULONG64 });
    if (!g_PtContext.TraceBuffer) {
        KdPrint(("[-] Failed to allocate trace buffer\n"));
        MmFreeContiguousMemory(g_PtContext.TopaBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Get physical address of trace buffer
    g_PtContext.TracePhysAddr = MmGetPhysicalAddress(g_PtContext.TraceBuffer);
    KdPrint(("[+] Trace buffer allocated at 0x%p (PA: 0x%llx)\n", 
             g_PtContext.TraceBuffer, g_PtContext.TracePhysAddr.QuadPart));
    
    // Initialize ToPA table
    PTOPA_ENTRY topaEntry = (PTOPA_ENTRY)g_PtContext.TopaBuffer;
    RtlZeroMemory(topaEntry, PAGE_SIZE);
    
    // Set up first ToPA entry
    topaEntry[0].Base = g_PtContext.TracePhysAddr.QuadPart >> 12; // 4KB aligned
    topaEntry[0].Size = 4; // 64KB (4KB * 2^4)
    topaEntry[0].End = 1;  // End of table
    topaEntry[0].Interrupt = 1; // Generate interrupt on overflow
    
    // Configure PT MSRs
    ULONG64 rtitCtl = RTIT_CTL_TRACEEN | RTIT_CTL_OS | RTIT_CTL_TOPA | RTIT_CTL_BRANCHEN;
    
    // Set output base to ToPA table
    __writemsr(IA32_RTIT_OUTPUT_BASE, g_PtContext.TopaPhysAddr.QuadPart);
    
    // Set output mask (not used in ToPA mode, but set to 0)
    __writemsr(IA32_RTIT_OUTPUT_MASK, 0);
    
    // Clear status register
    __writemsr(IA32_RTIT_STATUS, 0);
    
    // Register PMI handler
    status = RegisterPmiHandler();
    if (!NT_SUCCESS(status)) {
        KdPrint(("[-] Failed to register PMI handler: 0x%x\n", status));
        CleanupPtHooking();
        return status;
    }
    
    g_PtContext.IsInitialized = TRUE;
    KdPrint(("[+] PT hooking infrastructure initialized successfully\n"));
    
    return STATUS_SUCCESS;
}

VOID CleanupPtHooking(VOID)
{
    KdPrint(("[+] Cleaning up PT hooking infrastructure\n"));
    
    // Disable PT tracing
    DisablePtTracing();
    
    // Unregister PMI handler
    UnregisterPmiHandler();
    
    // Free allocated memory
    if (g_PtContext.TopaBuffer) {
        MmFreeContiguousMemory(g_PtContext.TopaBuffer);
        g_PtContext.TopaBuffer = NULL;
    }
    
    if (g_PtContext.TraceBuffer) {
        MmFreeContiguousMemory(g_PtContext.TraceBuffer);
        g_PtContext.TraceBuffer = NULL;
    }
    
    // Reset context
    RtlZeroMemory(&g_PtContext, sizeof(PT_HOOK_CONTEXT));
    
    KdPrint(("[+] PT hooking cleanup completed\n"));
}

BOOLEAN PtPmiHandler(
    _In_ PKINTERRUPT Interrupt,
    _In_ PVOID ServiceContext
)
{
    UNREFERENCED_PARAMETER(Interrupt);
    UNREFERENCED_PARAMETER(ServiceContext);
    
    // Read PT status register
    ULONG64 rtitStatus = __readmsr(IA32_RTIT_STATUS);
    
    // Check if this is a PT-related interrupt
    if (!(rtitStatus & (RTIT_STATUS_BUFFOVF | RTIT_STATUS_ERROR))) {
        return FALSE; // Not our interrupt
    }
    
    // Clear the status register to acknowledge the interrupt
    __writemsr(IA32_RTIT_STATUS, 0);
    
    // TODO: Add syscall interception logic here
    // This will be implemented in Task 3
    
    return TRUE; // Handled
}

NTSTATUS RegisterPmiHandler(VOID)
{
    // Note: This is a simplified implementation
    // In a real implementation, you would need to:
    // 1. Hook the PMI vector (typically vector 0xFE)
    // 2. Register with the Local APIC
    // 3. Configure performance monitoring
    
    // For now, we'll simulate successful registration
    KdPrint(("[+] PMI handler registered (simulated)\n"));
    return STATUS_SUCCESS;
}

VOID UnregisterPmiHandler(VOID)
{
    // Cleanup PMI handler registration
    if (g_PmiInterrupt) {
        // In real implementation, would disconnect interrupt
        g_PmiInterrupt = NULL;
    }
    
    KdPrint(("[+] PMI handler unregistered\n"));
}

VOID EnablePtTracing(VOID)
{
    if (!g_PtContext.IsInitialized) {
        return;
    }
    
    // Enable PT tracing
    ULONG64 rtitCtl = __readmsr(IA32_RTIT_CTL);
    rtitCtl |= RTIT_CTL_TRACEEN;
    __writemsr(IA32_RTIT_CTL, rtitCtl);
    
    g_PtContext.IsEnabled = TRUE;
    KdPrint(("[+] PT tracing enabled\n"));
}

VOID DisablePtTracing(VOID)
{
    if (!g_PtContext.IsInitialized) {
        return;
    }
    
    // Disable PT tracing
    ULONG64 rtitCtl = __readmsr(IA32_RTIT_CTL);
    rtitCtl &= ~RTIT_CTL_TRACEEN;
    __writemsr(IA32_RTIT_CTL, rtitCtl);
    
    g_PtContext.IsEnabled = FALSE;
    KdPrint(("[+] PT tracing disabled\n"));
}
