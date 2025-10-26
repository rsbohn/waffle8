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
      halt: document.getElementById("reg-halt"),
      switch: document.getElementById("reg-switch"),
    };
    this.teleprinterLog = document.getElementById("teleprinter-log");
    this.printerLog = document.getElementById("printer-log");
    this.teleprinterInput = document.getElementById("teleprinter-input");
    this.cycleInput = document.getElementById("cycle-count");
    this.autoRunTimer = null;
    this.autoRunning = false;
  }

  init() {
    this.bindEvents();
    this.refreshRegisters();
    this.refreshOutput();
    this.startAutoRun();
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
            this.clearHaltAndTrace(1);
            break;
          case "run":
            this.clearHaltAndTrace(this.readCycleBudget());
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

  async clearHaltAndTrace(cycles) {
    try {
      await fetch("/continue", { method: "POST" });
    } catch (error) {
      console.warn("Continue failed", error);
    }
    await this.queueTrace(cycles);
  }

  async queueTrace(cycles) {
    const params = new URLSearchParams();
    if (typeof cycles === "number" && Number.isFinite(cycles)) {
      params.set("cycles", String(Math.max(1, Math.floor(cycles))));
    }
    const suffix = params.toString();
    return this.queueCommand(`/trace${suffix ? `?${suffix}` : ""}`);
  }

  async handleRomUpload() {
    const field = /** @type {HTMLTextAreaElement|null} */ (document.getElementById("rom-text"));
    if (!field) {
      return;
    }

    const raw = field.value;
    const trimmed = raw.trim();
    if (!trimmed) {
      this.setStatus(this.romStatus, "Paste S-record data before uploading.");
      return;
    }

    this.setStatus(this.romStatus, "Uploading S-record payload…");
    try {
      const response = await fetch("/loader", {
        method: "POST",
        headers: { "Content-Type": "text/plain" },
        body: raw,
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
    if (typeof data.halted !== "undefined" && this.registerElements.halt) {
      this.registerElements.halt.textContent = data.halted ? "HALT" : "RUN";
      this.registerElements.halt.dataset.state = data.halted ? "halted" : "running";
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

  startAutoRun() {
    if (this.autoRunTimer) {
      clearInterval(this.autoRunTimer);
    }
    this.autoRunning = true;
    this.autoRunTimer = setInterval(() => this.autoRunTick(), 100);
  }

  stopAutoRun() {
    if (this.autoRunTimer) {
      clearInterval(this.autoRunTimer);
      this.autoRunTimer = null;
    }
    this.autoRunning = false;
  }

  async autoRunTick() {
    if (!this.autoRunning) {
      return;
    }
    try {
      const response = await fetch("/regs");
      if (!response.ok) {
        return;
      }
      const data = await response.json();
      this.updateRegisters(data);

      if (!data.halted) {
        await this.queueTrace(1);
      }

      this.refreshOutput();
    } catch (error) {
      console.warn("Auto-run tick failed", error);
    }
  }
}

document.addEventListener("DOMContentLoaded", () => {
  const app = new FactoryUI();
  app.init();
});
