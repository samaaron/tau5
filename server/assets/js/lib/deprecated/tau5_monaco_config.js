// monaco_config.js
let monacoConfigured = false;

export const configureMonaco = (monaco) => {
  if (!monacoConfigured) {
    self.MonacoEnvironment = {
      getWorkerUrl: function (moduleId, label) {
        // All workers are built to the same location
        const workerUrl = `${window.location.origin}/assets/js/monaco-worker/editor.worker.js`;
        console.log(`Monaco requesting worker for ${label}, returning: ${workerUrl}`);
        return workerUrl;
      },
      getWorker: function (moduleId, label) {
        // Monaco sometimes expects getWorker instead of getWorkerUrl
        const workerUrl = self.MonacoEnvironment.getWorkerUrl(moduleId, label);
        console.log(`Creating worker from URL: ${workerUrl}`);
        return new Worker(workerUrl);
      },
    };

    monaco.editor.defineTheme("tau5-dark", {
      base: "vs-dark",
      inherit: true,
      rules: [
        { token: "", foreground: "#ededed" },
        { token: "keyword", foreground: "#939bA2" },
        { token: "comment", foreground: "#808080" },
        { token: "number", foreground: "#82AAFF" },
        { token: "string", foreground: "#61CE3C" },
        { token: "keyword", foreground: "#ff1493" },
        { token: "identifier", foreground: "#d3ded3" },
      ],
      colors: {
        "editor.background": "#00000030",
        "editor.selectionBackground": "#FF8C0090",
        "editorBracketMatch.background": "#FF8C0050",
        "editorBracketMatch.border": "#FF8C0050",
        "editorLineNumber.foreground": "#808080",
        "editorBracketHighlight.foreground1": "#808080",
        "editorBracketHighlight.foreground2": "#707070",
        "editorBracketHighlight.foreground3": "#808080",
      },
    });

    monacoConfigured = true;
  }
};