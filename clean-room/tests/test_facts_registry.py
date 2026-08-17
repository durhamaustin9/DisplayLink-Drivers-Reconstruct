#!/usr/bin/env python3
"""Validate the source-only clean-room fact registry."""

from __future__ import annotations

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "facts.tsv"
EXPECTED_HEADER = [
    "fact_id",
    "maturity",
    "disposition",
    "trials",
    "contradiction",
    "boundary",
    "sources",
]
MATURITY = {f"E{level}" for level in range(9)}
DISPOSITION = {"active", "provisional", "rejected"}


def main() -> None:
    with REGISTRY.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        assert reader.fieldnames == EXPECTED_HEADER
        rows = list(reader)

    assert rows
    identifiers: set[str] = set()
    rejected = 0
    for row in rows:
        fact_id = row["fact_id"]
        assert fact_id.startswith("FACT-")
        assert fact_id not in identifiers
        identifiers.add(fact_id)
        assert row["maturity"] in MATURITY
        assert row["disposition"] in DISPOSITION
        assert row["trials"].isdigit() and int(row["trials"]) > 0
        assert row["contradiction"]
        assert row["boundary"] and "/" not in row["boundary"]
        if row["disposition"] == "rejected":
            rejected += 1
            assert row["contradiction"] != "none"
        for source_text in row["sources"].split(";"):
            source = Path(source_text)
            assert not source.is_absolute()
            assert ".." not in source.parts
            resolved = (ROOT / source).resolve()
            assert resolved.is_file()
            assert ROOT in resolved.parents

    assert rejected >= 1
    assert "FACT-STARTUP-ROLES-001" in identifiers
    assert "FACT-HOTPLUG-PREFIX-001" in identifiers
    assert "FACT-HOTUNPLUG-PROFILES-001" in identifiers


if __name__ == "__main__":
    main()
