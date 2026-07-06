import sys

data = open(r'D:\Prog\imwrap-v6\samples\demo-game-ags\iMWrapData\unpacked_demo\demo.soundir\90_Base Room.soun', 'rb').read()

offset = 0
count = 0
while True:
    idx = data.find(b'\xF0\x7D', offset)
    if idx == -1:
        break
    end = data.find(b'\xF7', idx)
    if end == -1:
        break
    sysex = data[idx:end+1]
    
    if len(sysex) > 2:
        code = sysex[2]
        if code == 0x00:  # AllocatePart
            if len(sysex) > 3:
                part = sysex[3] & 0x0F
                print(f'AllocatePart for part {part} at offset {idx}')
        elif code == 0x06: # HookJump
            print('HookJump at', idx)
        else:
            print(f'Sysex code {code:02X} at offset {idx}')
            
    offset = idx + 1
    count += 1

print('Total imwrap sysex found:', count)
