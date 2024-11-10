#!/usr/bin/env python3
import socket
import time
import threading
from spacepackets.ccsds.spacepacket import (
    SpacePacketHeader,
    PacketType,
    SequenceFlags,
    SpacePacket,
)
import argparse

def create_frame_header(data_length: int) -> bytes:
    """Create F´ frame header with start word and size."""
    START_WORD = 0xdeadbeef
    header = START_WORD.to_bytes(4, byteorder='big')
    size = data_length.to_bytes(4, byteorder='big')
    return header + size

def calculate_crc32(data: bytes) -> int:
    """Calculate CRC32 checksum."""
    polynomial = 0x104C11DB7
    crc = 0xFFFFFFFF

    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ polynomial) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF

    return crc

def frame_packet(packet: bytes) -> bytes:
    """Frame the packet with F´ framing."""
    frame_header = create_frame_header(len(packet))
    data = frame_header + packet
    crc = calculate_crc32(data)
    return data + crc.to_bytes(4, byteorder='big')

def create_ccsds_tc_packet(apid: int, user_data: bytes = b'') -> bytes:
    """Create a CCSDS TC packet."""
    sp_header = SpacePacketHeader(
        packet_type=PacketType.TC,
        apid=apid,
        seq_count=0,
        data_len=len(user_data),
        sec_header_flag=False,
        seq_flags=SequenceFlags.UNSEGMENTED
    )

    space_packet = SpacePacket(
        sp_header=sp_header,
        sec_header=None,
        user_data=user_data
    )

    return space_packet.pack()

class TcpServer:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket = None
        self.running = False

    def start(self):
        """Start the TCP server."""
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.running = True
        print(f"Server listening on {self.host}:{self.port}")

        # Start accepting connections in a separate thread
        self.accept_thread = threading.Thread(target=self.accept_connections)
        self.accept_thread.start()

    def accept_connections(self):
        """Accept incoming connections."""
        while self.running:
            try:
                self.client_socket, addr = self.server_socket.accept()
                print(f"Connection established from {addr}")
            except Exception as e:
                if self.running:  # Only print error if we're still meant to be running
                    print(f"Error accepting connection: {e}")

    def send_packet(self, packet: bytes) -> bool:
        """Send a packet to the connected client."""
        if self.client_socket is None:
            print("No client connected")
            return False
        try:
            self.client_socket.send(packet)
            return True
        except Exception as e:
            print(f"Error sending packet: {e}")
            self.client_socket = None
            return False

    def stop(self):
        """Stop the TCP server."""
        self.running = False
        if self.client_socket:
            self.client_socket.close()
        self.server_socket.close()
        self.accept_thread.join()
        print("Server stopped")

def parse_args():
    parser = argparse.ArgumentParser(description='TCP Server for F´ CCSDS TC packets')
    parser.add_argument('--host', default='0.0.0.0', help='Host address to bind to')
    parser.add_argument('--port', type=int, default=50001, help='Port number')
    parser.add_argument('--apid', type=lambda x: int(x,0), default=0x1FF, help='APID (hex)')
    return parser.parse_args()

def main():
    args = parse_args()

    # Create and start TCP server
    server = TcpServer(args.host, args.port)
    server.start()

    try:
        while True:
            cmd = input("Enter command (send/quit): ").strip().lower()

            if cmd == 'quit':
                break
            elif cmd == 'send':
                # Example command data (modify as needed)
                command_data = b'\x00\x01\x02\x03'

                # Create and frame CCSDS TC packet
                tc_packet = create_ccsds_tc_packet(apid=args.apid, user_data=command_data)
                framed_packet = frame_packet(tc_packet)

                # Print packet details for debugging
                print(f"Sending framed packet (hex): {framed_packet.hex()}")
                print(f"Packet length: {len(framed_packet)} bytes")

                # Send the packet
                if server.send_packet(framed_packet):
                    print("Packet sent successfully")
                else:
                    print("Failed to send packet")
            else:
                print("Unknown command")

    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        server.stop()

if __name__ == "__main__":
    main()
