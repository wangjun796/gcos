#!/usr/bin/env python3
"""
Test JCShell with TLP224 protocol
Send POWER_UP command and expect ATR response
"""

import socket
import time

def byte_to_ascii_hex(byte_val):
    """Convert a byte to 2 ASCII hex characters"""
    hi = (byte_val >> 4) & 0x0F
    lo = byte_val & 0x0F
    
    if hi < 10:
        hi += ord('0')
    else:
        hi += ord('A') - 10
        
    if lo < 10:
        lo += ord('0')
    else:
        lo += ord('A') - 10
    
    return bytes([hi, lo])

def send_tlp224(sock, data):
    """Send TLP224 message (ASCII hex encoded with EOT)"""
    # Encode each byte as ASCII hex
    encoded = b''
    for byte_val in data:
        encoded += byte_to_ascii_hex(byte_val)
    
    # Add EOT terminator
    encoded += bytes([0x03])
    
    print(f"Sending TLP224 ({len(data)} binary bytes -> {len(encoded)} ASCII bytes)")
    print(f"  Binary: {data.hex().upper()}")
    print(f"  ASCII:  {encoded.decode('ascii', errors='replace')}")
    
    sock.sendall(encoded)

print("=" * 70)
print("Testing JCShell with TLP224 Protocol")
print("=" * 70)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    # Step 1: Connect
    print("\n[1] Connecting to localhost:9000...")
    sock.connect(('localhost', 9000))
    print("[1] Connected!")
    
    # Wait a bit for server to send ATR (if it does)
    print("\n[2] Waiting 1 second for automatic ATR...")
    time.sleep(1)
    
    try:
        sock.settimeout(1.0)
        data = sock.recv(1024)
        if data:
            print(f"[2] Received automatic ATR: {data}")
    except socket.timeout:
        print("[2] No automatic ATR received")
    
    sock.settimeout(10.0)
    
    # Step 3: Send POWER_UP command via TLP224
    print("\n[3] Sending POWER_UP command via TLP224...")
    
    # Build POWER_UP message: [ACK][LenHi][LenLo][POWER_UP][LRC]
    power_up_msg = bytes([
        0x06,  # ACK
        0x00,  # Length Hi
        0x04,  # Length Lo (4 bytes follow)
        0x21,  # POWER_UP command
        0x00,  # Will be replaced with LRC
    ])
    
    # Calculate LRC (XOR of all bytes except LRC itself)
    lrc = 0
    for b in power_up_msg[:-1]:
        lrc ^= b
    power_up_msg = power_up_msg[:-1] + bytes([lrc])
    
    print(f"POWER_UP message: {power_up_msg.hex().upper()}")
    
    send_tlp224(sock, power_up_msg)
    print("[3] POWER_UP sent!")
    
    # Step 4: Receive response
    print("\n[4] Receiving response...")
    response = sock.recv(4096)
    
    if response:
        print(f"[4] Received {len(response)} bytes:")
        print(f"    Raw: {response}")
        
        # Try to decode ASCII hex back to binary
        try:
            ascii_str = response.decode('ascii')
            # Remove EOT
            if ascii_str.endswith('\x03'):
                ascii_str = ascii_str[:-1]
            
            # Decode pairs of ASCII hex to binary
            binary_data = bytes.fromhex(ascii_str)
            print(f"    Decoded: {binary_data.hex().upper()}")
            
            # Check for ATR indicator (0x28 at position 4)
            if len(binary_data) > 4 and binary_data[4] == 0x28:
                print(f"    ✓ This is an ATR response!")
                atr_start = 7
                atr_len = binary_data[6]
                if len(binary_data) >= atr_start + atr_len:
                    atr = binary_data[atr_start:atr_start+atr_len]
                    print(f"    ATR: {atr.hex().upper()}")
        except Exception as e:
            print(f"    ⚠ Could not decode as TLP224: {e}")
    else:
        print("[4] No response received")
    
    print("\n[5] Test completed!")
    print("=" * 70)
    
except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()
finally:
    sock.close()
    print("\n[6] Connection closed")
