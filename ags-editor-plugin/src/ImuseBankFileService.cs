using System;
using System.IO;

namespace AgsImuse.Editor
{
    internal static class ImuseBankFileService
    {
        public static byte[] BuildEmptyBankBytes()
        {
            byte[] bytes = new byte[40];
            bytes[0] = (byte)'I';
            bytes[1] = (byte)'M';
            bytes[2] = (byte)'S';
            bytes[3] = (byte)'B';
            WriteUInt32BigEndian(bytes, 4, 40);

            bytes[8] = (byte)'I';
            bytes[9] = (byte)'H';
            bytes[10] = (byte)'D';
            bytes[11] = (byte)'R';
            WriteUInt32BigEndian(bytes, 12, 24);
            WriteUInt16BigEndian(bytes, 16, 1);
            WriteUInt16BigEndian(bytes, 18, 0);
            WriteUInt32BigEndian(bytes, 20, 0);
            WriteUInt32BigEndian(bytes, 24, 0);
            WriteUInt32BigEndian(bytes, 28, 0);

            bytes[32] = (byte)'S';
            bytes[33] = (byte)'D';
            bytes[34] = (byte)'I';
            bytes[35] = (byte)'R';
            WriteUInt32BigEndian(bytes, 36, 8);

            return bytes;
        }

        public static void CreateEmptyBank(string fullPath)
        {
            EnsureParentDirectory(fullPath);
            File.WriteAllBytes(fullPath, BuildEmptyBankBytes());
        }

        public static void SaveBankBytes(string fullPath, byte[] bytes)
        {
            if (bytes == null)
            {
                throw new ArgumentNullException("bytes");
            }

            EnsureParentDirectory(fullPath);
            File.WriteAllBytes(fullPath, bytes);
        }

        private static void EnsureParentDirectory(string fullPath)
        {
            string directory = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }
        }

        private static void WriteUInt16BigEndian(byte[] bytes, int offset, ushort value)
        {
            bytes[offset] = (byte)(value >> 8);
            bytes[offset + 1] = (byte)(value & 0xFF);
        }

        private static void WriteUInt32BigEndian(byte[] bytes, int offset, uint value)
        {
            bytes[offset] = (byte)((value >> 24) & 0xFF);
            bytes[offset + 1] = (byte)((value >> 16) & 0xFF);
            bytes[offset + 2] = (byte)((value >> 8) & 0xFF);
            bytes[offset + 3] = (byte)(value & 0xFF);
        }
    }
}
