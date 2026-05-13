#!/usr/bin/env python3
"""
Test JCShell mode - connect to port 9000 and receive ATR
This simulates a card terminal tool connecting to JCShell
"""

import socket
import time

print("=" * 70)
print("Testing JCShell Mode (Port 9000)")
print("Simulating card terminal connection")
print("=" * 70)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    # Step 1: Connect to JCShell on port 9000
    print("\n[1] Connecting to localhost:9000...")
    sock.connect(('localhost', 9000))
    print("[1] Connected successfully!")
    
    # Step 2: Wait for ATR (JCShell should send ATR automatically)
    print("\n[2] Waiting for ATR from JCShell...")
    
    # Try to receive data
    data = sock.recv(1024)
    
    if data:
        print(f"[2] Received {len(data)} bytes:")
        print(f"    Hex: {data.hex().upper()}")
        
        # Check if it looks like an ATR
        if len(data) >= 2 and data[0] == 0x3B:
            print(f"    ✓ This appears to be a valid ATR!")
            print(f"    TS (Initial Character): 0x{data[0]:02X}")
            if len(data) > 1:
                print(f"    T0 (Format Byte): 0x{data[1]:02X}")
            if len(data) > 2:
                print(f"    Historical Bytes: {data[2:].hex().upper()}")
        else:
            print(f"    ⚠ Data received but doesn't look like standard ATR")
    else:
        print("[2] No data received (connection may have closed)")
    
    # Step 3: Keep connection open for a bit
    print("\n[3] Keeping connection open for 5 seconds...")
    time.sleep(5)
    
    print("\n[4] Test completed!")
    print("=" * 70)
    
except socket.timeout:
    print("\n[ERROR] Timeout - no ATR received within 10 seconds")
    print("This might indicate:")
    print("  1. JCShell is not running")
    print("  2. JCShell requires specific protocol handshake")
    print("  3. ATR is sent only after receiving POWER_UP command")
except ConnectionRefusedError:
    print("\n[ERROR] Connection refused - is gcos_demo.exe running in JCShell mode?")
    print("Try: ./build/Debug/gcos_demo.exe -j")
except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()
finally:
    sock.close()
    print("\n[5] Connection closed")
