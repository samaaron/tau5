import Tau5Editor from "../tau5_editor";

const Tau5EditorHook = {
  mounted() {
    const handleCodeChange = (editorId, event) => {
      editorId = "flibble"
      const changes = event.changes;
      this.pushEvent("update_code_diff", { id: editorId, changes });

    };
    // const path = this.el.dataset.path;
    const run_button_id = this.el.dataset;
    const language = this.el.dataset.language;
    const editor_id = this.el.dataset.editorId;
    const container = this.el.querySelector("[monaco-code-editor]");
    const code = this.el.dataset.content;

    this.editor = new Tau5Editor(code, language, editor_id, container, {
      onDidChangeModelContent: handleCodeChange,
    });
  },

  destroyed() {
    if (this.editor) this.editor.dispose();
  },
};

export default Tau5EditorHook;
