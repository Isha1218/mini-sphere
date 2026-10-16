"""`minisphere run` starts the daemon. `minisphere send <state>` sets the sphere's state."""
import argparse
import asyncio
import json
import sys

from websockets.sync.client import connect

from . import hub

STATES = ["idle", "thinking", "done", "error"]


def send(state):
    try:
        with connect(f"ws://localhost:{hub.WS_PORT}", open_timeout=1) as ws:
            ws.send(json.dumps({"t": "state", "v": state}))
    except OSError:
        # Never fail the caller (e.g. a Claude Code hook) just because the daemon is off.
        print("minisphere: daemon not running", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(prog="minisphere")
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("run", help="start the daemon")
    send_p = sub.add_parser("send", help="set the sphere's state")
    send_p.add_argument("state", choices=STATES)
    args = parser.parse_args()

    if args.cmd == "run":
        try:
            asyncio.run(hub.run())
        except KeyboardInterrupt:
            pass
    else:
        send(args.state)
