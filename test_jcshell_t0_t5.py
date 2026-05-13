#!/usr/bin/env python3
"""
Test JCShell T0/T5 protocol distinction
Connect to both port 9000 (T=0) and 9900 (T=CL) simultaneously
"""

import socket
import struct
import time
import threading

print("=" * 70)
print("Testing JCShell T0/T5 Protocol Distinction")
print("=" * 70)

def test_connection(port, conn_type_name):
    """Test a single connection on specified port"""
    print(f"\n[{conn_type_name}] Testing port {port}...")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)
    
    try:
        # Step 1: Connect
        print(f"[{conn_type_name}] Connecting to localhost:{port}...")
        sock.connect(('localhost', port))
        print(f"[{conn_type_name}] Connected!")
        
        # Step 2: Send POWER_UP command
        print(f"[{conn_type_name}] Sending POWER_UP command...")
        power_up_header = struct.pack('BBBB', 0, 0x21, 0, 0)
        sock.sendall(power_up_header)
        
        # Receive ATR
        header = sock.recv(4)
        if len(header) != 4:
            print(f"[{conn_type_name}] [ERROR] Received {len(header)} bytes instead of 4")
            return False
        
        resp_type, resp_cmd, size_hi, size_lo = struct.unpack('BBBB', header)
        data_size = (size_hi << 8) | size_lo
        
        print(f"[{conn_type_name}] Response header: type={resp_type}, cmd={resp_cmd:#04x}, size={data_size}")
        
        if data_size > 0:
            atr_data = sock.recv(data_size)
            print(f"[{conn_type_name}] Received ATR ({len(atr_data)} bytes):")
            print(f"[{conn_type_name}]     Hex: {atr_data.hex().upper()}")
            
            if len(atr_data) >= 6:
                ts = atr_data[0]
                print(f"[{conn_type_name}]     TS: 0x{ts:02X}")
                
                if ts == 0x3B:
                    print(f"[{conn_type_name}]     ✓ Valid ATR (TS=0x3B)")
                else:
                    print(f"[{conn_type_name}]     ⚠ Unexpected TS value")
        
        # Step 3: Send SELECT APDU
        print(f"\n[{conn_type_name}] Sending SELECT APDU...")
        select_apdu = bytes([0x00, 0xA4, 0x04, 0x00, 0x08, 
                            0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00])
        
        apdu_header = struct.pack('BBBB', 0, 0x00, 0, len(select_apdu))
        sock.sendall(apdu_header + select_apdu)
        
        # Receive response
        resp_header = sock.recv(4)
        if len(resp_header) != 4:
            print(f"[{conn_type_name}] [ERROR] Failed to receive response header")
            return False
        
        rtype, rcmd, shi, slo = struct.unpack('BBBB', resp_header)
        resp_len = (shi << 8) | slo
        
        print(f"[{conn_type_name}] Response length: {resp_len} bytes")
        
        if resp_len > 0:
            resp_data = sock.recv(resp_len)
            sw1 = resp_data[-2]
            sw2 = resp_data[-1]
            sw = (sw1 << 8) | sw2
            
            print(f"[{conn_type_name}] Status Word: 0x{sw:04X}")
            print(f"[{conn_type_name}] Response data: {resp_data.hex().upper()}")
        
        print(f"[{conn_type_name}] ✓ Test completed successfully!")
        return True
        
    except Exception as e:
        print(f"[{conn_type_name}] [ERROR] {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    finally:
        sock.close()
        print(f"[{conn_type_name}] Connection closed")

# Test T=0 (port 9000)
print("\n" + "=" * 70)
print("Phase 1: Testing T=0 Protocol (Port 9000)")
print("=" * 70)
result_t0 = test_connection(9000, "T=0")

# Test T=CL (port 9900)
print("\n" + "=" * 70)
print("Phase 2: Testing T=CL Protocol (Port 9900)")
print("=" * 70)
result_t5 = test_connection(9900, "T=CL")

# Test simultaneous connections
print("\n" + "=" * 70)
print("Phase 3: Testing Simultaneous Connections")
print("=" * 70)

thread_t0 = threading.Thread(target=test_connection, args=(9000, "T=0"))
thread_t5 = threading.Thread(target=test_connection, args=(9900, "T=CL"))

print("\nStarting both connections simultaneously...")
thread_t0.start()
time.sleep(0.5)  # Small delay to ensure order
thread_t5.start()

thread_t0.join()
thread_t5.join()

# Final summary
print("\n" + "=" * 70)
print("Test Summary")
print("=" * 70)
print(f"T=0  (Port 9000): {'✓ PASSED' if result_t0 else '✗ FAILED'}")
print(f"T=CL (Port 9900): {'✓ PASSED' if result_t5 else '✗ FAILED'}")

if result_t0 and result_t5:
    print("\n🎉 All tests passed! T0/T5 protocol distinction is working correctly.")
else:
    print("\n⚠ Some tests failed. Check the logs above for details.")

print("=" * 70)
