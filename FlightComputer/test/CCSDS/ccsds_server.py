import socket
import threading
import argparse
from spacepackets.ecss.tc import PusTc
from spacepackets.ccsds.spacepacket import PacketType, SequenceFlags

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

            self.peer_socket, addr = self.socket.accept()
            print(f"Connection established from {addr}")
        else:
            self.peer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            print(f"Connecting to {self.host}:{self.port}")
            self.peer_socket.connect((self.host, self.port))
            print("Connected to server")

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

                # Try to parse TC packets
                while buffer:
                    try:
                        tc = PusTc.unpack(buffer)
                        print(f"Received TC: Service={tc.service}, Subservice={tc.subservice}, "
                              f"APID={tc.apid}, Data={tc.app_data.hex()}")
                        buffer = buffer[tc.packet_len:]
                    except Exception as e:
                        if len(buffer) < 6:  # Minimum CCSDS header size
                            break
                        buffer = buffer[1:]  # Remove one byte and try again

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

def create_tc_packet(apid: int, service: int, subservice: int, data: bytes) -> bytes:
    """Create a TC packet using PusTc."""
    tc = PusTc(
        service=service,
        subservice=subservice,
        apid=apid,
        app_data=data
    )
    return tc.pack()

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
                    packet = create_tc_packet(
                        apid=args.apid,
                        service=17,  # Example service
                        subservice=1,  # Example subservice
                        data=command_data
                    )

                    print(f"Sending TC packet: {packet.hex()}")
                    if endpoint.send_packet(packet):
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
    main()
