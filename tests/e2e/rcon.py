"""Sends a command to a Quake III-protocol server over rcon and prints the reply.

    python -I tests/e2e/rcon.py --port 27961 --password e2e lc_status

Works for every id Tech 3 game here (Q3A, ET, RTCW, JK2, JA, Elite Force):
they share the out-of-band "rcon <password> <command>" packet.
"""

import argparse
import socket


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=27960)
    parser.add_argument("--password", required=True)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("command", nargs="+")
    args = parser.parse_args()

    packet = b"\xff\xff\xff\xffrcon " + args.password.encode() + b" " + " ".join(args.command).encode("latin-1")

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(args.timeout)
        sock.sendto(packet, (args.host, args.port))

        # A long reply comes in several packets, each with a "print\n" header.
        while True:
            try:
                data, _ = sock.recvfrom(65536)
            except socket.timeout:
                break

            text = data[4:].decode("latin-1")
            print(text.split("\n", 1)[1] if text.startswith("print") else text, end="")


if __name__ == "__main__":
    main()
