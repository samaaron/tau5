import * as monaco from "../../vendor/monaco-editor/esm/vs/editor/editor.main.js";
import { configureMonaco } from "./tau5_monaco_config";

export default class Tau5Editor {
  #id;
  #monaco_editor;

  constructor(code, language, id, monaco_container) {
    this.#id = id;
    configureMonaco(monaco); // Configure Monaco only once
    this.#monaco_editor = this.#init_monaco_editor(monaco_container, language, code);
  }

  getCode() {
    return this.#monaco_editor.getValue();
  }

  setCode(code) {
    this.#monaco_editor.setValue(code);
  }

  dispose() {
    this.#monaco_editor.dispose();
  }

  #init_monaco_editor(monaco_container, language, code) {
    const monaco_editor = monaco.editor.create(monaco_container, {
      theme: "tau5-dark",
      value: code,
      language: language,
      matchBrackets: true,
      bracketPairColorization: { enabled: true },
      scrollbar: { vertical: "hidden" },
      autoHeight: true,
      minimap: {
        enabled: false,
      },
      scrollBeyondLastLine: false,
    });

    monaco_editor.getDomNode().addEventListener(
      "wheel",
      function (e) {
        0 - window.scrollBy(0, e.deltaYy);
      },
      { passive: false }
    );

    monaco_editor.onDidChangeModelContent(() => {
      autoResizeMonacoEditor(monaco_editor);
    });

    const autoResizeMonacoEditor = (mon) => {
      const lineHeight = mon.getOption(monaco.editor.EditorOption.lineHeight);
      const lineCount = mon.getModel().getLineCount();
      const contentHeight = lineHeight * lineCount;

      mon.layout({
        width: monaco_container.clientWidth,
        height: contentHeight,
      });
    };

    autoResizeMonacoEditor(monaco_editor);
    return monaco_editor;
  }
}
