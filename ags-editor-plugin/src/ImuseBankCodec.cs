using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace AgsImuse.Editor
{
    internal static class ImuseBankCodec
    {
        private sealed class DirectoryEntry
        {
            public ushort SoundId;
            public ushort VariantMask;
            public uint SoundOffset;
            public uint SoundSize;
        }

        private sealed class ParsedSmfSequence
        {
            public ushort Format;
            public ushort Division;
            public List<ImuseTrackModel> Tracks;

            public ParsedSmfSequence()
            {
                Tracks = new List<ImuseTrackModel>();
            }
        }

        private sealed class MergedEvent
        {
            public uint AbsoluteTick;
            public int Order;
            public ImuseMidiEventModel Event;
        }

        public static ImuseBankProjectModel LoadFromFile(string fullPath, string relativePath)
        {
            byte[] bytes = File.ReadAllBytes(fullPath);
            return LoadFromBytes(bytes, relativePath);
        }

        public static ImuseBankProjectModel LoadFromBytes(byte[] bytes, string relativePath)
        {
            if (bytes == null)
            {
                throw new ArgumentNullException("bytes");
            }

            Chunk rootChunk = ReadChunk(bytes, 0);
            if (rootChunk.Id != "IMSB")
            {
                throw new InvalidOperationException("Root chunk is not IMSB.");
            }

            List<DirectoryEntry> directoryEntries = new List<DirectoryEntry>();
            int offset = rootChunk.BodyOffset;
            int endOffset = rootChunk.EndOffset;
            ushort versionMajor = 0;
            uint soundCount = 0;

            while (offset < endOffset)
            {
                Chunk chunk = ReadChunk(bytes, offset);
                if (chunk.Id == "IHDR")
                {
                    if (chunk.BodyLength != 16)
                    {
                        throw new InvalidOperationException("IHDR must contain exactly 16 bytes.");
                    }

                    versionMajor = ReadUInt16BigEndian(bytes, chunk.BodyOffset);
                    soundCount = ReadUInt32BigEndian(bytes, chunk.BodyOffset + 4);
                }
                else if (chunk.Id == "SDIR")
                {
                    if ((chunk.BodyLength % 12) != 0)
                    {
                        throw new InvalidOperationException("SDIR size is not a multiple of 12 bytes.");
                    }

                    int entryOffset = chunk.BodyOffset;
                    while (entryOffset < chunk.EndOffset)
                    {
                        DirectoryEntry entry = new DirectoryEntry();
                        entry.SoundId = ReadUInt16BigEndian(bytes, entryOffset);
                        entry.VariantMask = ReadUInt16BigEndian(bytes, entryOffset + 2);
                        entry.SoundOffset = ReadUInt32BigEndian(bytes, entryOffset + 4);
                        entry.SoundSize = ReadUInt32BigEndian(bytes, entryOffset + 8);
                        directoryEntries.Add(entry);
                        entryOffset += 12;
                    }
                }

                offset = chunk.EndOffset;
            }

            if (versionMajor != 1)
            {
                throw new InvalidOperationException("Unsupported IMS major version: " + versionMajor + ".");
            }

            if (soundCount != (uint)directoryEntries.Count)
            {
                throw new InvalidOperationException("IHDR sound count does not match SDIR entries.");
            }

            directoryEntries.Sort(delegate(DirectoryEntry left, DirectoryEntry right)
            {
                return left.SoundId.CompareTo(right.SoundId);
            });

            ImuseBankProjectModel bank = new ImuseBankProjectModel();
            bank.RelativePath = relativePath ?? string.Empty;

            for (int i = 0; i < directoryEntries.Count; ++i)
            {
                bank.Sounds.Add(ParseSound(bytes, directoryEntries[i]));
            }

            return bank;
        }

        public static byte[] SaveToBytes(ImuseBankProjectModel bank)
        {
            if (bank == null)
            {
                throw new ArgumentNullException("bank");
            }

            List<ImuseSoundModel> sounds = new List<ImuseSoundModel>(bank.Sounds);
            sounds.Sort(delegate(ImuseSoundModel left, ImuseSoundModel right)
            {
                return left.SoundId.CompareTo(right.SoundId);
            });

            for (int i = 1; i < sounds.Count; ++i)
            {
                if (sounds[i - 1].SoundId == sounds[i].SoundId)
                {
                    throw new InvalidOperationException("Duplicate sound ID: " + sounds[i].SoundId + ".");
                }
            }

            List<ushort> soundIds = new List<ushort>();
            List<ushort> variantMasks = new List<ushort>();
            List<byte[]> soundChunks = new List<byte[]>();

            for (int i = 0; i < sounds.Count; ++i)
            {
                ushort variantMask;
                byte[] soundChunk = BuildSoundChunk(sounds[i], out variantMask);
                soundIds.Add(sounds[i].SoundId);
                variantMasks.Add(variantMask);
                soundChunks.Add(soundChunk);
            }

            byte[] ihdrChunk = BuildIhdrChunk((uint)sounds.Count);
            byte[] sdirChunk = BuildSdirChunk(soundIds, variantMasks, soundChunks);

            List<byte> rootBody = new List<byte>();
            rootBody.AddRange(ihdrChunk);
            rootBody.AddRange(sdirChunk);

            for (int i = 0; i < soundChunks.Count; ++i)
            {
                rootBody.AddRange(soundChunks[i]);
            }

            return BuildChunk("IMSB", rootBody.ToArray());
        }

        public static List<ImuseTrackModel> ImportMidiTracks(string[] filePaths, out ushort division)
        {
            if (filePaths == null || filePaths.Length == 0)
            {
                division = 480;
                return new List<ImuseTrackModel>();
            }

            List<ImuseTrackModel> allTracks = new List<ImuseTrackModel>();
            ushort firstDivision = 0;

            for (int pathIdx = 0; pathIdx < filePaths.Length; pathIdx++)
            {
                string fullPath = filePaths[pathIdx];
                byte[] bytes = File.ReadAllBytes(fullPath);
                ParsedSmfSequence sequence = ParseSmf(bytes);
                if (pathIdx == 0)
                {
                    firstDivision = sequence.Division;
                }

                string fileName = Path.GetFileName(fullPath);
                string baseName = Path.GetFileNameWithoutExtension(fullPath);

                if (sequence.Format == 1)
                {
                    ImuseTrackModel mergedTrack = MergeTracksToFormat0(sequence.Tracks);
                    mergedTrack.Name = filePaths.Length == 1 ? baseName + " (Merged)" : baseName;
                    mergedTrack.SourceFileName = fileName;
                    allTracks.Add(mergedTrack);
                }
                else
                {
                    for (int i = 0; i < sequence.Tracks.Count; ++i)
                    {
                        ImuseTrackModel track = CloneTrack(sequence.Tracks[i]);
                        track.Name = sequence.Tracks.Count == 1 ? baseName : baseName + " (T" + i + ")";
                        track.SourceFileName = fileName;
                        allTracks.Add(track);
                    }
                }
            }

            division = firstDivision;
            return allTracks;
        }

        private static ImuseSoundModel ParseSound(byte[] bytes, DirectoryEntry directoryEntry)
        {
            Chunk soundChunk = ReadChunk(bytes, (int)directoryEntry.SoundOffset);
            if (soundChunk.Id != "SOUN")
            {
                throw new InvalidOperationException("Directory entry points to an invalid SOUN chunk.");
            }

            ImuseSoundModel sound = new ImuseSoundModel();
            sound.SoundId = directoryEntry.SoundId;
            sound.Name = string.Empty;

            bool hasSidn = false;
            int offset = soundChunk.BodyOffset;
            while (offset < soundChunk.EndOffset)
            {
                Chunk chunk = ReadChunk(bytes, offset);
                if (chunk.Id == "SIDN")
                {
                    if (chunk.BodyLength != 4)
                    {
                        throw new InvalidOperationException("Invalid SIDN chunk.");
                    }

                    ushort sidn = ReadUInt16BigEndian(bytes, chunk.BodyOffset);
                    if (sidn != directoryEntry.SoundId)
                    {
                        throw new InvalidOperationException("SIDN does not match the directory sound ID.");
                    }
                    hasSidn = true;
                }
                else if (chunk.Id == "NAME")
                {
                    sound.Name = Encoding.UTF8.GetString(bytes, chunk.BodyOffset, chunk.BodyLength);
                }
                else if (chunk.Id == "GMD " || chunk.Id == "ROL " || chunk.Id == "ADL ")
                {
                    sound.Variants.Add(ParseVariant(bytes, chunk.Id, chunk.BodyOffset, chunk.BodyLength));
                }

                offset = chunk.EndOffset;
            }

            if (!hasSidn)
            {
                throw new InvalidOperationException("SOUN chunk is missing SIDN.");
            }

            if (sound.Name.Length == 0)
            {
                sound.Name = "Sound " + sound.SoundId;
            }

            return sound;
        }

        private static ImuseVariantModel ParseVariant(byte[] bytes, string chunkId, int bodyOffset, int bodyLength)
        {
            int offset = bodyOffset;
            int endOffset = bodyOffset + bodyLength;

            ImuseVariantModel variant = new ImuseVariantModel();
            variant.Kind = VariantKindFromChunkId(chunkId);
            variant.IncludeVariant = true;

            if ((endOffset - offset) >= 8 && ReadAscii(bytes, offset, 4) == "MDhd")
            {
                Chunk mdhdChunk = ReadChunk(bytes, offset);
                if (mdhdChunk.BodyLength != 8)
                {
                    throw new InvalidOperationException("Invalid MDhd chunk.");
                }

                variant.IncludeMdhd = true;
                variant.Priority = bytes[mdhdChunk.BodyOffset + 2];
                variant.Volume = bytes[mdhdChunk.BodyOffset + 3];
                variant.Pan = unchecked((sbyte)bytes[mdhdChunk.BodyOffset + 4]);
                variant.Transpose = unchecked((sbyte)bytes[mdhdChunk.BodyOffset + 5]);
                variant.Detune = unchecked((sbyte)bytes[mdhdChunk.BodyOffset + 6]);
                variant.Speed = bytes[mdhdChunk.BodyOffset + 7];
                offset = mdhdChunk.EndOffset;
            }

            byte[] smfBytes = Slice(bytes, offset, endOffset - offset);
            ParsedSmfSequence sequence = ParseSmf(smfBytes);
            variant.Division = sequence.Division;
            variant.Tracks.AddRange(sequence.Tracks);
            return variant;
        }

        private static ParsedSmfSequence ParseSmf(byte[] bytes)
        {
            if (bytes == null || bytes.Length < 14)
            {
                throw new InvalidOperationException("SMF data is too small.");
            }

            if (ReadAscii(bytes, 0, 4) != "MThd")
            {
                throw new InvalidOperationException("SMF data does not start with MThd.");
            }

            uint headerLength = ReadUInt32BigEndian(bytes, 4);
            if (headerLength < 6 || (8 + headerLength) > bytes.Length)
            {
                throw new InvalidOperationException("SMF header length is invalid.");
            }

            ParsedSmfSequence sequence = new ParsedSmfSequence();
            sequence.Format = ReadUInt16BigEndian(bytes, 8);
            ushort trackCount = ReadUInt16BigEndian(bytes, 10);
            sequence.Division = ReadUInt16BigEndian(bytes, 12);

            int offset = (int)(8 + headerLength);
            for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
            {
                if ((offset + 8) > bytes.Length)
                {
                    throw new InvalidOperationException("SMF track header is truncated.");
                }

                if (ReadAscii(bytes, offset, 4) != "MTrk")
                {
                    throw new InvalidOperationException("Expected MTrk chunk.");
                }

                uint trackLength = ReadUInt32BigEndian(bytes, offset + 4);
                offset += 8;
                if ((offset + trackLength) > bytes.Length)
                {
                    throw new InvalidOperationException("SMF track data extends beyond the input size.");
                }

                ImuseTrackModel track = ParseTrack(bytes, offset, (int)trackLength, trackIndex);
                sequence.Tracks.Add(track);
                offset += (int)trackLength;
            }

            return sequence;
        }

        private static ImuseTrackModel ParseTrack(byte[] bytes, int offset, int length, int trackIndex)
        {
            ImuseTrackModel track = new ImuseTrackModel();
            track.Name = "Track " + trackIndex;
            track.SourceFileName = "Embedded";

            uint tick = 0;
            int endOffset = offset + length;
            byte runningStatus = 0;

            while (offset < endOffset)
            {
                uint delta = ReadVlq(bytes, ref offset, endOffset);
                tick += delta;
                if (offset >= endOffset)
                {
                    throw new InvalidOperationException("Unexpected end of track after delta-time.");
                }

                byte first = bytes[offset++];
                ImuseMidiEventModel midiEvent = new ImuseMidiEventModel();
                midiEvent.Tick = tick;
                midiEvent.Delta = delta;

                byte status = first;
                bool consumedData1 = false;
                byte impliedData1 = 0;

                if (first < 0x80)
                {
                    if (runningStatus == 0)
                    {
                        throw new InvalidOperationException("Running status used without a prior channel status.");
                    }

                    status = runningStatus;
                    consumedData1 = true;
                    impliedData1 = first;
                }

                if (status == 0xFF)
                {
                    midiEvent.Type = ImuseMidiEventType.Meta;
                    midiEvent.Status = status;
                    if (offset >= endOffset)
                    {
                        throw new InvalidOperationException("Meta event is missing its meta type.");
                    }

                    midiEvent.MetaType = bytes[offset++];
                    uint payloadLength = ReadVlq(bytes, ref offset, endOffset);
                    if ((offset + payloadLength) > endOffset)
                    {
                        throw new InvalidOperationException("Meta event payload length is invalid.");
                    }

                    midiEvent.Payload = Slice(bytes, offset, (int)payloadLength);
                    offset += (int)payloadLength;
                    if (midiEvent.MetaType == 0x03 && midiEvent.Payload.Length > 0)
                    {
                        track.Name = Encoding.UTF8.GetString(midiEvent.Payload);
                    }

                    track.Events.Add(midiEvent);
                    continue;
                }

                if (status == 0xF0 || status == 0xF7)
                {
                    midiEvent.Type = ImuseMidiEventType.SysEx;
                    midiEvent.Status = status;
                    uint payloadLength = ReadVlq(bytes, ref offset, endOffset);
                    if ((offset + payloadLength) > endOffset)
                    {
                        throw new InvalidOperationException("SysEx payload length is invalid.");
                    }

                    midiEvent.Payload = Slice(bytes, offset, (int)payloadLength);
                    offset += (int)payloadLength;
                    track.Events.Add(midiEvent);
                    continue;
                }

                byte statusClass = (byte)(status & 0xF0);
                if (statusClass >= 0x80 && statusClass <= 0xE0)
                {
                    midiEvent.Type = ImuseMidiEventType.Channel;
                    midiEvent.Status = status;
                    runningStatus = status;

                    if (consumedData1)
                    {
                        midiEvent.HasData1 = true;
                        midiEvent.Data1 = impliedData1;
                    }
                    else
                    {
                        if (offset >= endOffset)
                        {
                            throw new InvalidOperationException("Channel event is missing its first data byte.");
                        }

                        midiEvent.HasData1 = true;
                        midiEvent.Data1 = bytes[offset++];
                    }

                    if (statusClass != 0xC0 && statusClass != 0xD0)
                    {
                        if (offset >= endOffset)
                        {
                            throw new InvalidOperationException("Channel event is missing its second data byte.");
                        }

                        midiEvent.HasData2 = true;
                        midiEvent.Data2 = bytes[offset++];
                    }

                    track.Events.Add(midiEvent);
                    continue;
                }

                midiEvent.Type = ImuseMidiEventType.System;
                midiEvent.Status = status;
                runningStatus = 0;

                int payloadSize = 0;
                if (status == 0xF1 || status == 0xF3)
                {
                    payloadSize = 1;
                }
                else if (status == 0xF2)
                {
                    payloadSize = 2;
                }

                if ((offset + payloadSize) > endOffset)
                {
                    throw new InvalidOperationException("System event extends beyond the track data.");
                }

                if (payloadSize > 0)
                {
                    midiEvent.Payload = Slice(bytes, offset, payloadSize);
                    offset += payloadSize;
                }

                track.Events.Add(midiEvent);
            }

            return track;
        }

        private static ImuseTrackModel MergeTracksToFormat0(List<ImuseTrackModel> tracks)
        {
            List<MergedEvent> mergedEvents = new List<MergedEvent>();
            int order = 0;

            for (int trackIndex = 0; trackIndex < tracks.Count; ++trackIndex)
            {
                List<ImuseMidiEventModel> events = tracks[trackIndex].Events;
                uint absoluteTick = 0;
                for (int eventIndex = 0; eventIndex < events.Count; ++eventIndex)
                {
                    absoluteTick += events[eventIndex].Delta;
                    if (events[eventIndex].Type == ImuseMidiEventType.Meta && events[eventIndex].MetaType == 0x2F)
                    {
                        continue;
                    }
                    MergedEvent mergedEvent = new MergedEvent();
                    mergedEvent.AbsoluteTick = absoluteTick;
                    mergedEvent.Order = order++;
                    mergedEvent.Event = CloneEvent(events[eventIndex]);
                    mergedEvents.Add(mergedEvent);
                }
            }

            mergedEvents.Sort(delegate(MergedEvent left, MergedEvent right)
            {
                int tickCompare = left.AbsoluteTick.CompareTo(right.AbsoluteTick);
                if (tickCompare != 0)
                {
                    return tickCompare;
                }

                return left.Order.CompareTo(right.Order);
            });

            ImuseTrackModel mergedTrack = new ImuseTrackModel();
            mergedTrack.Name = "Merged Track";
            mergedTrack.SourceFileName = "Imported";

            uint lastTick = 0;
            for (int i = 0; i < mergedEvents.Count; ++i)
            {
                ImuseMidiEventModel midiEvent = mergedEvents[i].Event;
                midiEvent.Tick = mergedEvents[i].AbsoluteTick;
                midiEvent.Delta = mergedEvents[i].AbsoluteTick - lastTick;
                lastTick = mergedEvents[i].AbsoluteTick;
                mergedTrack.Events.Add(midiEvent);
            }

            // Append a single clean End of Track event
            ImuseMidiEventModel endOfTrack = new ImuseMidiEventModel();
            endOfTrack.Type = ImuseMidiEventType.Meta;
            endOfTrack.Status = 0xFF;
            endOfTrack.MetaType = 0x2F;
            endOfTrack.Tick = lastTick;
            endOfTrack.Delta = 0;
            mergedTrack.Events.Add(endOfTrack);

            return mergedTrack;
        }

        private static byte[] BuildIhdrChunk(uint soundCount)
        {
            List<byte> body = new List<byte>();
            WriteUInt16BigEndian(body, 1);
            WriteUInt16BigEndian(body, 0);
            WriteUInt32BigEndian(body, soundCount);
            WriteUInt32BigEndian(body, 0);
            WriteUInt32BigEndian(body, 0);
            return BuildChunk("IHDR", body.ToArray());
        }

        private static byte[] BuildSdirChunk(List<ushort> soundIds, List<ushort> variantMasks, List<byte[]> soundChunks)
        {
            int ihdrChunkSize = 24;
            int sdirChunkSize = 8 + (soundIds.Count * 12);
            uint soundOffset = (uint)(8 + ihdrChunkSize + sdirChunkSize);

            List<byte> body = new List<byte>();
            uint currentOffset = soundOffset;
            for (int i = 0; i < soundIds.Count; ++i)
            {
                WriteUInt16BigEndian(body, soundIds[i]);
                WriteUInt16BigEndian(body, variantMasks[i]);
                WriteUInt32BigEndian(body, currentOffset);
                WriteUInt32BigEndian(body, (uint)soundChunks[i].Length);
                currentOffset += (uint)soundChunks[i].Length;
            }

            return BuildChunk("SDIR", body.ToArray());
        }

        private static byte[] BuildSoundChunk(ImuseSoundModel sound, out ushort variantMask)
        {
            List<byte> body = new List<byte>();

            List<byte> sidnBody = new List<byte>();
            WriteUInt16BigEndian(sidnBody, sound.SoundId);
            WriteUInt16BigEndian(sidnBody, 0);
            body.AddRange(BuildChunk("SIDN", sidnBody.ToArray()));

            if (!string.IsNullOrEmpty(sound.Name))
            {
                body.AddRange(BuildChunk("NAME", Encoding.UTF8.GetBytes(sound.Name)));
            }

            variantMask = 0;
            for (int i = 0; i < sound.Variants.Count; ++i)
            {
                ImuseVariantModel variant = sound.Variants[i];
                if (!variant.IncludeVariant)
                {
                    continue;
                }

                byte[] variantChunk = BuildVariantChunk(variant);
                variantMask = (ushort)(variantMask | (ushort)variant.Kind);
                body.AddRange(variantChunk);
            }

            if (variantMask == 0)
            {
                throw new InvalidOperationException(
                    "Sound '" + sound.DisplayLabel + "' has no included variants. Import at least one variant before saving.");
            }

            return BuildChunk("SOUN", body.ToArray());
        }

        private static byte[] BuildVariantChunk(ImuseVariantModel variant)
        {
            List<byte> body = new List<byte>();
            if (variant.IncludeMdhd)
            {
                List<byte> mdhdBody = new List<byte>();
                mdhdBody.Add(0);
                mdhdBody.Add(0);
                mdhdBody.Add(variant.Priority);
                mdhdBody.Add(variant.Volume);
                mdhdBody.Add(unchecked((byte)variant.Pan));
                mdhdBody.Add(unchecked((byte)variant.Transpose));
                mdhdBody.Add(unchecked((byte)variant.Detune));
                mdhdBody.Add(variant.Speed);
                body.AddRange(BuildChunk("MDhd", mdhdBody.ToArray()));
            }

            body.AddRange(SerializeVariantSequence(variant));
            return BuildChunk(ChunkIdFromVariantKind(variant.Kind), body.ToArray());
        }

        private static byte[] SerializeVariantSequence(ImuseVariantModel variant)
        {
            List<byte> output = new List<byte>();

            output.Add((byte)'M');
            output.Add((byte)'T');
            output.Add((byte)'h');
            output.Add((byte)'d');
            WriteUInt32BigEndian(output, 6);
            WriteUInt16BigEndian(output, 2);
            WriteUInt16BigEndian(output, (ushort)variant.Tracks.Count);
            WriteUInt16BigEndian(output, variant.Division == 0 ? (ushort)480 : variant.Division);

            for (int i = 0; i < variant.Tracks.Count; ++i)
            {
                ImuseTrackModel track = variant.Tracks[i];
                List<byte> trackBytes = new List<byte>();
                for (int eventIndex = 0; eventIndex < track.Events.Count; ++eventIndex)
                {
                    ImuseMidiEventModel midiEvent = track.Events[eventIndex];
                    WriteVlq(trackBytes, midiEvent.Delta);
                    trackBytes.Add(midiEvent.Status);

                    if (midiEvent.Type == ImuseMidiEventType.Channel)
                    {
                        if (midiEvent.HasData1)
                        {
                            trackBytes.Add(midiEvent.Data1);
                        }

                        if (midiEvent.HasData2)
                        {
                            trackBytes.Add(midiEvent.Data2);
                        }
                    }
                    else if (midiEvent.Type == ImuseMidiEventType.Meta)
                    {
                        trackBytes.Add(midiEvent.MetaType);
                        WriteVlq(trackBytes, (uint)midiEvent.Payload.Length);
                        trackBytes.AddRange(midiEvent.Payload);
                    }
                    else if (midiEvent.Type == ImuseMidiEventType.SysEx)
                    {
                        WriteVlq(trackBytes, (uint)midiEvent.Payload.Length);
                        trackBytes.AddRange(midiEvent.Payload);
                    }
                    else
                    {
                        trackBytes.AddRange(midiEvent.Payload);
                    }
                }

                output.Add((byte)'M');
                output.Add((byte)'T');
                output.Add((byte)'r');
                output.Add((byte)'k');
                WriteUInt32BigEndian(output, (uint)trackBytes.Count);
                output.AddRange(trackBytes);
            }

            return output.ToArray();
        }

        private static ImuseVariantKind VariantKindFromChunkId(string chunkId)
        {
            if (chunkId == "GMD ")
            {
                return ImuseVariantKind.GeneralMidi;
            }

            if (chunkId == "ROL ")
            {
                return ImuseVariantKind.RolandMt32;
            }

            if (chunkId == "ADL ")
            {
                return ImuseVariantKind.Adlib;
            }

            throw new InvalidOperationException("Unknown variant chunk: " + chunkId + ".");
        }

        private static string ChunkIdFromVariantKind(ImuseVariantKind kind)
        {
            if (kind == ImuseVariantKind.GeneralMidi)
            {
                return "GMD ";
            }

            if (kind == ImuseVariantKind.RolandMt32)
            {
                return "ROL ";
            }

            if (kind == ImuseVariantKind.Adlib)
            {
                return "ADL ";
            }

            throw new InvalidOperationException("Unknown variant kind.");
        }

        private static ImuseTrackModel CloneTrack(ImuseTrackModel source)
        {
            ImuseTrackModel track = new ImuseTrackModel();
            track.Name = source.Name;
            track.SourceFileName = source.SourceFileName;
            for (int i = 0; i < source.Events.Count; ++i)
            {
                track.Events.Add(CloneEvent(source.Events[i]));
            }

            return track;
        }

        private static ImuseMidiEventModel CloneEvent(ImuseMidiEventModel source)
        {
            ImuseMidiEventModel midiEvent = new ImuseMidiEventModel();
            midiEvent.Type = source.Type;
            midiEvent.Tick = source.Tick;
            midiEvent.Delta = source.Delta;
            midiEvent.Status = source.Status;
            midiEvent.Data1 = source.Data1;
            midiEvent.Data2 = source.Data2;
            midiEvent.HasData1 = source.HasData1;
            midiEvent.HasData2 = source.HasData2;
            midiEvent.MetaType = source.MetaType;
            midiEvent.Payload = Slice(source.Payload, 0, source.Payload.Length);
            return midiEvent;
        }

        private static Chunk ReadChunk(byte[] bytes, int offset)
        {
            if (offset < 0 || (offset + 8) > bytes.Length)
            {
                throw new InvalidOperationException("Chunk header is truncated.");
            }

            Chunk chunk = new Chunk();
            chunk.Id = ReadAscii(bytes, offset, 4);
            uint chunkSize = ReadUInt32BigEndian(bytes, offset + 4);
            if (chunkSize < 8)
            {
                throw new InvalidOperationException("Chunk size is invalid.");
            }

            long endOffset = (long)offset + chunkSize;
            if (endOffset > bytes.Length)
            {
                throw new InvalidOperationException("Chunk extends beyond the end of the file.");
            }

            chunk.Offset = offset;
            chunk.BodyOffset = offset + 8;
            chunk.BodyLength = (int)chunkSize - 8;
            chunk.EndOffset = (int)endOffset;
            return chunk;
        }

        private static ushort ReadUInt16BigEndian(byte[] bytes, int offset)
        {
            return (ushort)((bytes[offset] << 8) | bytes[offset + 1]);
        }

        private static uint ReadUInt32BigEndian(byte[] bytes, int offset)
        {
            return ((uint)bytes[offset] << 24) |
                   ((uint)bytes[offset + 1] << 16) |
                   ((uint)bytes[offset + 2] << 8) |
                   bytes[offset + 3];
        }

        private static uint ReadVlq(byte[] bytes, ref int offset, int endOffset)
        {
            uint result = 0;
            int count = 0;
            while (offset < endOffset && count < 4)
            {
                byte value = bytes[offset++];
                result = (result << 7) | (uint)(value & 0x7F);
                count++;
                if ((value & 0x80) == 0)
                {
                    return result;
                }
            }

            throw new InvalidOperationException("Failed to read a MIDI variable-length value.");
        }

        private static string ReadAscii(byte[] bytes, int offset, int length)
        {
            return Encoding.ASCII.GetString(bytes, offset, length);
        }

        private static void WriteUInt16BigEndian(List<byte> output, ushort value)
        {
            output.Add((byte)((value >> 8) & 0xFF));
            output.Add((byte)(value & 0xFF));
        }

        private static void WriteUInt32BigEndian(List<byte> output, uint value)
        {
            output.Add((byte)((value >> 24) & 0xFF));
            output.Add((byte)((value >> 16) & 0xFF));
            output.Add((byte)((value >> 8) & 0xFF));
            output.Add((byte)(value & 0xFF));
        }

        private static void WriteVlq(List<byte> output, uint value)
        {
            uint buffer = value & 0x7F;
            while ((value >>= 7) > 0)
            {
                buffer <<= 8;
                buffer |= 0x80;
                buffer += (value & 0x7F);
            }

            while (true)
            {
                output.Add((byte)(buffer & 0xFF));
                if ((buffer & 0x80) != 0)
                {
                    buffer >>= 8;
                }
                else
                {
                    break;
                }
            }
        }

        private static byte[] Slice(byte[] bytes, int offset, int length)
        {
            byte[] copy = new byte[length];
            Buffer.BlockCopy(bytes, offset, copy, 0, length);
            return copy;
        }

        private static byte[] BuildChunk(string id, byte[] body)
        {
            List<byte> output = new List<byte>(8 + body.Length);
            output.Add((byte)id[0]);
            output.Add((byte)id[1]);
            output.Add((byte)id[2]);
            output.Add((byte)id[3]);
            WriteUInt32BigEndian(output, (uint)(body.Length + 8));
            output.AddRange(body);
            return output.ToArray();
        }

        private sealed class Chunk
        {
            public string Id;
            public int Offset;
            public int BodyOffset;
            public int BodyLength;
            public int EndOffset;
        }
    }
}
