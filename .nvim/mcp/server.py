#!/usr/bin/env python3
"""
Local-model MCP server: runs "checks" (prompt files in ./checks/) on code with
a model on this machine, through any OpenAI-compatible endpoint: Ollama
(the default, http://localhost:11434/v1) or LM Studio (http://localhost:1234/v1).

Neovim's lua/config/mcp.lua starts it and shows what it finds as an M in the
margin. Any other MCP client can use it too, for example Claude Code:
    claude mcp add local-model -- python3 ~/.config/nvim/mcp/server.py

Tools:
  list_checks              the checks in ./checks/ (name, title, languages, version)
  run_check                one check on one piece of code; returns findings
                           {"findings": [{"line", "severity", "message", "fix"}]}
  ask                      a free-form question to the local model

Settings (environment):
  RICK_MCP_URL     endpoint, default http://localhost:11434/v1
  RICK_MCP_MODEL   model, default qwen2.5-coder:7b
  RICK_MCP_CHECKS  folder of checks, default ./checks next to this file

Only the Python standard library; MCP over stdio (one JSON message per line).
"""
import hashlib
import json
import os
import queue
import re
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
URL = os.environ.get("RICK_MCP_URL", "http://localhost:11434/v1").rstrip("/")
MODEL = os.environ.get("RICK_MCP_MODEL", "qwen2.5-coder:7b")
CHECKS = os.environ.get("RICK_MCP_CHECKS", os.path.join(HERE, "checks"))
TIMEOUT = float(os.environ.get("RICK_MCP_TIMEOUT", "300"))
# A model on this machine is reached directly, never through an HTTP proxy.
LOCAL = urllib.parse.urlparse(URL).hostname in ("localhost", "127.0.0.1", "::1")
OPEN = urllib.request.build_opener(urllib.request.ProxyHandler({})).open if LOCAL else urllib.request.urlopen

FORMAT = """
Answer with JSON only, in exactly this form:
{"findings": [{"line": <line number from the listing>, "code": "<that line's code, copied exactly>", "severity": "error" | "warning" | "info", "message": "<one or two sentences>", "fix": "<the corrected code>"}]}
Report only real problems you are confident about, each once, on the line where it is.
"code" is the text of the line you point at, copied from the listing without its number.
"fix" is the code that should replace that line (one or a few lines, without line
numbers or the "|"), written in the same style as the code around it. If the fix
can't be shown as a few lines of code, say in one sentence what to change instead.
If nothing is wrong, answer {"findings": []}.
"""

out_lock = threading.Lock()
cancelled = set()
cache = {}          # hash of (check, model, code) -> findings relative to the code's first line
json_mode = [True]  # whether the endpoint takes response_format json_object
resolved = [False]  # whether MODEL was checked against what's installed
note = [""]         # said once to the client when another model is used


def send(msg):
    with out_lock:
        sys.stdout.write(json.dumps(msg) + "\n")
        sys.stdout.flush()


def log(text):
    sys.stderr.write(text.rstrip() + "\n")
    sys.stderr.flush()


def checks():
    found = {}
    try:
        names = sorted(os.listdir(CHECKS))
    except OSError:
        return found
    for f in names:
        if not f.endswith(".md"):
            continue
        try:
            with open(os.path.join(CHECKS, f), encoding="utf-8") as fh:
                body = fh.read()
        except OSError:
            continue
        # An optional "languages: cpp" line limits a check to those filetypes.
        m = re.search(r"^languages:(.*)$", body, re.M | re.I)
        langs = re.findall(r"[\w+-]+", m.group(1)) if m else []
        if m:
            body = body[:m.start()] + body[m.end() + 1:]
        # "when: <regex>" runs the check only on code that matches (the rest
        # costs nothing); "context: errno" adds what each errno code means;
        # "opt-in: yes" runs it only in projects that list it in vim.g.mcp_checks.
        extra = {}
        for k in ("when", "context", "opt-in"):
            m = re.search(r"^%s:[ \t]*(.*)$" % k, body, re.M | re.I)
            if m:
                extra[k] = m.group(1).strip()
                body = body[:m.start()] + body[m.end() + 1:]
        title = next((l.lstrip("# ").strip() for l in body.splitlines() if l.strip()), f[:-3])
        found[f[:-3]] = dict({"title": title, "prompt": body, "languages": langs}, **extra)
    return found


# errno codes and what they mean, read from the project's own errno.h (this
# path, relative to a folder above the file being checked; set
# NEOVIM_CLION_ERRNO_H for your tree, the default is Syllable's), else the system's.
ERRNO_H = os.environ.get("NEOVIM_CLION_ERRNO_H", "system/sys/include/syllable/posix/errno.h")
SYSTEM_ERRNO_H = ["/usr/include/sys/errno.h", "/usr/include/asm-generic/errno-base.h",
                  "/usr/include/asm-generic/errno.h"]
# Codes worth suggesting instead, when the header has them.
COMMON_ERRNOS = ("EPERM ENOENT EIO EBADF ENOMEM EACCES EBUSY EEXIST ENODEV ENOTDIR EISDIR "
                 "EINVAL ENFILE EMFILE ENOSPC EROFS ERANGE ENAMETOOLONG ENOSYS ENOTEMPTY "
                 "ENODATA EAGAIN ETIMEDOUT").split()
errno_tables = {}


def errno_table(path):
    d = os.path.dirname(os.path.abspath(path)) if path else os.getcwd()
    while True:
        if os.path.isfile(os.path.join(d, ERRNO_H)):
            files = [os.path.join(d, ERRNO_H)]
            break
        if os.path.dirname(d) == d:
            files = SYSTEM_ERRNO_H
            break
        d = os.path.dirname(d)
    key = tuple(files)
    if key not in errno_tables:
        table = {}
        for f in files:
            try:
                with open(f, encoding="utf-8", errors="replace") as fh:
                    for m in re.finditer(r"^#\s*define\s+(E[A-Z0-9]+)\s+\d+\s*/\*\s*(.*?)\s*\*/", fh.read(), re.M):
                        table.setdefault(m.group(1), m.group(2))
            except OSError:
                pass
        errno_tables[key] = table
    return errno_tables[key]


def errno_context(code, path):
    """What the errno codes in code mean, and some others that could fit."""
    table = errno_table(path)
    used = [e for e in dict.fromkeys(re.findall(r"\b(E[A-Z0-9]+)\b", code)) if e in table]
    if not used:
        return ""
    text = "What the error codes in this code mean (perror prints this after your message):\n"
    text += "".join(f"{e}: {table[e]}\n" for e in used)
    text += "Other codes:\n" + "".join(f"{e}: {table[e]}\n" for e in COMMON_ERRNOS if e in table and e not in used)
    return text


class ModelError(Exception):
    pass


def installed_models():
    """The models the endpoint has (OpenAI-style GET /models), or None if unreachable."""
    try:
        with OPEN(urllib.request.Request(URL + "/models"), timeout=5) as r:
            data = json.load(r)
    except (urllib.error.URLError, OSError, ValueError):
        return None
    items = data.get("data", []) if isinstance(data, dict) else []
    return [m["id"] for m in items if isinstance(m, dict) and isinstance(m.get("id"), str)]


def resolve_model():
    """If the configured model isn't installed but another is, use that one
    (a coding model first), so pulling a different size just works."""
    global MODEL
    if resolved[0]:
        return
    models = installed_models()
    if models is None:
        return  # not reachable yet; chat() says so
    resolved[0] = True
    if not models or MODEL in models or MODEL + ":latest" in models:
        return
    base = MODEL.split(":")[0]
    pick = (next((m for m in models if m.split(":")[0] == base), None)
            or next((m for m in models if "coder" in m), None) or models[0])
    note[0] = f"{MODEL} isn't installed, so this uses {pick} (set vim.g.rick_mcp_model to choose)"
    log(note[0])
    MODEL = pick


def chat(system, user, retried=False):
    body = {"model": MODEL, "temperature": 0, "stream": False,
            "messages": [{"role": "system", "content": system}, {"role": "user", "content": user}]}
    if json_mode[0]:
        body["response_format"] = {"type": "json_object"}
    req = urllib.request.Request(URL + "/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    try:
        with OPEN(req, timeout=TIMEOUT) as r:
            data = json.load(r)
    except urllib.error.HTTPError as e:
        detail = e.read().decode(errors="replace")[:300]
        if json_mode[0] and e.code == 400:   # this endpoint doesn't take json mode; ask plainly
            json_mode[0] = False
            return chat(system, user, retried)
        if e.code == 404 and "model" in detail.lower():
            was = MODEL
            resolved[0] = False  # maybe a model was pulled since the last look
            resolve_model()
            if MODEL != was and not retried:
                return chat(system, user, True)
            raise ModelError(f"the model {MODEL} isn't installed (ollama pull {MODEL})")
        raise ModelError(f"{URL} answered {e.code}: {detail}")
    except (urllib.error.URLError, OSError) as e:
        hint = (f"\nIs Ollama running? brew install ollama, then: ollama serve, and ollama pull {MODEL}"
                if ":11434" in URL else "")
        raise ModelError(f"can't reach the local model at {URL} ({getattr(e, 'reason', e)}){hint}")
    try:
        return data["choices"][0]["message"]["content"] or ""
    except (KeyError, IndexError, TypeError):
        raise ModelError(f"unexpected answer from {URL}")


def squash(s):
    return re.sub(r"\s+", "", s)


def locate(quote, lines, first, line):
    """The line the model's quoted code is on, nearest the number it gave, or None
    when the quote isn't in the code at all (it made the problem up). Small models
    often get the number wrong but copy the line right. No quote: trust the number."""
    q = next((squash(l) for l in str(quote or "").split("\n") if squash(l)), "")
    q = re.sub(r"^\d+\|", "", q)  # a quote that kept the listing's "12|"
    if not q:
        return line
    hits = [first + i for i, l in enumerate(lines) if squash(l) and (q in squash(l) or len(squash(l)) >= 8 and squash(l) in q)]
    return min(hits, key=lambda n: abs(n - line)) if hits else None


ALLOC = r"(?:new\b|(?:malloc|calloc|realloc|strdup|strndup)\s*\()"


def bogus_free(fix, lines):
    """Whether a suggested fix deletes or frees something that was never allocated:
    "delete new X();", or a local declared as a plain object or array ("os::String
    cPath", "char buf[10]"), which frees itself. Small models suggest these.
    Members, parameters and local pointers aren't judged: they may come from elsewhere."""
    code = "\n".join(lines)
    for m in re.finditer(r"\bdelete\s*(?:\[\s*\])?\s*([^;\n]+)|\bfree\s*\(([^;\n]+)\)", fix):
        target = re.sub(r"^\s*\([^()]*\)\s*", "", (m.group(1) or m.group(2))).strip()  # drop a cast
        if re.match(ALLOC, target):
            return True
        if re.fullmatch(r"\w+", target) and re.search(r"[\w>]\s+" + target + r"\s*(?:[=;({,]|\[)", code) \
                and not re.search(r"[*&]\s*" + target + r"\b", code):
            return True
    return False


# Toolkits like Syllable's hand objects over by passing new straight to whoever keeps them:
# os::Settings( new os::File(...) ), AddChild( new os::Button(...) ), return new X,
# m_pcFile = new ... The receiver (or the caller, or the object) deletes them later.
HANDED_OVER = r"[(,]\s*new\b|\breturn\s+new\b|(?:\bm_\w+|->\s*\w+)\s*=\s*new\b"
LEAK = r"leak|not (?:deleted|freed)|never (?:deleted|freed)|\bdelete\b|\bfree\b"


def handed_over(line_text, msg, fix):
    """Whether a leak finding points at a new that's handed over, which isn't a leak."""
    return bool(re.search(HANDED_OVER, line_text) and re.search(LEAK, msg + " " + fix, re.I))


REPORT = r"\b(?:perror|strerror)\s*\("


def errno_reported(lines, idx, msg):
    """Whether an errno finding is about "errno = E;" followed straight away by
    perror (or strerror), near the flagged line. That's the right way to report
    an error, which small models often call wrong or read backwards."""
    if not re.search(r"\berrno\b", msg):
        return False
    for i in range(max(idx - 3, 0), min(idx + 4, len(lines))):
        if re.search(r"\berrno\s*=[^=]", lines[i]):
            after = [l for l in lines[i + 1:i + 4] if l.strip()]
            if after and re.search(REPORT, after[0]):
                return True
    return False


def parse_findings(text, first, lines, keep_errno=False):
    last = first + len(lines) - 1
    m = re.search(r"\{.*\}", text, re.S)
    try:
        data = json.loads(m.group(0) if m else text)
    except ValueError:
        return []
    items = data.get("findings", []) if isinstance(data, dict) else data
    out, seen = [], set()
    for f in items if isinstance(items, list) else []:
        if not isinstance(f, dict):
            continue
        try:
            line = int(f.get("line"))
        except (TypeError, ValueError):
            continue
        msg = str(f.get("message", "")).strip()
        sev = str(f.get("severity", "warning")).lower()
        line = locate(f.get("code"), lines, first, line)
        if not msg or line is None or not first <= line <= last or (line, msg) in seen:
            continue
        seen.add((line, msg))
        fix = f.get("fix", "")
        fix = "\n".join(fix) if isinstance(fix, list) else str(fix or "")
        fix = re.sub(r"^```\w*\n?|\n?```$", "", fix.strip()).strip("\n")  # drop a Markdown fence
        if (bogus_free(fix, lines) or handed_over(lines[line - first], msg, fix)
                or (not keep_errno and errno_reported(lines, line - first, msg))):
            continue
        out.append({"line": line, "severity": sev if sev in ("error", "warning", "info") else "warning",
                    "message": msg, "fix": fix})
    return out


def run_check(args):
    name = args.get("check")
    found = checks()
    if name not in found:
        raise ModelError(f"no check named {name!r} in {CHECKS}")
    lang = args.get("language", "")
    if lang and found[name]["languages"] and lang not in found[name]["languages"]:
        return {"findings": []}
    code = args.get("code")
    first = int(args.get("start_line", 1))
    path = args.get("path", "")
    if code is None:
        with open(path, encoding="utf-8", errors="replace") as fh:
            code = fh.read()
    if found[name].get("when") and not re.search(found[name]["when"], code):
        return {"findings": []}
    lines = code.split("\n")
    context = errno_context(code, path) if found[name].get("context") == "errno" else ""
    key = hashlib.sha256(json.dumps([name, MODEL, found[name]["prompt"], FORMAT, code, context]).encode()).hexdigest()
    if key not in cache:
        width = len(str(first + len(lines)))
        listing = "\n".join(f"{str(first + i).rjust(width)}| {l}" for i, l in enumerate(lines))
        user = f"File: {os.path.basename(path) or 'code'}{(' (' + lang + ')') if lang else ''}\n\n{listing}"
        if context:
            user += "\n\n" + context
        found_items = parse_findings(chat(found[name]["prompt"] + "\n" + FORMAT, user), first, lines,
                                     keep_errno=found[name].get("context") == "errno")
        cache[key] = [dict(f, line=f["line"] - first) for f in found_items]
    return {"findings": [dict(f, line=f["line"] + first) for f in cache[key]]}


TOOLS = [
    {"name": "list_checks", "description": "List the code checks this server can run.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "run_check",
     "description": "Run one check (a prompt in the checks folder) on code with the local model. "
                    "Returns JSON findings with absolute line numbers.",
     "inputSchema": {"type": "object", "required": ["check"], "properties": {
         "check": {"type": "string", "description": "check name, see list_checks"},
         "path": {"type": "string", "description": "file the code is from (read when code is missing)"},
         "code": {"type": "string", "description": "the code to check (a function, or a whole file)"},
         "start_line": {"type": "integer", "description": "line number of the code's first line (default 1)"},
         "language": {"type": "string"}}}},
    {"name": "ask", "description": "Ask the local model a question and get its answer as text.",
     "inputSchema": {"type": "object", "required": ["prompt"], "properties": {
         "prompt": {"type": "string"}, "system": {"type": "string"}}}},
]


def call_tool(name, args):
    resolve_model()
    if name == "list_checks":
        return {"checks": [{"name": n, "title": c["title"], "languages": c["languages"],
                            "opt_in": c.get("opt-in", "").lower() in ("yes", "true", "1"),
                            "version": hashlib.sha256((c["prompt"] + FORMAT).encode()).hexdigest()[:12]}
                           for n, c in checks().items()],
                "note": note[0],
                "model": MODEL, "url": URL}
    if name == "run_check":
        return run_check(args)
    if name == "ask":
        json_was, json_mode[0] = json_mode[0], False
        try:
            return {"answer": chat(args.get("system", "You are a helpful programming assistant."), args["prompt"])}
        finally:
            json_mode[0] = json_was
    raise ModelError(f"unknown tool {name}")


def handle(msg):
    mid, method = msg.get("id"), msg.get("method")
    if method == "initialize":
        send({"jsonrpc": "2.0", "id": mid, "result": {
            "protocolVersion": msg.get("params", {}).get("protocolVersion", "2025-06-18"),
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "rick-local-model", "version": "1.0"}}})
    elif method == "tools/list":
        send({"jsonrpc": "2.0", "id": mid, "result": {"tools": TOOLS}})
    elif method == "tools/call":
        p = msg.get("params", {})
        try:
            result = call_tool(p.get("name"), p.get("arguments") or {})
            send({"jsonrpc": "2.0", "id": mid, "result": {
                "content": [{"type": "text", "text": json.dumps(result)}], "structuredContent": result}})
        except Exception as e:  # noqa: BLE001 - reported to the client as a tool error
            send({"jsonrpc": "2.0", "id": mid, "result": {
                "content": [{"type": "text", "text": str(e)}], "isError": True}})
    elif method == "ping":
        send({"jsonrpc": "2.0", "id": mid, "result": {}})
    elif mid is not None:
        send({"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": f"unknown method {method}"}})


def main():
    # Requests are answered one at a time (the model runs one thing at a
    # time anyway); a cancel for a request still waiting drops it.
    work = queue.Queue()

    def worker():
        while True:
            msg = work.get()
            if msg is None:
                return
            if msg.get("id") in cancelled:
                cancelled.discard(msg.get("id"))
                continue
            handle(msg)

    t = threading.Thread(target=worker, daemon=True)
    t.start()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        method = msg.get("method")
        if method == "notifications/cancelled":
            cancelled.add(msg.get("params", {}).get("requestId"))
        elif method in ("initialize", "tools/list", "ping") or (
                method == "tools/call" and (msg.get("params") or {}).get("name") == "list_checks"):
            handle(msg)   # quick ones skip the queue
        elif "id" in msg:
            work.put(msg)
    work.put(None)   # stdin closed: finish what's queued, then exit
    t.join()


if __name__ == "__main__":
    main()
