import struct
from spacepackets.spacepacket import (
    SpacePacketHeader,
    PacketType,
    SequenceFlags,
    SpacePacket,
)

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

def parse_frame(data: bytes) -> tuple[bool, bytes]:
    """Parse an F´ frame and return (success, payload)."""
    if len(data) < 12:  # Minimum frame size (header + CRC)
        return False, b''

    # Extract header components
    start_word = struct.unpack('>I', data[0:4])[0]
    size = struct.unpack('>I', data[4:8])[0]

    if start_word != 0xdeadbeef:
        return False, b''

    if len(data) < 8 + size + 4:  # Header + payload + CRC
        return False, b''

    frame_data = data[0:8+size]
    received_crc = struct.unpack('>I', data[8+size:8+size+4])[0]
    calculated_crc = calculate_crc32(frame_data)

    if received_crc != calculated_crc:
        return False, b''

    return True, data[8:8+size]
