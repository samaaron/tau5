// Monaco Editor Hook for LiveView
// Handles initialization and lifecycle of Monaco editor instances

export const MonacoEditor = {
  async mounted() {
    // Set up Monaco environment
    self.MonacoEnvironment = {
      getWorkerUrl: function (_moduleId, label) {
        return '/assets/js/monaco-worker/editor.worker.js';
      }
    };

    try {
      // Dynamically import Monaco
      const monaco = await import('../../vendor/monaco-editor/esm/vs/editor/editor.main.js');

      // Get initial code content
      const initialCode = getInitialCodeForPanel();

      // Define custom transparent theme
      monaco.editor.defineTheme('tau5-transparent', {
        base: 'vs-dark',
        inherit: true,
        rules: [
          { token: 'comment', fontStyle: 'italic' },
          { token: 'comment.line', fontStyle: 'italic' },
          { token: 'comment.block', fontStyle: 'italic' },
        ],
        colors: {
          'editor.background': '#00000000',  // Transparent background
          'editor.lineHighlightBackground': '#ffffff10',  // Subtle line highlight
          'editorLineNumber.foreground': '#858585',
          'editorLineNumber.activeForeground': '#c6c6c6',
          'editorGutter.background': '#00000000',  // Transparent gutter (line numbers area)
          'minimap.background': '#00000000',  // Transparent minimap background
          'minimapSlider.background': '#ffffff20',  // Subtle slider
          'minimapSlider.hoverBackground': '#ffffff30',
          'minimapSlider.activeBackground': '#ffffff40',
          'scrollbar.shadow': '#00000000',  // No scrollbar shadow
          'scrollbarSlider.background': '#ffffff20',  // Semi-transparent scrollbar
          'scrollbarSlider.hoverBackground': '#ffffff30',
          'scrollbarSlider.activeBackground': '#ffffff40',
          'editorOverviewRuler.border': '#00000000',  // No overview ruler border
          'editorOverviewRuler.background': '#00000000',  // Transparent overview ruler background
        }
      });

      // Create the editor
      this.editor = monaco.editor.create(this.el, {
        value: initialCode,
        language: 'lua',
        theme: 'tau5-transparent',
        automaticLayout: true,
        minimap: {
          enabled: true,
          renderCharacters: false,  // Render blocks instead of characters (thinner)
          maxColumn: 80  // Limit minimap width
        },
        fontSize: 18,
        fontWeight: 'bold',
        lineNumbers: 'on',
        roundedSelection: false,
        scrollBeyondLastLine: false,
        readOnly: false,
        cursorStyle: 'line',
        wordWrap: 'off',
        tabSize: 2,
        insertSpaces: true,
        fontFamily: "'Cascadia Code PL', 'Consolas', 'Monaco', monospace",
        fontLigatures: true,
        renderWhitespace: 'selection',
        bracketPairColorization: {
          enabled: true
        },
      });

      // Store reference
      this.monaco = monaco;

      // Handle editor changes
      this.editor.onDidChangeModelContent((e) => {
        const value = this.editor.getValue();
        // Optionally push changes to server
        // this.pushEvent("editor_changed", { value: value });
      });

      // Bind and listen for resize events
      this.handleResize = this.handleResize.bind(this);
      window.addEventListener('resize', this.handleResize);

      console.log('Monaco editor initialized for Lua');
    } catch (error) {
      console.error('Failed to initialize Monaco editor:', error);
      this.showError(error.message);
    }
  },

  updated() {
    // Layout the editor when the component updates
    if (this.editor) {
      this.editor.layout();
    }
  },

  destroyed() {
    // Clean up
    if (this.editor) {
      this.editor.dispose();
      this.editor = null;
    }
    window.removeEventListener('resize', this.handleResize);
  },

  handleResize() {
    if (this.editor) {
      this.editor.layout();
    }
  },

  showError(message) {
    this.el.innerHTML = `
      <div style="
        display: flex;
        align-items: center;
        justify-content: center;
        height: 100%;
        color: #f44336;
        font-family: monospace;
        padding: 20px;
        text-align: center;
      ">
        <div>
          <h3>Monaco Editor Error</h3>
          <p>${message}</p>
        </div>
      </div>
    `;
  }
};

// Helper function to determine language based on panel index
function getLanguageForPanel(index) {
  return 'lua';
}

// Helper function to get initial code for each panel
function getInitialCodeForPanel(index) {
  return `-- Welcome to Tau5!
-- This is a Lua code editor powered by the Monaco Editor.
`;
}
