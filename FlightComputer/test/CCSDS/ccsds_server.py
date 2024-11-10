#!/usr/bin/env python3
import socket
import threading
import argparse
import time
import struct
from ccsds_common import create_ccsds_tc_packet, frame_packet, parse_frame

class TcpEndpoint:
    def __init__(self, host: str, port: int, is_server: bool):
        self.host = host
        self.port = port
        self.is_server = is_server
        self.socket = None
        self.peer_socket = None
        self.running = False
        self.receive_thread = None

    def start(self):
        """Start the TCP endpoint."""
        self.running = True

        if self.is_server:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind((self.host, self.port))
            self.socket.listen(1)
            print(f"Server listening on {self.host}:{self.port}")

            # Accept connection
            self.peer_socket, addr = self.socket.accept()
            print(f"Connection established from {addr}")
        else:
            self.peer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            print(f"Connecting to {self.host}:{self.port}")
            self.peer_socket.connect((self.host, self.port))
            print("Connected to server")

        # Start receive thread
        self.receive_thread = threading.Thread(target=self.receive_loop)
        self.receive_thread.start()

    def receive_loop(self):
        """Continuously receive and process data."""
        buffer = bytearray()
        while self.running:
            try:
                data = self.peer_socket.recv(1024)
                if not data:
                    print("Connection closed by peer")
                    break

                buffer.extend(data)

                # Try to parse complete frames
                while len(buffer) >= 12:  # Minimum frame size
                    success, payload = parse_frame(buffer)
                    if success:
                        print(f"Received payload: {payload.hex()}")
                        # Remove processed frame
                        frame_size = 8 + struct.unpack('>I', buffer[4:8])[0] + 4
                        buffer = buffer[frame_size:]
                    else:
                        # If parse failed, remove one byte and try again
                        buffer = buffer[1:]

            except Exception as e:
                if self.running:
                    print(f"Error receiving data: {e}")
                break

    def send_packet(self, packet: bytes) -> bool:
        """Send a packet to the peer."""
        if not self.peer_socket:
            print("No connection established")
            return False
        try:
            self.peer_socket.send(packet)
            return True
        except Exception as e:
            print(f"Error sending packet: {e}")
            return False

    def stop(self):
        """Stop the TCP endpoint."""
        self.running = False
        if self.peer_socket:
            self.peer_socket.close()
        if self.socket:
            self.socket.close()
        if self.receive_thread:
            self.receive_thread.join()
        print("Endpoint stopped")

def parse_args():
    parser = argparse.ArgumentParser(description='CCSDS TCP Endpoint')
    parser.add_argument('--mode', choices=['server', 'client'], required=True,
                      help='Operating mode')
    parser.add_argument('--host', default='localhost',
                      help='Host address (server: bind address, client: connect address)')
    parser.add_argument('--port', type=int, default=50000,
                      help='Port number')
    parser.add_argument('--apid', type=lambda x: int(x,0), default=0x1FF,
                      help='APID (hex)')
    return parser.parse_args()

def main():
    args = parse_args()

    endpoint = TcpEndpoint(
        host=args.host,
        port=args.port,
        is_server=(args.mode == 'server')
    )

    try:
        endpoint.start()

        while True:
            cmd = input("Enter command (send <hex_data>/status/quit): ").strip().lower()

            if cmd == 'quit':
                break
            elif cmd == 'status':
                print(f"Status: {'Connected' if endpoint.peer_socket else 'Not connected'}")
            elif cmd.startswith('send '):
                try:
                    hex_data = cmd.split(' ')[1]
                    command_data = bytes.fromhex(hex_data)
                    tc_packet = create_ccsds_tc_packet(apid=args.apid, user_data=command_data)
                    framed_packet = frame_packet(tc_packet)

                    print(f"Sending framed packet (hex): {framed_packet.hex()}")
                    if endpoint.send_packet(framed_packet):
                        print("Packet sent successfully")
                    else:
                        print("Failed to send packet")
                except Exception as e:
                    print(f"Error processing command: {e}")
            else:
                print("Unknown command")

    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        endpoint.stop()

if __name__ == "__main__":
    """
    To use these scripts:
        To run as a server (waiting for F´ or another client):
            python ccsds_tcp.py --mode server --port 50000 --apid 0x1FF

        To run as a client (connecting to F´ or another server):
            python ccsds_tcp.py --mode client --host localhost --port 50000 --apid 0x1FF

    You can test the communication by:

        Running two instances, one as server and one as client:
            # Terminal 1
            python ccsds_tcp.py --mode server --port 50000

            # Terminal 2
            python ccsds_tcp.py --mode client --port 50000

        Using the interactive commands:
            Enter command: status
            Enter command: send 00010203
            Enter command: quit

    Both endpoints will:
        + Display received packets
        + Allow sending packets
        + Handle connection state
        + Properly frame and parse CCSDS packets

    This setup allows you to:
        Test communication between two instances
        Act as either client or server when talking to F´
        Debug packet formatting and parsing
        Verify connection handling
    """

    main()
