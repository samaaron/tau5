// If you want to use Phoenix channels, run `mix help phx.gen.channel`
// to get started and then uncomment the line below.
// import "./user_socket.js"

// You can include dependencies in two ways.
//
// The simplest option is to put them in assets/vendor and
// import them using relative paths:
//
//     import "../vendor/some-package.js"
//
// Alternatively, you can `npm install some-package --prefix assets` and import
// them using a path starting with the package name:
//
//     import "some-package"
//

// Include phoenix_html to handle method=PUT/DELETE in forms and buttons.
import "phoenix_html";
// Establish Phoenix Socket and LiveView configuration.
import { Socket } from "phoenix";
import { LiveSocket } from "phoenix_live_view";
import topbar from "../vendor/topbar";
import { Tau5Shader } from "./lib/tau5_shader.js";
import { MonacoEditor } from "./lib/monaco_hook.js";

let Hooks = {
  MonacoEditor: MonacoEditor,
  LuaShell: {
    mounted() {
      this.setupResize();
      this.setupHistory();

      this.handleEvent("focus_input", () => {
        const input = document.getElementById("lua-shell-input");
        if (input) {
          input.focus();
        }
      });

      // Scroll to bottom when new output is added
      this.handleEvent("scroll_to_bottom", () => {
        const output = document.getElementById("shell-output");
        if (output) {
          output.scrollTop = output.scrollHeight;
        }
      });

      // Clear input after command execution
      this.handleEvent("clear_input", () => {
        const input = document.getElementById("lua-shell-input");
        if (input) {
          input.value = "";
        }
      });

      // Handle console toggle
      this.handleEvent("toggle_console", ({visible}) => {
        if (visible) {
          setTimeout(() => {
            const input = document.getElementById("lua-shell-input");
            if (input) input.focus();
          }, 100);
        }
      });
    },

    setupHistory() {
      const MAX_HISTORY = 100;
      const STORAGE_KEY = 'tau5-lua-history';

      this.history = [];
      try {
        const stored = localStorage.getItem(STORAGE_KEY);
        if (stored) {
          this.history = JSON.parse(stored);
          if (!Array.isArray(this.history)) {
            this.history = [];
          } else {
            this.history = this.history.slice(0, MAX_HISTORY);
          }
        }
      } catch (e) {
        console.warn('Failed to load command history:', e);
        this.history = [];
      }

      this.historyIndex = -1;
      this.tempInput = "";

      const input = document.getElementById("lua-shell-input");
      if (!input) return;

      this.historyKeydownHandler = (e) => {
        if (e.key === 'ArrowUp') {
          e.preventDefault();
          this.navigateHistory(1);
        } else if (e.key === 'ArrowDown') {
          e.preventDefault();
          this.navigateHistory(-1);
        } else if (e.key === 'Enter') {
          const command = input.value.trim();
          if (command) {
            this.addToHistory(command);
          }
        }
      };

      this.historyInputHandler = () => {
        if (this.historyIndex === -1) {
          this.tempInput = input.value;
        }
      };

      input.addEventListener('keydown', this.historyKeydownHandler);
      input.addEventListener('input', this.historyInputHandler);
    },

    navigateHistory(direction) {
      const input = document.getElementById("lua-shell-input");
      if (!input || this.history.length === 0) return;

      if (this.historyIndex === -1 && direction > 0) {
        this.tempInput = input.value;
      }

      const newIndex = this.historyIndex + direction;

      if (newIndex < -1) return;
      if (newIndex >= this.history.length) return;

      this.historyIndex = newIndex;

      if (this.historyIndex === -1) {
        input.value = this.tempInput;
      } else {
        input.value = this.history[this.historyIndex];
      }

      input.setSelectionRange(input.value.length, input.value.length);
    },

    addToHistory(command) {
      if (this.history.length > 0 && this.history[0] === command) {
        this.historyIndex = -1;
        return;
      }

      this.history.unshift(command);

      const MAX_HISTORY = 100;
      if (this.history.length > MAX_HISTORY) {
        this.history = this.history.slice(0, MAX_HISTORY);
      }

      try {
        localStorage.setItem('tau5-lua-history', JSON.stringify(this.history));
      } catch (e) {
        console.warn('Failed to save command history:', e);
      }

      this.historyIndex = -1;
      this.tempInput = "";
    },

    setupResize() {
      const container = this.el;
      const handle = document.getElementById("shell-resize-handle");
      if (!handle) return;

      let isResizing = false;
      let startY = 0;
      let startHeight = 0;

      const startResize = (e) => {
        isResizing = true;
        startY = e.clientY;
        startHeight = container.offsetHeight;

        // Prevent text selection while dragging
        document.body.style.userSelect = 'none';
        document.body.style.cursor = 'ns-resize';

        // Add overlay to prevent iframe interference
        const overlay = document.createElement('div');
        overlay.id = 'resize-overlay';
        overlay.style.position = 'fixed';
        overlay.style.top = '0';
        overlay.style.left = '0';
        overlay.style.right = '0';
        overlay.style.bottom = '0';
        overlay.style.zIndex = '9998';
        overlay.style.cursor = 'ns-resize';
        document.body.appendChild(overlay);
      };

      const doResize = (e) => {
        if (!isResizing) return;

        const deltaY = e.clientY - startY;
        const newHeight = Math.min(
          Math.max(startHeight + deltaY, 150), // Min height 150px
          window.innerHeight * 0.8 // Max 80% of viewport
        );

        container.style.height = newHeight + 'px';

        // Store the height for persistence (optional)
        localStorage.setItem('tau5-console-height', newHeight);
      };

      const stopResize = () => {
        if (!isResizing) return;

        isResizing = false;
        document.body.style.userSelect = '';
        document.body.style.cursor = '';

        // Remove overlay
        const overlay = document.getElementById('resize-overlay');
        if (overlay) overlay.remove();
      };

      // Attach event listeners
      handle.addEventListener('mousedown', startResize);
      document.addEventListener('mousemove', doResize);
      document.addEventListener('mouseup', stopResize);

      // Load saved height
      const savedHeight = localStorage.getItem('tau5-console-height');
      if (savedHeight) {
        container.style.height = savedHeight + 'px';
      }

      // Store cleanup function for unmount
      this.resizeCleanup = () => {
        handle.removeEventListener('mousedown', startResize);
        document.removeEventListener('mousemove', doResize);
        document.removeEventListener('mouseup', stopResize);
      };
    },

    destroyed() {
      // Clean up resize handlers
      if (this.resizeCleanup) {
        this.resizeCleanup();
      }

      // Clean up history event listeners
      const input = document.getElementById("lua-shell-input");
      if (input && this.historyKeydownHandler) {
        input.removeEventListener('keydown', this.historyKeydownHandler);
        input.removeEventListener('input', this.historyInputHandler);
      }
    }
  },

  Tau5ShaderCanvas: {
    mounted() {
      this.shader = new Tau5Shader(this.el);
      this.shader.init().then((success) => {
        if (success) {
          this.shader.start();
        }
      });
    },
    destroyed() {
      if (this.shader) {
        this.shader.destroy();
      }
    }
  },

  ShaderCanvas: {
    mounted() {
      this.initShader();
    },

    updated() {
      // Re-init if shader isn't running
      if (!this.animationFrame) {
        this.initShader();
      }
    },

    initShader() {
      const vertexId = this.el.dataset.vertexShaderId;
      const fragmentId = this.el.dataset.fragmentShaderId;

      const vertexSource = document.getElementById(vertexId)?.textContent;
      const fragmentSource = document.getElementById(fragmentId)?.textContent;

      if (!vertexSource || !fragmentSource) {
        console.error('Shader sources not found for', this.el.id);
        return;
      }

      this.initWebGL(vertexSource, fragmentSource);
    },

    initWebGL(vertexSource, fragmentSource) {
      const canvas = this.el;

      // Stop any existing animation
      if (this.animationFrame) {
        cancelAnimationFrame(this.animationFrame);
        this.animationFrame = null;
      }

      // Try to get WebGL context with performance-conscious settings
      const gl = canvas.getContext('webgl', {
        failIfMajorPerformanceCaveat: false,
        preserveDrawingBuffer: false,
        antialias: false,
        depth: false,
        stencil: false,
        powerPreference: 'low-power'
      }) || canvas.getContext('experimental-webgl');

      if (!gl) {
        console.warn('WebGL not supported, using fallback for panel', canvas.id);
        this.showFallback();
        return;
      }

      // Compile shaders
      const vertexShader = this.compileShader(gl, gl.VERTEX_SHADER, vertexSource);
      const fragmentShader = this.compileShader(gl, gl.FRAGMENT_SHADER, fragmentSource);

      if (!vertexShader || !fragmentShader) return;

      // Link program
      const program = gl.createProgram();
      gl.attachShader(program, vertexShader);
      gl.attachShader(program, fragmentShader);
      gl.linkProgram(program);

      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        console.error('Failed to link program');
        return;
      }

      // Set up geometry
      const vertices = new Float32Array([
        -1.0, -1.0,
         1.0, -1.0,
        -1.0,  1.0,
         1.0,  1.0
      ]);

      const buffer = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);

      const aPos = gl.getAttribLocation(program, 'aPos');
      gl.enableVertexAttribArray(aPos);
      gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

      // Start render loop
      this.gl = gl;
      this.program = program;
      this.startTime = Date.now();

      // Cache uniform locations
      this.timeLocation = gl.getUniformLocation(program, 'time');
      this.resolutionLocation = gl.getUniformLocation(program, 'resolution');

      // Framerate limiting (60 FPS)
      this.targetFPS = 60;
      this.frameInterval = 1000 / this.targetFPS;
      this.lastFrameTime = 0;

      this.render();
    },

    compileShader(gl, type, source) {
      const shader = gl.createShader(type);
      gl.shaderSource(shader, source);
      gl.compileShader(shader);

      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.error('Shader compile error:', gl.getShaderInfoLog(shader));
        return null;
      }

      return shader;
    },

    render(currentTime) {
      // Request next frame immediately
      this.animationFrame = requestAnimationFrame((time) => this.render(time));

      if (!this.gl || !this.program) return;

      const canvas = this.el;
      const gl = this.gl;

      // Check if panel is visible (for performance)
      const isVisible = canvas.offsetParent !== null &&
                       canvas.style.visibility !== 'hidden';

      if (!isVisible) {
        // Skip rendering but keep the loop going
        return;
      }

      // Framerate limiting - only render if enough time has passed
      currentTime = currentTime || 0;
      const elapsed = currentTime - this.lastFrameTime;

      // Only render if enough time has passed (60 FPS = ~16.67ms per frame)
      if (elapsed < this.frameInterval) {
        return;
      }

      // Adjust for any time drift to maintain smooth framerate
      this.lastFrameTime = currentTime - (elapsed % this.frameInterval);

      // Check for context loss
      if (gl.isContextLost()) {
        console.warn('WebGL context lost for', canvas.id);
        this.showFallback();
        return;
      }

      // Resize canvas if needed
      if (canvas.width !== canvas.clientWidth || canvas.height !== canvas.clientHeight) {
        canvas.width = canvas.clientWidth;
        canvas.height = canvas.clientHeight;
        gl.viewport(0, 0, canvas.width, canvas.height);
      }

      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);

      gl.useProgram(this.program);

      // Set uniforms (using cached locations)
      const time = (Date.now() - this.startTime) / 1000.0;

      if (this.timeLocation) gl.uniform1f(this.timeLocation, time);
      if (this.resolutionLocation) gl.uniform2f(this.resolutionLocation, canvas.width, canvas.height);

      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    },

    showFallback() {
      // Replace canvas with a fallback div
      const panelIndex = this.el.dataset.panelIndex || '?';
      const fallback = document.createElement('div');
      fallback.className = 'shader-fallback';
      fallback.style.cssText = `
        width: 100%;
        height: 100%;
        display: flex;
        align-items: center;
        justify-content: center;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        font-size: 24px;
        font-weight: bold;
      `;
      fallback.textContent = `Panel ${panelIndex}`;
      this.el.parentNode.replaceChild(fallback, this.el);
    },

    destroyed() {
      if (this.animationFrame) {
        cancelAnimationFrame(this.animationFrame);
      }

      // Clean up WebGL resources
      if (this.gl && this.program) {
        this.gl.deleteProgram(this.program);
        this.program = null;
      }
    }
  },

  Splitter: {
    mounted() {
      const splitter = this.el;
      const container = splitter.parentElement;
      const direction = splitter.dataset.direction;
      const splitId = splitter.dataset.splitId;

      let dragging = false;
      let touchIdentifier = null;

      const getPosition = (e) => {
        const rect = container.getBoundingClientRect();
        // Handle both mouse and touch events
        const clientX = e.clientX || (e.touches && e.touches[0] && e.touches[0].clientX) ||
                       (e.changedTouches && e.changedTouches[0] && e.changedTouches[0].clientX);
        const clientY = e.clientY || (e.touches && e.touches[0] && e.touches[0].clientY) ||
                       (e.changedTouches && e.changedTouches[0] && e.changedTouches[0].clientY);

        if (direction === "horizontal") {
          return (clientX - rect.left) / rect.width;
        } else {
          return (clientY - rect.top) / rect.height;
        }
      };

      const onMove = (e) => {
        if (!dragging) return;
        e.preventDefault();

        // For touch events, check if it's the same touch that started the drag
        if (e.type === 'touchmove' && touchIdentifier !== null) {
          const touch = Array.from(e.touches).find(t => t.identifier === touchIdentifier);
          if (!touch) return;
        }

        const ratio = Math.min(0.8, Math.max(0.2, getPosition(e)));
        const prev = splitter.previousElementSibling;
        const next = splitter.nextElementSibling;

        if (prev && next) {
          prev.style.flex = `0 0 ${ratio * 100}%`;
          next.style.flex = `1 1 ${(1 - ratio) * 100}%`;
        }
      };

      const onEnd = (e) => {
        if (!dragging) return;

        // For touch events, check if it's the same touch that started the drag
        if (e.type === 'touchend' && touchIdentifier !== null) {
          const touch = Array.from(e.changedTouches).find(t => t.identifier === touchIdentifier);
          if (!touch) return;
        }

        dragging = false;
        touchIdentifier = null;

        splitter.classList.remove("dragging");
        splitter.classList.remove("touch-active");
        document.body.classList.remove("dragging");

        // Remove all event listeners
        document.removeEventListener("mousemove", onMove);
        document.removeEventListener("mouseup", onEnd);
        document.removeEventListener("touchmove", onMove, { passive: false });
        document.removeEventListener("touchend", onEnd);
        document.removeEventListener("touchcancel", onEnd);

        const ratio = getPosition(e);
        this.pushEvent("resize_split", { id: splitId, ratio: ratio });
      };

      const onStart = (e) => {
        e.preventDefault();
        dragging = true;

        // For touch events, store the touch identifier
        if (e.type === 'touchstart') {
          touchIdentifier = e.touches[0].identifier;
          splitter.classList.add("touch-active");
        }

        splitter.classList.add("dragging");
        document.body.classList.add("dragging");

        // Add appropriate move and end listeners based on input type
        if (e.type === 'mousedown') {
          document.addEventListener("mousemove", onMove);
          document.addEventListener("mouseup", onEnd);
        } else if (e.type === 'touchstart') {
          document.addEventListener("touchmove", onMove, { passive: false });
          document.addEventListener("touchend", onEnd);
          document.addEventListener("touchcancel", onEnd);
        }
      };

      // Add both mouse and touch event listeners
      splitter.addEventListener("mousedown", onStart);
      splitter.addEventListener("touchstart", onStart, { passive: false });
    }
  },

  TerminalScroll: {
    mounted() {
      this.handleUpdated = () => {
        this.el.scrollTop = this.el.scrollHeight;
      };
      this.handleUpdated();
    },

    updated() {
      this.handleUpdated();
    }
  },

  ConsoleInput: {
    mounted() {
      this.el.focus();

      if (this.el.tagName === 'TEXTAREA') {
        const len = this.el.value.length;
        this.el.setSelectionRange(len, len);
      }

      this.handleEvent("update_input_value", ({value}) => {
        this.el.value = value;
        this.el.focus();
      });

      this.handleEvent("focus_input", () => {
        setTimeout(() => {
          this.el.focus();
          if (this.el.tagName === 'TEXTAREA') {
            const len = this.el.value.length;
            this.el.setSelectionRange(len, len);
          }
        }, 0);
      });

      this.killRing = "";

      this.handleKeydown = (e) => {
        if (e.key === 'Enter') {
          if (e.altKey) {
            e.preventDefault();
            this.pushEvent("handle_keydown", {key: "force_execute"});
            return;
          }

          if (this.el.tagName === 'INPUT') {
            return;
          }

          if (!e.shiftKey) {
            e.preventDefault();
            const start = this.el.selectionStart;
            const end = this.el.selectionEnd;
            const value = this.el.value;

            this.el.value = value.slice(0, start) + '\n' + value.slice(end);
            const newPos = start + 1;
            this.el.setSelectionRange(newPos, newPos);

            this.el.dispatchEvent(new Event('input', { bubbles: true }));

            setTimeout(() => this.el.form.requestSubmit(), 0);

            return;
          }
        }

        if (e.ctrlKey && e.key === 'j') {
          e.preventDefault();
          this.pushEvent("handle_keydown", {key: "insert_newline"});
          return;
        }

        if (!e.ctrlKey && !e.altKey) return;

        const {value, selectionStart: pos, selectionEnd: end} = this.el;

        const handlers = {
          ctrl: {
            p: () => this.pushEvent("handle_keydown", {key: "ArrowUp"}),
            n: () => this.pushEvent("handle_keydown", {key: "ArrowDown"}),
            f: () => this.setCursor(Math.min(pos + 1, value.length)),
            b: () => this.setCursor(Math.max(pos - 1, 0)),
            a: () => this.setCursor(0),
            e: () => this.setCursor(value.length),
            k: () => {
              this.killRing = value.slice(pos);
              this.updateInput(value.slice(0, pos), pos);
            },
            y: () => {
              if (!this.killRing) return;
              this.updateInput(
                value.slice(0, pos) + this.killRing + value.slice(end),
                pos + this.killRing.length
              );
            },
            g: () => {
              this.pushEvent("handle_keydown", {key: "cancel_multiline"});
              this.updateInput("", 0);
            },
            d: () => {
              if (pos < value.length) {
                this.updateInput(value.slice(0, pos) + value.slice(pos + 1), pos);
              }
            },
            h: () => {
              if (pos > 0) {
                this.updateInput(value.slice(0, pos - 1) + value.slice(end), pos - 1);
              }
            }
          },
          alt: {
            f: () => {
              let i = pos;
              while (i < value.length && /\w/.test(value[i])) i++;
              while (i < value.length && /\s/.test(value[i])) i++;
              this.setCursor(i);
            },
            b: () => {
              let i = pos;
              while (i > 0 && /\s/.test(value[i - 1])) i--;
              while (i > 0 && /\w/.test(value[i - 1])) i--;
              this.setCursor(i);
            },
            a: () => this.el.setSelectionRange(0, value.length)
          }
        };

        const keyMap = e.ctrlKey ? handlers.ctrl : handlers.alt;
        const handler = keyMap[e.key];

        if (handler) {
          e.preventDefault();
          handler();
        }
      };

      this.setCursor = (pos) => {
        this.el.setSelectionRange(pos, pos);
      };

      this.updateInput = (value, cursor) => {
        this.el.value = value;
        this.setCursor(cursor);
        this.el.dispatchEvent(new Event('input', { bubbles: true }));
      };

      this.el.addEventListener("keydown", this.handleKeydown);
    },

    destroyed() {
      this.el.removeEventListener("keydown", this.handleKeydown);
    }
  },

  HydraBackground: {
    mounted() {
      // Store reference to iframe
      this.iframe = document.getElementById("hydra-background");

      // Handle Hydra sketch updates from server
      this.handleEvent("update_hydra_sketch", (payload) => {
        if (this.iframe && this.iframe.contentWindow) {
          // Send message to iframe with new sketch code
          this.iframe.contentWindow.postMessage({
            type: "update_sketch",
            code: payload.code
          }, "*");
        }
      });

      // Optionally handle responses from iframe
      window.addEventListener("message", this.handleMessage.bind(this));
    },

    handleMessage(event) {
      // Only handle messages from our iframe
      if (event.source !== this.iframe.contentWindow) return;

      if (event.data.type === "hydra_ready") {
        console.log("Hydra iframe is ready");
        // Notify server that Hydra is ready if needed
        this.pushEvent("hydra_ready", {});
      } else if (event.data.type === "hydra_error") {
        console.error("Hydra error:", event.data.error);
        // Notify server of errors if needed
        this.pushEvent("hydra_error", {error: event.data.error});
      }
    },

    destroyed() {
      window.removeEventListener("message", this.handleMessage);
    }
  }
};

let csrfToken = document
  .querySelector("meta[name='csrf-token']")
  .getAttribute("content");

let liveSocket = new LiveSocket("/live", Socket, {
  hooks: Hooks,
  longPollFallbackMs: 2500,
  params: { _csrf_token: csrfToken },
});

// Show progress bar on live navigation and form submits
topbar.config({ barColors: { 0: "#29d" }, shadowColor: "rgba(0, 0, 0, .3)" });
window.addEventListener("phx:page-loading-start", (_info) => topbar.show(300));
window.addEventListener("phx:page-loading-stop", (_info) => topbar.hide());

// connect if there are any LiveViews on the page
liveSocket.connect();

// expose liveSocket on window for web console debug logs and latency simulation:
// >> liveSocket.enableDebug()
// >> liveSocket.enableLatencySim(1000)  // enabled for duration of browser session
// >> liveSocket.disableLatencySim()
window.liveSocket = liveSocket;
