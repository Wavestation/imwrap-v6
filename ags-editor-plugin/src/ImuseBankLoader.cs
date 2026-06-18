using System;
using System.Text;

namespace AgsImuse.Editor
{
    internal static class ImuseBankLoader
    {
        private const int TextBufferSize = 1024;

        public static ImuseBankSnapshot Load(string fullPath, string relativePath, ImuseAuthoringProfile profile)
        {
            IntPtr bankHandle = ImuseShim.BankCreate();
            if (bankHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Unable to create an iMUSE bank handle.");
            }

            try
            {
                StringBuilder errorBuffer = new StringBuilder(TextBufferSize);
                if (ImuseShim.BankLoad(bankHandle, fullPath, errorBuffer, new UIntPtr((uint)errorBuffer.Capacity)) == 0)
                {
                    throw new InvalidOperationException(ReadString(errorBuffer, "Unable to load the selected .ims bank."));
                }

                ImuseBankSnapshot snapshot = new ImuseBankSnapshot(relativePath, profile);
                ulong soundCount = ImuseShim.BankSoundCount(bankHandle).ToUInt64();
                for (ulong soundIndex = 0; soundIndex < soundCount; ++soundIndex)
                {
                    ushort soundId = ImuseShim.BankSoundIdAt(bankHandle, new UIntPtr(soundIndex));
                    string soundName = ReadSoundName(bankHandle, soundId);
                    ImuseSoundSnapshot sound = new ImuseSoundSnapshot(soundId, soundName);

                    ulong trackCount = ImuseShim.BankTrackCount(bankHandle, soundId, profile).ToUInt64();
                    for (ulong trackIndex = 0; trackIndex < trackCount; ++trackIndex)
                    {
                        StringBuilder trackNameBuffer = new StringBuilder(TextBufferSize);
                        UIntPtr trackEventCount;
                        int ok = ImuseShim.BankTrackSummary(
                            bankHandle,
                            soundId,
                            profile,
                            new UIntPtr(trackIndex),
                            trackNameBuffer,
                            new UIntPtr((uint)trackNameBuffer.Capacity),
                            out trackEventCount);

                        if (ok != 0)
                        {
                            sound.Tracks.Add(new ImuseTrackSnapshot(
                                (int)trackIndex,
                                ReadString(trackNameBuffer, string.Format("Track {0}", trackIndex)),
                                trackEventCount.ToUInt64()));
                        }
                    }

                    ulong eventCount = ImuseShim.BankEventCount(bankHandle, soundId, profile).ToUInt64();
                    for (ulong eventIndex = 0; eventIndex < eventCount; ++eventIndex)
                    {
                        StringBuilder eventSummaryBuffer = new StringBuilder(TextBufferSize);
                        uint tick;
                        ushort track;
                        int ok = ImuseShim.BankEventSummary(
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
                            sound.Events.Add(new ImuseEventSnapshot(
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
                ImuseShim.BankDestroy(bankHandle);
            }
        }

        private static string ReadSoundName(IntPtr bankHandle, ushort soundId)
        {
            StringBuilder buffer = new StringBuilder(TextBufferSize);
            int ok = ImuseShim.BankSoundName(bankHandle, soundId, buffer, new UIntPtr((uint)buffer.Capacity));
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
