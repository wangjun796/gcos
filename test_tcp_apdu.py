# Test TCP Server Mode
# Usage: python test_tcp_apdu.py

import socket
import sys
import time

def send_apdu(sock, apdu_hex):
    """Send APDU to server"""
    # Convert hex string to bytes
    apdu_bytes = bytes.fromhex(apdu_hex)
    
    # Send length (2 bytes, big-endian)
    length = len(apdu_bytes)
    sock.sendall(length.to_bytes(2, byteorder='big'))
    
    # Send APDU data
    sock.sendall(apdu_bytes)
    
    print(f"Sent APDU ({length} bytes): {apdu_hex}")

def receive_response(sock):
    """Receive response from server"""
    # Receive length (2 bytes)
    length_bytes = sock.recv(2)
    if len(length_bytes) != 2:
        return None, None
    
    total_length = int.from_bytes(length_bytes, byteorder='big')
    
    # Receive response data
    response_data = b''
    while len(response_data) < total_length:
        chunk = sock.recv(total_length - len(response_data))
        if not chunk:
            break
        response_data += chunk
    
    if len(response_data) < 2:
        return None, None
    
    # Extract SW (last 2 bytes)
    sw = int.from_bytes(response_data[-2:], byteorder='big')
    data = response_data[:-2] if len(response_data) > 2 else b''
    
    return data, sw

def main():
    host = 'localhost'
    port = 9028
    
    print(f"Connecting to {host}:{port}...")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print("Connected!\n")
        
        # Test APDU sequence
        test_apdus = [
            "00A4040008A000000003000000",  # SELECT
            "80F21000020000",               # GET STATUS
            "0070000000",                    # MANAGE CHANNEL
        ]
        
        for i, apdu in enumerate(test_apdus, 1):
            print(f"--- Test #{i} ---")
            send_apdu(sock, apdu)
            
            data, sw = receive_response(sock)
            
            if data is not None:
                print(f"Response Data ({len(data)} bytes): {data.hex().upper()}")
                print(f"SW: {sw:04X}")
                
                if sw == 0x9000:
                    print("✓ Success\n")
                else:
                    print(f"✗ Failed (SW={sw:04X})\n")
            else:
                print("ERROR: No response received\n")
            
            time.sleep(0.5)
        
        print("Test completed!")
        sock.close()
        
    except ConnectionRefusedError:
        print("ERROR: Connection refused. Is the server running?")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
