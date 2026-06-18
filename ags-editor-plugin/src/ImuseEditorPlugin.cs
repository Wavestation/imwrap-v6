using System;
using AGS.Types;

namespace AgsImuse.Editor
{
    [RequiredAGSVersion("3.6.2.0")]
    public sealed class ImuseEditorPlugin : IAGSEditorPlugin
    {
        private readonly ImuseEditorComponent _component;

        public ImuseEditorPlugin(IAGSEditor editor)
        {
            if (editor == null)
            {
                throw new ArgumentNullException("editor");
            }

            _component = new ImuseEditorComponent(editor);
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
