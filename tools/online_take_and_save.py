#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shlex
import signal
import subprocess
import sys
import time

try:
    from .babiconjson_client import BabiconJsonClient
except ImportError:
    from babiconjson_client import BabiconJsonClient


def ts() -> str:
    return time.strftime("%H:%M:%S")


def log(msg: str) -> None:
    print(f"[{ts()}] {msg}", flush=True)


def build_analyzer_cmd(analyzer: str, online_host: str, maxevt: int, output: str) -> list[str]:
    return [
        analyzer,
        "--online",
        "-b",
        "-n",
        str(maxevt),
        online_host,
        "-o",
        output,
    ]


def terminate_process(proc: subprocess.Popen[bytes] | subprocess.Popen[str], timeout: float = 3.0) -> None:
    proc.terminate()
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            pass


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Start DAQ, wait for trigger, and save online ROOT output in one flow"
    )
    ap.add_argument("--eb-host", default="localhost", help="babiconjson EB host")
    ap.add_argument(
        "--online-host",
        default="localhost",
        help="host/IP passed to rfsoc_ridf_analyzer --online",
    )
    ap.add_argument(
        "--babiconjson",
        default="/usr/babirl/babicon/babiconjson",
        help="path to babiconjson",
    )
    ap.add_argument(
        "--analyzer",
        default="/home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer/bin/rfsoc_ridf_analyzer",
        help="path to rfsoc_ridf_analyzer",
    )
    ap.add_argument("-n", "--maxevt", type=int, default=200, help="events to collect in analyzer")
    ap.add_argument("-o", "--output", default="online_capture.root", help="output ROOT file")
    ap.add_argument("--start", choices=["nssta", "start"], default="nssta", help="DAQ start mode")
    ap.add_argument("--trigger-timeout", type=float, default=15.0, help="seconds to wait for trigger")
    ap.add_argument("--trigger-poll", type=float, default=0.5, help="trigger poll interval")
    ap.add_argument("--ender", default="online_take_and_save", help="stop ender text")
    ap.add_argument("--force-if-active", action="store_true", help="proceed even if runstatus is not IDLE")
    ap.add_argument("--no-stop", action="store_true", help="do not send stop after capture")
    args = ap.parse_args()

    if args.maxevt <= 0:
        print("Error: --maxevt must be > 0 for this script", file=sys.stderr)
        return 2
    if not os.path.isfile(args.analyzer):
        print(f"Error: analyzer not found: {args.analyzer}", file=sys.stderr)
        return 2
    if not os.access(args.analyzer, os.X_OK):
        print(f"Error: analyzer is not executable: {args.analyzer}", file=sys.stderr)
        return 2

    client = BabiconJsonClient(host=args.eb_host, binary=args.babiconjson)

    # Preflight checks
    runstatus = client.get_runstatus()
    evtn0 = client.get_event_built_number()
    log(f"precheck runstatus={runstatus}, eventbuiltnumber={evtn0}")

    if runstatus != "IDLE" and not args.force_if_active:
        log("DAQ already active. Abort for safety (use --force-if-active to override).")
        return 3

    # Start analyzer first so it is ready when DAQ starts.
    analyzer_cmd = build_analyzer_cmd(args.analyzer, args.online_host, args.maxevt, args.output)
    log(f"starting analyzer: {shlex.join(analyzer_cmd)}")
    try:
        analyzer_proc = subprocess.Popen(analyzer_cmd)
    except OSError as e:
        print(f"Error: failed to start analyzer: {e}", file=sys.stderr)
        return 2

    try:
        time.sleep(0.5)

        start_out = client.start(no_save=(args.start == "nssta"))
        if "error" in start_out:
            log(f"start returned error: {start_out}")
            terminate_process(analyzer_proc)
            return 4

        # Wait for first trigger/event (counter increase)
        baseline = evtn0
        deadline = time.time() + args.trigger_timeout
        triggered = False
        last = baseline

        while time.time() < deadline:
            proc_rc = analyzer_proc.poll()
            if proc_rc is not None:
                log(f"analyzer exited early with code {proc_rc} before trigger")
                if not args.no_stop:
                    client.stop(args.ender)
                return proc_rc if proc_rc != 0 else 5

            cur = client.get_event_built_number()
            if cur != last:
                log(f"eventbuiltnumber={cur}")
                last = cur
            if cur > baseline:
                triggered = True
                break
            time.sleep(args.trigger_poll)

        if not triggered:
            log(f"timeout waiting trigger ({args.trigger_timeout}s)")
            terminate_process(analyzer_proc)
            if not args.no_stop:
                client.stop(args.ender)
            return 1

        log(f"trigger detected: {baseline} -> {last}")
        log(f"waiting analyzer completion (maxevt={args.maxevt})")

        rc = analyzer_proc.wait()
        log(f"analyzer exited with code {rc}")

        if not args.no_stop:
            stop_out = client.stop(args.ender)
            if "error" in stop_out:
                log(f"stop returned error: {stop_out}")
            else:
                log("stop sent")

        return 0 if rc == 0 else rc

    except KeyboardInterrupt:
        log("KeyboardInterrupt: terminating analyzer")
        try:
            analyzer_proc.send_signal(signal.SIGINT)
            analyzer_proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            analyzer_proc.kill()
            try:
                analyzer_proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass
        if not args.no_stop:
            client.stop(args.ender)
        return 130


if __name__ == "__main__":
    sys.exit(main())
