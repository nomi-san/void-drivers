# Void Drivers - Design Docs

Design and specification documents for the Void driver suite.

| Doc | Scope |
|-----|-------|
| [architecture.md](architecture.md) | System overview: components, control plane, device model, persistence |
| [voiddisplay-design.md](voiddisplay-design.md) | VoidDisplay - IddCx virtual display driver design and control IOCTL spec |
| [voidinput-design.md](voidinput-design.md) | VoidInput - UMDF + VHF virtual HID driver design, device types, control IOCTL spec |

Planned (not yet written):

- `libvoidrv-api.md` - SDK surface (`voidrv.h`) and ABI
- `edid.md` - VoidDisplay EDID layout and generation

> Implementation order is VoidDisplay first, then VoidInput. Docs are written
> ahead of each component.
