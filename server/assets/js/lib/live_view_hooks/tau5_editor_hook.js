import Tau5Editor from "../tau5_editor";
//dslkfsdlf
const Tau5EditorHook = {
  mounted() {
    // const path = this.el.dataset.path;
    const language = this.el.dataset.language;
    const editor_id = this.el.dataset.editorId;
    const container = this.el.querySelector("[monaco-code-editor]");
    const code = this.el.dataset.content;

    this.editor = new Tau5Editor(code, language, editor_id, container);
  },

  destroyed() {
    if (this.editor) this.editor.dispose();
  },
};

export default Tau5EditorHook;
