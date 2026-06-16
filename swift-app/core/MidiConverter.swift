/* ==========================================================================
 *
 * iMWRAP V6 - A modern iMuse implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

import Foundation

public struct SwiftMidiEvent: Hashable {
    public var tick: UInt64
    public var status: UInt8
    public var eventStatus: UInt8
    public var metaType: UInt8?
    public var payload: [UInt8]
    public var data1: UInt8?
    public var data2: UInt8?
    
    public init(tick: UInt64, status: UInt8, eventStatus: UInt8, metaType: UInt8? = nil, payload: [UInt8] = [], data1: UInt8? = nil, data2: UInt8? = nil) {
        self.tick = tick
        self.status = status
        self.eventStatus = eventStatus
        self.metaType = metaType
        self.payload = payload
        self.data1 = data1
        self.data2 = data2
    }
}

public struct SwiftMidiTrack: Hashable {
    public var events: [SwiftMidiEvent]
    
    public init(events: [SwiftMidiEvent] = []) {
        self.events = events
    }
}

public struct SwiftMidiFile: Hashable {
    public var format: UInt16
    public var division: UInt16
    public var tracks: [SwiftMidiTrack]
    
    public init(format: UInt16 = 0, division: UInt16 = 480, tracks: [SwiftMidiTrack] = []) {
        self.format = format
        self.division = division
        self.tracks = tracks
    }
}

public class MidiConverter {
    
    public static func readVlq(_ data: [UInt8], _ offset: inout Int) -> UInt64 {
        var val: UInt64 = 0
        while offset < data.count {
            let b = data[offset]
            offset += 1
            val = (val << 7) | UInt64(b & 0x7F)
            if (b & 0x80) == 0 {
                break
            }
        }
        return val
    }

    public static func writeVlq(_ val: UInt64) -> [UInt8] {
        var bytesList: [UInt8] = []
        bytesList.append(UInt8(val & 0x7F))
        var v = val >> 7
        while v > 0 {
            bytesList.append(UInt8((v & 0x7F) | 0x80))
            v >>= 7
        }
        return Array(bytesList.reversed())
    }

    public static func parseTrack(_ data: [UInt8]) -> [SwiftMidiEvent] {
        var offset = 0
        var events: [SwiftMidiEvent] = []
        var runningStatus: UInt8? = nil
        var tick: UInt64 = 0
        
        while offset < data.count {
            let delta = readVlq(data, &offset)
            tick += delta
            
            if offset >= data.count { break }
            
            let status = data[offset]
            offset += 1
            
            let eventStatus: UInt8
            if status >= 0x80 {
                eventStatus = status
                if status < 0xF0 {
                    runningStatus = status
                }
            } else {
                guard let rs = runningStatus else {
                    continue
                }
                eventStatus = rs
                offset -= 1
            }
            
            var event = SwiftMidiEvent(tick: tick, status: status, eventStatus: eventStatus)
            
            if eventStatus == 0xFF {
                if offset >= data.count { break }
                let metaType = data[offset]
                offset += 1
                let length = Int(readVlq(data, &offset))
                let payload = Array(data[offset..<min(offset+length, data.count)])
                offset += length
                event.metaType = metaType
                event.payload = payload
            } else if eventStatus == 0xF0 || eventStatus == 0xF7 {
                let length = Int(readVlq(data, &offset))
                let payload = Array(data[offset..<min(offset+length, data.count)])
                offset += length
                event.payload = payload
            } else {
                let statusClass = eventStatus & 0xF0
                if statusClass == 0xC0 || statusClass == 0xD0 {
                    if offset >= data.count { break }
                    event.data1 = data[offset]
                    offset += 1
                } else {
                    if offset + 1 >= data.count { break }
                    event.data1 = data[offset]
                    event.data2 = data[offset+1]
                    offset += 2
                }
            }
            events.append(event)
        }
        return events
    }

    public static func serializeTrack(_ events: [SwiftMidiEvent], useRunningStatus: Bool = true) -> [UInt8] {
        var out: [UInt8] = []
        var lastTick: UInt64 = 0
        var activeRunningStatus: UInt8? = nil
        
        for event in events {
            let delta = event.tick - lastTick
            lastTick = event.tick
            
            out.append(contentsOf: writeVlq(delta))
            
            let eventStatus = event.eventStatus
            
            if useRunningStatus {
                if eventStatus >= 0x80 && eventStatus < 0xF0 {
                    if activeRunningStatus != eventStatus {
                        out.append(eventStatus)
                        activeRunningStatus = eventStatus
                    }
                } else {
                    out.append(eventStatus)
                    activeRunningStatus = nil
                }
            } else {
                out.append(eventStatus)
            }
            
            if eventStatus == 0xFF {
                if let mt = event.metaType {
                    out.append(mt)
                }
                out.append(contentsOf: writeVlq(UInt64(event.payload.count)))
                out.append(contentsOf: event.payload)
            } else if eventStatus == 0xF0 || eventStatus == 0xF7 {
                out.append(contentsOf: writeVlq(UInt64(event.payload.count)))
                out.append(contentsOf: event.payload)
            } else {
                let statusClass = eventStatus & 0xF0
                if statusClass == 0xC0 || statusClass == 0xD0 {
                    if let d1 = event.data1 {
                        out.append(d1)
                    }
                } else {
                    if let d1 = event.data1, let d2 = event.data2 {
                        out.append(d1)
                        out.append(d2)
                    }
                }
            }
        }
        return out
    }

    public static func parseMidiFile(_ data: [UInt8]) -> SwiftMidiFile? {
        if data.count < 14 { return nil }
        if Array(data[0..<4]) != [0x4D, 0x54, 0x68, 0x64] { return nil } // 'MThd'
        
        let headerLen = Int(UInt32(data[4]) << 24 | UInt32(data[5]) << 16 | UInt32(data[6]) << 8 | UInt32(data[7]))
        if headerLen < 6 || 8 + headerLen > data.count { return nil }
        
        let format = UInt16(data[8]) << 8 | UInt16(data[9])
        let trackCount = UInt16(data[10]) << 8 | UInt16(data[11])
        let division = UInt16(data[12]) << 8 | UInt16(data[13])
        
        var offset = 8 + headerLen
        var tracks: [SwiftMidiTrack] = []
        
        for _ in 0..<trackCount {
            if offset + 8 > data.count { break }
            let type = Array(data[offset..<offset+4])
            if type != [0x4D, 0x54, 0x72, 0x6B] { // 'MTrk'
                break
            }
            let length = Int(UInt32(data[offset+4]) << 24 | UInt32(data[offset+5]) << 16 | UInt32(data[offset+6]) << 8 | UInt32(data[offset+7]))
            offset += 8
            if offset + length > data.count { break }
            let trackBytes = Array(data[offset..<offset+length])
            offset += length
            
            let events = parseTrack(trackBytes)
            tracks.append(SwiftMidiTrack(events: events))
        }
        
        return SwiftMidiFile(format: format, division: division, tracks: tracks)
    }

    public static func serializeMidiFile(_ file: SwiftMidiFile) -> [UInt8] {
        var out: [UInt8] = []
        // MThd
        out.append(contentsOf: [0x4D, 0x54, 0x68, 0x64])
        // Length (6)
        out.append(contentsOf: [0, 0, 0, 6])
        // Format
        out.append(UInt8((file.format >> 8) & 0xFF))
        out.append(UInt8(file.format & 0xFF))
        // Track count
        out.append(UInt8((file.tracks.count >> 8) & 0xFF))
        out.append(UInt8(file.tracks.count & 0xFF))
        // Division
        out.append(UInt8((file.division >> 8) & 0xFF))
        out.append(UInt8(file.division & 0xFF))
        
        for track in file.tracks {
            let trackBytes = serializeTrack(track.events, useRunningStatus: true)
            out.append(contentsOf: [0x4D, 0x54, 0x72, 0x6B]) // 'MTrk'
            let len = UInt32(trackBytes.count)
            out.append(UInt8((len >> 24) & 0xFF))
            out.append(UInt8((len >> 16) & 0xFF))
            out.append(UInt8((len >> 8) & 0xFF))
            out.append(UInt8(len & 0xFF))
            out.append(contentsOf: trackBytes)
        }
        return out
    }

    public static func mergeTracksToFormat0(_ file: SwiftMidiFile) -> SwiftMidiFile {
        if file.format == 0 {
            return file
        }
        
        var allEvents: [SwiftMidiEvent] = []
        var maxTick: UInt64 = 0
        
        for track in file.tracks {
            for event in track.events {
                // Exclude End of Track events
                if event.eventStatus == 0xFF && event.metaType == 0x2F {
                    continue
                }
                if event.tick > maxTick {
                    maxTick = event.tick
                }
                allEvents.append(event)
            }
        }
        
        // Sort events
        allEvents.sort { e1, e2 in
            if e1.tick != e2.tick {
                return e1.tick < e2.tick
            }
            
            let p1 = eventPriority(e1)
            let p2 = eventPriority(e2)
            return p1 < p2
        }
        
        // Append a single End of Track event at the end
        let endEvent = SwiftMidiEvent(tick: maxTick, status: 0xFF, eventStatus: 0xFF, metaType: 0x2F, payload: [])
        allEvents.append(endEvent)
        
        let singleTrack = SwiftMidiTrack(events: allEvents)
        return SwiftMidiFile(format: 0, division: file.division, tracks: [singleTrack])
    }

    private static func eventPriority(_ event: SwiftMidiEvent) -> Int {
        if event.eventStatus == 0xFF {
            let mt = event.metaType ?? 0
            if mt == 0x51 || mt == 0x58 || mt == 0x59 {
                return 0 // Conductor / Tempo events first
            }
            if mt == 0x2F {
                return 9 // End of track last
            }
            return 2
        }
        if event.eventStatus == 0xF0 || event.eventStatus == 0xF7 {
            return 1 // SysEx second
        }
        return 3 // Channel events third
    }
}
