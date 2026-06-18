using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using AGS.Types;

namespace AgsImuse.Editor
{
    internal sealed class ImuseEditorPane : EditorContentPanel
    {
        private const int DefaultMainSplitterDistance = 240;

        private readonly Label _bankPathValue;
        private readonly Label _summaryLabel;
        private readonly Label _statusLabel;
        private readonly ListBox _soundList;
        private readonly SplitContainer _mainSplit;
        private readonly TextBox _soundNameTextBox;
        private readonly NumericUpDown _soundIdSpin;
        private readonly ComboBox _variantKindCombo;
        private readonly CheckBox _includeVariantCheck;
        private readonly CheckBox _includeMdhdCheck;
        private readonly NumericUpDown _prioritySpin;
        private readonly NumericUpDown _volumeSpin;
        private readonly NumericUpDown _panSpin;
        private readonly NumericUpDown _transposeSpin;
        private readonly NumericUpDown _detuneSpin;
        private readonly NumericUpDown _speedSpin;
        private readonly ListView _tracksView;
        private readonly Button _applyButton;
        private readonly Button _importButton;
        private readonly Button _refreshButton;
        private readonly Button _saveButton;

        private ImuseBankProjectModel _bank;
        private bool _isDirty;
        private int _preferredMainSplitterDistance;
        private bool _suppressUiEvents;

        public ImuseEditorPane()
        {
            Dock = DockStyle.Fill;
            _preferredMainSplitterDistance = DefaultMainSplitterDistance;

            TableLayoutPanel root = new TableLayoutPanel();
            root.ColumnCount = 1;
            root.RowCount = 3;
            root.Dock = DockStyle.Fill;
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            Controls.Add(root);

            TableLayoutPanel header = new TableLayoutPanel();
            header.ColumnCount = 4;
            header.Dock = DockStyle.Top;
            header.AutoSize = true;
            header.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            header.Padding = new Padding(8);
            header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            root.Controls.Add(header, 0, 0);

            Label bankLabel = new Label();
            bankLabel.AutoSize = true;
            bankLabel.Margin = new Padding(0, 6, 8, 0);
            bankLabel.Text = "Bank:";
            header.Controls.Add(bankLabel, 0, 0);

            _bankPathValue = new Label();
            _bankPathValue.AutoEllipsis = true;
            _bankPathValue.Dock = DockStyle.Fill;
            _bankPathValue.Margin = new Padding(0, 6, 12, 0);
            _bankPathValue.Text = "(none)";
            header.Controls.Add(_bankPathValue, 1, 0);

            _saveButton = new Button();
            _saveButton.AutoSize = true;
            _saveButton.Text = "Save";
            _saveButton.Click += SaveButton_Click;
            header.Controls.Add(_saveButton, 2, 0);

            _refreshButton = new Button();
            _refreshButton.AutoSize = true;
            _refreshButton.Text = "Refresh";
            _refreshButton.Click += RefreshButton_Click;
            header.Controls.Add(_refreshButton, 3, 0);

            _mainSplit = new SplitContainer();
            _mainSplit.Dock = DockStyle.Fill;
            _mainSplit.Orientation = Orientation.Vertical;
            _mainSplit.SplitterDistance = DefaultMainSplitterDistance;
            _mainSplit.SplitterMoved += MainSplit_SplitterMoved;
            _mainSplit.SizeChanged += MainSplit_SizeChanged;
            root.Controls.Add(_mainSplit, 0, 1);

            GroupBox soundsGroup = new GroupBox();
            soundsGroup.Text = "Sounds";
            soundsGroup.Dock = DockStyle.Fill;
            _mainSplit.Panel1.Controls.Add(soundsGroup);

            TableLayoutPanel soundsLayout = new TableLayoutPanel();
            soundsLayout.ColumnCount = 1;
            soundsLayout.RowCount = 2;
            soundsLayout.Dock = DockStyle.Fill;
            soundsLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
            soundsLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            soundsGroup.Controls.Add(soundsLayout);

            _soundList = new ListBox();
            _soundList.Dock = DockStyle.Fill;
            _soundList.IntegralHeight = false;
            _soundList.SelectedIndexChanged += SoundList_SelectedIndexChanged;
            soundsLayout.Controls.Add(_soundList, 0, 0);

            FlowLayoutPanel soundButtons = new FlowLayoutPanel();
            soundButtons.Dock = DockStyle.Fill;
            soundButtons.FlowDirection = FlowDirection.LeftToRight;
            soundButtons.WrapContents = false;
            soundButtons.Padding = new Padding(8, 4, 8, 8);
            soundsLayout.Controls.Add(soundButtons, 0, 1);

            Button addSoundButton = new Button();
            addSoundButton.AutoSize = true;
            addSoundButton.Text = "+";
            addSoundButton.Click += AddSoundButton_Click;
            soundButtons.Controls.Add(addSoundButton);

            Button deleteSoundButton = new Button();
            deleteSoundButton.AutoSize = true;
            deleteSoundButton.Text = "-";
            deleteSoundButton.Click += DeleteSoundButton_Click;
            soundButtons.Controls.Add(deleteSoundButton);

            Panel rightPanel = new Panel();
            rightPanel.Dock = DockStyle.Fill;
            _mainSplit.Panel2.Controls.Add(rightPanel);

            TableLayoutPanel rightLayout = new TableLayoutPanel();
            rightLayout.ColumnCount = 1;
            rightLayout.RowCount = 4;
            rightLayout.Dock = DockStyle.Fill;
            rightLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            rightLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            rightLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
            rightLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            rightPanel.Controls.Add(rightLayout);

            TableLayoutPanel soundProperties = new TableLayoutPanel();
            soundProperties.ColumnCount = 6;
            soundProperties.Dock = DockStyle.Top;
            soundProperties.AutoSize = true;
            soundProperties.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            soundProperties.Padding = new Padding(8, 8, 8, 0);
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            soundProperties.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            rightLayout.Controls.Add(soundProperties, 0, 0);

            Label soundNameLabel = new Label();
            soundNameLabel.AutoSize = true;
            soundNameLabel.Margin = new Padding(0, 6, 8, 0);
            soundNameLabel.Text = "Name:";
            soundProperties.Controls.Add(soundNameLabel, 0, 0);

            _soundNameTextBox = new TextBox();
            _soundNameTextBox.Dock = DockStyle.Fill;
            soundProperties.Controls.Add(_soundNameTextBox, 1, 0);

            Label soundIdLabel = new Label();
            soundIdLabel.AutoSize = true;
            soundIdLabel.Margin = new Padding(12, 6, 8, 0);
            soundIdLabel.Text = "ID:";
            soundProperties.Controls.Add(soundIdLabel, 2, 0);

            _soundIdSpin = new NumericUpDown();
            _soundIdSpin.Maximum = 65535;
            _soundIdSpin.Width = 90;
            soundProperties.Controls.Add(_soundIdSpin, 3, 0);

            _applyButton = new Button();
            _applyButton.AutoSize = true;
            _applyButton.Text = "Apply Sound Changes";
            _applyButton.Click += ApplyButton_Click;
            soundProperties.Controls.Add(_applyButton, 4, 0);

            TableLayoutPanel variantHeader = new TableLayoutPanel();
            variantHeader.ColumnCount = 2;
            variantHeader.Dock = DockStyle.Top;
            variantHeader.AutoSize = true;
            variantHeader.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            variantHeader.Padding = new Padding(8);
            variantHeader.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            variantHeader.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            rightLayout.Controls.Add(variantHeader, 0, 1);

            Label variantLabel = new Label();
            variantLabel.AutoSize = true;
            variantLabel.Margin = new Padding(0, 6, 8, 0);
            variantLabel.Text = "Variant:";
            variantHeader.Controls.Add(variantLabel, 0, 0);

            _variantKindCombo = new ComboBox();
            _variantKindCombo.DropDownStyle = ComboBoxStyle.DropDownList;
            _variantKindCombo.Width = 220;
            _variantKindCombo.Items.Add("General MIDI (GMD)");
            _variantKindCombo.Items.Add("Roland MT-32 (ROL)");
            _variantKindCombo.Items.Add("AdLib (ADL)");
            _variantKindCombo.SelectedIndexChanged += VariantKindCombo_SelectedIndexChanged;
            variantHeader.Controls.Add(_variantKindCombo, 1, 0);

            GroupBox variantGroup = new GroupBox();
            variantGroup.Text = "Variant Editor";
            variantGroup.Dock = DockStyle.Fill;
            rightLayout.Controls.Add(variantGroup, 0, 2);

            TableLayoutPanel variantLayout = new TableLayoutPanel();
            variantLayout.ColumnCount = 1;
            variantLayout.RowCount = 4;
            variantLayout.Dock = DockStyle.Fill;
            variantLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            variantLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            variantLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
            variantLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            variantGroup.Controls.Add(variantLayout);

            FlowLayoutPanel variantFlags = new FlowLayoutPanel();
            variantFlags.Dock = DockStyle.Fill;
            variantFlags.FlowDirection = FlowDirection.LeftToRight;
            variantFlags.WrapContents = false;
            variantFlags.Padding = new Padding(8, 8, 8, 0);
            variantLayout.Controls.Add(variantFlags, 0, 0);

            _includeVariantCheck = new CheckBox();
            _includeVariantCheck.AutoSize = true;
            _includeVariantCheck.Text = "Include this variant";
            variantFlags.Controls.Add(_includeVariantCheck);

            _includeMdhdCheck = new CheckBox();
            _includeMdhdCheck.AutoSize = true;
            _includeMdhdCheck.Text = "Include MDhd";
            variantFlags.Controls.Add(_includeMdhdCheck);

            TableLayoutPanel mdhdLayout = new TableLayoutPanel();
            mdhdLayout.ColumnCount = 4;
            mdhdLayout.Dock = DockStyle.Top;
            mdhdLayout.AutoSize = true;
            mdhdLayout.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            mdhdLayout.Padding = new Padding(8);
            mdhdLayout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            mdhdLayout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            mdhdLayout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            mdhdLayout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            variantLayout.Controls.Add(mdhdLayout, 0, 1);

            _prioritySpin = CreateNumeric(mdhdLayout, "Priority:", 0, 255, 128, 0, 0);
            _volumeSpin = CreateNumeric(mdhdLayout, "Volume:", 0, 127, 127, 2, 0);
            _panSpin = CreateNumeric(mdhdLayout, "Pan:", -128, 127, 0, 0, 1);
            _transposeSpin = CreateNumeric(mdhdLayout, "Transpose:", -128, 127, 0, 2, 1);
            _detuneSpin = CreateNumeric(mdhdLayout, "Detune:", -128, 127, 0, 0, 2);
            _speedSpin = CreateNumeric(mdhdLayout, "Speed:", 0, 255, 128, 2, 2);

            GroupBox tracksGroup = new GroupBox();
            tracksGroup.Text = "Tracks";
            tracksGroup.Dock = DockStyle.Fill;
            variantLayout.Controls.Add(tracksGroup, 0, 2);

            _tracksView = new ListView();
            _tracksView.Dock = DockStyle.Fill;
            _tracksView.FullRowSelect = true;
            _tracksView.GridLines = true;
            _tracksView.HideSelection = false;
            _tracksView.MultiSelect = false;
            _tracksView.View = System.Windows.Forms.View.Details;
            _tracksView.Columns.Add("Name", 200);
            _tracksView.Columns.Add("Source", 170);
            _tracksView.Columns.Add("Events", 70);
            tracksGroup.Controls.Add(_tracksView);

            FlowLayoutPanel importPanel = new FlowLayoutPanel();
            importPanel.Dock = DockStyle.Fill;
            importPanel.FlowDirection = FlowDirection.LeftToRight;
            importPanel.WrapContents = false;
            importPanel.Padding = new Padding(8, 0, 8, 8);
            variantLayout.Controls.Add(importPanel, 0, 3);

            _importButton = new Button();
            _importButton.AutoSize = true;
            _importButton.Text = "Import MIDI...";
            _importButton.Click += ImportButton_Click;
            importPanel.Controls.Add(_importButton);

            FlowLayoutPanel footer = new FlowLayoutPanel();
            footer.Dock = DockStyle.Fill;
            footer.AutoSize = true;
            footer.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            footer.FlowDirection = FlowDirection.TopDown;
            footer.Padding = new Padding(8, 0, 8, 8);
            footer.WrapContents = false;
            root.Controls.Add(footer, 0, 2);

            _summaryLabel = new Label();
            _summaryLabel.AutoSize = true;
            _summaryLabel.Text = "No bank loaded.";
            footer.Controls.Add(_summaryLabel);

            _statusLabel = new Label();
            _statusLabel.AutoSize = true;
            _statusLabel.MaximumSize = new Size(900, 0);
            _statusLabel.Text = "Open or create a bank to start editing.";
            footer.Controls.Add(_statusLabel);

            _variantKindCombo.SelectedIndex = 0;
            UpdateEnabledState();
        }

        public event EventHandler RefreshRequested;
        public event EventHandler SaveRequested;
        public event EventHandler DirtyStateChanged;
        public event EventHandler LayoutStateChanged;

        public bool IsDirty
        {
            get { return _isDirty; }
        }

        public ImuseBankProjectModel Bank
        {
            get { return _bank; }
        }

        public int MainSplitterDistance
        {
            get { return _mainSplit != null ? _mainSplit.SplitterDistance : _preferredMainSplitterDistance; }
            set
            {
                _preferredMainSplitterDistance = value > 0 ? value : DefaultMainSplitterDistance;
                ApplyMainSplitterDistance();
            }
        }

        public void LoadBank(ImuseBankProjectModel bank)
        {
            _bank = bank;
            _bankPathValue.Text = bank != null ? bank.RelativePath : "(none)";
            _suppressUiEvents = true;
            try
            {
                PopulateSoundList(null);
                if (_soundList.Items.Count > 0)
                {
                    _soundList.SelectedIndex = 0;
                }
                else
                {
                    ClearEditorFields();
                }
            }
            finally
            {
                _suppressUiEvents = false;
            }

            UpdateSoundDetailsFromSelection();
            _isDirty = false;
            UpdateSummary();
            UpdateVariantUi();
            RaiseDirtyStateChanged();
        }

        public void SetRelativePath(string relativePath)
        {
            if (_bank != null)
            {
                _bank.RelativePath = relativePath ?? string.Empty;
            }

            _bankPathValue.Text = string.IsNullOrEmpty(relativePath) ? "(none)" : relativePath;
        }

        public void MarkClean(string message)
        {
            _isDirty = false;
            if (!string.IsNullOrEmpty(message))
            {
                _statusLabel.Text = message;
            }

            UpdateSummary();
            RaiseDirtyStateChanged();
        }

        public byte[] BuildBankBytes()
        {
            ApplyCurrentSoundChanges();
            return ImuseBankCodec.SaveToBytes(_bank);
        }

        public void ShowStatus(string message)
        {
            if (!string.IsNullOrEmpty(message))
            {
                _statusLabel.Text = message;
            }
        }

        private static NumericUpDown CreateNumeric(TableLayoutPanel parent, string labelText, int minimum, int maximum, int value, int column, int row)
        {
            while (parent.RowCount <= row)
            {
                parent.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                parent.RowCount++;
            }

            Label label = new Label();
            label.AutoSize = true;
            label.Margin = new Padding(0, 6, 8, 0);
            label.Text = labelText;
            parent.Controls.Add(label, column, row);

            NumericUpDown numeric = new NumericUpDown();
            numeric.Minimum = minimum;
            numeric.Maximum = maximum;
            numeric.Value = value;
            numeric.Width = 90;
            parent.Controls.Add(numeric, column + 1, row);
            return numeric;
        }

        private void MainSplit_SplitterMoved(object sender, SplitterEventArgs e)
        {
            _preferredMainSplitterDistance = _mainSplit.SplitterDistance;
            RaiseLayoutStateChanged();
        }

        private void MainSplit_SizeChanged(object sender, EventArgs e)
        {
            ApplyMainSplitterDistance();
        }

        private void RefreshButton_Click(object sender, EventArgs e)
        {
            if (RefreshRequested != null)
            {
                RefreshRequested(this, EventArgs.Empty);
            }
        }

        private void SaveButton_Click(object sender, EventArgs e)
        {
            if (SaveRequested != null)
            {
                SaveRequested(this, EventArgs.Empty);
            }
        }

        private void AddSoundButton_Click(object sender, EventArgs e)
        {
            if (_bank == null)
            {
                return;
            }

            ushort newId = 0;
            for (int i = 0; i < _bank.Sounds.Count; ++i)
            {
                if (_bank.Sounds[i].SoundId >= newId)
                {
                    newId = (ushort)(_bank.Sounds[i].SoundId + 1);
                }
            }

            ImuseSoundModel sound = new ImuseSoundModel();
            sound.SoundId = newId;
            sound.Name = "NewSound";
            _bank.Sounds.Add(sound);
            PopulateSoundList(newId);
            MarkDirty("Added sound " + sound.DisplayLabel + ".");
        }

        private void DeleteSoundButton_Click(object sender, EventArgs e)
        {
            ImuseSoundModel sound = GetSelectedSound();
            if (_bank == null || sound == null)
            {
                return;
            }

            int index = _bank.Sounds.IndexOf(sound);
            _bank.Sounds.Remove(sound);
            PopulateSoundList(null);
            if (_soundList.Items.Count > 0)
            {
                _soundList.SelectedIndex = Math.Min(index, _soundList.Items.Count - 1);
            }
            else
            {
                ClearEditorFields();
            }

            MarkDirty("Removed sound " + sound.DisplayLabel + ".");
        }

        private void ApplyButton_Click(object sender, EventArgs e)
        {
            ApplyCurrentSoundChanges();
        }

        private void ImportButton_Click(object sender, EventArgs e)
        {
            ImuseSoundModel sound = GetSelectedSound();
            if (sound == null)
            {
                return;
            }

            OpenFileDialog dialog = new OpenFileDialog();
            dialog.Filter = "MIDI files (*.mid;*.midi)|*.mid;*.midi|All files (*.*)|*.*";
            dialog.Title = "Import MIDI";
            if (dialog.ShowDialog(GetDialogOwner()) != DialogResult.OK)
            {
                return;
            }

            try
            {
                ushort division;
                ImuseVariantModel variant = sound.EnsureVariant(GetSelectedVariantKind());
                variant.IncludeVariant = true;
                ApplyVariantSettings(variant);
                variant.Tracks.Clear();
                variant.Tracks.AddRange(ImuseBankCodec.ImportMidiTracks(dialog.FileName, out division));
                variant.Division = division;

                UpdateVariantUi();
                MarkDirty("Imported MIDI file '" + Path.GetFileName(dialog.FileName) + "'.");
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    GetDialogOwner(),
                    ex.Message,
                    "Import MIDI",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private void SoundList_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            UpdateSoundDetailsFromSelection();
            UpdateVariantUi();
        }

        private void VariantKindCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            UpdateVariantUi();
        }

        private void ApplyCurrentSoundChanges()
        {
            ImuseSoundModel sound = GetSelectedSound();
            if (_bank == null || sound == null)
            {
                return;
            }

            string trimmedName = _soundNameTextBox.Text.Trim();
            if (trimmedName.Length == 0)
            {
                MessageBox.Show(
                    GetDialogOwner(),
                    "Sound name cannot be empty.",
                    "Apply Sound Changes",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }

            ushort newId = (ushort)_soundIdSpin.Value;
            for (int i = 0; i < _bank.Sounds.Count; ++i)
            {
                if (!object.ReferenceEquals(_bank.Sounds[i], sound) && _bank.Sounds[i].SoundId == newId)
                {
                    MessageBox.Show(
                        GetDialogOwner(),
                        "Another sound already uses ID " + newId + ".",
                        "Apply Sound Changes",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                    return;
                }
            }

            sound.Name = trimmedName;
            sound.SoundId = newId;

            ImuseVariantKind variantKind = GetSelectedVariantKind();
            ImuseVariantModel variant = sound.FindVariant(variantKind);
            if (variant == null && _includeVariantCheck.Checked)
            {
                variant = sound.EnsureVariant(variantKind);
            }

            if (variant != null)
            {
                ApplyVariantSettings(variant);
            }

            PopulateSoundList(newId);
            MarkDirty("Applied changes to sound " + sound.DisplayLabel + ".");
        }

        private void ApplyVariantSettings(ImuseVariantModel variant)
        {
            variant.Kind = GetSelectedVariantKind();
            variant.IncludeVariant = _includeVariantCheck.Checked;
            variant.IncludeMdhd = _includeMdhdCheck.Checked;
            variant.Priority = (byte)_prioritySpin.Value;
            variant.Volume = (byte)_volumeSpin.Value;
            variant.Pan = (sbyte)_panSpin.Value;
            variant.Transpose = (sbyte)_transposeSpin.Value;
            variant.Detune = (sbyte)_detuneSpin.Value;
            variant.Speed = (byte)_speedSpin.Value;
            if (variant.Division == 0)
            {
                variant.Division = 480;
            }
        }

        private void PopulateSoundList(ushort? preferredSoundId)
        {
            _suppressUiEvents = true;
            try
            {
                _soundList.BeginUpdate();
                _soundList.Items.Clear();

                if (_bank != null)
                {
                    _bank.Sounds.Sort(delegate(ImuseSoundModel left, ImuseSoundModel right)
                    {
                        return left.SoundId.CompareTo(right.SoundId);
                    });

                    for (int i = 0; i < _bank.Sounds.Count; ++i)
                    {
                        _soundList.Items.Add(_bank.Sounds[i]);
                    }
                }

                if (preferredSoundId.HasValue)
                {
                    for (int i = 0; i < _soundList.Items.Count; ++i)
                    {
                        ImuseSoundModel sound = _soundList.Items[i] as ImuseSoundModel;
                        if (sound != null && sound.SoundId == preferredSoundId.Value)
                        {
                            _soundList.SelectedIndex = i;
                            break;
                        }
                    }
                }
            }
            finally
            {
                _soundList.EndUpdate();
                _suppressUiEvents = false;
            }

            UpdateSoundDetailsFromSelection();
            UpdateSummary();
            UpdateEnabledState();
        }

        private void UpdateSoundDetailsFromSelection()
        {
            ImuseSoundModel sound = GetSelectedSound();
            if (sound == null)
            {
                ClearEditorFields();
                return;
            }

            _soundNameTextBox.Text = sound.Name;
            _soundIdSpin.Value = sound.SoundId;
        }

        private void UpdateVariantUi()
        {
            ImuseSoundModel sound = GetSelectedSound();
            ImuseVariantModel variant = sound != null ? sound.FindVariant(GetSelectedVariantKind()) : null;

            _suppressUiEvents = true;
            try
            {
                if (sound == null)
                {
                    _includeVariantCheck.Checked = false;
                    _includeMdhdCheck.Checked = false;
                    _prioritySpin.Value = 128;
                    _volumeSpin.Value = 127;
                    _panSpin.Value = 0;
                    _transposeSpin.Value = 0;
                    _detuneSpin.Value = 0;
                    _speedSpin.Value = 128;
                    _tracksView.Items.Clear();
                }
                else if (variant == null)
                {
                    _includeVariantCheck.Checked = false;
                    _includeMdhdCheck.Checked = false;
                    _prioritySpin.Value = 128;
                    _volumeSpin.Value = 127;
                    _panSpin.Value = 0;
                    _transposeSpin.Value = 0;
                    _detuneSpin.Value = 0;
                    _speedSpin.Value = 128;
                    _tracksView.Items.Clear();
                }
                else
                {
                    _includeVariantCheck.Checked = variant.IncludeVariant;
                    _includeMdhdCheck.Checked = variant.IncludeMdhd;
                    _prioritySpin.Value = variant.Priority;
                    _volumeSpin.Value = variant.Volume;
                    _panSpin.Value = variant.Pan;
                    _transposeSpin.Value = variant.Transpose;
                    _detuneSpin.Value = variant.Detune;
                    _speedSpin.Value = variant.Speed;

                    _tracksView.BeginUpdate();
                    _tracksView.Items.Clear();
                    for (int i = 0; i < variant.Tracks.Count; ++i)
                    {
                        ImuseTrackModel track = variant.Tracks[i];
                        ListViewItem item = new ListViewItem(track.Name);
                        item.SubItems.Add(track.SourceFileName);
                        item.SubItems.Add(track.EventCount.ToString());
                        _tracksView.Items.Add(item);
                    }
                    _tracksView.EndUpdate();
                }
            }
            finally
            {
                _suppressUiEvents = false;
            }

            UpdateEnabledState();
        }

        private void UpdateEnabledState()
        {
            bool hasSelection = GetSelectedSound() != null;

            _soundNameTextBox.Enabled = hasSelection;
            _soundIdSpin.Enabled = hasSelection;
            _variantKindCombo.Enabled = hasSelection;
            _includeVariantCheck.Enabled = hasSelection;
            _includeMdhdCheck.Enabled = hasSelection;
            _prioritySpin.Enabled = hasSelection;
            _volumeSpin.Enabled = hasSelection;
            _panSpin.Enabled = hasSelection;
            _transposeSpin.Enabled = hasSelection;
            _detuneSpin.Enabled = hasSelection;
            _speedSpin.Enabled = hasSelection;
            _tracksView.Enabled = hasSelection;
            _applyButton.Enabled = hasSelection;
            _importButton.Enabled = hasSelection;
            _saveButton.Enabled = _bank != null;
        }

        private void ClearEditorFields()
        {
            _soundNameTextBox.Text = string.Empty;
            _soundIdSpin.Value = 0;
            _tracksView.Items.Clear();
            UpdateEnabledState();
        }

        private void UpdateSummary()
        {
            int count = _bank != null ? _bank.Sounds.Count : 0;
            string dirtySuffix = _isDirty ? " (modified)" : string.Empty;
            _summaryLabel.Text = count + " sound(s)." + dirtySuffix;
        }

        private void MarkDirty(string message)
        {
            _isDirty = true;
            UpdateSummary();
            if (!string.IsNullOrEmpty(message))
            {
                _statusLabel.Text = message;
            }

            RaiseDirtyStateChanged();
        }

        private void RaiseDirtyStateChanged()
        {
            if (DirtyStateChanged != null)
            {
                DirtyStateChanged(this, EventArgs.Empty);
            }
        }

        private void RaiseLayoutStateChanged()
        {
            if (LayoutStateChanged != null)
            {
                LayoutStateChanged(this, EventArgs.Empty);
            }
        }

        private ImuseSoundModel GetSelectedSound()
        {
            return _soundList.SelectedItem as ImuseSoundModel;
        }

        private ImuseVariantKind GetSelectedVariantKind()
        {
            if (_variantKindCombo.SelectedIndex == 1)
            {
                return ImuseVariantKind.RolandMt32;
            }

            if (_variantKindCombo.SelectedIndex == 2)
            {
                return ImuseVariantKind.Adlib;
            }

            return ImuseVariantKind.GeneralMidi;
        }

        private IWin32Window GetDialogOwner()
        {
            Form owner = FindForm();
            return owner;
        }

        private void ApplyMainSplitterDistance()
        {
            if (_mainSplit == null || _mainSplit.IsDisposed)
            {
                return;
            }

            int availableSize = _mainSplit.Orientation == Orientation.Vertical
                ? _mainSplit.ClientSize.Width
                : _mainSplit.ClientSize.Height;
            if (availableSize <= 0)
            {
                return;
            }

            int minimumDistance = _mainSplit.Panel1MinSize;
            int maximumDistance = availableSize - _mainSplit.Panel2MinSize - _mainSplit.SplitterWidth;
            if (maximumDistance < minimumDistance)
            {
                return;
            }

            int clampedDistance = Math.Max(minimumDistance, Math.Min(maximumDistance, _preferredMainSplitterDistance));
            if (_mainSplit.SplitterDistance != clampedDistance)
            {
                _mainSplit.SplitterDistance = clampedDistance;
            }
        }
    }
}
