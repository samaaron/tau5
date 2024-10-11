import * as monaco from "../../vendor/monaco-editor/esm/vs/editor/editor.main.js";
import { configureMonaco } from "./tau5_monaco_config";
import Tau5Hydra from "./tau5_hydra";

export default class Tau5Editor {
  #id;
  #container;
  #monacoEditor;
  #handlers;
  #isResizing = false;

  constructor(container, code, language, handlers = {}) {
    configureMonaco(monaco); // Configure Monaco only once
    const monacoContainer = container.querySelector("[monaco-code-editor]");
    const resizeHandle = container.querySelector("[resize-handle]");
    const hydraCanvas = container.querySelector("[hydra]");
    const editor = this.#initMonacoEditor(monacoContainer, language, code);
    this.#id = container.id;
    this.#container = container;
    this.#handlers = handlers;
    this.#monacoEditor = editor;

    const hydra = new Tau5Hydra(hydraCanvas);
    hydra.injectGlobalSketch(`osc(8,-0.5, 1).blend(o0).rotate(-0.5, -0.5).modulate(shape(4).rotate(0.5, 0.5).scale(2).repeatX(2, 2).modulate(o0, () => mouse.x * 0.0005).repeatY(2, 2)).out(o0)
speed = 0.1
`);

    const resizeElementsWithMouseMoveEvent = (e) => {
      const width = e.clientX - initialRect.left;
      const height = e.clientY - initialRect.top;
      if (height > 100) {
        container.style.height = `${height}px`;
      }
    };

    const resizeElementsToBoundingBox = () => {
      const { width, height } = monacoContainer.getBoundingClientRect();

      hydraCanvas.style.width = `${width}px`;
      hydraCanvas.style.height = `${height}px`;
      this.#monacoEditor.layout();
    };

    // Initialize ResizeObserver
    const resizeObserver = new ResizeObserver(() => {
      resizeElementsToBoundingBox();
    });

    // Observe the container for size changes
    resizeObserver.observe(container);

    resizeHandle.addEventListener("mousedown", (e) => {
      e.preventDefault();
      this.#isResizing = true;
    });

    window.addEventListener("mouseup", () => {
      this.#isResizing = false;
    });

    const initialRect = container.getBoundingClientRect();
    window.addEventListener("mousemove", (e) => {
      if (this.#isResizing) {
        resizeElementsWithMouseMoveEvent(e);
      }
    });

    window.addEventListener("mousemove", (e) => {
      if (this.#isResizing) {
        const width = e.clientX - container.getBoundingClientRect().left;
        const height = e.clientY - container.getBoundingClientRect().top;
        if (height > 100) {
          container.style.height = `${height}px`;
          resizeElementsToBoundingBox();
        }
      }
    });

    setTimeout(() => {
      resizeElementsToBoundingBox();
    }, 500);
  }

  getCode() {
    return this.#monacoEditor.getValue();
  }

  setCode(code) {
    this.#monacoEditor.setValue(code);
  }

  setSize(width, height) {
    this.#monacoEditor.layout({
      width: width,
      height: height,
    });
  }

  dispose() {
    this.#monacoEditor.dispose();
    if (this.resizeObserver) {
      this.resizeObserver.disconnect(); // Stop observing when no longer needed
    }
  }

  #initMonacoEditor(monacoContainer, language, code) {
    const monacoEditor = monaco.editor.create(monacoContainer, {
      theme: "tau5-dark",
      value: code,
      language: language,
      matchBrackets: true,
      bracketPairColorization: { enabled: true },
      scrollbar: { vertical: "visible" },
      autoHeight: true,
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
      //autoResizeMonacoEditor(monacoEditor);
      this.#handlers.onDidChangeModelContent?.(this.#id, event); // Send the editor ID and the event
    });

    //autoResizeMonacoEditor(monacoEditor);
    return monacoEditor;
  }
}
