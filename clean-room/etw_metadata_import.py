#!/usr/bin/env python3
"""Convert typed Microsoft USB ETW XML into private metadata-only envelopes."""

from __future__ import annotations

import argparse
import hashlib
import os
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
import re
import tempfile
import xml.etree.ElementTree as ET


TARGET_VENDOR = "0x17e9"
TARGET_PRODUCT = "0x4323"
TARGET_REVISION = "0x3156"
XHCI_PROVIDER = "Microsoft-Windows-USB-USBXHCI"
UCX_PROVIDER = "Microsoft-Windows-USB-UCX"
MAX_XML_BYTES = 256 * 1024 * 1024
MAX_XML_ELEMENTS = 2_000_000
MAX_ENVELOPE_EVENTS = 4096
MAX_ACTIVATION_BYTES = 80 * 1024 * 1024

ALLOWED_DATA_FIELDS = {
    "fid_idVendor",
    "fid_idProduct",
    "fid_bcdDevice",
    "fid_UsbDevice",
    "fid_PipeHandle",
    "fid_URB_PipeHandle",
    "fid_bEndpointAddress",
    "fid_URB_TransferBufferLength",
    "fid_IRP_NtStatus",
    "fid_URB_Hdr_Status",
}


class ImportFailure(Exception):
    """Raised when an ETW export does not satisfy the bounded input model."""


@dataclass(frozen=True)
class Event:
    provider: str
    event_id: int
    timestamp: datetime
    data: dict[str, str]


@dataclass(frozen=True)
class Transfer:
    timestamp: datetime
    endpoint: int
    length: int


@dataclass
class Trial:
    device_handle: str
    start: datetime | None = None
    end: datetime | None = None
    transfers: list[Transfer] = field(default_factory=list)


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def parse_timestamp(value: str) -> datetime:
    match = re.fullmatch(
        r"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})"
        r"(?:\.(\d+))?([+-]\d{2}:\d{2}|Z)",
        value,
    )
    if match is None:
        raise ImportFailure("ETW event has an invalid timestamp")
    fraction = (match.group(2) or "")[:6].ljust(6, "0")
    zone = "+00:00" if match.group(3) == "Z" else match.group(3)
    return datetime.fromisoformat(f"{match.group(1)}.{fraction}{zone}")


def reject_unsafe_xml(path: Path) -> None:
    size = path.stat().st_size
    if size <= 0 or size > MAX_XML_BYTES:
        raise ImportFailure("XML size is empty or exceeds the configured bound")
    with path.open("rb") as source:
        tail = b""
        while chunk := source.read(1024 * 1024):
            upper = (tail + chunk).upper()
            if b"<!DOCTYPE" in upper or b"<!ENTITY" in upper:
                raise ImportFailure("DTD and entity declarations are forbidden")
            tail = chunk[-16:]


def iter_events(path: Path):
    element_count = 0
    try:
        for _, element in ET.iterparse(path, events=("end",)):
            element_count += 1
            if element_count > MAX_XML_ELEMENTS:
                raise ImportFailure("XML element count exceeds the configured bound")
            if local_name(element.tag) != "Event":
                continue

            system = next(
                (child for child in element if local_name(child.tag) == "System"),
                None,
            )
            if system is None:
                element.clear()
                continue

            provider = ""
            event_id_text = ""
            timestamp_text = ""
            for child in system:
                name = local_name(child.tag)
                if name == "Provider":
                    provider = child.attrib.get("Name", "")
                elif name == "EventID":
                    event_id_text = (child.text or "").strip()
                elif name == "TimeCreated":
                    timestamp_text = child.attrib.get("SystemTime", "")

            if provider not in {XHCI_PROVIDER, UCX_PROVIDER}:
                element.clear()
                continue
            try:
                event_id = int(event_id_text, 10)
            except ValueError as error:
                raise ImportFailure("USB event has an invalid event ID") from error

            data: dict[str, str] = {}
            for child in element.iter():
                if local_name(child.tag) != "Data":
                    continue
                name = child.attrib.get("Name", "")
                if name in ALLOWED_DATA_FIELDS:
                    data[name] = (child.text or "").strip()

            yield Event(provider, event_id, parse_timestamp(timestamp_text), data)
            element.clear()
    except ET.ParseError as error:
        raise ImportFailure(f"invalid XML: {error}") from error


def parse_hex(value: str, maximum: int, label: str) -> int:
    try:
        parsed = int(value, 0)
    except ValueError as error:
        raise ImportFailure(f"invalid {label}") from error
    if parsed < 0 or parsed > maximum:
        raise ImportFailure(f"{label} exceeds the configured bound")
    return parsed


def is_target_identity(data: dict[str, str]) -> bool:
    return (
        data.get("fid_idVendor", "").lower() == TARGET_VENDOR
        and data.get("fid_idProduct", "").lower() == TARGET_PRODUCT
        and data.get("fid_bcdDevice", "").lower() == TARGET_REVISION
    )


def discover_trials(path: Path) -> list[Trial]:
    handles: list[str] = []
    for event in iter_events(path):
        if (
            event.provider == XHCI_PROVIDER
            and event.event_id == 9
            and is_target_identity(event.data)
        ):
            handle = event.data.get("fid_UsbDevice", "")
            if handle and handle not in handles:
                handles.append(handle)
    if not handles:
        raise ImportFailure("the exact target device was not found")

    trials = {handle: Trial(handle) for handle in handles}
    pipe_map: dict[str, tuple[str, int]] = {}
    for event in iter_events(path):
        if event.provider != UCX_PROVIDER:
            continue
        handle = event.data.get("fid_UsbDevice", "")
        if event.event_id == 16 and handle in trials:
            pipe = event.data.get("fid_PipeHandle", "")
            endpoint = parse_hex(
                event.data.get("fid_bEndpointAddress", ""), 0xFF, "endpoint"
            )
            if not pipe:
                raise ImportFailure("endpoint-create event is missing a pipe handle")
            pipe_map[pipe] = (handle, endpoint)
            if endpoint == 0:
                if trials[handle].start is not None:
                    raise ImportFailure("target lifetime has duplicate endpoint-zero creation")
                trials[handle].start = event.timestamp
            continue

        if event.event_id == 17 and handle in trials:
            endpoint = parse_hex(
                event.data.get("fid_bEndpointAddress", ""), 0xFF, "endpoint"
            )
            if endpoint == 0:
                if trials[handle].end is not None:
                    raise ImportFailure("target lifetime has duplicate endpoint-zero deletion")
                trials[handle].end = event.timestamp
            continue

        if event.event_id != 27:
            continue
        pipe = event.data.get("fid_PipeHandle") or event.data.get(
            "fid_URB_PipeHandle", ""
        )
        mapping = pipe_map.get(pipe)
        if mapping is None or mapping[0] not in trials or mapping[1] not in {0x02, 0x84}:
            continue
        if (
            parse_hex(event.data.get("fid_IRP_NtStatus", ""), 0xFFFFFFFF, "NT status")
            != 0
            or parse_hex(
                event.data.get("fid_URB_Hdr_Status", ""),
                0xFFFFFFFF,
                "URB status",
            )
            != 0
        ):
            continue
        length = parse_hex(
            event.data.get("fid_URB_TransferBufferLength", ""),
            16 * 1024 * 1024,
            "transfer length",
        )
        trials[mapping[0]].transfers.append(
            Transfer(event.timestamp, mapping[1], length)
        )

    for trial in trials.values():
        if trial.start is None or trial.end is None or trial.end <= trial.start:
            raise ImportFailure("target lifetime is missing ordered start/end events")
    ordered = sorted(trials.values(), key=lambda trial: trial.start)
    for trial in ordered:
        trial.transfers.sort(key=lambda transfer: transfer.timestamp)
        trial.transfers = [
            transfer
            for transfer in trial.transfers
            if trial.start <= transfer.timestamp < trial.end
        ]
        if not trial.transfers:
            raise ImportFailure("target lifetime contains no successful display transfers")
    return ordered


def microseconds(delta) -> int:
    return (
        delta.days * 86_400_000_000
        + delta.seconds * 1_000_000
        + delta.microseconds
    )


def render_envelope(
    trial: Trial, trial_number: int, stable_upper_bound_us: int, source_hash: str
) -> tuple[str, int, int]:
    assert trial.start is not None and trial.end is not None
    end_us = microseconds(trial.end - trial.start)
    if stable_upper_bound_us <= 0 or stable_upper_bound_us >= end_us:
        raise ImportFailure("stable upper bound must fall inside every target lifetime")

    records: list[tuple[int, str]] = []
    activation_bytes = 0
    activation_transfers = 0
    for transfer in trial.transfers:
        relative_us = microseconds(transfer.timestamp - trial.start)
        if relative_us < 0 or relative_us >= end_us:
            raise ImportFailure("transfer falls outside its target lifetime")
        direction = "out" if transfer.endpoint == 0x02 else "in"
        records.append(
            (
                relative_us,
                f"transfer {direction} bulk {transfer.endpoint:02x} {transfer.length}",
            )
        )
        if relative_us < stable_upper_bound_us:
            activation_transfers += 1
            activation_bytes += transfer.length

    if activation_transfers == 0:
        raise ImportFailure("stable upper bound contains no display transfers")
    if activation_bytes > MAX_ACTIVATION_BYTES:
        raise ImportFailure("activation byte total exceeds the envelope bound")
    if len(records) + 4 > MAX_ENVELOPE_EVENTS:
        raise ImportFailure("target lifetime exceeds the envelope event bound")

    lines = [
        "# Derived from typed Microsoft USB ETW metadata only.",
        "# Transfer buffers and control setup fields are deliberately absent.",
        "# output-stable is a conservative observer-reported upper bound.",
        f"# source-xml-sha256 {source_hash}",
        f"# trial {trial_number}",
        "dockbridge-activation-envelope-v1",
        "origin black-box",
        "device 17e9 4323 3156",
        "action cold-connect",
    ]
    sequence = 0
    lines.append(f"event {sequence} 0 marker capture-start")
    sequence += 1
    lines.append(f"event {sequence} 0 marker action-issued")
    sequence += 1
    stable_written = False
    for timestamp_us, record in records:
        if not stable_written and timestamp_us >= stable_upper_bound_us:
            lines.append(
                f"event {sequence} {stable_upper_bound_us} marker output-stable"
            )
            sequence += 1
            stable_written = True
        lines.append(f"event {sequence} {timestamp_us} {record}")
        sequence += 1
    if not stable_written:
        lines.append(
            f"event {sequence} {stable_upper_bound_us} marker output-stable"
        )
        sequence += 1
    lines.append(f"event {sequence} {end_us} marker capture-end")
    return "\n".join(lines) + "\n", activation_transfers, activation_bytes


def write_exclusive(path: Path, content: str) -> None:
    if path.exists() or path.is_symlink():
        raise ImportFailure(f"refusing to overwrite {path.name}")
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write(content)
            output.flush()
            os.fsync(output.fileno())
        os.link(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def convert(
    xml_path: Path,
    output_directory: Path,
    stable_upper_bound_us: int,
    expected_trials: int,
) -> list[tuple[Path, int, int]]:
    xml_path = xml_path.resolve(strict=True)
    output_directory = output_directory.resolve(strict=True)
    if "observations-private" not in output_directory.parts:
        raise ImportFailure("output directory must be under observations-private")
    reject_unsafe_xml(xml_path)
    trials = discover_trials(xml_path)
    if len(trials) != expected_trials:
        raise ImportFailure(
            f"expected {expected_trials} target lifetimes, observed {len(trials)}"
        )
    source_hash = hashlib.sha256(xml_path.read_bytes()).hexdigest()
    outputs = []
    for index, trial in enumerate(trials, 1):
        content, transfer_count, byte_count = render_envelope(
            trial, index, stable_upper_bound_us, source_hash
        )
        destination = output_directory / f"trial-{index:02d}.dba"
        write_exclusive(destination, content)
        outputs.append((destination, transfer_count, byte_count))
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create private payload-free envelopes from typed USB ETW XML"
    )
    parser.add_argument("xml", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--stable-upper-bound-us", type=int, required=True)
    parser.add_argument("--expected-trials", type=int, default=3)
    arguments = parser.parse_args()
    try:
        outputs = convert(
            arguments.xml,
            arguments.output_directory,
            arguments.stable_upper_bound_us,
            arguments.expected_trials,
        )
    except (ImportFailure, OSError) as error:
        parser.exit(65, f"etw-metadata-import: {error}\n")
    for path, transfer_count, byte_count in outputs:
        print(
            f"{path.name}: activation-transfers={transfer_count} "
            f"activation-bytes={byte_count} payloads=absent"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
