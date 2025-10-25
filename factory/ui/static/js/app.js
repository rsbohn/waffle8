"use strict";

class FactoryUI {
  constructor() {
    this.romForm = document.getElementById("rom-form");
    this.romStatus = document.getElementById("rom-status");
    this.executionStatus = document.getElementById("execution-status");
    this.registerElements = {
      pc: document.getElementById("reg-pc"),
      ac: document.getElementById("reg-ac"),
      link: document.getElementById("reg-link"),
      switch: document.getElementById("reg-switch"),
    };
    this.teleprinterLog = document.getElementById("teleprinter-log");
    this.printerLog = document.getElementById("printer-log");
    this.teleprinterInput = document.getElementById("teleprinter-input");
    this.cycleInput = document.getElementById("cycle-count");
  }

  init() {
    this.bindEvents();
    this.refreshRegisters();
    this.refreshOutput();
  }

  bindEvents() {
    if (this.romForm) {
      this.romForm.addEventListener("submit", (event) => {
        event.preventDefault();
        this.handleRomUpload();
      });
    }

    document.querySelectorAll("[data-action]").forEach((button) => {
      button.addEventListener("click", () => {
        const action = button.getAttribute("data-action");
        switch (action) {
          case "reset":
            this.queueCommand("/reset");
            break;
          case "step":
            this.queueCommand("/trace", { cycles: 1 });
            break;
          case "run":
            this.queueCommand("/trace", { cycles: this.readCycleBudget() });
            break;
          case "halt":
            this.queueCommand("/halt");
            break;
          case "refresh-registers":
            this.refreshRegisters();
            break;
          case "send-input":
            this.sendTeleprinterInput();
            break;
          default:
            console.warn("Unknown action", action);
        }
      });
    });
  }

  async handleRomUpload() {
    const input = /** @type {HTMLInputElement|null} */ (document.getElementById("rom-file"));
    if (!input || !input.files || input.files.length === 0) {
      this.setStatus(this.romStatus, "Select a ROM first.");
      return;
    }

    const file = input.files[0];
    const formData = new FormData();
    formData.append("file", file);

    this.setStatus(this.romStatus, `Uploading ${file.name}…`);
    try {
      const response = await fetch("/loader", {
        method: "POST",
        body: formData,
      });
      if (!response.ok) {
        const text = await response.text();
        throw new Error(text || response.statusText);
      }

      const payload = await response.json();
      this.setStatus(
        this.romStatus,
        `Loaded ${payload.written.length} words` +
          (payload.start ? `; start vector ${payload.start}` : "") +
          ".",
      );
      this.refreshRegisters();
    } catch (error) {
      this.setStatus(this.romStatus, `Upload failed: ${error}`, "error");
      console.error(error);
    }
  }

  readCycleBudget() {
    const value = Number(this.cycleInput?.value ?? 1024);
    if (Number.isNaN(value) || value < 1) {
      return 1024;
    }
    return Math.min(value, 100000);
  }

  async queueCommand(endpoint, body = undefined) {
    this.setStatus(this.executionStatus, "Sending command…");
    try {
      const response = await fetch(endpoint, {
        method: body ? "POST" : "GET",
        headers: body ? { "Content-Type": "application/json" } : undefined,
        body: body ? JSON.stringify(body) : undefined,
      });
      if (!response.ok) {
        const text = await response.text();
        throw new Error(text || response.statusText);
      }

      const payload = await response.json().catch(() => ({}));
      this.setStatus(this.executionStatus, "Command dispatched.");
      if (payload?.registers) {
        this.updateRegisters(payload.registers);
      } else {
        this.refreshRegisters();
      }
      this.refreshOutput();
    } catch (error) {
      this.setStatus(this.executionStatus, `Command failed: ${error}`, "error");
      console.error(error);
    }
  }

  async refreshRegisters() {
    try {
      const response = await fetch("/regs");
      if (!response.ok) {
        throw new Error(response.statusText);
      }
      const data = await response.json();
      this.updateRegisters(data);
      this.setStatus(this.executionStatus, "Registers refreshed.");
    } catch (error) {
      this.setStatus(this.executionStatus, `Unable to refresh registers: ${error}`, "error");
    }
  }

  updateRegisters(data) {
    if (!data) {
      return;
    }
    if (data.pc && this.registerElements.pc) {
      this.registerElements.pc.textContent = data.pc;
    }
    if (data.ac && this.registerElements.ac) {
      this.registerElements.ac.textContent = data.ac;
    }
    if (typeof data.link !== "undefined" && this.registerElements.link) {
      this.registerElements.link.textContent = data.link;
    }
    if (data.switch && this.registerElements.switch) {
      this.registerElements.switch.textContent = data.switch;
    }
  }

  async refreshOutput() {
    await Promise.all([this.loadTeleprinter(), this.loadPrinter()]);
  }

  async loadTeleprinter() {
    try {
      const response = await fetch("/output/teleprinter");
      if (!response.ok) {
        return;
      }
      const payload = await response.json();
      if (payload.text && this.teleprinterLog) {
        this.appendLog(this.teleprinterLog, payload.text);
      }
    } catch (error) {
      console.warn("Teleprinter fetch failed", error);
    }
  }

  async loadPrinter() {
    try {
      const response = await fetch("/output/printer");
      if (!response.ok) {
        return;
      }
      const payload = await response.json();
      if (payload.text && this.printerLog) {
        this.appendLog(this.printerLog, payload.text);
      }
    } catch (error) {
      console.warn("Printer fetch failed", error);
    }
  }

  appendLog(element, text) {
    if (!text) {
      return;
    }
    element.textContent = `${element.textContent || ""}${text}`;
    element.scrollTop = element.scrollHeight;
  }

  async sendTeleprinterInput() {
    const value = this.teleprinterInput?.value;
    if (!value) {
      return;
    }
    try {
      const response = await fetch("/input/keyboard", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ chars: value }),
      });
      if (!response.ok) {
        throw new Error(response.statusText);
      }
      this.teleprinterInput.value = "";
      this.refreshOutput();
    } catch (error) {
      this.setStatus(this.executionStatus, `Keyboard input failed: ${error}`, "error");
    }
  }

  setStatus(container, message, tone = "info") {
    if (!container) {
      return;
    }
    container.textContent = message;
    container.dataset.tone = tone;
  }
}

document.addEventListener("DOMContentLoaded", () => {
  const app = new FactoryUI();
  app.init();
});
