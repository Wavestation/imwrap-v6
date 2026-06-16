import Foundation

public struct SwiftMdhd: Hashable, Codable {
    public var present: Bool = false
    public var priority: UInt8 = 90
    public var volume: UInt8 = 127
    public var pan: Int8 = 0
    public var transpose: Int8 = 0
    public var detune: Int8 = 0
    public var speed: UInt8 = 128
    
    public init(present: Bool = false, priority: UInt8 = 90, volume: UInt8 = 127, pan: Int8 = 0, transpose: Int8 = 0, detune: Int8 = 0, speed: UInt8 = 128) {
        self.present = present
        self.priority = priority
        self.volume = volume
        self.pan = pan
        self.transpose = transpose
        self.detune = detune
        self.speed = speed
    }
}

public struct SwiftVariant: Hashable, Codable {
    public var kind: String // "GMD " or "ROL "
    public var mdhd: SwiftMdhd = SwiftMdhd()
    public var smfData: [UInt8] = []
    
    public init(kind: String, mdhd: SwiftMdhd = SwiftMdhd(), smfData: [UInt8] = []) {
        self.kind = kind
        self.mdhd = mdhd
        self.smfData = smfData
    }
}

public struct SwiftSound: Identifiable, Hashable, Codable {
    public var id: UInt16
    public var name: String = ""
    public var variants: [SwiftVariant] = []
    
    public init(id: UInt16, name: String = "", variants: [SwiftVariant] = []) {
        self.id = id
        self.name = name
        self.variants = variants
    }
}

public struct SwiftImsProject: Hashable, Codable {
    public var sounds: [SwiftSound] = []
    
    public init(sounds: [SwiftSound] = []) {
        self.sounds = sounds
    }
}

public class ImsSerializer {
    
    public static func parseChunks(_ data: [UInt8], offset: inout Int, endOffset: Int) -> [(id: String, body: [UInt8])] {
        var result: [(id: String, body: [UInt8])] = []
        while offset < endOffset {
            if endOffset - offset < 8 { break }
            let idBytes = data[offset..<offset+4]
            let id = String(bytes: idBytes, encoding: .ascii) ?? "UNKN"
            let size = Int(UInt32(data[offset+4]) << 24 | UInt32(data[offset+5]) << 16 | UInt32(data[offset+6]) << 8 | UInt32(data[offset+7]))
            if offset + size > endOffset || size < 8 { break }
            let body = Array(data[offset+8..<offset+size])
            offset += size
            result.append((id: id, body: body))
        }
        return result
    }
    
    public static func makeChunk(id: String, body: [UInt8]) -> [UInt8] {
        var out: [UInt8] = []
        let idBytes = Array(id.utf8)
        if idBytes.count == 4 {
            out.append(contentsOf: idBytes)
        } else {
            out.append(contentsOf: [0x20, 0x20, 0x20, 0x20])
        }
        let size = UInt32(body.count + 8)
        out.append(UInt8((size >> 24) & 0xFF))
        out.append(UInt8((size >> 16) & 0xFF))
        out.append(UInt8((size >> 8) & 0xFF))
        out.append(UInt8(size & 0xFF))
        out.append(contentsOf: body)
        return out
    }
    
    public static func loadIms(from data: [UInt8]) -> SwiftImsProject? {
        if data.count < 8 || Array(data[0..<4]) != [0x49, 0x4D, 0x53, 0x42] { return nil } // 'IMSB'
        let imsbSize = Int(UInt32(data[4]) << 24 | UInt32(data[5]) << 16 | UInt32(data[6]) << 8 | UInt32(data[7]))
        if imsbSize > data.count { return nil }
        
        var offset = 8
        let rootChunks = parseChunks(data, offset: &offset, endOffset: imsbSize)
        
        guard let _ = rootChunks.first(where: { $0.id == "IHDR" }),
              let _ = rootChunks.first(where: { $0.id == "SDIR" }) else {
            return nil
        }
        
        let sounChunks = rootChunks.filter { $0.id == "SOUN" }
        var project = SwiftImsProject()
        
        for soun in sounChunks {
            var sounOffset = 0
            let subChunks = parseChunks(soun.body, offset: &sounOffset, endOffset: soun.body.count)
            
            guard let sidnChunk = subChunks.first(where: { $0.id == "SIDN" }), sidnChunk.body.count >= 2 else {
                continue
            }
            let soundId = UInt16(sidnChunk.body[0]) << 8 | UInt16(sidnChunk.body[1])
            var name = ""
            if let nameChunk = subChunks.first(where: { $0.id == "NAME" }) {
                name = String(bytes: nameChunk.body, encoding: .utf8) ?? ""
            }
            
            var variants: [SwiftVariant] = []
            for sub in subChunks {
                if sub.id == "GMD " || sub.id == "ROL " {
                    var varOffset = 0
                    var mdhd = SwiftMdhd()
                    if sub.body.count >= 8 && Array(sub.body[0..<4]) == [0x4D, 0x44, 0x68, 0x64] { // 'MDhd'
                        let mdhdSize = Int(UInt32(sub.body[4]) << 24 | UInt32(sub.body[5]) << 16 | UInt32(sub.body[6]) << 8 | UInt32(sub.body[7]))
                        if mdhdSize == 16 && sub.body.count >= 16 {
                            mdhd.present = true
                            mdhd.priority = sub.body[10]
                            mdhd.volume = sub.body[11]
                            mdhd.pan = Int8(bitPattern: sub.body[12])
                            mdhd.transpose = Int8(bitPattern: sub.body[13])
                            mdhd.detune = Int8(bitPattern: sub.body[14])
                            mdhd.speed = sub.body[15]
                            varOffset = 16
                        }
                    }
                    let smf = Array(sub.body[varOffset...])
                    variants.append(SwiftVariant(kind: sub.id, mdhd: mdhd, smfData: smf))
                }
            }
            project.sounds.append(SwiftSound(id: soundId, name: name, variants: variants))
        }
        
        return project
    }
    
    public static func saveIms(_ project: SwiftImsProject) -> [UInt8] {
        var sounChunks: [(soundId: UInt16, variantMask: UInt16, data: [UInt8])] = []
        
        for sound in project.sounds {
            var body: [UInt8] = []
            
            // SIDN
            let sidnBody: [UInt8] = [
                UInt8((sound.id >> 8) & 0xFF), UInt8(sound.id & 0xFF),
                0, 0
            ]
            body.append(contentsOf: makeChunk(id: "SIDN", body: sidnBody))
            
            // NAME
            if !sound.name.isEmpty {
                let nameBytes = Array(sound.name.utf8)
                body.append(contentsOf: makeChunk(id: "NAME", body: nameBytes))
            }
            
            // Variants
            var mask: UInt16 = 0
            for variant in sound.variants {
                var varBody: [UInt8] = []
                if variant.mdhd.present {
                    let mdhdBody: [UInt8] = [
                        0, 0,
                        variant.mdhd.priority,
                        variant.mdhd.volume,
                        UInt8(bitPattern: variant.mdhd.pan),
                        UInt8(bitPattern: variant.mdhd.transpose),
                        UInt8(bitPattern: variant.mdhd.detune),
                        variant.mdhd.speed
                    ]
                    varBody.append(contentsOf: makeChunk(id: "MDhd", body: mdhdBody))
                }
                varBody.append(contentsOf: variant.smfData)
                
                if variant.kind == "GMD " {
                    mask |= 1
                    body.append(contentsOf: makeChunk(id: "GMD ", body: varBody))
                } else if variant.kind == "ROL " {
                    mask |= 2
                    body.append(contentsOf: makeChunk(id: "ROL ", body: varBody))
                }
            }
            
            let sounChunkBytes = makeChunk(id: "SOUN", body: body)
            sounChunks.append((soundId: sound.id, variantMask: mask, data: sounChunkBytes))
        }
        
        sounChunks.sort { $0.soundId < $1.soundId }
        
        let ihdrSize = 8 + 16
        let sdirSize = 8 + sounChunks.count * 12
        var soundOffset = 8 + ihdrSize + sdirSize
        
        var sdirBody: [UInt8] = []
        for soun in sounChunks {
            let sounSize = soun.data.count
            
            sdirBody.append(UInt8((soun.soundId >> 8) & 0xFF))
            sdirBody.append(UInt8(soun.soundId & 0xFF))
            sdirBody.append(UInt8((soun.variantMask >> 8) & 0xFF))
            sdirBody.append(UInt8(soun.variantMask & 0xFF))
            
            sdirBody.append(UInt8((soundOffset >> 24) & 0xFF))
            sdirBody.append(UInt8((soundOffset >> 16) & 0xFF))
            sdirBody.append(UInt8((soundOffset >> 8) & 0xFF))
            sdirBody.append(UInt8(soundOffset & 0xFF))
            
            sdirBody.append(UInt8((sounSize >> 24) & 0xFF))
            sdirBody.append(UInt8((sounSize >> 16) & 0xFF))
            sdirBody.append(UInt8((sounSize >> 8) & 0xFF))
            sdirBody.append(UInt8(sounSize & 0xFF))
            
            soundOffset += sounSize
        }
        
        var ihdrBody: [UInt8] = [
            0, 1,
            0, 0
        ]
        let soundCount = UInt32(sounChunks.count)
        ihdrBody.append(UInt8((soundCount >> 24) & 0xFF))
        ihdrBody.append(UInt8((soundCount >> 16) & 0xFF))
        ihdrBody.append(UInt8((soundCount >> 8) & 0xFF))
        ihdrBody.append(UInt8(soundCount & 0xFF))
        ihdrBody.append(contentsOf: [0, 0, 0, 0, 0, 0, 0, 0])
        
        let ihdrChunk = makeChunk(id: "IHDR", body: ihdrBody)
        let sdirChunk = makeChunk(id: "SDIR", body: sdirBody)
        
        var imsbBody: [UInt8] = []
        imsbBody.append(contentsOf: ihdrChunk)
        imsbBody.append(contentsOf: sdirChunk)
        for soun in sounChunks {
            imsbBody.append(contentsOf: soun.data)
        }
        
        return makeChunk(id: "IMSB", body: imsbBody)
    }
}
