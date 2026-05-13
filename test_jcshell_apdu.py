#!/usr/bin/env python3
"""
Test JCShell APDU forwarding
Send SELECT APDU command and verify response
"""

import socket
import struct
import time

print("=" * 70)
print("Testing JCShell APDU Forwarding")
print("=" * 70)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    # Step 1: Connect to JCShell on port 9000
    print("\n[1] Connecting to localhost:9000...")
    sock.connect(('localhost', 9000))
    print("[1] Connected!")
    
    # Step 2: Send POWER_UP command
    print("\n[2] Sending POWER_UP command...")
    power_up_header = struct.pack('BBBB', 0, 0x21, 0, 0)
    sock.sendall(power_up_header)
    
    # Receive ATR
    header = sock.recv(4)
    resp_type, resp_cmd, size_hi, size_lo = struct.unpack('BBBB', header)
    data_size = (size_hi << 8) | size_lo
    
    if data_size > 0:
        atr_data = sock.recv(data_size)
        print(f"[2] Received ATR ({len(atr_data)} bytes): {atr_data.hex().upper()}")
    else:
        print("[2] ERROR: No ATR received")
        exit(1)
    
    # Step 3: Send SELECT APDU command
    print("\n[3] Sending SELECT APDU command...")
    
    # SELECT command: 00 A4 04 00 08 A0 00 00 00 03 00 00 00
    select_apdu = bytes.fromhex('00A4040008A000000003000000')
    
    # Build binary message: [type][cmd][size_hi][size_lo][apdu_data...]
    apdu_type = 0
    apdu_cmd = 0  # Regular APDU (not POWER_UP)
    apdu_size = len(select_apdu)
    
    apdu_header = struct.pack('BBBB', apdu_type, apdu_cmd, 
                              (apdu_size >> 8) & 0xFF, apdu_size & 0xFF)
    full_message = apdu_header + select_apdu
    
    print(f"    APDU: {select_apdu.hex().upper()}")
    print(f"    Message size: {len(full_message)} bytes")
    
    sock.sendall(full_message)
    print("[3] SELECT APDU sent!")
    
    # Step 4: Receive response
    print("\n[4] Waiting for response...")
    
    # Receive response header
    resp_header = sock.recv(4)
    if len(resp_header) != 4:
        print(f"[ERROR] Received {len(resp_header)} bytes instead of 4")
        exit(1)
    
    resp_type, resp_cmd, size_hi, size_lo = struct.unpack('BBBB', resp_header)
    response_data_size = (size_hi << 8) | size_lo
    
    print(f"[4] Response header: type={resp_type}, cmd={resp_cmd:#04x}, size={response_data_size}")
    
    # Receive response data (including SW)
    if response_data_size > 0:
        response_data = sock.recv(response_data_size)
        print(f"[4] Received {len(response_data)} bytes:")
        print(f"    Hex: {response_data.hex().upper()}")
        
        # Extract SW (last 2 bytes)
        if len(response_data) >= 2:
            sw1 = response_data[-2]
            sw2 = response_data[-1]
            sw = (sw1 << 8) | sw2
            
            print(f"\n[5] Status Word: 0x{sw:04X}")
            
            if sw == 0x9000:
                print("    ✓ SUCCESS - Command executed successfully!")
                
                # Extract response data (excluding SW)
                if len(response_data) > 2:
                    resp_payload = response_data[:-2]
                    print(f"    Response payload: {resp_payload.hex().upper()}")
            elif sw == 0x6D00:
                print("    ⚠ INS NOT SUPPORTED - VM may not have APDU handler")
            else:
                print(f"    ⚠ Error or warning status")
        else:
            print("[ERROR] Response too short (no SW)")
    else:
        print("[ERROR] No response data received")
    
    print("\n[6] Test completed!")
    
except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()

finally:
    print("\n[7] Connection closed")
    sock.close()

print("=" * 70)
