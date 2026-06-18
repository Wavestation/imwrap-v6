using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace AgsImuse.Editor
{
    internal static class EmbeddedIconLoader
    {
        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern bool DestroyIcon(IntPtr handle);

        public static Icon LoadPngIcon(string resourceName, Icon fallback)
        {
            Assembly assembly = typeof(EmbeddedIconLoader).Assembly;
            Stream stream = assembly.GetManifestResourceStream(resourceName);
            if (stream == null)
            {
                return fallback;
            }

            using (stream)
            using (Bitmap bitmap = new Bitmap(stream))
            {
                IntPtr handle = bitmap.GetHicon();
                try
                {
                    using (Icon temporaryIcon = Icon.FromHandle(handle))
                    {
                        return (Icon)temporaryIcon.Clone();
                    }
                }
                finally
                {
                    DestroyIcon(handle);
                }
            }
        }
    }
}
