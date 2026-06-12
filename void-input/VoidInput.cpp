#include "VoidInput.h"

// ---------------------------------------------------------------------------
// DriverEntry / device add
// ---------------------------------------------------------------------------
EXTERN_C NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT pDriverObject,
                              _In_ PUNICODE_STRING pRegistryPath)
{
    WDF_DRIVER_CONFIG     config;
    WDF_OBJECT_ATTRIBUTES attributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDF_DRIVER_CONFIG_INIT(&config, VoidInputDeviceAdd);
    config.DriverPoolTag = VOIDINPUT_POOL_TAG;

    NTSTATUS status = WdfDriverCreate(pDriverObject, pRegistryPath, &attributes,
                                      &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfDriverCreate failed 0x%08X", status);
    }
    return status;
}

_Use_decl_annotations_
NTSTATUS VoidInputDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);

    // We sit as an upper filter on the VHF device (which acts as the function
    // driver for the HID children we spawn).
    WdfFdoInitSetFilter(pDeviceInit);

    // Each open handle gets its own file context = one virtual HID device.
    WDF_OBJECT_ATTRIBUTES fileAttr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&fileAttr, VOIDINPUT_FILE_CONTEXT);
    fileAttr.SynchronizationScope = WdfSynchronizationScopeNone;

    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, VoidInputFileCreate, VoidInputFileClose,
                               WDF_NO_EVENT_CALLBACK);
    fileConfig.AutoForwardCleanupClose = WdfFalse;
    WdfDeviceInitSetFileObjectConfig(pDeviceInit, &fileConfig, &fileAttr);

    WDF_OBJECT_ATTRIBUTES deviceAttr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttr, VOIDINPUT_DEVICE_CONTEXT);

    WDFDEVICE device = nullptr;
    NTSTATUS status = WdfDeviceCreate(&pDeviceInit, &deviceAttr, &device);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfDeviceCreate failed 0x%08X", status);
        return status;
    }

    auto* dc = VoidInputDeviceGetContext(device);
    dc->Device    = device;
    dc->LiveCount = 0;
    RtlZeroMemory(dc->Live, sizeof(dc->Live));

    WDF_OBJECT_ATTRIBUTES lockAttr;
    WDF_OBJECT_ATTRIBUTES_INIT(&lockAttr);
    lockAttr.ParentObject = device;
    status = WdfWaitLockCreate(&lockAttr, &dc->Lock);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfWaitLockCreate failed 0x%08X", status);
        return status;
    }

    // Default queue: control IOCTLs and input-report writes.
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.PowerManaged       = WdfFalse;
    queueConfig.EvtIoDeviceControl = VoidInputIoDeviceControl;
    queueConfig.EvtIoWrite         = VoidInputIoWrite;

    WDFQUEUE queue = nullptr;
    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfIoQueueCreate failed 0x%08X", status);
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_VOIDINPUT, nullptr);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfDeviceCreateDeviceInterface failed 0x%08X", status);
        return status;
    }

    VOID_LOG("Device added (VHF enumerator ready)");
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// File object lifetime: open the lower VHF device per handle. The virtual HID
// device itself is created later, on IOCTL_VOIDINPUT_CREATE.
// ---------------------------------------------------------------------------
_Use_decl_annotations_
VOID VoidInputFileCreate(WDFDEVICE Device, WDFREQUEST Request, WDFFILEOBJECT FileObject)
{
    auto* fc = VoidInputFileGetContext(FileObject);
    RtlZeroMemory(fc, sizeof(*fc));
    fc->FileObject = FileObject;
    fc->Type       = VoidInputDeviceNone;

    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = FileObject;

    NTSTATUS status = WdfIoTargetCreate(Device, &attr, &fc->VhfIoTarget);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfIoTargetCreate failed 0x%08X", status);
        fc->VhfIoTarget = nullptr;
        WdfRequestComplete(Request, status);
        return;
    }

    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_FILE(&openParams, NULL);
    status = WdfIoTargetOpen(fc->VhfIoTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        VOID_LOG("WdfIoTargetOpen (VHF) failed 0x%08X", status);
        WdfObjectDelete(fc->VhfIoTarget);
        fc->VhfIoTarget = nullptr;
        WdfRequestComplete(Request, status);
        return;
    }

    // Seed the VHF config with the lower target handle; the report descriptor and
    // identity are filled from the device type at CREATE.
    VHF_CONFIG_INIT(&fc->VhfConfig,
                    WdfIoTargetWdmGetTargetFileHandle(fc->VhfIoTarget), 0, NULL);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

_Use_decl_annotations_
VOID VoidInputFileClose(WDFFILEOBJECT FileObject)
{
    auto* fc = VoidInputFileGetContext(FileObject);

    if (fc->VhfHandle) {
        // Synchronous teardown: the HID device departs and VHF drains.
        VhfDelete(fc->VhfHandle, TRUE);
        fc->VhfHandle = nullptr;
        // TODO(device-type milestones): drop this device from the driver live
        // table once CREATE registers it there.
    }

    if (fc->VhfIoTarget) {
        WdfObjectDelete(fc->VhfIoTarget);
        fc->VhfIoTarget = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Control IOCTLs
// ---------------------------------------------------------------------------
_Use_decl_annotations_
VOID VoidInputIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request,
                              size_t OutputBufferLength, size_t InputBufferLength,
                              ULONG IoControlCode)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    auto* dc = VoidInputDeviceGetContext(WdfIoQueueGetDevice(Queue));
    auto* fc = VoidInputFileGetContext(WdfRequestGetFileObject(Request));

    NTSTATUS  status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info   = 0;

    switch (IoControlCode) {
    case IOCTL_VOIDINPUT_VERSION: {
        ULONG* out = nullptr;
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(ULONG), (PVOID*)&out, nullptr);
        if (NT_SUCCESS(status)) { *out = VOIDINPUT_VERSION; info = sizeof(ULONG); }
        break;
    }
    case IOCTL_VOIDINPUT_LIST: {
        VOIDINPUT_STATE* out = nullptr;
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*out), (PVOID*)&out, nullptr);
        if (NT_SUCCESS(status)) {
            WdfWaitLockAcquire(dc->Lock, NULL);
            RtlZeroMemory(out, sizeof(*out));
            out->Count = dc->LiveCount;
            for (UINT32 i = 0; i < dc->LiveCount && i < VOIDINPUT_MAX_DEVICES; ++i) {
                out->Entries[i] = dc->Live[i];
            }
            WdfWaitLockRelease(dc->Lock);
            info = sizeof(*out);
        }
        break;
    }
    case IOCTL_VOIDINPUT_CREATE: {
        VOIDINPUT_CREATE* in = nullptr;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(*in), (PVOID*)&in, nullptr);
        if (NT_SUCCESS(status)) {
            if (fc->VhfHandle) {
                status = STATUS_INVALID_DEVICE_STATE;   // one device per handle
            } else {
                // Device-type descriptors (mouse, keyboard, Xbox One, DS4, DS5,
                // touch) are added in the build-order milestones that follow the
                // enumerator bring-up. Until a type's compiled-in HID descriptor
                // exists, the typed VhfCreate path has nothing to build from.
                VOID_LOG("CREATE type=%u: device-type descriptors not implemented yet",
                         in->Type);
                status = STATUS_NOT_SUPPORTED;
            }
        }
        break;
    }
    case IOCTL_VOIDINPUT_DESTROY: {
        if (fc->VhfHandle) {
            VhfDelete(fc->VhfHandle, TRUE);
            fc->VhfHandle = nullptr;
            fc->Type      = VoidInputDeviceNone;
            // TODO(device-type milestones): drop from the driver live table.
        }
        status = STATUS_SUCCESS;   // idempotent: no device is a no-op
        break;
    }
    case IOCTL_VOIDINPUT_GET_EVENT:
    case IOCTL_VOIDINPUT_COMPLETE_EVENT:
        // The output/feature event channel lands with the first device type that
        // emits output reports (rumble/LED/lightbar).
        status = STATUS_NOT_SUPPORTED;
        break;
    default:
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, info);
}

// ---------------------------------------------------------------------------
// Input report hot path: WriteFile -> VhfReadReportSubmit. Valid only after a
// device has been created on this handle.
// ---------------------------------------------------------------------------
_Use_decl_annotations_
VOID VoidInputIoWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    UNREFERENCED_PARAMETER(Queue);

    auto* fc = VoidInputFileGetContext(WdfRequestGetFileObject(Request));
    if (!fc->VhfHandle) {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_STATE);
        return;
    }

    PUCHAR buffer = nullptr;
    NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, 1, (PVOID*)&buffer, nullptr);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }
    if (Length > MAXUINT32) {
        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return;
    }

    HID_XFER_PACKET packet;
    packet.reportId       = fc->NumberedReports ? buffer[0] : 0;
    packet.reportBuffer   = buffer;
    packet.reportBufferLen = (ULONG)Length;

    status = VhfReadReportSubmit(fc->VhfHandle, &packet);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
}
