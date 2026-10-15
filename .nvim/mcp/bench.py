#!/usr/bin/env python3
"""
Time the local models on real code, with the same checks and prompts Neovim
uses, and show what each one reports, so you can pick a model by speed and by
how much nonsense it finds.

    python3 bench.py                         every installed coder model, on bench/settings.cpp
    python3 bench.py path/to/file.cpp        your own file
    python3 bench.py --models qwen2.5-coder:3b,qwen2.5-coder:14b
    python3 bench.py --runs 3                repeat and average (the first run of a model
                                             also includes loading it, shown separately)

The file is split into top-level functions the simple way: a function ends at a
"}" in the first column. Ollama must be running (brew services start ollama).
"""
import argparse
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import server  # noqa: E402  (same folder; its main() only runs as a script)


def functions(text):
    """[(first line number, code)] for each top-level block ending in a column-0 "}"."""
    out, start, lines = [], 0, text.split("\n")
    for i, line in enumerate(lines):
        if line.startswith("}"):
            while start < i and not lines[start].strip():
                start += 1
            out.append((start + 1, "\n".join(lines[start:i + 1])))
            start = i + 1
    return out or [(1, text)]


def name_of(code):
    first = code.split("\n", 1)[0]
    return (first[:40] + "...") if len(first) > 43 else first


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("file", nargs="?", default=os.path.join(HERE, "bench", "settings.cpp"))
    ap.add_argument("--models", help="comma-separated; default: every installed model with 'coder' in its name")
    ap.add_argument("--runs", type=int, default=1)
    args = ap.parse_args()

    installed = server.installed_models()
    if installed is None:
        sys.exit(f"Can't reach {server.URL}. Is Ollama running? brew services start ollama")
    models = args.models.split(",") if args.models else [m for m in installed if "coder" in m] or installed
    missing = [m for m in models if m not in installed and m + ":latest" not in installed]
    if missing:
        sys.exit("Not installed: " + ", ".join(missing) + "  (ollama pull NAME)")

    with open(args.file, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    lang = "c" if args.file.endswith((".c", ".h")) else "cpp"
    funcs = functions(text)
    checks = {n: c for n, c in server.checks().items() if not c["languages"] or lang in c["languages"]}
    print(f"{os.path.basename(args.file)}: {len(funcs)} functions, checks: {', '.join(checks)}, "
          f"{args.runs} run(s) per model\n")

    results = []
    for model in models:
        server.MODEL, server.resolved[0] = model, True
        # Loading: one tiny request, so the timings below are the model working.
        t = time.time()
        try:
            server.chat("Answer with one word.", "Say ok.")
        except server.ModelError as e:
            print(f"{model}: {e}")
            continue
        load = time.time() - t
        times, found = [], {}
        for run in range(args.runs):
            server.cache.clear()
            for start, code in funcs:
                for check in checks:
                    t = time.time()
                    try:
                        res = server.run_check({"check": check, "code": code, "start_line": start,
                                                "path": args.file, "language": lang})
                    except server.ModelError as e:
                        print(f"{model}: {e}")
                        res = {"findings": []}
                    times.append(time.time() - t)
                    if run == 0:
                        for f in res["findings"]:
                            found.setdefault(name_of(code), []).append((check, f))
        total = sum(times) / args.runs
        results.append((model, load, total, total / len(funcs), sum(len(v) for v in found.values()), found))
        print(f"{model}: loaded in {load:.1f} s, {total:.1f} s for the file "
              f"({total / len(funcs):.1f} s per function, all checks)")

    print("\n%-22s %8s %10s %13s %9s" % ("model", "load s", "file s", "per function", "findings"))
    for model, load, total, per, n, _ in results:
        print("%-22s %8.1f %10.1f %13.1f %9d" % (model, load, total, per, n))

    for model, *_, found in results:
        print(f"\n== {model}")
        if not found:
            print("   nothing found")
        for fn, items in found.items():
            print(f"   {fn}")
            for check, f in items:
                print(f"     line {f['line']} [{f['severity']}, {check}] {f['message']}")
                if f.get("fix"):
                    print("       fix: " + f["fix"].replace("\n", "\n            "))


if __name__ == "__main__":
    main()
