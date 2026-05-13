#!/usr/bin/env python3
"""
Test script for GCOS TLP Server (JCRE mode, port 9025)
This simulates JCShell behavior: connect, send handshake, send APDU
"""

import socket
import struct
import time

print("=" * 70)
print("Testing GCOS TLP Server (JCRE mode)")
print("Architecture: Test Client -> TLP Protocol -> GCOS (port 9025)")
print("=" * 70)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    # Step 1: Connect to TLP server
    print("\n[1] Connecting to localhost:9025...")
    sock.connect(('localhost', 9025))
    print("[1] Connected!")
    
    # Step 2: Send handshake (ConnectInfo)
    print("\n[2] Sending handshake (ConnectInfo)...")
    magic = 0x5a5a1234
    connect_type = 0  # 0=contacted, 2=contactless
    
    handshake_data = struct.pack('<II', magic, connect_type)
    print(f"    Magic: 0x{magic:08X}")
    print(f"    Connect Type: {connect_type} (CONTACTED)")
    print(f"    Data: {handshake_data.hex().upper()}")
    
    sock.sendall(handshake_data)
    print("[2] Handshake sent!")
    
    time.sleep(0.5)
    
    # Step 3: Send POWER_UP command (type=0, cmd=0x21)
    print("\n[3] Sending POWER_UP command...")
    power_up_header = struct.pack('BBBB', 0, 0x21, 0, 0)  # type=0, cmd=0x21, size=0
    print(f"    Header: {power_up_header.hex().upper()}")
    
    sock.sendall(power_up_header)
    print("[3] POWER_UP sent!")
    
    # Receive ATR response
    print("\n[4] Receiving ATR response...")
    resp_header = sock.recv(4)
    if len(resp_header) == 4:
        rtype, rcmd, rlen_hi, rlen_lo = struct.unpack('BBBB', resp_header)
        rlen = (rlen_hi << 8) | rlen_lo
        print(f"    Response header: type={rtype}, cmd={rcmd:#04x}, length={rlen}")
        
        if rlen > 0:
            atr_data = sock.recv(rlen)
            print(f"    ATR: {atr_data.hex().upper()}")
    
    # Step 4: Send SELECT APDU
    print("\n[5] Sending SELECT APDU...")
    select_apdu = bytes.fromhex('00A4040008A000000003000000')
    
    # Build message: [type][cmd][size_hi][size_lo][APDU data]
    apdu_type = 0
    apdu_cmd = 0x22  # Regular APDU command
    apdu_size = len(select_apdu)
    
    apdu_header = struct.pack('BBH', apdu_type, apdu_cmd, apdu_size)
    full_message = apdu_header + select_apdu
    
    print(f"    Header: {apdu_header.hex().upper()}")
    print(f"    APDU: {select_apdu.hex().upper()}")
    print(f"    Full message ({len(full_message)} bytes): {full_message.hex().upper()}")
    
    sock.sendall(full_message)
    print("[5] APDU sent!")
    
    # Step 5: Receive response
    print("\n[6] Receiving response...")
    resp_header = sock.recv(4)
    if len(resp_header) == 4:
        rtype, rcmd, rlen_hi, rlen_lo = struct.unpack('BBBB', resp_header)
        rlen = (rlen_hi << 8) | rlen_lo
        print(f"    Response header: type={rtype}, cmd={rcmd:#04x}, length={rlen}")
        
        if rlen >= 2:
            resp_data = sock.recv(rlen)
            if len(resp_data) >= 2:
                sw = struct.unpack('>H', resp_data[-2:])[0]
                data = resp_data[:-2] if len(resp_data) > 2 else b''
                print(f"    Response Data: {data.hex().upper() if data else '(empty)'}")
                print(f"    Status Word: 0x{sw:04X}")
    
    print("\n[7] Test completed successfully!")
    print("=" * 70)
    
except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()
finally:
    sock.close()
    print("\n[8] Connection closed")
