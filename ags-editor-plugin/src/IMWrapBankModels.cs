using System.Collections.Generic;

namespace AgsIMWrap.Editor
{
    internal enum IMWrapVariantKind
    {
        GeneralMidi = 1,
        RolandMt32 = 2,
        Adlib = 4
    }

    internal enum IMWrapMidiEventType
    {
        Channel,
        Meta,
        SysEx,
        System
    }

    internal sealed class IMWrapMidiEventModel
    {
        public IMWrapMidiEventType Type { get; set; }
        public uint Tick { get; set; }
        public uint Delta { get; set; }
        public byte Status { get; set; }
        public byte Data1 { get; set; }
        public byte Data2 { get; set; }
        public bool HasData1 { get; set; }
        public bool HasData2 { get; set; }
        public byte MetaType { get; set; }
        public byte[] Payload { get; set; }

        public IMWrapMidiEventModel()
        {
            Payload = new byte[0];
        }
    }

    internal sealed class IMWrapTrackModel
    {
        public IMWrapTrackModel()
        {
            Name = string.Empty;
            SourceFileName = string.Empty;
            Events = new List<IMWrapMidiEventModel>();
        }

        public string Name { get; set; }
        public string SourceFileName { get; set; }
        public List<IMWrapMidiEventModel> Events { get; private set; }

        public int EventCount
        {
            get { return Events.Count; }
        }
    }

    internal sealed class IMWrapVariantModel
    {
        public IMWrapVariantModel()
        {
            IncludeVariant = false;
            IncludeMdhd = false;
            Priority = 128;
            Volume = 127;
            Pan = 0;
            Transpose = 0;
            Detune = 0;
            Speed = 128;
            Division = 480;
            Tracks = new List<IMWrapTrackModel>();
        }

        public IMWrapVariantKind Kind { get; set; }
        public bool IncludeVariant { get; set; }
        public bool IncludeMdhd { get; set; }
        public byte Priority { get; set; }
        public byte Volume { get; set; }
        public sbyte Pan { get; set; }
        public sbyte Transpose { get; set; }
        public sbyte Detune { get; set; }
        public byte Speed { get; set; }
        public ushort Division { get; set; }
        public List<IMWrapTrackModel> Tracks { get; private set; }
    }

    internal sealed class IMWrapSoundModel
    {
        public IMWrapSoundModel()
        {
            Name = string.Empty;
            Variants = new List<IMWrapVariantModel>();
        }

        public ushort SoundId { get; set; }
        public string Name { get; set; }
        public List<IMWrapVariantModel> Variants { get; private set; }

        public string DisplayLabel
        {
            get
            {
                List<string> present = new List<string>();
                IMWrapVariantModel gmd = FindVariant(IMWrapVariantKind.GeneralMidi);
                if (gmd != null && gmd.IncludeVariant)
                {
                    present.Add("GMD");
                }
                IMWrapVariantModel rol = FindVariant(IMWrapVariantKind.RolandMt32);
                if (rol != null && rol.IncludeVariant)
                {
                    present.Add("ROL");
                }
                IMWrapVariantModel adl = FindVariant(IMWrapVariantKind.Adlib);
                if (adl != null && adl.IncludeVariant)
                {
                    present.Add("ADL");
                }

                if (present.Count > 0)
                {
                    return string.Format("{0}: {1} [{2}]", SoundId, Name, string.Join(", ", present));
                }
                return string.Format("{0}: {1}", SoundId, Name);
            }
        }

        public IMWrapVariantModel FindVariant(IMWrapVariantKind kind)
        {
            for (int i = 0; i < Variants.Count; ++i)
            {
                if (Variants[i].Kind == kind)
                {
                    return Variants[i];
                }
            }

            return null;
        }

        public IMWrapVariantModel EnsureVariant(IMWrapVariantKind kind)
        {
            IMWrapVariantModel variant = FindVariant(kind);
            if (variant != null)
            {
                return variant;
            }

            variant = new IMWrapVariantModel();
            variant.Kind = kind;
            Variants.Add(variant);
            return variant;
        }

        public override string ToString()
        {
            return DisplayLabel;
        }
    }

    internal sealed class IMWrapBankProjectModel
    {
        public IMWrapBankProjectModel()
        {
            RelativePath = string.Empty;
            Sounds = new List<IMWrapSoundModel>();
        }

        public string RelativePath { get; set; }
        public List<IMWrapSoundModel> Sounds { get; private set; }
    }
}
