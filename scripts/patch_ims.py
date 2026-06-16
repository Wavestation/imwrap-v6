#!/usr/bin/env python3
import sys
import os

def read_vlq(data, offset):
    val = 0
    while True:
        b = data[offset]
        offset += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return val, offset

def write_vlq(val):
    bytes_list = []
    bytes_list.append(val & 0x7F)
    val >>= 7
    while val > 0:
        bytes_list.append((val & 0x7F) | 0x80)
        val >>= 7
    return bytes(reversed(bytes_list))

def parse_track(track_data):
    offset = 0
    events = []
    running_status = None
    
    while offset < len(track_data):
        delta_time, offset = read_vlq(track_data, offset)
        if offset >= len(track_data):
            break
            
        status = track_data[offset]
        offset += 1
        
        if status >= 0x80:
            event_status = status
            if status < 0xF0:
                running_status = status
        else:
            if running_status is None:
                raise ValueError("Running status used before any status byte")
            event_status = running_status
            offset -= 1
            
        event = {
            'delta_time': delta_time,
            'status': status,
            'event_status': event_status
        }
        
        if event_status == 0xFF:
            event['meta_type'] = track_data[offset]
            offset += 1
            length, offset = read_vlq(track_data, offset)
            event['payload'] = track_data[offset:offset+length]
            offset += length
        elif event_status in (0xF0, 0xF7):
            length, offset = read_vlq(track_data, offset)
            event['payload'] = track_data[offset:offset+length]
            offset += length
        else:
            status_class = event_status & 0xF0
            if status_class in (0xC0, 0xD0):
                event['data1'] = track_data[offset]
                offset += 1
            else:
                event['data1'] = track_data[offset]
                event['data2'] = track_data[offset+1]
                offset += 2
                
        events.append(event)
        
    return events

def serialize_track(events):
    out = bytearray()
    for event in events:
        out.extend(write_vlq(event['delta_time']))
        
        status = event['status']
        event_status = event['event_status']
        
        if status >= 0x80:
            out.append(status)
            
        if event_status == 0xFF:
            out.append(event['meta_type'])
            out.extend(write_vlq(len(event['payload'])))
            out.extend(event['payload'])
        elif event_status in (0xF0, 0xF7):
            out.extend(write_vlq(len(event['payload'])))
            out.extend(event['payload'])
        else:
            status_class = event_status & 0xF0
            if status_class in (0xC0, 0xD0):
                out.append(event['data1'])
            else:
                out.append(event['data1'])
                out.append(event['data2'])
    return bytes(out)

def patch_sysex_payload(payload):
    # Check if this is a malformed iMUSE allocate-part SysEx (length 20, starts with 7D 00, ends with F7)
    if len(payload) == 20 and payload[0] == 0x7D and payload[1] == 0x00 and payload[19] == 0xF7:
        part = payload[2]
        unknown = payload[3]
        nibbles = list(payload[4:19]) # 15 nibbles
        
        new_nibbles = []
        for i in range(7):
            new_nibbles.append(nibbles[2*i+1])
            new_nibbles.append(nibbles[2*i])
            
        # The 8th pair is high nibble 0x00, low nibble nibbles[14]
        new_nibbles.append(0x00)
        new_nibbles.append(nibbles[14])
        
        patched_payload = bytearray([0x7D, 0x00, part, unknown])
        patched_payload.extend(new_nibbles)
        patched_payload.append(0xF7)
        
        return bytes(patched_payload)
        
    return payload

def patch_smf(smf_bytes):
    offset = 0
    new_smf = bytearray()
    
    if smf_bytes[offset:offset+4] != b'MThd':
        raise ValueError("Not a valid SMF file (missing MThd)")
    
    mthd_len = int.from_bytes(smf_bytes[offset+4:offset+8], 'big')
    new_smf.extend(smf_bytes[offset:offset+8+mthd_len])
    offset += 8 + mthd_len
    
    patched_count = 0
    
    while offset < len(smf_bytes):
        chunk_type = smf_bytes[offset:offset+4]
        chunk_len = int.from_bytes(smf_bytes[offset+4:offset+8], 'big')
        
        if chunk_type == b'MTrk':
            track_data = smf_bytes[offset+8:offset+8+chunk_len]
            events = parse_track(track_data)
            
            for event in events:
                if event['event_status'] in (0xF0, 0xF7):
                    old_payload = event['payload']
                    new_payload = patch_sysex_payload(old_payload)
                    if new_payload != old_payload:
                        event['payload'] = new_payload
                        patched_count += 1
            
            serialized_track = serialize_track(events)
            new_smf.extend(b'MTrk')
            new_smf.extend(len(serialized_track).to_bytes(4, 'big'))
            new_smf.extend(serialized_track)
        else:
            new_smf.extend(smf_bytes[offset:offset+8+chunk_len])
            
        offset += 8 + chunk_len
        
    return bytes(new_smf), patched_count

class Chunk:
    def __init__(self, chunk_id, body=b''):
        self.chunk_id = chunk_id
        self.body = body

    def serialize(self):
        if isinstance(self.body, list):
            serialized_body = b''.join(sub.serialize() for sub in self.body)
        else:
            serialized_body = self.body
        total_size = len(serialized_body) + 8
        return self.chunk_id + total_size.to_bytes(4, 'big') + serialized_body

def parse_chunks(data):
    offset = 0
    chunks = []
    while offset < len(data):
        if len(data) - offset < 8:
            break
        chunk_id = data[offset:offset+4]
        size = int.from_bytes(data[offset+4:offset+8], 'big')
        body = data[offset+8:offset+size]
        chunks.append(Chunk(chunk_id, body))
        offset += size
    return chunks

def parse_variant_body(body):
    offset = 0
    mdhd_chunk = None
    if body.startswith(b'MDhd'):
        size = int.from_bytes(body[4:8], 'big')
        mdhd_chunk = Chunk(b'MDhd', body[8:size])
        offset += size
    smf_data = body[offset:]
    return mdhd_chunk, smf_data

def serialize_variant_body(mdhd_chunk, smf_data):
    res = b''
    if mdhd_chunk is not None:
        res += mdhd_chunk.serialize()
    res += smf_data
    return res

def patch_ims_file(input_path, output_path):
    print(f"Processing IMS file: {input_path}")
    with open(input_path, 'rb') as f:
        data = f.read()

    if len(data) < 8 or data[0:4] != b'IMSB':
        raise ValueError("Not a valid IMS file (missing IMSB root)")

    imsb_size = int.from_bytes(data[4:8], 'big')
    imsb_body = data[8:imsb_size]
    
    root_chunks = parse_chunks(imsb_body)
    
    ihdr_chunk = None
    sdir_chunk = None
    soun_chunks = []
    
    for chunk in root_chunks:
        if chunk.chunk_id == b'IHDR':
            ihdr_chunk = chunk
        elif chunk.chunk_id == b'SDIR':
            sdir_chunk = chunk
        elif chunk.chunk_id == b'SOUN':
            soun_chunks.append(chunk)

    if ihdr_chunk is None or sdir_chunk is None:
        raise ValueError("IMS file is missing IHDR or SDIR chunk")

    total_patched = 0
    
    # Process each SOUN chunk
    for soun in soun_chunks:
        sub_chunks = parse_chunks(soun.body)
        soun_id = None
        
        # Find SIDN and NAME if any
        for sub in sub_chunks:
            if sub.chunk_id == b'SIDN':
                soun_id = int.from_bytes(sub.body[0:2], 'big')
                break
                
        patched_soun = False
        for sub in sub_chunks:
            if sub.chunk_id in (b'GMD ', b'ROL '):
                mdhd, smf_data = parse_variant_body(sub.body)
                patched_smf_bytes, cnt = patch_smf(smf_data)
                if cnt > 0:
                    print(f"  Sound {soun_id} ({sub.chunk_id.decode().strip()}): patched {cnt} allocate-part SysEx(s).")
                    total_patched += cnt
                    patched_soun = True
                    sub.body = serialize_variant_body(mdhd, patched_smf_bytes)
                    
        if patched_soun:
            # Re-serialize SOUN body
            soun.body = b''.join(sub.serialize() for sub in sub_chunks)

    # Re-calculate SDIR and SOUN offsets
    # soundOffset is after IMSB header (8), IHDR chunk (24 bytes since IHDR body is 16 bytes), and SDIR chunk.
    # SDIR chunk has 8 bytes header + 12 bytes per SOUN chunk.
    sdir_body_size = len(soun_chunks) * 12
    sdir_chunk_size = 8 + sdir_body_size
    sound_offset = 8 + 24 + sdir_chunk_size
    
    sdir_body = bytearray()
    
    for soun in soun_chunks:
        # Find soundId and variantMask from its sub-chunks
        sound_id = 0
        variant_mask = 0
        sub_chunks = parse_chunks(soun.body)
        for sub in sub_chunks:
            if sub.chunk_id == b'SIDN':
                sound_id = int.from_bytes(sub.body[0:2], 'big')
            elif sub.chunk_id == b'GMD ':
                variant_mask |= 1  # VariantKind::Gmd = 1
            elif sub.chunk_id == b'ROL ':
                variant_mask |= 2  # VariantKind::Rol = 2
                
        soun_serialized_size = len(soun.serialize())
        
        sdir_body.extend(sound_id.to_bytes(2, 'big'))
        sdir_body.extend(variant_mask.to_bytes(2, 'big'))
        sdir_body.extend(sound_offset.to_bytes(4, 'big'))
        sdir_body.extend(soun_serialized_size.to_bytes(4, 'big'))
        
        sound_offset += soun_serialized_size

    # Update SDIR chunk body
    sdir_chunk.body = bytes(sdir_body)
    
    # Rebuild IMSB
    root = Chunk(b'IMSB', [ihdr_chunk, sdir_chunk] + soun_chunks)
    final_bytes = root.serialize()
    
    with open(output_path, 'wb') as f:
        f.write(final_bytes)
        
    print(f"Successfully wrote {output_path}. Total patched SysEx messages: {total_patched}\n")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: patch_ims.py input.ims [output.ims]")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file
    
    try:
        patch_ims_file(input_file, output_file)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
