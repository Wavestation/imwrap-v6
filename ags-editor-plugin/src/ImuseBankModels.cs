using System.Collections.Generic;

namespace AgsImuse.Editor
{
    internal enum ImuseVariantKind
    {
        GeneralMidi = 1,
        RolandMt32 = 2,
        Adlib = 4
    }

    internal enum ImuseMidiEventType
    {
        Channel,
        Meta,
        SysEx,
        System
    }

    internal sealed class ImuseMidiEventModel
    {
        public ImuseMidiEventType Type { get; set; }
        public uint Tick { get; set; }
        public uint Delta { get; set; }
        public byte Status { get; set; }
        public byte Data1 { get; set; }
        public byte Data2 { get; set; }
        public bool HasData1 { get; set; }
        public bool HasData2 { get; set; }
        public byte MetaType { get; set; }
        public byte[] Payload { get; set; }

        public ImuseMidiEventModel()
        {
            Payload = new byte[0];
        }
    }

    internal sealed class ImuseTrackModel
    {
        public ImuseTrackModel()
        {
            Name = string.Empty;
            SourceFileName = string.Empty;
            Events = new List<ImuseMidiEventModel>();
        }

        public string Name { get; set; }
        public string SourceFileName { get; set; }
        public List<ImuseMidiEventModel> Events { get; private set; }

        public int EventCount
        {
            get { return Events.Count; }
        }
    }

    internal sealed class ImuseVariantModel
    {
        public ImuseVariantModel()
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
            Tracks = new List<ImuseTrackModel>();
        }

        public ImuseVariantKind Kind { get; set; }
        public bool IncludeVariant { get; set; }
        public bool IncludeMdhd { get; set; }
        public byte Priority { get; set; }
        public byte Volume { get; set; }
        public sbyte Pan { get; set; }
        public sbyte Transpose { get; set; }
        public sbyte Detune { get; set; }
        public byte Speed { get; set; }
        public ushort Division { get; set; }
        public List<ImuseTrackModel> Tracks { get; private set; }
    }

    internal sealed class ImuseSoundModel
    {
        public ImuseSoundModel()
        {
            Name = string.Empty;
            Variants = new List<ImuseVariantModel>();
        }

        public ushort SoundId { get; set; }
        public string Name { get; set; }
        public List<ImuseVariantModel> Variants { get; private set; }

        public string DisplayLabel
        {
            get
            {
                List<string> present = new List<string>();
                ImuseVariantModel gmd = FindVariant(ImuseVariantKind.GeneralMidi);
                if (gmd != null && gmd.IncludeVariant)
                {
                    present.Add("GMD");
                }
                ImuseVariantModel rol = FindVariant(ImuseVariantKind.RolandMt32);
                if (rol != null && rol.IncludeVariant)
                {
                    present.Add("ROL");
                }
                ImuseVariantModel adl = FindVariant(ImuseVariantKind.Adlib);
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

        public ImuseVariantModel FindVariant(ImuseVariantKind kind)
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

        public ImuseVariantModel EnsureVariant(ImuseVariantKind kind)
        {
            ImuseVariantModel variant = FindVariant(kind);
            if (variant != null)
            {
                return variant;
            }

            variant = new ImuseVariantModel();
            variant.Kind = kind;
            Variants.Add(variant);
            return variant;
        }

        public override string ToString()
        {
            return DisplayLabel;
        }
    }

    internal sealed class ImuseBankProjectModel
    {
        public ImuseBankProjectModel()
        {
            RelativePath = string.Empty;
            Sounds = new List<ImuseSoundModel>();
        }

        public string RelativePath { get; set; }
        public List<ImuseSoundModel> Sounds { get; private set; }
    }
}
