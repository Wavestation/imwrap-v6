using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using AGS.Types;
using Microsoft.Win32;

namespace AgsIMWrap.Editor
{
    internal sealed class IMWrapEditorPane : EditorContentPanel
    {
        private const string RegistryKeyPath = @"Software\iGaST MegaZik Engine\AGS.Plugin.IMWrap.Editor";
        private const string RegistryMainSplitterDistanceValue = "MainSplitterDistance";

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
        private readonly Button _deleteTrackButton;
        private readonly Button _moveTrackUpButton;
        private readonly Button _moveTrackDownButton;
        private readonly Button _refreshButton;
        private readonly Button _saveButton;
        // Disabled for now: persistence proved unreliable inside AGS.
        // private readonly Timer _splitterPersistTimer;

        private IMWrapBankProjectModel _bank;
        private bool _isDirty;
        private bool _layoutRestorePending;
        private int _preferredMainSplitterDistance;
        private bool _pendingImportAsReplace;
        private bool _suppressUiEvents;

        public IMWrapEditorPane()
        {
            Dock = DockStyle.Fill;
            // Disabled for now: persistence proved unreliable inside AGS.
            // _preferredMainSplitterDistance = LoadPersistedMainSplitterDistance();
            _preferredMainSplitterDistance = 0;
            HandleCreated += IMWrapEditorPane_HandleCreated;
            Layout += IMWrapEditorPane_Layout;
            VisibleChanged += IMWrapEditorPane_VisibleChanged;
            ParentChanged += IMWrapEditorPane_ParentChanged;
            // Disabled for now: persistence proved unreliable inside AGS.
            // _splitterPersistTimer = new Timer();
            // _splitterPersistTimer.Interval = 200;
            // _splitterPersistTimer.Tick += SplitterPersistTimer_Tick;

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
            _mainSplit.SplitterDistance = 0;
            _mainSplit.SplitterMoved += MainSplit_SplitterMoved;
            _mainSplit.MouseUp += MainSplit_MouseUp;
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
            _soundNameTextBox.TextChanged += SoundNameTextBox_TextChanged;
            _soundNameTextBox.Leave += SoundIdentityControl_Leave;
            soundProperties.Controls.Add(_soundNameTextBox, 1, 0);

            Label soundIdLabel = new Label();
            soundIdLabel.AutoSize = true;
            soundIdLabel.Margin = new Padding(12, 6, 8, 0);
            soundIdLabel.Text = "ID:";
            soundProperties.Controls.Add(soundIdLabel, 2, 0);

            _soundIdSpin = new NumericUpDown();
            _soundIdSpin.Maximum = 65535;
            _soundIdSpin.Width = 90;
            _soundIdSpin.ValueChanged += SoundIdSpin_ValueChanged;
            _soundIdSpin.Leave += SoundIdentityControl_Leave;
            soundProperties.Controls.Add(_soundIdSpin, 3, 0);

            _applyButton = new Button();
            _applyButton.AutoSize = true;
            _applyButton.Text = "Apply Sound Changes";
            _applyButton.Click += ApplyButton_Click;
            _applyButton.Visible = false;
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
            _includeVariantCheck.CheckedChanged += VariantSettingsControlChanged;
            variantFlags.Controls.Add(_includeVariantCheck);

            _includeMdhdCheck = new CheckBox();
            _includeMdhdCheck.AutoSize = true;
            _includeMdhdCheck.Text = "Include MDhd";
            _includeMdhdCheck.CheckedChanged += VariantSettingsControlChanged;
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
            _prioritySpin.ValueChanged += VariantSettingsControlChanged;
            _volumeSpin.ValueChanged += VariantSettingsControlChanged;
            _panSpin.ValueChanged += VariantSettingsControlChanged;
            _transposeSpin.ValueChanged += VariantSettingsControlChanged;
            _detuneSpin.ValueChanged += VariantSettingsControlChanged;
            _speedSpin.ValueChanged += VariantSettingsControlChanged;

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
            _tracksView.Columns.Add("Track", 50);
            _tracksView.Columns.Add("Name", 200);
            _tracksView.Columns.Add("Source", 170);
            _tracksView.Columns.Add("Events", 70);
            _tracksView.Enter += TracksView_Enter;
            _tracksView.Leave += TracksView_Leave;
            _tracksView.SelectedIndexChanged += TracksView_SelectedIndexChanged;
            tracksGroup.Controls.Add(_tracksView);

            FlowLayoutPanel importPanel = new FlowLayoutPanel();
            importPanel.Dock = DockStyle.Fill;
            importPanel.FlowDirection = FlowDirection.LeftToRight;
            importPanel.WrapContents = true;
            importPanel.Padding = new Padding(8, 0, 8, 8);
            variantLayout.Controls.Add(importPanel, 0, 3);

            _importButton = new Button();
            _importButton.AutoSize = true;
            _importButton.Text = "Import MIDI...";
            _importButton.MouseDown += ImportButton_MouseDown;
            _importButton.Click += ImportButton_Click;
            importPanel.Controls.Add(_importButton);

            _deleteTrackButton = new Button();
            _deleteTrackButton.AutoSize = true;
            _deleteTrackButton.Text = "Delete Track";
            _deleteTrackButton.Click += DeleteTrackButton_Click;
            importPanel.Controls.Add(_deleteTrackButton);

            _moveTrackUpButton = new Button();
            _moveTrackUpButton.AutoSize = true;
            _moveTrackUpButton.Text = "Move Up";
            _moveTrackUpButton.Click += MoveTrackUpButton_Click;
            importPanel.Controls.Add(_moveTrackUpButton);

            _moveTrackDownButton = new Button();
            _moveTrackDownButton.AutoSize = true;
            _moveTrackDownButton.Text = "Move Down";
            _moveTrackDownButton.Click += MoveTrackDownButton_Click;
            importPanel.Controls.Add(_moveTrackDownButton);

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

        public IMWrapBankProjectModel Bank
        {
            get { return _bank; }
        }

        public int MainSplitterDistance
        {
            get { return _mainSplit != null ? _mainSplit.SplitterDistance : _preferredMainSplitterDistance; }
            set
            {
                _preferredMainSplitterDistance = value > 0 ? value : 0;
                ApplyMainSplitterDistance();
            }
        }

        public void LoadBank(IMWrapBankProjectModel bank)
        {
            _bank = bank;
            _bankPathValue.Text = bank != null ? bank.RelativePath : "(none)";
            RefreshSplitterLayout();
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

            SelectPreferredVariantForSelectedSound();
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
            bool stateChanged = _isDirty;
            _isDirty = false;
            if (!string.IsNullOrEmpty(message))
            {
                _statusLabel.Text = message;
            }

            UpdateSummary();
            if (stateChanged)
            {
                RaiseDirtyStateChanged();
            }
        }

        public byte[] BuildBankBytes()
        {
            EnsureCurrentEditorStateIsValid();
            return IMWrapBankCodec.SaveToBytes(_bank);
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
            RaiseLayoutStateChanged();
        }

        private void MainSplit_MouseUp(object sender, MouseEventArgs e)
        {
            CaptureCurrentSplitterDistance();
        }

        private void MainSplit_SizeChanged(object sender, EventArgs e)
        {
            RefreshSplitterLayout();
        }

        private void IMWrapEditorPane_HandleCreated(object sender, EventArgs e)
        {
            RefreshSplitterLayout();
        }

        private void IMWrapEditorPane_Layout(object sender, LayoutEventArgs e)
        {
            RefreshSplitterLayout();
        }

        private void IMWrapEditorPane_VisibleChanged(object sender, EventArgs e)
        {
            if (Visible)
            {
                RefreshSplitterLayout();
            }
        }

        private void IMWrapEditorPane_ParentChanged(object sender, EventArgs e)
        {
            RefreshSplitterLayout();
        }

        private void SplitterPersistTimer_Tick(object sender, EventArgs e)
        {
            // Disabled for now: persistence proved unreliable inside AGS.
            // _splitterPersistTimer.Stop();
            // SaveCurrentSplitterDistance();
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

            IMWrapSoundModel sound = new IMWrapSoundModel();
            sound.SoundId = newId;
            sound.Name = "NewSound";
            _bank.Sounds.Add(sound);
            PopulateSoundList(newId);
            MarkDirty("Added sound " + sound.DisplayLabel + ".");
        }

        private void DeleteSoundButton_Click(object sender, EventArgs e)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (_bank == null || sound == null)
            {
                return;
            }

            if (!ConfirmDestructiveAction(
                "Delete Sound",
                "Delete sound '" + sound.DisplayLabel + "'?" + Environment.NewLine +
                "All variants and tracks in this sound will be removed."))
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

        private void SoundNameTextBox_TextChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            TryApplySelectedSoundIdentity(true, true);
        }

        private void SoundIdSpin_ValueChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            TryApplySelectedSoundIdentity(true, true);
        }

        private void SoundIdentityControl_Leave(object sender, EventArgs e)
        {
            RefreshSoundListDisplay();
        }

        private void VariantSettingsControlChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            TryApplyVariantSettingsFromUi(true, true);
        }

        private void TracksView_Enter(object sender, EventArgs e)
        {
            UpdateImportButtonMode();
        }

        private void TracksView_Leave(object sender, EventArgs e)
        {
            if (!_pendingImportAsReplace &&
                Control.MouseButtons == MouseButtons.Left &&
                _importButton != null &&
                GetSelectedTrackIndex() >= 0 &&
                _importButton.ClientRectangle.Contains(_importButton.PointToClient(Cursor.Position)))
            {
                _pendingImportAsReplace = true;
            }

            UpdateImportButtonMode();
        }

        private void ImportButton_MouseDown(object sender, MouseEventArgs e)
        {
            _pendingImportAsReplace = e.Button == MouseButtons.Left && HasFocusedTrackSelection();
            UpdateImportButtonMode();
        }

        private void ImportButton_Click(object sender, EventArgs e)
        {
            bool replaceTrack = _pendingImportAsReplace || HasFocusedTrackSelection();
            try
            {
                if (replaceTrack)
                {
                    ReplaceTrackButton_Click(sender, e);
                    return;
                }

                IMWrapSoundModel sound = GetSelectedSound();
                if (sound == null)
                {
                    return;
                }

                OpenFileDialog dialog = new OpenFileDialog();
                dialog.Filter = "MIDI files (*.mid;*.midi)|*.mid;*.midi|All files (*.*)|*.*";
                dialog.Title = "Import MIDI";
                dialog.Multiselect = true;
                if (dialog.ShowDialog(GetDialogOwner()) != DialogResult.OK)
                {
                    return;
                }

                try
                {
                    ushort division;
                    IMWrapVariantModel variant = sound.EnsureVariant(GetSelectedVariantKind());
                    ApplyVariantSettings(variant, true);
                    // Do not clear. Append tracks.
                    variant.Tracks.AddRange(IMWrapBankCodec.ImportMidiTracks(dialog.FileNames, out division));
                    variant.Division = division;

                    UpdateVariantUi();
                    RefreshSelectedSoundListItem();
                    string filesDescription = dialog.FileNames.Length == 1 ?
                        "'" + Path.GetFileName(dialog.FileName) + "'" :
                        dialog.FileNames.Length + " MIDI files";
                    MarkDirty("Imported MIDI file " + filesDescription + ".");
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
            finally
            {
                _pendingImportAsReplace = false;
                UpdateImportButtonMode();
            }
        }

        private void ReplaceTrackButton_Click(object sender, EventArgs e)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null)
            {
                return;
            }

            IMWrapVariantModel variant = sound.FindVariant(GetSelectedVariantKind());
            if (variant == null)
            {
                return;
            }

            int selectedIndex = GetSelectedTrackIndex();
            if (selectedIndex < 0 || selectedIndex >= variant.Tracks.Count)
            {
                return;
            }

            IMWrapTrackModel selectedTrack = variant.Tracks[selectedIndex];

            OpenFileDialog dialog = new OpenFileDialog();
            dialog.Filter = "MIDI files (*.mid;*.midi)|*.mid;*.midi|All files (*.*)|*.*";
            dialog.Title = "Replace Track with MIDI";
            dialog.Multiselect = false;
            if (dialog.ShowDialog(GetDialogOwner()) != DialogResult.OK)
            {
                return;
            }

            try
            {
                ushort division;
                List<IMWrapTrackModel> importedTracks = IMWrapBankCodec.ImportMidiTracks(new[] { dialog.FileName }, out division);
                if (importedTracks.Count == 0)
                {
                    MessageBox.Show(
                        GetDialogOwner(),
                        "The selected MIDI file did not contain any importable tracks.",
                        "Replace Track",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                    return;
                }

                string fileName = Path.GetFileName(dialog.FileName);
                string trackLabel = BuildTrackLabel(selectedTrack, selectedIndex);
                string confirmationMessage;
                if (importedTracks.Count == 1)
                {
                    confirmationMessage = "Replace track '" + trackLabel + "' with '" + fileName + "'?";
                }
                else
                {
                    confirmationMessage =
                        "Replace track '" + trackLabel + "' with the first track from '" + fileName + "'?" + Environment.NewLine +
                        (importedTracks.Count - 1) + " additional track(s) will be inserted right after it.";
                }

                if (!ConfirmDestructiveAction("Replace Track", confirmationMessage))
                {
                    return;
                }

                ApplyVariantSettings(variant, true);
                variant.Tracks.RemoveAt(selectedIndex);
                for (int i = 0; i < importedTracks.Count; ++i)
                {
                    variant.Tracks.Insert(selectedIndex + i, importedTracks[i]);
                }
                variant.Division = division;

                UpdateVariantUi();
                RefreshSelectedSoundListItem();
                if (_tracksView.Items.Count > selectedIndex)
                {
                    _tracksView.Items[selectedIndex].Selected = true;
                    _tracksView.Items[selectedIndex].EnsureVisible();
                }

                if (importedTracks.Count == 1)
                {
                    MarkDirty("Replaced track with MIDI file '" + fileName + "'.");
                }
                else
                {
                    MarkDirty(
                        "Replaced track and inserted " + (importedTracks.Count - 1) +
                        " additional track(s) from '" + fileName + "'.");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    GetDialogOwner(),
                    ex.Message,
                    "Replace Track",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private void TracksView_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateEnabledState();
        }

        private void DeleteTrackButton_Click(object sender, EventArgs e)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null) return;
            IMWrapVariantModel variant = sound.FindVariant(GetSelectedVariantKind());
            if (variant == null) return;

            if (_tracksView.SelectedIndices.Count > 0)
            {
                int selectedIndex = _tracksView.SelectedIndices[0];
                IMWrapTrackModel selectedTrack = variant.Tracks[selectedIndex];
                if (!ConfirmDestructiveAction(
                    "Delete Track",
                    "Delete track '" + BuildTrackLabel(selectedTrack, selectedIndex) + "'?"))
                {
                    return;
                }

                variant.Tracks.RemoveAt(selectedIndex);
                UpdateVariantUi();
                
                if (variant.Tracks.Count > 0)
                {
                    int newIndex = Math.Min(selectedIndex, variant.Tracks.Count - 1);
                    _tracksView.Items[newIndex].Selected = true;
                }
                
                MarkDirty("Deleted track.");
            }
        }

        private void MoveTrackUpButton_Click(object sender, EventArgs e)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null) return;
            IMWrapVariantModel variant = sound.FindVariant(GetSelectedVariantKind());
            if (variant == null) return;

            if (_tracksView.SelectedIndices.Count > 0)
            {
                int selectedIndex = _tracksView.SelectedIndices[0];
                if (selectedIndex > 0)
                {
                    IMWrapTrackModel track = variant.Tracks[selectedIndex];
                    variant.Tracks.RemoveAt(selectedIndex);
                    variant.Tracks.Insert(selectedIndex - 1, track);
                    UpdateVariantUi();
                    _tracksView.Items[selectedIndex - 1].Selected = true;
                    _tracksView.Items[selectedIndex - 1].EnsureVisible();
                    MarkDirty("Moved track up.");
                }
            }
        }

        private void MoveTrackDownButton_Click(object sender, EventArgs e)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null) return;
            IMWrapVariantModel variant = sound.FindVariant(GetSelectedVariantKind());
            if (variant == null) return;

            if (_tracksView.SelectedIndices.Count > 0)
            {
                int selectedIndex = _tracksView.SelectedIndices[0];
                if (selectedIndex < variant.Tracks.Count - 1)
                {
                    IMWrapTrackModel track = variant.Tracks[selectedIndex];
                    variant.Tracks.RemoveAt(selectedIndex);
                    variant.Tracks.Insert(selectedIndex + 1, track);
                    UpdateVariantUi();
                    _tracksView.Items[selectedIndex + 1].Selected = true;
                    _tracksView.Items[selectedIndex + 1].EnsureVisible();
                    MarkDirty("Moved track down.");
                }
            }
        }

        private void SoundList_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_suppressUiEvents)
            {
                return;
            }

            SelectPreferredVariantForSelectedSound();
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
            if (!TryApplySelectedSoundIdentity(true, true))
            {
                return;
            }

            TryApplyVariantSettingsFromUi(true, true);
        }

        private bool TryApplySelectedSoundIdentity(bool showValidationStatus, bool markDirtyOnChange)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (_bank == null || sound == null)
            {
                return true;
            }

            string name = _soundNameTextBox.Text;
            ushort newId = (ushort)_soundIdSpin.Value;
            bool soundIdChanged = sound.SoundId != newId;
            bool soundNameChanged = !string.Equals(sound.Name, name, StringComparison.Ordinal);
            if (!soundIdChanged && !soundNameChanged)
            {
                if (showValidationStatus)
                {
                    string validationMessage = ValidateSelectedSoundIdentity(sound, name, newId);
                    if (validationMessage != null)
                    {
                        ShowStatus(validationMessage);
                    }
                }

                return true;
            }

            sound.Name = name;
            sound.SoundId = newId;
            RefreshSoundListDisplay();

            if (markDirtyOnChange)
            {
                string validationMessage = ValidateSelectedSoundIdentity(sound, name, newId);
                MarkDirty(validationMessage ?? "Unsaved changes pending.");
            }

            return true;
        }

        private string ValidateSelectedSoundIdentity(IMWrapSoundModel sound, string trimmedName, ushort newId)
        {
            if (_bank == null || sound == null)
            {
                return null;
            }

            if (trimmedName == null || trimmedName.Trim().Length == 0)
            {
                return "Sound name cannot be empty.";
            }

            for (int i = 0; i < _bank.Sounds.Count; ++i)
            {
                if (!object.ReferenceEquals(_bank.Sounds[i], sound) && _bank.Sounds[i].SoundId == newId)
                {
                    return "Another sound already uses ID " + newId + ".";
                }
            }

            return null;
        }

        private bool TryApplyVariantSettingsFromUi(bool createVariantWhenNeeded, bool markDirtyOnChange)
        {
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null)
            {
                return true;
            }

            IMWrapVariantKind variantKind = GetSelectedVariantKind();
            IMWrapVariantModel variant = sound.FindVariant(variantKind);
            if (variant == null && !createVariantWhenNeeded)
            {
                return true;
            }

            bool variantChanged =
                variant == null ||
                variant.Kind != variantKind ||
                variant.IncludeVariant != _includeVariantCheck.Checked ||
                variant.IncludeMdhd != _includeMdhdCheck.Checked ||
                variant.Priority != (byte)_prioritySpin.Value ||
                variant.Volume != (byte)_volumeSpin.Value ||
                variant.Pan != (sbyte)_panSpin.Value ||
                variant.Transpose != (sbyte)_transposeSpin.Value ||
                variant.Detune != (sbyte)_detuneSpin.Value ||
                variant.Speed != (byte)_speedSpin.Value;

            if (!variantChanged)
            {
                return true;
            }

            if (variant == null)
            {
                variant = sound.EnsureVariant(variantKind);
            }

            ApplyVariantSettings(variant);
            RefreshSelectedSoundListItem();
            if (markDirtyOnChange)
            {
                MarkDirty("Unsaved changes pending.");
            }

            return true;
        }

        private void ApplyVariantSettings(IMWrapVariantModel variant, bool forceIncludeVariant)
        {
            variant.Kind = GetSelectedVariantKind();
            variant.IncludeVariant = forceIncludeVariant || _includeVariantCheck.Checked;
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

        private void ApplyVariantSettings(IMWrapVariantModel variant)
        {
            ApplyVariantSettings(variant, false);
        }

        private void EnsureCurrentEditorStateIsValid()
        {
            if (_bank == null)
            {
                return;
            }

            for (int i = 0; i < _bank.Sounds.Count; ++i)
            {
                IMWrapSoundModel sound = _bank.Sounds[i];
                string validationMessage = ValidateSelectedSoundIdentity(sound, sound.Name, sound.SoundId);
                if (validationMessage != null)
                {
                    throw new InvalidOperationException(validationMessage);
                }
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
                    _bank.Sounds.Sort(delegate(IMWrapSoundModel left, IMWrapSoundModel right)
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
                        IMWrapSoundModel sound = _soundList.Items[i] as IMWrapSoundModel;
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
            IMWrapSoundModel sound = GetSelectedSound();
            if (sound == null)
            {
                ClearEditorFields();
                return;
            }

            _suppressUiEvents = true;
            try
            {
                _soundNameTextBox.Text = sound.Name;
                _soundIdSpin.Value = sound.SoundId;
            }
            finally
            {
                _suppressUiEvents = false;
            }
        }

        private void RefreshSelectedSoundListItem()
        {
            RefreshSoundListDisplay();
        }

        private void RefreshSoundListDisplay()
        {
            if (_soundList != null && !_soundList.IsDisposed)
            {
                _soundList.Invalidate();
            }
        }

        private void SelectPreferredVariantForSelectedSound()
        {
            IMWrapSoundModel sound = GetSelectedSound();
            int selectedIndex = GetPreferredVariantComboIndex(sound);

            _suppressUiEvents = true;
            try
            {
                if (_variantKindCombo.SelectedIndex != selectedIndex)
                {
                    _variantKindCombo.SelectedIndex = selectedIndex;
                }
            }
            finally
            {
                _suppressUiEvents = false;
            }
        }

        private static int GetPreferredVariantComboIndex(IMWrapSoundModel sound)
        {
            if (sound == null)
            {
                return 0;
            }

            if (IsVariantFilled(sound.FindVariant(IMWrapVariantKind.GeneralMidi)))
            {
                return 0;
            }

            if (IsVariantFilled(sound.FindVariant(IMWrapVariantKind.RolandMt32)))
            {
                return 1;
            }

            if (IsVariantFilled(sound.FindVariant(IMWrapVariantKind.Adlib)))
            {
                return 2;
            }

            return 0;
        }

        private static bool IsVariantFilled(IMWrapVariantModel variant)
        {
            return variant != null &&
                (variant.IncludeVariant || variant.IncludeMdhd || variant.Tracks.Count > 0);
        }

        private void UpdateVariantUi()
        {
            IMWrapSoundModel sound = GetSelectedSound();
            IMWrapVariantModel variant = sound != null ? sound.FindVariant(GetSelectedVariantKind()) : null;

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
                        IMWrapTrackModel track = variant.Tracks[i];
                        ListViewItem item = new ListViewItem(i.ToString());
                        item.SubItems.Add(track.Name);
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

            bool hasSelectedTrack = hasSelection && _tracksView.SelectedIndices.Count > 0;
            _deleteTrackButton.Enabled = hasSelectedTrack;
            _moveTrackUpButton.Enabled = hasSelectedTrack && _tracksView.SelectedIndices[0] > 0;
            _moveTrackDownButton.Enabled = hasSelectedTrack && _tracksView.SelectedIndices[0] < _tracksView.Items.Count - 1;
            UpdateImportButtonMode();
        }

        private void ClearEditorFields()
        {
            _pendingImportAsReplace = false;
            _suppressUiEvents = true;
            try
            {
                _soundNameTextBox.Text = string.Empty;
                _soundIdSpin.Value = 0;
            }
            finally
            {
                _suppressUiEvents = false;
            }

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
            bool stateChanged = !_isDirty;
            _isDirty = true;
            UpdateSummary();
            if (!string.IsNullOrEmpty(message))
            {
                _statusLabel.Text = message;
            }

            if (stateChanged)
            {
                RaiseDirtyStateChanged();
            }
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

        private IMWrapSoundModel GetSelectedSound()
        {
            return _soundList.SelectedItem as IMWrapSoundModel;
        }

        private IMWrapVariantKind GetSelectedVariantKind()
        {
            if (_variantKindCombo.SelectedIndex == 1)
            {
                return IMWrapVariantKind.RolandMt32;
            }

            if (_variantKindCombo.SelectedIndex == 2)
            {
                return IMWrapVariantKind.Adlib;
            }

            return IMWrapVariantKind.GeneralMidi;
        }

        private void UpdateImportButtonMode()
        {
            if (_importButton == null)
            {
                return;
            }

            _importButton.Text = HasFocusedTrackSelection() || _pendingImportAsReplace
                ? "Replace Track..."
                : "Import MIDI...";
        }

        private int GetSelectedTrackIndex()
        {
            return _tracksView.SelectedIndices.Count > 0 ? _tracksView.SelectedIndices[0] : -1;
        }

        private bool HasFocusedTrackSelection()
        {
            return _tracksView != null && _tracksView.Focused && GetSelectedTrackIndex() >= 0;
        }

        private static string BuildTrackLabel(IMWrapTrackModel track, int index)
        {
            string name = track != null && !string.IsNullOrEmpty(track.Name)
                ? track.Name
                : "Track " + index;
            return name + " (#" + index + ")";
        }

        private bool ConfirmDestructiveAction(string title, string message)
        {
            return MessageBox.Show(
                GetDialogOwner(),
                message + Environment.NewLine + Environment.NewLine + "This action cannot be undone.",
                title,
                MessageBoxButtons.OKCancel,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2) == DialogResult.OK;
        }

        protected override void OnPanelClosing(bool canCancel, ref bool cancelClose)
        {
            base.OnPanelClosing(canCancel, ref cancelClose);
            if (cancelClose)
            {
                return;
            }

            if (!canCancel || !_isDirty)
            {
                return;
            }

            DialogResult result = MessageBox.Show(
                GetDialogOwner(),
                "Unsaved changes will be lost. Do you want to continue?",
                "Close Document",
                MessageBoxButtons.OKCancel,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2);

            if (result != DialogResult.OK)
            {
                cancelClose = true;
            }
        }

        private IWin32Window GetDialogOwner()
        {
            Form owner = FindForm();
            return owner;
        }

        public void RefreshSplitterLayout()
        {
            ApplyMainSplitterDistance();
            ScheduleDeferredSplitterLayout();
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

            int requestedDistance = _preferredMainSplitterDistance > 0
                ? _preferredMainSplitterDistance
                : availableSize / 4;

            int clampedDistance = Math.Max(minimumDistance, Math.Min(maximumDistance, requestedDistance));
            if (_mainSplit.SplitterDistance != clampedDistance)
            {
                _mainSplit.SplitterDistance = clampedDistance;
            }
        }

        private void ScheduleDeferredSplitterLayout()
        {
            if (_layoutRestorePending || !IsHandleCreated || IsDisposed)
            {
                return;
            }

            _layoutRestorePending = true;
            BeginInvoke((MethodInvoker)delegate
            {
                _layoutRestorePending = false;
                ApplyMainSplitterDistance();
                if (IsHandleCreated && !IsDisposed)
                {
                    BeginInvoke((MethodInvoker)ApplyMainSplitterDistance);
                }
            });
        }

        private void CaptureCurrentSplitterDistance()
        {
            if (_mainSplit == null || _mainSplit.IsDisposed)
            {
                return;
            }

            if (_mainSplit.SplitterDistance > 0)
            {
                _preferredMainSplitterDistance = _mainSplit.SplitterDistance;
            }
        }

        private void ScheduleSplitterPersistence()
        {
            // Disabled for now: persistence proved unreliable inside AGS.
            // if (_splitterPersistTimer == null)
            // {
            //     return;
            // }
            //
            // _splitterPersistTimer.Stop();
            // _splitterPersistTimer.Start();
        }

        private void SaveCurrentSplitterDistance()
        {
            CaptureCurrentSplitterDistance();
            // Disabled for now: persistence proved unreliable inside AGS.
            // if (_preferredMainSplitterDistance > 0)
            // {
            //     SavePersistedMainSplitterDistance(_preferredMainSplitterDistance);
            // }
        }

        private static int LoadPersistedMainSplitterDistance()
        {
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RegistryKeyPath, false))
                {
                    if (key != null)
                    {
                        object value = key.GetValue(RegistryMainSplitterDistanceValue);
                        int splitterDistance;
                        if (value != null && int.TryParse(value.ToString(), out splitterDistance) && splitterDistance > 0)
                        {
                            return splitterDistance;
                        }
                    }
                }
            }
            catch
            {
            }

            return 0;
        }

        private static void SavePersistedMainSplitterDistance(int splitterDistance)
        {
            if (splitterDistance <= 0)
            {
                return;
            }

            try
            {
                using (RegistryKey key = Registry.CurrentUser.CreateSubKey(RegistryKeyPath))
                {
                    if (key != null)
                    {
                        key.SetValue(RegistryMainSplitterDistanceValue, splitterDistance, RegistryValueKind.DWord);
                    }
                }
            }
            catch
            {
            }
        }
    }
}
