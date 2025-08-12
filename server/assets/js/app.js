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
  
  Splitter: {
    mounted() {
      try {
        const splitter = this.el;
        const container = splitter.parentElement;
        const prev = splitter.previousElementSibling;
        const next = splitter.nextElementSibling;
        const nodeId = splitter.dataset.nodeId;
        const orientation = splitter.dataset.orientation;
        
        // Validate elements exist
        if (!container || !prev || !next) {
          console.error('Splitter: Missing required elements');
          return;
        }
        
        let dragging = false;
        let currentRatio = 0.5;
        
        // Simple position getter
        const getEventPos = (e) => {
          if (e.touches && e.touches.length > 0) {
            return orientation === "horizontal" ? e.touches[0].clientX : e.touches[0].clientY;
          }
          return orientation === "horizontal" ? e.clientX : e.clientY;
        };
        
        // Update layout immediately without RAF for mobile
        const updateLayout = (ratio) => {
          if (!prev || !next) return;
          
          // Ensure ratio is valid - match server-side clamping
          ratio = Math.min(0.8, Math.max(0.2, ratio));
          if (isNaN(ratio)) ratio = 0.5;
          
          prev.style.flex = `0 0 ${ratio * 100}%`;
          next.style.flex = `1 1 ${(1 - ratio) * 100}%`;
          currentRatio = ratio;
        };
        
        const onMove = (e) => {
          if (!dragging) return;
          
          try {
            e.preventDefault();
            
            const rect = container.getBoundingClientRect();
            const pos = getEventPos(e);
            const size = orientation === "horizontal" ? rect.width : rect.height;
            const start = orientation === "horizontal" ? rect.left : rect.top;
            
            let ratio = (pos - start) / size;
            updateLayout(ratio);
          } catch (err) {
            console.error('Splitter onMove error:', err);
          }
        };
        
        const onEnd = (e) => {
          if (!dragging) return;
          
          try {
            dragging = false;
            
            // Clean up
            document.removeEventListener("mousemove", onMove);
            document.removeEventListener("mouseup", onEnd);
            document.removeEventListener("touchmove", onMove);
            document.removeEventListener("touchend", onEnd);
            document.removeEventListener("touchcancel", onEnd);
            
            splitter.classList.remove("dragging");
            document.body.classList.remove("dragging");
            
            // Send final ratio to server
            if (this.pushEvent) {
              this.pushEvent("resize_split", { id: nodeId, ratio: currentRatio });
            }
          } catch (err) {
            console.error('Splitter onEnd error:', err);
          }
        };
        
        const onStart = (e) => {
          try {
            e.preventDefault();
            dragging = true;
            
            splitter.classList.add("dragging");
            document.body.classList.add("dragging");
            
            // Use passive: false only for touchmove
            document.addEventListener("mousemove", onMove);
            document.addEventListener("mouseup", onEnd);
            document.addEventListener("touchmove", onMove, { passive: false });
            document.addEventListener("touchend", onEnd);
            document.addEventListener("touchcancel", onEnd);
          } catch (err) {
            console.error('Splitter onStart error:', err);
          }
        };
        
        // Add event listeners
        splitter.addEventListener("mousedown", onStart);
        splitter.addEventListener("touchstart", onStart, { passive: false });
        
      } catch (err) {
        console.error('Splitter mount error:', err);
      }
    },
    
    destroyed() {
      // Clean up if needed
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
