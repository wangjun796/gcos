#!/usr/bin/env python3
"""
Test script for ECHO APDU handler (all commands)
Sends various APDU commands and verifies they are echoed back.
"""

import socket
import struct
import sys

def send_apdu(sock, cla, ins, p1, p2, data=b''):
    """Send APDU command via binary protocol"""
    
    # Build APDU: CLA INS P1 P2 [Lc] [Data]
    if data:
        apdu = bytes([cla, ins, p1, p2, len(data)]) + data
    else:
        apdu = bytes([cla, ins, p1, p2])
    
    print(f"\n[CMD] Sending: CLA=0x{cla:02X} INS=0x{ins:02X} P1=0x{p1:02X} P2=0x{p2:02X}")
    if data:
        print(f"      Data ({len(data)} bytes): {data.hex().upper()}")
    print(f"      Full APDU: {apdu.hex().upper()}")
    
    # Send via binary protocol
    # Header: [type:1][cmd:1][size_hi:1][size_lo:1]
    header = struct.pack('>BBH', 0x00, 0x01, len(apdu))
    sock.sendall(header + apdu)
    
    # Receive response header
    resp_header = sock.recv(4)
    if len(resp_header) < 4:
        print(f"[ERR] Incomplete response header")
        return None
    
    rtype, rcmd, rsize = struct.unpack('>BBH', resp_header)
    print(f"[RSP] Header: type={rtype}, cmd=0x{rcmd:02X}, size={rsize}")
    
    # Receive response data + SW
    if rsize > 0:
        response = sock.recv(rsize)
        if len(response) < rsize:
            print(f"[ERR] Incomplete response")
            return None
        
        # Last 2 bytes are SW
        if rsize >= 2:
            sw = (response[-2] << 8) | response[-1]
            data_part = response[:-2] if rsize > 2 else b''
            
            print(f"[RSP] Status Word: 0x{sw:04X}")
            if data_part:
                print(f"[RSP] Data ({len(data_part)} bytes): {data_part.hex().upper()}")
                
                # Verify echo
                if data and data_part == data:
                    print(f"[OK]  ✓ Echo verified - data matches!")
                elif data:
                    print(f"[WARN] ✗ Echo mismatch!")
                    print(f"       Sent:     {data.hex().upper()}")
                    print(f"       Received: {data_part.hex().upper()}")
            else:
                print(f"[RSP] No data in response")
            
            return sw
        else:
            print(f"[ERR] Response too short for SW")
            return None
    else:
        print(f"[RSP] No response data")
        return None

def main():
    print("=" * 70)
    print("Testing GCOS ECHO Handler (All Commands)")
    print("=" * 70)
    
    try:
        # Connect to JCShell
        print("\n[1] Connecting to localhost:9000...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('localhost', 9000))
        print("[1] Connected!")
        
        # Test 1: SELECT command with data
        print("\n" + "=" * 70)
        print("Test 1: SELECT Command (INS=0xA4)")
        print("=" * 70)
        select_data = bytes([0x01, 0x02, 0x03, 0x04, 0x05])
        sw = send_apdu(sock, 0x80, 0xA4, 0x00, 0x00, select_data)
        
        # Test 2: LOAD command with data
        print("\n" + "=" * 70)
        print("Test 2: LOAD Command (INS=0xE4)")
        print("=" * 70)
        load_data = bytes(range(0x00, 0x10))  # 16 bytes: 00-0F
        sw = send_apdu(sock, 0x80, 0xE4, 0x00, 0x00, load_data)
        
        # Test 3: INSTALL command with data
        print("\n" + "=" * 70)
        print("Test 3: INSTALL Command (INS=0xE6)")
        print("=" * 70)
        install_data = bytes([0xAA, 0xBB, 0xCC, 0xDD])
        sw = send_apdu(sock, 0x80, 0xE6, 0x00, 0x00, install_data)
        
        # Test 4: DELETE command without data
        print("\n" + "=" * 70)
        print("Test 4: DELETE Command (INS=0xE2) - No Data")
        print("=" * 70)
        sw = send_apdu(sock, 0x80, 0xE2, 0x00, 0x00)
        
        # Test 5: GET STATUS command
        print("\n" + "=" * 70)
        print("Test 5: GET STATUS Command (INS=0xF2)")
        print("=" * 70)
        status_data = bytes([0x01])
        sw = send_apdu(sock, 0x80, 0xF2, 0x00, 0x00, status_data)
        
        # Test 6: MANAGE CHANNEL command
        print("\n" + "=" * 70)
        print("Test 6: MANAGE CHANNEL Command (INS=0x70)")
        print("=" * 70)
        channel_data = bytes([0x00])
        sw = send_apdu(sock, 0x80, 0x70, 0x00, 0x00, channel_data)
        
        # Test 7: Custom ECHO command (INS=0xFF)
        print("\n" + "=" * 70)
        print("Test 7: Custom ECHO Command (INS=0xFF)")
        print("=" * 70)
        echo_data = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE])
        sw = send_apdu(sock, 0x80, 0xFF, 0x00, 0x00, echo_data)
        
        print("\n" + "=" * 70)
        print("All tests completed!")
        print("=" * 70)
        
    except Exception as e:
        print(f"\n[ERROR] {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        print("\n[END] Connection closed")
        sock.close()

if __name__ == '__main__':
    main()
