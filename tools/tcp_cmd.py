#!/usr/bin/env python3
"""Send one command to the cycle window's TCP debug server and print the reply.
Usage: tcp_cmd.py PORT '{"cmd":"ping","id":1}'"""
import socket, sys, time
for _ in range(100):
    try:
        s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=5); break
    except OSError:
        time.sleep(0.2)
else:
    sys.exit("no server")
s.sendall(sys.argv[2].encode() + b"\n")
buf = b""
while not buf.endswith(b"\n"):
    chunk = s.recv(65536)
    if not chunk: break
    buf += chunk
print(buf.decode().strip())
