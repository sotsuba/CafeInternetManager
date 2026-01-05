
import socket
import struct
import sys
import time

HOST = '127.0.0.1'
PORT = 9091

def send_packet(sock, msg):
    payload = msg.encode('utf-8')
    # Header: len (4), cid (4), bid (4) - Network Byte Order (!)
    length = len(payload)
    cid = 100 # Arbitrary Client ID for testing
    bid = 1   # Target Backend ID

    header = struct.pack('!III', length, cid, bid)
    sock.sendall(header + payload)
    print(f"Sent: {msg}")

def recv_packet(sock):
    header = sock.recv(12)
    if not header:
        return None

    length, cid, bid = struct.unpack('!III', header)
    payload = b''
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            break
        payload += chunk

    return payload.decode('utf-8', errors='ignore')

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((HOST, PORT))
    print(f"Connected to {HOST}:{PORT}")

    # Send Command
    send_packet(s, "file_list .")

    # Listen for response (wait up to 5 seconds)
    start = time.time()
    while time.time() - start < 5:
        resp = recv_packet(s)
        if resp:
            print(f"Received: {resp[:1000]}") # Print first 1000 chars
            if "DATA:FILES:" in resp:
                print("\n✅ Verification SUCCESS: Received file list!")
                break
        else:
            time.sleep(0.1)

    s.close()

except Exception as e:
    print(f"Error: {e}")
