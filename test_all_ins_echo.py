#!/usr/bin/env python3
"""
Test script to verify ALL INS codes use echo handler
Sends various INS codes and verifies they all echo back data.
"""

import socket
import struct
import sys

def send_apdu(sock, ins, data=b''):
    """Send APDU command via binary protocol"""
    
    cla = 0x80
    p1 = 0x00
    p2 = 0x00
    
    # Build APDU: CLA INS P1 P2 [Lc] [Data]
    if data:
        apdu = bytes([cla, ins, p1, p2, len(data)]) + data
    else:
        apdu = bytes([cla, ins, p1, p2])
    
    print(f"\n[CMD] INS=0x{ins:02X} | APDU: {apdu.hex().upper()}")
    if data:
        print(f"      Data ({len(data)} bytes): {data.hex().upper()}")
    
    # Send via binary protocol
    header = struct.pack('>BBH', 0x00, 0x01, len(apdu))
    sock.sendall(header + apdu)
    
    # Receive response header
    resp_header = sock.recv(4)
    if len(resp_header) < 4:
        print(f"[ERR] Incomplete response header")
        return None
    
    rtype, rcmd, rsize = struct.unpack('>BBH', resp_header)
    
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
            
            print(f"[RSP] SW=0x{sw:04X}", end='')
            if data_part:
                print(f" | Data ({len(data_part)} bytes): {data_part.hex().upper()}", end='')
                
                # Verify echo
                if data and data_part == data:
                    print(" | [OK] Echo OK")
                    return True
                elif data:
                    print(f" | [FAIL] Mismatch!")
                    return False
            else:
                print(" | [OK] No data")
                return True
        else:
            print(f"[ERR] Response too short")
            return None
    else:
        print(f"[RSP] No response")
        return None

def main():
    print("=" * 70)
    print("Testing ALL INS Codes Use Echo Handler")
    print("=" * 70)
    
    try:
        # Connect to JCShell
        print("\n[1] Connecting to localhost:9000...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('localhost', 9000))
        print("[1] Connected!")
        
        # Test various INS codes
        test_cases = [
            (0xA4, bytes([0x01, 0x02, 0x03]), "SELECT (registered)"),
            (0xCA, bytes([0xAA, 0xBB, 0xCC]), "GET DATA (unregistered)"),
            (0xD4, bytes([0x11, 0x22]), "UPDATE DATA (unregistered)"),
            (0xE0, bytes([0xDE, 0xAD, 0xBE, 0xEF]), "SET STATUS (unregistered)"),
            (0xF0, bytes([0xCA, 0xFE]), "CUSTOM (unregistered)"),
            (0xFF, bytes([0x00, 0x01, 0x02, 0x03, 0x04]), "ECHO (registered)"),
            (0x12, bytes([0xAB, 0xCD]), "RANDOM (unregistered)"),
            (0x99, bytes([0x55, 0x66, 0x77]), "ANOTHER (unregistered)"),
        ]
        
        results = []
        for ins, data, desc in test_cases:
            print(f"\n{'=' * 70}")
            print(f"Test: {desc}")
            print('=' * 70)
            result = send_apdu(sock, ins, data)
            results.append((ins, desc, result))
        
        # Summary
        print("\n" + "=" * 70)
        print("SUMMARY")
        print("=" * 70)
        
        passed = sum(1 for _, _, r in results if r)
        total = len(results)
        
        for ins, desc, result in results:
            status = "[PASS]" if result else "[FAIL]"
            print(f"{status} INS=0x{ins:02X} - {desc}")
        
        print(f"\nTotal: {passed}/{total} tests passed")
        
        if passed == total:
            print("\n[SUCCESS] All INS codes use echo handler!")
        else:
            print(f"\n[WARNING] {total - passed} tests failed")
        
    except Exception as e:
        print(f"\n[ERROR] {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        print("\n[END] Connection closed")
        sock.close()

if __name__ == '__main__':
    main()
