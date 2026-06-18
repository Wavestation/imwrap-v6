using System;
using System.Drawing;
using System.Windows.Forms;

namespace AgsImuse.Editor
{
    internal sealed class TextPromptDialog : Form
    {
        private readonly TextBox _valueTextBox;

        private TextPromptDialog(string title, string prompt, string initialValue)
        {
            Text = title;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            StartPosition = FormStartPosition.CenterParent;
            MinimizeBox = false;
            MaximizeBox = false;
            ShowInTaskbar = false;
            ClientSize = new Size(420, 120);

            Label promptLabel = new Label();
            promptLabel.AutoSize = true;
            promptLabel.Location = new Point(12, 12);
            promptLabel.Text = prompt;
            Controls.Add(promptLabel);

            _valueTextBox = new TextBox();
            _valueTextBox.Location = new Point(12, 38);
            _valueTextBox.Size = new Size(396, 23);
            _valueTextBox.Text = initialValue ?? string.Empty;
            Controls.Add(_valueTextBox);

            Button okButton = new Button();
            okButton.Text = "OK";
            okButton.DialogResult = DialogResult.OK;
            okButton.Location = new Point(252, 78);
            okButton.Size = new Size(75, 28);
            Controls.Add(okButton);

            Button cancelButton = new Button();
            cancelButton.Text = "Cancel";
            cancelButton.DialogResult = DialogResult.Cancel;
            cancelButton.Location = new Point(333, 78);
            cancelButton.Size = new Size(75, 28);
            Controls.Add(cancelButton);

            AcceptButton = okButton;
            CancelButton = cancelButton;
        }

        public string Value
        {
            get { return _valueTextBox.Text; }
        }

        public static string ShowDialog(IWin32Window owner, string title, string prompt, string initialValue)
        {
            using (TextPromptDialog dialog = new TextPromptDialog(title, prompt, initialValue))
            {
                return dialog.ShowDialog(owner) == DialogResult.OK ? dialog.Value : null;
            }
        }
    }
}
