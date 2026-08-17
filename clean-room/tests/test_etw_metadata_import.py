#!/usr/bin/env python3

from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from etw_metadata_import import ImportFailure, convert


def event(provider: str, event_id: int, timestamp: str, fields: dict[str, str]) -> str:
    data = "".join(
        f'<Data Name="{name}">{value}</Data>' for name, value in fields.items()
    )
    return (
        "<Event><System>"
        f'<Provider Name="{provider}"/><EventID>{event_id}</EventID>'
        f'<TimeCreated SystemTime="{timestamp}"/>'
        f"</System><EventData>{data}</EventData></Event>"
    )


def main() -> None:
    xhci = "Microsoft-Windows-USB-USBXHCI"
    ucx = "Microsoft-Windows-USB-UCX"
    identity = {
        "fid_idVendor": "0x17E9",
        "fid_idProduct": "0x4323",
        "fid_bcdDevice": "0x3156",
        "fid_UsbDevice": "device-a",
    }
    pipe_zero = {
        "fid_UsbDevice": "device-a",
        "fid_PipeHandle": "pipe-zero",
        "fid_bEndpointAddress": "0x0",
    }
    pipe_out = {
        "fid_UsbDevice": "device-a",
        "fid_PipeHandle": "pipe-out",
        "fid_bEndpointAddress": "0x2",
    }
    pipe_in = {
        "fid_UsbDevice": "device-a",
        "fid_PipeHandle": "pipe-in",
        "fid_bEndpointAddress": "0x84",
    }
    success_out = {
        "fid_PipeHandle": "pipe-out",
        "fid_URB_TransferBufferLength": "0x40",
        "fid_IRP_NtStatus": "0x0",
        "fid_URB_Hdr_Status": "0x0",
        "fid_URB_TransferBuffer": "payload-must-not-appear",
    }
    success_in = {
        "fid_PipeHandle": "pipe-in",
        "fid_URB_TransferBufferLength": "0x20",
        "fid_IRP_NtStatus": "0x0",
        "fid_URB_Hdr_Status": "0x0",
    }
    cancelled_in = dict(success_in)
    cancelled_in["fid_IRP_NtStatus"] = "0xC0000120"

    xml = "<Events>" + "".join(
        [
            event(ucx, 16, "2026-08-14T00:00:00.0000000-07:00", pipe_zero),
            event(xhci, 9, "2026-08-14T00:00:00.0010000-07:00", identity),
            event(ucx, 16, "2026-08-14T00:00:00.0100000-07:00", pipe_out),
            event(ucx, 16, "2026-08-14T00:00:00.0110000-07:00", pipe_in),
            event(ucx, 27, "2026-08-14T00:00:00.1000000-07:00", success_out),
            event(ucx, 27, "2026-08-14T00:00:00.2000000-07:00", success_in),
            event(ucx, 27, "2026-08-14T00:00:09.0000000-07:00", cancelled_in),
            event(ucx, 17, "2026-08-14T00:00:10.0000000-07:00", pipe_zero),
        ]
    ) + "</Events>"

    with tempfile.TemporaryDirectory() as temporary_root:
        root = Path(temporary_root)
        source = root / "trace.xml"
        source.write_text(xml, encoding="utf-8")
        private = root / "observations-private"
        private.mkdir()
        outputs = convert(source, private, 5_000_000, 1)
        assert len(outputs) == 1
        destination, transfers, byte_count = outputs[0]
        assert transfers == 2
        assert byte_count == 96
        content = destination.read_text(encoding="utf-8")
        assert "transfer out bulk 02 64" in content
        assert "transfer in bulk 84 32" in content
        assert "payload-must-not-appear" not in content
        assert "5000000 marker output-stable" in content
        try:
            convert(source, private, 5_000_000, 1)
        except ImportFailure:
            pass
        else:
            raise AssertionError("converter overwrote an existing envelope")

        public = root / "public"
        public.mkdir()
        try:
            convert(source, public, 5_000_000, 1)
        except ImportFailure:
            pass
        else:
            raise AssertionError("converter accepted a public output path")


if __name__ == "__main__":
    main()
