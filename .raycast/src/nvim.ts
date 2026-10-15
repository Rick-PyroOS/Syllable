// Talks to running Neovims over the sockets Syllable/.nvim/lua/config/raycast.lua
// opens in ~/.local/state/nvim/raycast/<pid>.sock. Nothing here imports Raycast,
// so it can be tested with plain node.
import { execFile } from "node:child_process";
import { readdir } from "node:fs/promises";
import { homedir } from "node:os";
import path from "node:path";

export const SOCKET_DIR = path.join(homedir(), ".local", "state", "nvim", "raycast");

// api: what the Neovim side supports (see raycast.lua); missing means an old one.
export type Instance = { sock: string; pid: number; cwd: string; file: string; focused: number; api?: number };
export type NvimAction =
  | { kind: "map"; title: string; keys: string[]; lhs: string }
  | { kind: "cmd"; title: string; cmd: string; nargs: string; detail: string; args?: string; file?: boolean }
  | { kind: "ex"; title: string; line: string };
export type Buffer = { buf: number; path: string; modified: boolean; current: boolean; lastused: number };

export class NvimError extends Error {}

function exec(file: string, args: string[], timeout = 3000): Promise<string> {
  return new Promise((resolve, reject) => {
    execFile(file, args, { timeout, maxBuffer: 64 * 1024 * 1024 }, (err, stdout, stderr) => {
      if (err) {
        const msg = (stderr || "").trim() || (err.killed ? "Neovim didn't answer (is a prompt open?)" : err.message);
        reject(new NvimError(msg));
      } else resolve(stdout);
    });
  });
}

// Call require("config.raycast").<fn>(arg) in the Neovim behind `sock`.
export async function call(nvim: string, sock: string, fn: string, arg?: unknown): Promise<string> {
  const lua = `require'config.raycast'.${fn}(${arg === undefined ? "" : "_A"})`;
  const expr =
    arg === undefined
      ? `luaeval("${lua}")`
      : `luaeval("${lua}", json_decode('${JSON.stringify(arg).replace(/'/g, "''")}'))`;
  return exec(nvim, ["--server", sock, "--remote-expr", expr]);
}

async function callJson<T>(nvim: string, sock: string, fn: string): Promise<T> {
  return JSON.parse(await call(nvim, sock, fn)) as T;
}

// Every Neovim that answers, the one you used last first.
export async function instances(nvim: string): Promise<Instance[]> {
  let names: string[] = [];
  try {
    names = (await readdir(SOCKET_DIR)).filter((n) => n.endsWith(".sock"));
  } catch {
    return [];
  }
  const found = await Promise.all(
    names.map(async (n) => {
      const sock = path.join(SOCKET_DIR, n);
      try {
        return { sock, ...(await callJson<Omit<Instance, "sock">>(nvim, sock, "info")) };
      } catch {
        return undefined; // a Neovim that exited without cleaning up, or one that's busy
      }
    }),
  );
  return found.filter((i): i is Instance => !!i).sort((a, b) => b.focused - a.focused);
}

export const actions = (nvim: string, sock: string) => callJson<NvimAction[]>(nvim, sock, "actions");
export const buffers = (nvim: string, sock: string) => callJson<Buffer[]>(nvim, sock, "buffers");
export const run = (nvim: string, sock: string, action: NvimAction) => call(nvim, sock, "run", action);
export const switchTo = (nvim: string, sock: string, buf: number) => call(nvim, sock, "switch", buf);
export const openFile = (nvim: string, sock: string, file: string, split = false) =>
  call(nvim, sock, "open", { path: file, split });

// The project's files: git's list (tracked plus untracked, minus ignored),
// or every file when it isn't a git repo.
export async function projectFiles(cwd: string): Promise<string[]> {
  try {
    const out = await exec("git", ["-C", cwd, "ls-files", "--cached", "--others", "--exclude-standard"], 15000);
    return out.split("\n").filter(Boolean);
  } catch {
    const out = await exec("find", [cwd, "-type", "f", "-not", "-path", "*/.git/*"], 15000);
    return out
      .split("\n")
      .filter(Boolean)
      .map((f) => path.relative(cwd, f));
  }
}

// Fuzzy match like a file finder: the query's letters in order, scored higher
// when they land in the file name, at word starts, and close together.
export function score(query: string, file: string): number {
  if (!query) return 0;
  const q = query.toLowerCase().replace(/\s+/g, "");
  const f = file.toLowerCase();
  const nameStart = f.lastIndexOf("/") + 1;
  let s = 0;
  let pos = -1;
  for (const ch of q) {
    const next = f.indexOf(ch, pos + 1);
    if (next < 0) return -1;
    s += 1;
    if (next >= nameStart) s += 2;
    if (next === pos + 1) s += 3;
    if (next === 0 || "/_-. ".includes(f[next - 1])) s += 2;
    pos = next;
  }
  if (f.slice(nameStart).includes(q)) s += 10;
  return s - f.length * 0.01;
}

export function bestMatches(query: string, files: string[], limit = 200): string[] {
  if (!query) return files.slice(0, limit);
  const scored: [number, string][] = [];
  for (const f of files) {
    const s = score(query, f);
    if (s >= 0) scored.push([s, f]);
  }
  scored.sort((a, b) => b[0] - a[0]);
  return scored.slice(0, limit).map(([, f]) => f);
}
