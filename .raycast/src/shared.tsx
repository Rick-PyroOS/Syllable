import { Application, closeMainWindow, getPreferenceValues, List, open, showToast, Toast } from "@raycast/api";
import { usePromise } from "@raycast/utils";
import path from "node:path";
import { useState } from "react";
import { Instance, instances, SOCKET_DIR } from "./nvim";

type Preferences = { nvimPath?: string; terminal?: Application };

export function prefs() {
  const p = getPreferenceValues<Preferences>();
  return { nvim: p.nvimPath || "/opt/homebrew/bin/nvim", terminal: p.terminal };
}

export function label(i: Instance) {
  return `${path.basename(i.cwd)}${i.file ? ` · ${path.basename(i.file)}` : ""}`;
}

// The running Neovims, with the one you used last picked; a dropdown in the
// search bar switches when more than one is open.
export function useInstance() {
  const { nvim } = prefs();
  const { data, isLoading } = usePromise(instances, [nvim]);
  const [chosen, setChosen] = useState<string>();
  const list = data ?? [];
  const instance = list.find((i) => i.sock === chosen) ?? list[0];
  const dropdown =
    list.length > 1 ? (
      <List.Dropdown tooltip="Neovim" value={instance?.sock} onChange={setChosen}>
        {list.map((i) => (
          <List.Dropdown.Item key={i.sock} value={i.sock} title={label(i)} />
        ))}
      </List.Dropdown>
    ) : undefined;
  return { nvim, instance, dropdown, isLoading };
}

export function NoNeovim() {
  return (
    <List.EmptyView
      title="No Neovim running"
      description={`Start nvim with the Syllable .nvim config; it listens in ${SOCKET_DIR.replace(/^\/Users\/[^/]+/, "~")}`}
    />
  );
}

// Do something in Neovim, then bring the terminal forward so you see it.
export async function inNeovim(work: () => Promise<unknown>) {
  try {
    await work();
    await closeMainWindow({ clearRootSearch: true });
    const { terminal } = prefs();
    if (terminal?.path) await open(terminal.path);
  } catch (e) {
    await showToast({ style: Toast.Style.Failure, title: "Neovim didn't take it", message: String(e) });
  }
}
