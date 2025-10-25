from datetime import datetime
from flask import Flask, jsonify, render_template, request
import ctypes
import ctypes.util
from pathlib import Path
import tempfile
import sys

# Ensure the repository root is on sys.path so 'factory' (a sibling package)
# can be imported when running this script directly (python tools/webdp8.py).
ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

# Use the S-record loader from the factory helper
from factory.driver import load_srec
from factory.ui import STATIC_DIR, TEMPLATES_DIR

app = Flask(
    __name__,
    static_folder=str(STATIC_DIR),
    template_folder=str(TEMPLATES_DIR),
)

# --- emulator init (singleton for now) ---
# Prefer the packaged library under `./factory/libpdp8.so` so the web
# front-end can be run from the project root without copying the shared
# object into the top-level directory.
lib = ctypes.CDLL("./factory/libpdp8.so")

# configure signatures (same as debug_cal3.py) :contentReference[oaicite:1]{index=1}
lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
lib.pdp8_api_create.restype = ctypes.c_void_p
lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
lib.pdp8_api_destroy.restype = None
lib.pdp8_api_reset.argtypes = [ctypes.c_void_p]
lib.pdp8_api_reset.restype = None
lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_api_set_pc.restype = None
lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
lib.pdp8_api_get_pc.restype = ctypes.c_uint16
lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
lib.pdp8_api_get_ac.restype = ctypes.c_uint16
lib.pdp8_api_get_link.argtypes = [ctypes.c_void_p]
lib.pdp8_api_get_link.restype = ctypes.c_bool
lib.pdp8_api_read_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_api_read_mem.restype = ctypes.c_uint16
lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
lib.pdp8_api_write_mem.restype = ctypes.c_int
lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
lib.pdp8_api_step.restype = ctypes.c_int
lib.pdp8_api_clear_halt.argtypes = [ctypes.c_void_p]
lib.pdp8_api_clear_halt.restype = None

# Extra device / console APIs (optional). These mirror signatures used by
# `factory.driver.configure_api` so we can create and attach the KL8E console
# and line-printer and capture their output in temporary files for the web UI.
lib.pdp8_kl8e_console_create.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.pdp8_kl8e_console_create.restype = ctypes.c_void_p
lib.pdp8_kl8e_console_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.pdp8_kl8e_console_attach.restype = ctypes.c_int
lib.pdp8_kl8e_console_output_pending.argtypes = [ctypes.c_void_p]
lib.pdp8_kl8e_console_output_pending.restype = ctypes.c_size_t
lib.pdp8_kl8e_console_pop_output.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8)]
lib.pdp8_kl8e_console_pop_output.restype = ctypes.c_int
lib.pdp8_kl8e_console_queue_input.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
lib.pdp8_kl8e_console_queue_input.restype = ctypes.c_int

lib.pdp8_line_printer_create.argtypes = [ctypes.c_void_p]
lib.pdp8_line_printer_create.restype = ctypes.c_void_p
lib.pdp8_line_printer_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.pdp8_line_printer_attach.restype = ctypes.c_int
lib.pdp8_line_printer_set_column_limit.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_line_printer_set_column_limit.restype = ctypes.c_int

# Switch register access
lib.pdp8_api_set_switch_register.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_api_set_switch_register.restype = None
lib.pdp8_api_get_switch_register.argtypes = [ctypes.c_void_p]
lib.pdp8_api_get_switch_register.restype = ctypes.c_uint16

cpu = lib.pdp8_api_create(0x1000)  # 4K core, matches debug_cal3.py :contentReference[oaicite:2]{index=2}
cycles_counter = 0

# Console / printer capture state
_libc = None
_printer_fp = None
_tele_fp = None
_printer_tmp: Path | None = None
_tele_tmp: Path | None = None
_printer_last_pos = 0
_console_obj = None
_printer_obj = None
_tele_last_pos = 0

# Attempt to create and attach a KL8E console and a line-printer which write
# into temporary files we can expose through HTTP endpoints. This is optional
# and will silently skip if any symbol is missing or creation fails.
try:
    # libc fopen to create FILE* objects for the native C code
    libc_name = ctypes.util.find_library("c") or "libc.so.6"
    _libc = ctypes.CDLL(libc_name)
    _libc.fopen.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    _libc.fopen.restype = ctypes.c_void_p

    # create temp files
    tprinter = tempfile.NamedTemporaryFile(prefix="pdp8_printer_", delete=False)
    tprinter.close()
    _printer_tmp = Path(tprinter.name)
    ttele = tempfile.NamedTemporaryFile(prefix="pdp8_tele_", delete=False)
    ttele.close()
    _tele_tmp = Path(ttele.name)

    # open FILE* for C library
    _printer_fp = _libc.fopen(str(_printer_tmp).encode("utf-8"), b"w+")
    _tele_fp = _libc.fopen(str(_tele_tmp).encode("utf-8"), b"w+")

    if _printer_fp:
        _printer_obj = lib.pdp8_line_printer_create(_printer_fp)
        if _printer_obj and lib.pdp8_line_printer_attach(cpu, _printer_obj) == 0:
            # set a reasonable column limit
            try:
                lib.pdp8_line_printer_set_column_limit(_printer_obj, ctypes.c_uint16(132))
            except Exception:
                pass

    if _tele_fp:
        _console_obj = lib.pdp8_kl8e_console_create(None, _tele_fp)
        if _console_obj and lib.pdp8_kl8e_console_attach(cpu, _console_obj) != 0:
            # attach failed
            _console_obj = None
except Exception:
    # non-fatal: fail silently and continue without devices attached
    _libc = None
    _printer_fp = None
    _tele_fp = None
    _printer_tmp = None
    _tele_tmp = None
    _console_obj = None
    _printer_obj = None

@app.get("/")
def index():
    """Serve the waffle factory control room shell."""
    return render_template("index.html", current_year=datetime.now().year)

def parse_num(v):
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        if len(v) > 1 and v.startswith("0"):
            return int(v, 8)
        return int(v, 10)
    raise ValueError("Bad numeric value")

def to_octal(n):
    return f"{n & 0o7777:04o}"


# ---------- /loader POST ----------
@app.post("/loader")
def post_loader():
    """Load an uploaded S-record into the emulator memory.

    Accepts a multipart file upload with field name 'file', or raw body
    content. Uses `factory.driver.load_srec` to parse the S-record and then
    writes decoded words into emulator memory. If the S-record contains a
    start address (S9/S8/S7), the PC will be set to that word address.
    """
    # Acquire uploaded file or raw body
    upload = None
    if 'file' in request.files:
        upload = request.files['file']
        data = upload.read()
    else:
        data = request.get_data()

    if not data:
        return jsonify({"error": "no S-record data provided"}), 400

    # Write to a temporary file so we can reuse the factory loader
    try:
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as tf:
            tf.write(data)
            tf.flush()
            temp_path = Path(tf.name)

        rom_words, start_word = load_srec(temp_path)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 400
    finally:
        try:
            if 'temp_path' in locals() and temp_path.exists():
                temp_path.unlink()
        except Exception:
            pass

    written = []
    for address, value in rom_words:
        rc = lib.pdp8_api_write_mem(cpu, address & 0o7777, value & 0x0FFF)
        if rc != 0:
            return jsonify({"error": f"write failed at {address:04o}"}), 500
        written.append({"addr": to_octal(address), "val": to_octal(value)})

    # If the S-record provided a start address, set PC there
    if start_word is not None:
        lib.pdp8_api_set_pc(cpu, start_word & 0o7777)

    # Clear any previous HALT so the loaded program can run
    try:
        lib.pdp8_api_clear_halt(cpu)
    except Exception:
        pass

    return jsonify({"written": written, "start": to_octal(start_word) if start_word is not None else None})

# ---------- /regs ----------
@app.get("/regs")
def get_regs():
    pc = lib.pdp8_api_get_pc(cpu)
    ac = lib.pdp8_api_get_ac(cpu)
    lk = lib.pdp8_api_get_link(cpu)
    return jsonify({
        "pc": to_octal(pc),
        "ac": to_octal(ac),
        "link": 1 if lk else 0,
        "cycles": cycles_counter  # local counter
    })


@app.get("/switch")
def get_switch():
    """Read the PDP-8 switch register (S)."""
    try:
        sval = lib.pdp8_api_get_switch_register(cpu)
        return jsonify({"switch": to_octal(sval)})
    except Exception as exc:
        return jsonify({"error": str(exc)}), 500


@app.put("/switch")
def put_switch():
    """Set the PDP-8 switch register. JSON: {"val": "03751"} or {"val":1234}.

    This lets programs that use `OSR` read values from S.
    """
    try:
        body = request.get_json(force=True, silent=False)
    except Exception as exc:
        return jsonify({"error": "invalid JSON"}), 400

    if not body or "val" not in body:
        return jsonify({"error": "need {val: octal|decimal}"}), 400

    try:
        v = parse_num(body["val"])
    except Exception as exc:
        return jsonify({"error": f"bad value: {exc}"}), 400

    try:
        lib.pdp8_api_set_switch_register(cpu, v & 0x0FFF)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 500

    return jsonify({"written": to_octal(v)})

# ---------- /mem GET ----------
@app.get("/mem")
def get_mem():
    try:
        addr_raw = request.args.get("addr", None)
        if addr_raw is None:
            return jsonify({"error": "addr required"}), 400
        start = parse_num(addr_raw)

        length_raw = request.args.get("len", "1")
        length = parse_num(length_raw)

        words = []
        for i in range(length):
            a = (start + i) & 0o7777
            val = lib.pdp8_api_read_mem(cpu, a)
            words.append({
                "addr": to_octal(a),
                "val": to_octal(val),
            })

        return jsonify({
            "start": to_octal(start),
            "words": words
        })

    except ValueError as e:
        return jsonify({"error": str(e)}), 400


# ---------- teleprinter output (KL8E) ----------
@app.get("/output/teleprinter")
def get_teleprinter_output():
    """Return and consume available teleprinter output recorded by the KL8E console."""
    # Prefer reading the teleprinter temp file if available (non-destructive
    # read). If not present, fall back to popping the console internal buffer.
    peek = request.args.get("peek", "0") in ("1", "true", "True")

    if _tele_tmp is not None:
        try:
            with open(_tele_tmp, "rb") as f:
                f.seek(_tele_last_pos)
                chunk = f.read()
                if not peek:
                    # advance the last-pos so subsequent calls return only new
                    # data
                    _tele_last_pos = f.tell()
        except Exception as exc:
            return jsonify({"error": str(exc)}), 500

        try:
            text = chunk.decode("utf-8", errors="replace")
        except Exception:
            text = ""

        return jsonify({"text": text, "bytes": [f"{b:03o}" for b in chunk]})

    if _console_obj is None:
        return jsonify({"error": "no console attached"}), 404

    out_bytes = bytearray()
    # Pop until empty (pop returns non-zero when no data)
    try:
        while True:
            ch = ctypes.c_uint8()
            rc = lib.pdp8_kl8e_console_pop_output(_console_obj, ctypes.byref(ch))
            if rc != 0:
                break
            out_bytes.append(ch.value)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 500

    try:
        text = out_bytes.decode("utf-8", errors="replace")
    except Exception:
        text = ""

    return jsonify({"text": text, "bytes": [f"{b:03o}" for b in out_bytes]})


# ---------- line printer output (PRN) ----------
@app.get("/output/printer")
def get_printer_output():
    """Return incremental output written by the native line-printer to its temp file."""
    global _printer_last_pos
    if _printer_tmp is None:
        return jsonify({"error": "no printer attached"}), 404

    try:
        with open(_printer_tmp, "rb") as f:
            f.seek(_printer_last_pos)
            chunk = f.read()
            _printer_last_pos = f.tell()
    except Exception as exc:
        return jsonify({"error": str(exc)}), 500

    try:
        text = chunk.decode("utf-8", errors="replace")
    except Exception:
        text = ""

    return jsonify({"text": text, "bytes": [f"{b:03o}" for b in chunk]})


# ---------- keyboard input (queue to KL8E console) ----------
@app.post("/input/keyboard")
def post_keyboard_input():
    if _console_obj is None:
        return jsonify({"error": "no console attached"}), 404
    body = request.get_json(force=True, silent=False)
    if not body or "chars" not in body:
        return jsonify({"error": "need {chars: string}"}), 400
    text = str(body["chars"])
    queued = 0
    for ch in text:
        code = ord(ch) & 0x7F
        rc = lib.pdp8_kl8e_console_queue_input(_console_obj, ctypes.c_uint8(code))
        if rc == 0:
            queued += 1
    return jsonify({"queued": queued})

# ---------- /mem PUT ----------
@app.put("/mem")
def put_mem():
    body = request.get_json(force=True, silent=False)

    written = []

    if "addr" in body and "val" in body:
        a = parse_num(body["addr"])
        v = parse_num(body["val"])
        rc = lib.pdp8_api_write_mem(cpu, a & 0o7777, v & 0o7777)
        if rc != 0:
            return jsonify({"error": "write failed"}), 500
        written.append({"addr": to_octal(a), "val": to_octal(v)})

    elif "start" in body and "values" in body:
        start = parse_num(body["start"])
        for offset, vraw in enumerate(body["values"]):
            a = (start + offset) & 0o7777
            v = parse_num(vraw)
            rc = lib.pdp8_api_write_mem(cpu, a, v & 0o7777)
            if rc != 0:
                return jsonify({"error": f"write failed at {to_octal(a)}"}), 500
            written.append({"addr": to_octal(a), "val": to_octal(v)})
    else:
        return jsonify({"error": "need {addr,val} or {start,values}"}), 400

    return jsonify({"written": written})

# ---------- /trace GET ----------
@app.get("/trace")
def get_trace():
    global cycles_counter

    try:
        # Ensure any previously-set HLT condition is cleared before tracing.
        # Tests and other frontends call `pdp8_api_clear_halt` when they want to
        # resume execution; do the same here so /trace always attempts to run.
        lib.pdp8_api_clear_halt(cpu)

        # optional start PC override
        if "start" in request.args:
            start_pc = parse_num(request.args["start"]) & 0o7777
            lib.pdp8_api_set_pc(cpu, start_pc)

        # cycles to execute
        if "cycles" in request.args:
            ncycles = parse_num(request.args["cycles"])
        else:
            ncycles = 1

        # clamp
        if ncycles < 1:
            ncycles = 1
        if ncycles > 1024:
            ncycles = 1024

        steps = []
        halted = False

        begin_pc = lib.pdp8_api_get_pc(cpu)

        for step_i in range(ncycles):
            pc_before = lib.pdp8_api_get_pc(cpu)
            ac_before = lib.pdp8_api_get_ac(cpu)
            lk_before = lib.pdp8_api_get_link(cpu)
            instr = lib.pdp8_api_read_mem(cpu, pc_before)

            rc = lib.pdp8_api_step(cpu)
            if rc == 0:
                halted = True

            pc_after = lib.pdp8_api_get_pc(cpu)
            ac_after = lib.pdp8_api_get_ac(cpu)
            lk_after = lib.pdp8_api_get_link(cpu)

            steps.append({
                "step": step_i,
                "pc_before": to_octal(pc_before),
                "ac_before": to_octal(ac_before),
                "link_before": 1 if lk_before else 0,
                "instr": to_octal(instr),
                "pc_after": to_octal(pc_after),
                "ac_after": to_octal(ac_after),
                "link_after": 1 if lk_after else 0,
                "halted": (rc == 0)
            })

            cycles_counter += 1

            if rc == 0:
                break

        return jsonify({
            "begin_pc": to_octal(begin_pc),
            "steps": steps,
            "halted": halted
        })

    except ValueError as e:
        return jsonify({"error": str(e)}), 400

# (run with app.run(host="0.0.0.0", port=5000, debug=True))
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
