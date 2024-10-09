import * as monaco from "../../vendor/monaco-editor/esm/vs/editor/editor.main.js";
import { configureMonaco } from "./tau5_monaco_config";

export default class Tau5Editor {
  #id;
  #monacoEditor;
  #handlers;

  constructor(code, language, id, monacoContainer, handlers = {}) {
    this.#id = id;
    this.#handlers = handlers;
    configureMonaco(monaco); // Configure Monaco only once
    this.#monacoEditor = this.#initMonacoEditor(
      monacoContainer,
      language,
      code
    );
  }

  getCode() {
    return this.#monacoEditor.getValue();
  }

  setCode(code) {
    this.#monacoEditor.setValue(code);
  }

  dispose() {
    this.#monacoEditor.dispose();
  }

  #initMonacoEditor(monacoContainer, language, code) {
    const monacoEditor = monaco.editor.create(monacoContainer, {
      theme: "tau5-dark",
      value: code,
      language: language,
      matchBrackets: true,
      bracketPairColorization: { enabled: true },
      scrollbar: { vertical: "visible" },
      // autoHeight: true,
      minimap: {
        enabled: false,
      },
      automaticLayout: true,
      scrollBeyondLastLine: false,
    });

    monacoEditor.getDomNode().addEventListener(
      "wheel",
      function (e) {
        0 - window.scrollBy(0, e.deltaYy);
      },
      { passive: false }
    );

    const autoResizeMonacoEditor = (mon) => {
      const lineHeight = mon.getOption(monaco.editor.EditorOption.lineHeight);
      const lineCount = mon.getModel().getLineCount();
      const contentHeight = lineHeight * lineCount;

      mon.layout({
        width: monacoContainer.clientWidth,
        height: contentHeight,
      });
    };

    monacoEditor.onDidChangeModelContent((event) => {
      autoResizeMonacoEditor(monacoEditor);
      this.#handlers.onDidChangeModelContent?.(this.#id, event); // Send the editor ID and the event
    });
    autoResizeMonacoEditor(monacoEditor);
    return monacoEditor;
  }
}
