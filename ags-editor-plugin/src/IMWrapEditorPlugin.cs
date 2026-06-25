using System;
using AGS.Types;

namespace AgsIMWrap.Editor
{
    [RequiredAGSVersion("3.6.2.0")]
    public sealed class IMWrapEditorPlugin : IAGSEditorPlugin
    {
        private readonly IMWrapEditorComponent _component;

        public IMWrapEditorPlugin(IAGSEditor editor)
        {
            if (editor == null)
            {
                throw new ArgumentNullException("editor");
            }

            _component = new IMWrapEditorComponent(editor);
            editor.AddComponent(_component);
        }

        public void Dispose()
        {
            try
            {
                _component.EditorShutdown();
            }
            catch
            {
            }
        }
    }
}
