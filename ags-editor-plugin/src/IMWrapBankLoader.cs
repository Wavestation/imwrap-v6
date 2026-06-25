using System;
using System.Text;

namespace AgsIMWrap.Editor
{
    internal static class IMWrapBankLoader
    {
        private const int TextBufferSize = 1024;

        public static IMWrapBankSnapshot Load(string fullPath, string relativePath, IMWrapAuthoringProfile profile)
        {
            IntPtr bankHandle = IMWrapShim.BankCreate();
            if (bankHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Unable to create an iMWrap bank handle.");
            }

            try
            {
                StringBuilder errorBuffer = new StringBuilder(TextBufferSize);
                if (IMWrapShim.BankLoad(bankHandle, fullPath, errorBuffer, new UIntPtr((uint)errorBuffer.Capacity)) == 0)
                {
                    throw new InvalidOperationException(ReadString(errorBuffer, "Unable to load the selected .ims bank."));
                }

                IMWrapBankSnapshot snapshot = new IMWrapBankSnapshot(relativePath, profile);
                ulong soundCount = IMWrapShim.BankSoundCount(bankHandle).ToUInt64();
                for (ulong soundIndex = 0; soundIndex < soundCount; ++soundIndex)
                {
                    ushort soundId = IMWrapShim.BankSoundIdAt(bankHandle, new UIntPtr(soundIndex));
                    string soundName = ReadSoundName(bankHandle, soundId);
                    IMWrapSoundSnapshot sound = new IMWrapSoundSnapshot(soundId, soundName);

                    ulong trackCount = IMWrapShim.BankTrackCount(bankHandle, soundId, profile).ToUInt64();
                    for (ulong trackIndex = 0; trackIndex < trackCount; ++trackIndex)
                    {
                        StringBuilder trackNameBuffer = new StringBuilder(TextBufferSize);
                        UIntPtr trackEventCount;
                        int ok = IMWrapShim.BankTrackSummary(
                            bankHandle,
                            soundId,
                            profile,
                            new UIntPtr(trackIndex),
                            trackNameBuffer,
                            new UIntPtr((uint)trackNameBuffer.Capacity),
                            out trackEventCount);

                        if (ok != 0)
                        {
                            sound.Tracks.Add(new IMWrapTrackSnapshot(
                                (int)trackIndex,
                                ReadString(trackNameBuffer, string.Format("Track {0}", trackIndex)),
                                trackEventCount.ToUInt64()));
                        }
                    }

                    ulong eventCount = IMWrapShim.BankEventCount(bankHandle, soundId, profile).ToUInt64();
                    for (ulong eventIndex = 0; eventIndex < eventCount; ++eventIndex)
                    {
                        StringBuilder eventSummaryBuffer = new StringBuilder(TextBufferSize);
                        uint tick;
                        ushort track;
                        int ok = IMWrapShim.BankEventSummary(
                            bankHandle,
                            soundId,
                            profile,
                            new UIntPtr(eventIndex),
                            out tick,
                            out track,
                            eventSummaryBuffer,
                            new UIntPtr((uint)eventSummaryBuffer.Capacity));

                        if (ok != 0)
                        {
                            sound.Events.Add(new IMWrapEventSnapshot(
                                tick,
                                track,
                                ReadString(eventSummaryBuffer, "(event)")));
                        }
                    }

                    snapshot.Sounds.Add(sound);
                }

                return snapshot;
            }
            finally
            {
                IMWrapShim.BankDestroy(bankHandle);
            }
        }

        private static string ReadSoundName(IntPtr bankHandle, ushort soundId)
        {
            StringBuilder buffer = new StringBuilder(TextBufferSize);
            int ok = IMWrapShim.BankSoundName(bankHandle, soundId, buffer, new UIntPtr((uint)buffer.Capacity));
            if (ok == 0)
            {
                return string.Format("Sound {0}", soundId);
            }

            return ReadString(buffer, string.Format("Sound {0}", soundId));
        }

        private static string ReadString(StringBuilder buffer, string fallback)
        {
            string value = buffer.ToString().Trim();
            return string.IsNullOrEmpty(value) ? fallback : value;
        }
    }
}
