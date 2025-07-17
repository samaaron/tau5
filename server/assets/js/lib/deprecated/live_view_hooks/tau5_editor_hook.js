import Tau5Editor from "../tau5_editor";

const Tau5EditorHook = {
  mounted() {
    const language = this.el.dataset.language;
    const code = this.el.dataset.content;

    const handleOnDidChangeModelContent = (id, event) => {
      const changes = event.changes;
      this.pushEvent("monaco_on_did_change_model_content", {
        id,
        changes,
      });
    };

    this.editor = new Tau5Editor(this.el, code, language, {
      onDidChangeModelContent: handleOnDidChangeModelContent,
    });
  },

  destroyed() {
    if (this.editor) {
      this.editor.dispose();
    }
    window.removeEventListener("resize", resizeElements);
  },
};

export default Tau5EditorHook;
