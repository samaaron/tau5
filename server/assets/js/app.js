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

let Hooks = {
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
    
    render() {
      if (!this.gl || !this.program) return;
      
      const canvas = this.el;
      const gl = this.gl;
      
      // Check if panel is visible (for performance)
      const isVisible = canvas.offsetParent !== null && 
                       canvas.style.visibility !== 'hidden';
      
      if (!isVisible) {
        // Skip rendering but keep the loop going
        this.animationFrame = requestAnimationFrame(() => this.render());
        return;
      }
      
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
      
      // Set uniforms
      const time = (Date.now() - this.startTime) / 1000.0;
      const timeLocation = gl.getUniformLocation(this.program, 'time');
      const resolutionLocation = gl.getUniformLocation(this.program, 'resolution');
      
      if (timeLocation) gl.uniform1f(timeLocation, time);
      if (resolutionLocation) gl.uniform2f(resolutionLocation, canvas.width, canvas.height);
      
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      
      this.animationFrame = requestAnimationFrame(() => this.render());
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
      
      const getPosition = (e) => {
        const rect = container.getBoundingClientRect();
        if (direction === "horizontal") {
          return (e.clientX - rect.left) / rect.width;
        } else {
          return (e.clientY - rect.top) / rect.height;
        }
      };
      
      const onMove = (e) => {
        if (!dragging) return;
        e.preventDefault();
        
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
        dragging = false;
        
        splitter.classList.remove("dragging");
        document.body.classList.remove("dragging");
        document.removeEventListener("mousemove", onMove);
        document.removeEventListener("mouseup", onEnd);
        
        const ratio = getPosition(e);
        this.pushEvent("resize_split", { id: splitId, ratio: ratio });
      };
      
      const onStart = (e) => {
        e.preventDefault();
        dragging = true;
        
        splitter.classList.add("dragging");
        document.body.classList.add("dragging");
        document.addEventListener("mousemove", onMove);
        document.addEventListener("mouseup", onEnd);
      };
      
      splitter.addEventListener("mousedown", onStart);
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
