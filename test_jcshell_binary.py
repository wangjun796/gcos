#!/usr/bin/env python3
"""
Test JCShell with binary protocol (cref-compatible)
Send POWER_UP command in binary format and expect ATR response
"""

import socket
import struct
import time

print("=" * 70)
print("Testing JCShell with Binary Protocol (cref-compatible)")
print("=" * 70)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    # Step 1: Connect to JCShell on port 9000
    print("\n[1] Connecting to localhost:9000...")
    sock.connect(('localhost', 9000))
    print("[1] Connected!")
    
    # Step 2: Send POWER_UP command in binary format
    # Format: [type][cmd][size_hi][size_lo][data...]
    # POWER_UP: type=0, cmd=0x21, size=0, no data
    print("\n[2] Sending POWER_UP command (binary)...")
    power_up_header = struct.pack('BBBB', 0, 0x21, 0, 0)
    print(f"    Header: {power_up_header.hex().upper()}")
    sock.sendall(power_up_header)
    print("[2] POWER_UP sent!")
    
    # Step 3: Receive ATR response
    print("\n[3] Waiting for ATR response...")
    
    # First receive 4-byte header
    header = sock.recv(4)
    if len(header) != 4:
        print(f"[ERROR] Received {len(header)} bytes instead of 4")
    else:
        resp_type, resp_cmd, size_hi, size_lo = struct.unpack('BBBB', header)
        data_size = (size_hi << 8) | size_lo
        print(f"[3] Response header: type={resp_type}, cmd={resp_cmd:#04x}, size={data_size}")
        
        # Receive ATR data
        if data_size > 0:
            atr_data = sock.recv(data_size)
            print(f"[3] Received ATR ({len(atr_data)} bytes):")
            print(f"    Hex: {atr_data.hex().upper()}")
            
            # Parse ATR
            if len(atr_data) >= 6:
                ts = atr_data[0]
                print(f"    TS (initial character): 0x{ts:02X}")
                print(f"    ATR: {atr_data.hex().upper()}")
                
                if ts == 0x3B:
                    print("    ✓ Valid ATR (TS=0x3B indicates direct convention)")
                else:
                    print(f"    ⚠ Unexpected TS value")
        else:
            print("[3] No ATR data received")
    
    print("\n[4] Test completed successfully!")
    
except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()

finally:
    print("\n[5] Connection closed")
    sock.close()

print("=" * 70)
