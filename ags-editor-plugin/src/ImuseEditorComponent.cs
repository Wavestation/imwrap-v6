using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using System.Xml;
using AGS.Types;
using Microsoft.Win32;

namespace AgsImuse.Editor
{
    internal sealed class ImuseEditorComponent : IEditorComponent
    {
        private sealed class OpenBankDocument
        {
            public string RelativePath;
            public ContentDocument Document;
            public ImuseEditorPane Pane;
        }

        private const string RootNodeId = "Imuse.Root";
        private const string BanksNodeId = "Imuse.Banks";
        private const string RootMenuId = "ImuseMenu";
        private const string RootIconKey = "Imuse.RootIcon";
        private const string FolderIconKey = "Imuse.FolderIcon";
        private const string FileIconKey = "Imuse.FileIcon";
        private const string EditorIconKey = "Imuse.EditorIcon";
        private const string FileNodePrefix = "Imuse.File:";
        private const string RegistryKeyPath = @"Software\iGaST MegaZik Engine\AGS.Plugin.Imuse.Editor";
        private const string RegistryMainSplitterDistanceValue = "MainSplitterDistance";
        private const string XmlNodeName = "ImuseEditor";
        private const string XmlMainSplitterDistanceAttribute = "MainSplitterDistance";
        private const string XmlSelectedBankAttribute = "SelectedBank";
        private const string CommandOpenEditor = "OpenEditor";
        private const string CommandOpenSelected = "OpenSelectedBank";
        private const string CommandCreateBank = "CreateBank";
        private const string CommandRenameBank = "RenameBank";
        private const string CommandDeleteBank = "DeleteBank";
        private const string CommandRefreshBanks = "RefreshBanks";
        private const int DefaultMainSplitterDistance = 240;

        private static readonly string[] IgnoredDirectories = new[]
        {
            ".git",
            ".svn",
            ".vs",
            "bin",
            "obj",
            "Compiled",
            "_Debug"
        };

        private readonly IAGSEditor _editor;
        private readonly IGUIController _gui;
        private readonly Dictionary<string, string> _fileNodePaths;
        private readonly List<string> _knownBankPaths;
        private readonly Dictionary<string, OpenBankDocument> _openDocuments;
        private readonly MenuCommands _mainMenuCommands;

        private int _mainSplitterDistance;
        private string _selectedBankRelativePath;
        private string _contextBankRelativePath;
        private string _contextDirectoryRelativePath;
        private bool _isShutdown;

        public ImuseEditorComponent(IAGSEditor editor)
        {
            _editor = editor;
            _gui = editor.GUIController;
            _fileNodePaths = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            _knownBankPaths = new List<string>();
            _openDocuments = new Dictionary<string, OpenBankDocument>(StringComparer.OrdinalIgnoreCase);
            _contextDirectoryRelativePath = string.Empty;
            _mainSplitterDistance = LoadPersistedMainSplitterDistance();

            _gui.RegisterIcon(RootIconKey, EmbeddedIconLoader.LoadPngIcon("AgsImuse.Editor.Icons.imuse.png", SystemIcons.Application));
            _gui.RegisterIcon(FolderIconKey, EmbeddedIconLoader.LoadPngIcon("AgsImuse.Editor.Icons.bank_folder.png", SystemIcons.Shield));
            _gui.RegisterIcon(FileIconKey, EmbeddedIconLoader.LoadPngIcon("AgsImuse.Editor.Icons.bank_element.png", SystemIcons.Information));
            _gui.RegisterIcon(EditorIconKey, EmbeddedIconLoader.LoadPngIcon("AgsImuse.Editor.Icons.packer_tab.png", SystemIcons.Application));

            _gui.AddMenu(this, RootMenuId, "&iMUSE", _gui.FileMenuID);
            _mainMenuCommands = new MenuCommands(RootMenuId);
            _mainMenuCommands.Commands.Add(new MenuCommand(CommandOpenEditor, "&Open iMUSE Packer", EditorIconKey));
            _mainMenuCommands.Commands.Add(new MenuCommand(CommandCreateBank, "&New Bank...", FolderIconKey));
            _mainMenuCommands.Commands.Add(new MenuCommand(CommandRefreshBanks, "&Refresh Banks", FileIconKey));
            _gui.AddMenuItems(this, _mainMenuCommands);

            _gui.ProjectTree.AddTreeRoot(this, RootNodeId, "iMUSE", RootIconKey);
            RefreshTree();
        }

        public string ComponentID
        {
            get { return "ImuseEditorComponent"; }
        }

        public void CommandClick(string controlID)
        {
            if (string.Equals(controlID, RootNodeId, StringComparison.OrdinalIgnoreCase) ||
                string.Equals(controlID, BanksNodeId, StringComparison.OrdinalIgnoreCase) ||
                string.Equals(controlID, CommandOpenEditor, StringComparison.OrdinalIgnoreCase))
            {
                OpenBank(_selectedBankRelativePath);
                return;
            }

            if (string.Equals(controlID, CommandOpenSelected, StringComparison.OrdinalIgnoreCase))
            {
                OpenBank(_contextBankRelativePath);
                return;
            }

            if (string.Equals(controlID, CommandCreateBank, StringComparison.OrdinalIgnoreCase))
            {
                CreateBankFromCurrentContext();
                return;
            }

            if (string.Equals(controlID, CommandRenameBank, StringComparison.OrdinalIgnoreCase))
            {
                RenameContextBank();
                return;
            }

            if (string.Equals(controlID, CommandDeleteBank, StringComparison.OrdinalIgnoreCase))
            {
                DeleteContextBank();
                return;
            }

            if (string.Equals(controlID, CommandRefreshBanks, StringComparison.OrdinalIgnoreCase))
            {
                RefreshTree();
                return;
            }

            string relativePath;
            if (_fileNodePaths.TryGetValue(controlID, out relativePath))
            {
                OpenBank(relativePath);
            }
        }

        public IList<MenuCommand> GetContextMenu(string controlID)
        {
            List<MenuCommand> commands = new List<MenuCommand>();
            _contextBankRelativePath = null;
            _contextDirectoryRelativePath = string.Empty;

            string relativePath;
            if (_fileNodePaths.TryGetValue(controlID, out relativePath))
            {
                _contextBankRelativePath = relativePath;
                _contextDirectoryRelativePath = GetRelativeDirectory(relativePath);
                commands.Add(new MenuCommand(CommandOpenSelected, "Open in iMUSE Packer", EditorIconKey));
                commands.Add(MenuCommand.Separator);
                commands.Add(new MenuCommand(CommandCreateBank, "New Bank...", FolderIconKey));
                commands.Add(new MenuCommand(CommandRenameBank, "Rename Bank...", FileIconKey));
                commands.Add(new MenuCommand(CommandDeleteBank, "Delete Bank...", FileIconKey));
                commands.Add(MenuCommand.Separator);
                commands.Add(new MenuCommand(CommandRefreshBanks, "Refresh Banks", FileIconKey));
                return commands;
            }

            if (string.Equals(controlID, RootNodeId, StringComparison.OrdinalIgnoreCase) ||
                string.Equals(controlID, BanksNodeId, StringComparison.OrdinalIgnoreCase))
            {
                commands.Add(new MenuCommand(CommandCreateBank, "New Bank...", FolderIconKey));
                commands.Add(MenuCommand.Separator);
                commands.Add(new MenuCommand(CommandRefreshBanks, "Refresh Banks", FileIconKey));
                return commands;
            }

            commands.Add(new MenuCommand(CommandOpenEditor, "Open iMUSE Packer", EditorIconKey));
            commands.Add(new MenuCommand(CommandCreateBank, "New Bank...", FolderIconKey));
            commands.Add(new MenuCommand(CommandRefreshBanks, "Refresh Banks", FileIconKey));
            return commands;
        }

        public void PropertyChanged(string propertyName, object oldValue)
        {
        }

        public void BeforeSaveGame()
        {
            List<OpenBankDocument> openDocuments = new List<OpenBankDocument>(_openDocuments.Values);
            for (int i = 0; i < openDocuments.Count; ++i)
            {
                SaveDocument(openDocuments[i], false);
            }
        }

        public void RefreshDataFromGame()
        {
            RefreshTree();
        }

        public void GameSettingsChanged()
        {
            RefreshTree();
        }

        public void ToXml(XmlTextWriter writer)
        {
            try
            {
                writer.WriteStartElement(XmlNodeName);
                if (!string.IsNullOrEmpty(_selectedBankRelativePath))
                {
                    writer.WriteAttributeString(XmlSelectedBankAttribute, _selectedBankRelativePath);
                }
                writer.WriteEndElement();
            }
            catch
            {
            }
        }

        public void FromXml(XmlNode node)
        {
            _selectedBankRelativePath = null;

            try
            {
                if (node == null)
                {
                    return;
                }

                XmlNode configNode = node.SelectSingleNode(XmlNodeName);
                if (configNode == null)
                {
                    return;
                }

                TryLoadLegacyXmlSplitterDistance(configNode);

                string selectedBank = ReadAttribute(configNode, XmlSelectedBankAttribute);
                if (!string.IsNullOrEmpty(selectedBank))
                {
                    _selectedBankRelativePath = NormalizeRelativePath(selectedBank);
                }

                RefreshTree();
            }
            catch
            {
                RefreshTree();
            }
        }

        public void EditorShutdown()
        {
            if (_isShutdown)
            {
                return;
            }

            _isShutdown = true;

            try
            {
                if (_mainMenuCommands != null)
                {
                    _gui.RemoveMenuItems(_mainMenuCommands);
                }
            }
            catch
            {
            }

            List<OpenBankDocument> openDocuments = new List<OpenBankDocument>(_openDocuments.Values);
            for (int i = 0; i < openDocuments.Count; ++i)
            {
                try
                {
                    CloseDocument(openDocuments[i]);
                }
                catch
                {
                }
            }
        }

        private void RefreshTree()
        {
            _fileNodePaths.Clear();
            _knownBankPaths.Clear();
            _contextBankRelativePath = null;
            _contextDirectoryRelativePath = string.Empty;

            _gui.ProjectTree.RemoveAllChildNodes(this, RootNodeId);
            AddBanksContainerNode();

            string projectDirectory = GetProjectDirectory();
            if (string.IsNullOrEmpty(projectDirectory) || !Directory.Exists(projectDirectory))
            {
                _selectedBankRelativePath = null;
                return;
            }

            try
            {
                List<string> discoveredBanks = DiscoverBanks(projectDirectory);
                discoveredBanks.Sort(StringComparer.OrdinalIgnoreCase);
                for (int i = 0; i < discoveredBanks.Count; ++i)
                {
                    AddTreePath(discoveredBanks[i]);
                    _knownBankPaths.Add(discoveredBanks[i]);
                }
            }
            catch (Exception ex)
            {
                ShowError("Refresh Banks", ex.Message);
            }

            if (!ContainsKnownBank(_selectedBankRelativePath))
            {
                _selectedBankRelativePath = _knownBankPaths.Count > 0 ? _knownBankPaths[0] : null;
            }
        }

        private void AddBanksContainerNode()
        {
            _gui.ProjectTree.StartFromNode(this, RootNodeId);
            _gui.ProjectTree.AddTreeLeaf(this, BanksNodeId, "Banks", FolderIconKey, false);
        }

        private void AddTreePath(string relativePath)
        {
            string normalizedPath = NormalizeRelativePath(relativePath);
            if (string.IsNullOrEmpty(normalizedPath))
            {
                return;
            }

            _gui.ProjectTree.StartFromNode(this, BanksNodeId);
            string fileNodeId = FileNodePrefix + normalizedPath;
            _gui.ProjectTree.AddTreeLeaf(this, fileNodeId, BuildBankNodeLabel(normalizedPath), FileIconKey, false);
            _fileNodePaths[fileNodeId] = normalizedPath;
        }

        private List<string> DiscoverBanks(string projectDirectory)
        {
            List<string> banks = new List<string>();
            string[] files = Directory.GetFiles(projectDirectory, "*.ims", SearchOption.AllDirectories);
            for (int i = 0; i < files.Length; ++i)
            {
                string relativePath = NormalizeRelativePath(MakeRelativePath(projectDirectory, files[i]));
                if (!ShouldSkipRelativePath(relativePath))
                {
                    banks.Add(relativePath);
                }
            }
            return banks;
        }

        private void OpenBank(string relativePath)
        {
            if (string.IsNullOrEmpty(relativePath))
            {
                if (_knownBankPaths.Count == 0)
                {
                    return;
                }

                relativePath = _knownBankPaths[0];
            }

            relativePath = NormalizeRelativePath(relativePath);
            _selectedBankRelativePath = relativePath;

            OpenBankDocument existingDocument;
            if (_openDocuments.TryGetValue(relativePath, out existingDocument))
            {
                _gui.AddOrShowPane(existingDocument.Document);
                _gui.ProjectTree.SelectNode(this, FileNodePrefix + relativePath);
                return;
            }

            string fullPath = BuildFullPath(relativePath);
            ImuseBankProjectModel model;
            try
            {
                model = ImuseBankCodec.LoadFromFile(fullPath, relativePath);
            }
            catch (Exception ex)
            {
                ShowError("Open Bank", ex.Message);
                return;
            }

            OpenBankDocument documentContext = new OpenBankDocument();
            documentContext.RelativePath = relativePath;
            documentContext.Pane = new ImuseEditorPane();
            documentContext.Pane.MainSplitterDistance = _mainSplitterDistance;
            documentContext.Pane.LoadBank(model);
            documentContext.Pane.RefreshRequested += delegate(object sender, EventArgs e) { RefreshDocument(documentContext); };
            documentContext.Pane.SaveRequested += delegate(object sender, EventArgs e) { SaveDocument(documentContext, true); };
            documentContext.Pane.DirtyStateChanged += delegate(object sender, EventArgs e) { UpdateDocumentCaption(documentContext); };
            documentContext.Pane.LayoutStateChanged += delegate(object sender, EventArgs e) { RememberPaneLayout(documentContext); };

            documentContext.Document = new ContentDocument(documentContext.Pane, Path.GetFileName(relativePath), this, EditorIconKey);
            documentContext.Document.TreeNodeID = FileNodePrefix + relativePath;
            documentContext.Document.PreferredDockData = new DockData(DockingState.Document, new Rectangle(0, 0, 960, 720));
            documentContext.Document.PanelClosed += delegate(object sender, EventArgs e) { OnDocumentClosed(documentContext); };

            _openDocuments[relativePath] = documentContext;
            UpdateDocumentCaption(documentContext);
            _gui.AddOrShowPane(documentContext.Document);
            _gui.ProjectTree.SelectNode(this, FileNodePrefix + relativePath);
        }

        private void RefreshDocument(OpenBankDocument documentContext)
        {
            RefreshTree();

            if (documentContext == null || documentContext.Pane == null)
            {
                return;
            }

            if (documentContext.Pane.IsDirty)
            {
                documentContext.Pane.ShowStatus("Refreshed the bank tree. This document has unsaved changes and was not reloaded.");
                return;
            }

            try
            {
                ImuseBankProjectModel model = ImuseBankCodec.LoadFromFile(BuildFullPath(documentContext.RelativePath), documentContext.RelativePath);
                documentContext.Pane.LoadBank(model);
                documentContext.Pane.ShowStatus("Reloaded from disk.");
                UpdateDocumentCaption(documentContext);
            }
            catch (Exception ex)
            {
                ShowError("Refresh Bank", ex.Message);
            }
        }

        private void SaveDocument(OpenBankDocument documentContext, bool showStatus)
        {
            if (documentContext == null || documentContext.Pane == null)
            {
                return;
            }

            if (documentContext.Pane.Bank == null)
            {
                return;
            }

            try
            {
                byte[] bytes = documentContext.Pane.BuildBankBytes();
                ImuseBankFileService.SaveBankBytes(BuildFullPath(documentContext.RelativePath), bytes);
                documentContext.Pane.MarkClean(showStatus ? "Saved " + Path.GetFileName(documentContext.RelativePath) + "." : null);
                UpdateDocumentCaption(documentContext);
            }
            catch (Exception ex)
            {
                ShowError("Save Bank", ex.Message);
            }
        }

        private void CreateBankFromCurrentContext()
        {
            string targetDirectory = _contextDirectoryRelativePath;
            if (string.IsNullOrEmpty(targetDirectory))
            {
                targetDirectory = GetRelativeDirectory(_selectedBankRelativePath);
            }

            string requestedName = TextPromptDialog.ShowDialog(
                GetDialogOwner(),
                "Create Bank",
                "Bank file name:",
                "new-bank.ims");

            if (requestedName == null)
            {
                return;
            }

            string fileName = NormalizeBankFileName(requestedName);
            if (fileName == null)
            {
                ShowError("Create Bank", "Please enter a valid bank file name.");
                return;
            }

            string relativePath = CombineRelativePath(targetDirectory, fileName);
            string fullPath = BuildFullPath(relativePath);
            if (File.Exists(fullPath))
            {
                ShowError("Create Bank", "A bank with that name already exists.");
                return;
            }

            try
            {
                ImuseBankFileService.CreateEmptyBank(fullPath);
                RefreshTree();
                OpenBank(relativePath);
            }
            catch (Exception ex)
            {
                ShowError("Create Bank", ex.Message);
            }
        }

        private void RenameContextBank()
        {
            if (string.IsNullOrEmpty(_contextBankRelativePath))
            {
                return;
            }

            string requestedName = TextPromptDialog.ShowDialog(
                GetDialogOwner(),
                "Rename Bank",
                "New bank file name:",
                Path.GetFileName(_contextBankRelativePath));

            if (requestedName == null)
            {
                return;
            }

            string fileName = NormalizeBankFileName(requestedName);
            if (fileName == null)
            {
                ShowError("Rename Bank", "Please enter a valid bank file name.");
                return;
            }

            string sourceRelativePath = _contextBankRelativePath;
            string targetRelativePath = CombineRelativePath(GetRelativeDirectory(sourceRelativePath), fileName);
            if (string.Equals(sourceRelativePath, targetRelativePath, StringComparison.OrdinalIgnoreCase))
            {
                return;
            }

            string sourceFullPath = BuildFullPath(sourceRelativePath);
            string targetFullPath = BuildFullPath(targetRelativePath);
            if (File.Exists(targetFullPath))
            {
                ShowError("Rename Bank", "A bank with that name already exists.");
                return;
            }

            try
            {
                File.Move(sourceFullPath, targetFullPath);

                OpenBankDocument documentContext;
                if (_openDocuments.TryGetValue(sourceRelativePath, out documentContext))
                {
                    _openDocuments.Remove(sourceRelativePath);
                    documentContext.RelativePath = targetRelativePath;
                    documentContext.Pane.SetRelativePath(targetRelativePath);
                    documentContext.Document.TreeNodeID = FileNodePrefix + targetRelativePath;
                    _openDocuments[targetRelativePath] = documentContext;
                    UpdateDocumentCaption(documentContext);
                }

                if (string.Equals(_selectedBankRelativePath, sourceRelativePath, StringComparison.OrdinalIgnoreCase))
                {
                    _selectedBankRelativePath = targetRelativePath;
                }

                RefreshTree();
                OpenBank(targetRelativePath);
            }
            catch (Exception ex)
            {
                ShowError("Rename Bank", ex.Message);
            }
        }

        private void DeleteContextBank()
        {
            if (string.IsNullOrEmpty(_contextBankRelativePath))
            {
                return;
            }

            DialogResult result = MessageBox.Show(
                GetDialogOwner(),
                "Delete bank '" + Path.GetFileName(_contextBankRelativePath) + "'?",
                "Delete Bank",
                MessageBoxButtons.OKCancel,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2);

            if (result != DialogResult.OK)
            {
                return;
            }

            try
            {
                OpenBankDocument documentContext;
                if (_openDocuments.TryGetValue(_contextBankRelativePath, out documentContext))
                {
                    CloseDocument(documentContext);
                }

                File.Delete(BuildFullPath(_contextBankRelativePath));
                if (string.Equals(_selectedBankRelativePath, _contextBankRelativePath, StringComparison.OrdinalIgnoreCase))
                {
                    _selectedBankRelativePath = null;
                }

                RefreshTree();
            }
            catch (Exception ex)
            {
                ShowError("Delete Bank", ex.Message);
            }
        }

        private void UpdateDocumentCaption(OpenBankDocument documentContext)
        {
            if (documentContext == null || documentContext.Document == null || documentContext.Pane == null)
            {
                return;
            }

            string name = Path.GetFileName(documentContext.RelativePath);
            if (documentContext.Pane.IsDirty)
            {
                name += "*";
            }

            documentContext.Document.Name = name;
        }

        private void OnDocumentClosed(OpenBankDocument documentContext)
        {
            if (documentContext == null)
            {
                return;
            }

            RememberPaneLayout(documentContext);

            OpenBankDocument existingContext;
            if (_openDocuments.TryGetValue(documentContext.RelativePath, out existingContext) &&
                object.ReferenceEquals(existingContext, documentContext))
            {
                _openDocuments.Remove(documentContext.RelativePath);
            }

            documentContext.Pane = null;
            documentContext.Document = null;
        }

        private void CloseDocument(OpenBankDocument documentContext)
        {
            if (documentContext == null)
            {
                return;
            }

            RememberPaneLayout(documentContext);

            ContentDocument document = documentContext.Document;
            documentContext.Document = null;
            documentContext.Pane = null;
            _openDocuments.Remove(documentContext.RelativePath);

            if (document != null)
            {
                try
                {
                    _gui.RemovePaneIfExists(document);
                }
                catch
                {
                }

                try
                {
                    document.Dispose();
                }
                catch
                {
                }
            }
        }

        private bool ContainsKnownBank(string relativePath)
        {
            if (string.IsNullOrEmpty(relativePath))
            {
                return false;
            }

            for (int i = 0; i < _knownBankPaths.Count; ++i)
            {
                if (string.Equals(_knownBankPaths[i], relativePath, StringComparison.OrdinalIgnoreCase))
                {
                    return true;
                }
            }

            return false;
        }

        private string GetProjectDirectory()
        {
            if (_editor.CurrentGame == null || string.IsNullOrEmpty(_editor.CurrentGame.DirectoryPath))
            {
                return null;
            }

            return Path.GetFullPath(_editor.CurrentGame.DirectoryPath);
        }

        private string BuildFullPath(string relativePath)
        {
            string projectDirectory = GetProjectDirectory();
            return Path.Combine(projectDirectory, NormalizeRelativePath(relativePath).Replace('/', Path.DirectorySeparatorChar));
        }

        private static string BuildBankNodeLabel(string relativePath)
        {
            string normalizedPath = NormalizeRelativePath(relativePath);
            return string.IsNullOrEmpty(normalizedPath) ? "(unnamed bank)" : normalizedPath;
        }

        private static string NormalizeRelativePath(string path)
        {
            if (string.IsNullOrEmpty(path))
            {
                return path;
            }

            return path
                .Replace(Path.DirectorySeparatorChar, '/')
                .Replace(Path.AltDirectorySeparatorChar, '/')
                .TrimStart('/');
        }

        private static string NormalizeBankFileName(string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                return null;
            }

            string trimmed = value.Trim();
            if (trimmed.Length == 0)
            {
                return null;
            }

            if (trimmed.IndexOf(Path.DirectorySeparatorChar) >= 0 || trimmed.IndexOf(Path.AltDirectorySeparatorChar) >= 0)
            {
                return null;
            }

            char[] invalidCharacters = Path.GetInvalidFileNameChars();
            for (int i = 0; i < invalidCharacters.Length; ++i)
            {
                if (trimmed.IndexOf(invalidCharacters[i]) >= 0)
                {
                    return null;
                }
            }

            if (!trimmed.EndsWith(".ims", StringComparison.OrdinalIgnoreCase))
            {
                trimmed += ".ims";
            }

            return trimmed;
        }

        private static string GetRelativeDirectory(string relativePath)
        {
            string normalized = NormalizeRelativePath(relativePath);
            if (string.IsNullOrEmpty(normalized))
            {
                return string.Empty;
            }

            int separatorIndex = normalized.LastIndexOf('/');
            return separatorIndex >= 0 ? normalized.Substring(0, separatorIndex) : string.Empty;
        }

        private static string CombineRelativePath(string directory, string fileName)
        {
            string normalizedDirectory = NormalizeRelativePath(directory);
            return string.IsNullOrEmpty(normalizedDirectory)
                ? fileName
                : normalizedDirectory + "/" + fileName;
        }

        private static string MakeRelativePath(string rootDirectory, string fullPath)
        {
            string normalizedRoot = rootDirectory;
            if (!normalizedRoot.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal))
            {
                normalizedRoot += Path.DirectorySeparatorChar;
            }

            Uri rootUri = new Uri(normalizedRoot);
            Uri fullUri = new Uri(fullPath);
            string relativePath = Uri.UnescapeDataString(rootUri.MakeRelativeUri(fullUri).ToString());
            return relativePath.Replace('/', Path.DirectorySeparatorChar);
        }

        private static bool ShouldSkipRelativePath(string relativePath)
        {
            string[] segments = NormalizeRelativePath(relativePath).Split(new[] { '/' }, StringSplitOptions.RemoveEmptyEntries);
            for (int i = 0; i < segments.Length - 1; ++i)
            {
                for (int ignoredIndex = 0; ignoredIndex < IgnoredDirectories.Length; ++ignoredIndex)
                {
                    if (string.Equals(segments[i], IgnoredDirectories[ignoredIndex], StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        private void RememberPaneLayout(OpenBankDocument documentContext)
        {
            if (documentContext == null || documentContext.Pane == null)
            {
                return;
            }

            if (documentContext.Pane.MainSplitterDistance > 0)
            {
                _mainSplitterDistance = documentContext.Pane.MainSplitterDistance;
                SavePersistedMainSplitterDistance(_mainSplitterDistance);
            }
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

            return DefaultMainSplitterDistance;
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

        private void TryLoadLegacyXmlSplitterDistance(XmlNode configNode)
        {
            if (configNode == null)
            {
                return;
            }

            int splitterDistance;
            if (int.TryParse(ReadAttribute(configNode, XmlMainSplitterDistanceAttribute), out splitterDistance) &&
                splitterDistance > 0)
            {
                _mainSplitterDistance = splitterDistance;
                SavePersistedMainSplitterDistance(splitterDistance);
            }
        }

        private IWin32Window GetDialogOwner()
        {
            return null;
        }

        private void ShowError(string title, string message)
        {
            MessageBox.Show(
                GetDialogOwner(),
                message,
                title,
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }

        private static string ReadAttribute(XmlNode node, string attributeName)
        {
            XmlAttribute attribute = node.Attributes != null ? node.Attributes[attributeName] : null;
            return attribute != null ? attribute.Value : null;
        }
    }
}
