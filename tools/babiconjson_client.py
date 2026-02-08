#!/usr/bin/env python3
"""Lightweight client for /usr/babirl/babicon/babiconjson."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import time
from dataclasses import dataclass
from typing import Any


_RUNSTATUS_RE = re.compile(r'"runstatus"\s*:\s*"([^"]+)"')


@dataclass
class BabiconJsonClient:
    host: str = "localhost"
    binary: str = "/usr/babirl/babicon/babiconjson"
    _has_stdbuf_cache: bool | None = None

    def _run_raw(self, cmd: str, timeout: float = 10.0) -> str:
        """Run one command and return merged stdout/stderr text."""
        payload = f"{cmd}\n"
        merged = ""

        cmdline = [self.binary, self.host]
        # Prefer line-buffer bypass to get immediate JSON output.
        if self._has_stdbuf():
            cmdline = ["stdbuf", "-o0", "-e0", *cmdline]

        try:
            proc = subprocess.run(
                cmdline,
                input=payload,
                text=True,
                capture_output=True,
                timeout=timeout,
                check=False,
            )
            merged = f"{proc.stdout or ''}\n{proc.stderr or ''}"
        except subprocess.TimeoutExpired as e:
            out = e.stdout if isinstance(e.stdout, str) else ""
            err = e.stderr if isinstance(e.stderr, str) else ""
            merged = f"{out}\n{err}"
        except FileNotFoundError:
            merged = f"babiconjson_not_found: {self.binary}"
        return merged

    def command(self, cmd: str, timeout: float = 10.0) -> dict[str, Any]:
        """Run one command and return the first JSON object from output."""
        merged = self._run_raw(cmd, timeout=timeout)

        parsed = self._extract_first_json_object(merged)
        if parsed is None:
            return {"error": "no_json_response", "raw": merged.strip()}

        return parsed

    @classmethod
    def _has_stdbuf(cls) -> bool:
        if cls._has_stdbuf_cache is None:
            cls._has_stdbuf_cache = shutil.which("stdbuf") is not None
        return cls._has_stdbuf_cache

    @staticmethod
    def _extract_first_json_object(raw: str) -> dict[str, Any] | None:
        decoder = json.JSONDecoder()
        for idx, ch in enumerate(raw):
            if ch != "{":
                continue
            try:
                obj, _ = decoder.raw_decode(raw[idx:])
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                return obj
            return {"error": "invalid_json_response", "raw": raw[idx:].strip()}
        return None

    def start(self, no_save: bool = True) -> dict[str, Any]:
        return self.command("nssta" if no_save else "start")

    def stop(self, ender: str = "quick_stop") -> dict[str, Any]:
        return self.command(f"stop {ender}")

    def get_event_built_number(self) -> int:
        out = self.command("getevtnumber")
        try:
            return int(out.get("eventbuiltnumber", 0))
        except (TypeError, ValueError):
            return 0

    def get_runstatus(self) -> str:
        raw = self._run_raw("getconfig", timeout=3.0)
        m = _RUNSTATUS_RE.search(raw)
        if m:
            return m.group(1).strip().upper()

        out = self.command("getconfig", timeout=3.0)
        # babiconjson getconfig usually returns {"runinfo": {"runstatus": "..."}}
        runinfo = out.get("runinfo")
        if isinstance(runinfo, dict):
            rs = runinfo.get("runstatus")
            if isinstance(rs, str):
                return rs.strip().upper()
        # fallback for unexpected flat format
        rs = out.get("runstatus")
        if isinstance(rs, str):
            return rs.strip().upper()
        return "UNKNOWN"

    def wait_for_event(self, timeout_sec: float = 15.0, poll_sec: float = 0.5) -> tuple[bool, int]:
        deadline = time.monotonic() + timeout_sec
        last = 0
        while time.monotonic() < deadline:
            last = self.get_event_built_number()
            if last > 0:
                return True, last
            time.sleep(poll_sec)
        return False, last
