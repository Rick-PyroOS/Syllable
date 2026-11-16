import { Action, ActionPanel, Icon, List, LocalStorage, showToast, Toast } from "@raycast/api";
import { usePromise } from "@raycast/utils";
import path from "node:path";
import { useEffect, useState } from "react";
import { actions, bestMatches, Instance, NvimAction, projectFiles, run } from "./nvim";
import { inNeovim, NoNeovim, useInstance } from "./shared";

type Cmd = Extract<NvimAction, { kind: "cmd" }>;

// Actions you hid with "Hide from List" (Ctrl+H), kept in Raycast's storage.
// "Show Hidden Actions" (Ctrl+Shift+H) lists them again so you can unhide.
const HIDDEN_KEY = "hidden-actions";
const actionId = (a: NvimAction) => (a.kind === "map" ? `map:${a.lhs}` : a.kind === "cmd" ? `cmd:${a.cmd}` : "");

function useHidden() {
  const [hidden, setHidden] = useState<Set<string>>(new Set());
  useEffect(() => {
    LocalStorage.getItem<string>(HIDDEN_KEY).then((v) => setHidden(new Set(v ? (JSON.parse(v) as string[]) : [])));
  }, []);
  const toggle = (id: string) =>
    setHidden((old) => {
      const next = new Set(old);
      if (!next.delete(id)) next.add(id);
      LocalStorage.setItem(HIDDEN_KEY, JSON.stringify([...next]));
      return next;
    });
  return { hidden, toggle };
}

// The Neovim side that understands arguments and ":" lines.
const NEEDS_API = 2;

function runIn(nvim: string, instance: Instance, a: NvimAction) {
  if ((a.kind === "ex" || (a.kind === "cmd" && a.args !== undefined)) && (instance.api ?? 0) < NEEDS_API) {
    return showToast({
      style: Toast.Style.Failure,
      title: "Restart Neovim",
      message: "It's running the old Raycast setup",
    });
  }
  return inNeovim(() => run(nvim, instance.sock, a));
}

// Every mapping with a description (its keys on the right) and every command,
// like Search Menu Items. Enter runs it in Neovim. A command that takes
// arguments opens a second search where you type them, and typing ":" then
// anything runs that line as if typed in Neovim (":w notes.txt").
export default function Command() {
  const { nvim, instance, dropdown, isLoading } = useInstance();
  const [search, setSearch] = useState("");
  const { hidden, toggle } = useHidden();
  const [showHidden, setShowHidden] = useState(false);
  const { data, isLoading: loadingActions } = usePromise(
    async (sock?: string) => (sock ? actions(nvim, sock) : []),
    [instance?.sock],
  );
  const items = (data ?? []).filter((a) => showHidden || !hidden.has(actionId(a)));
  const maps = items.filter((a) => a.kind === "map");
  const cmds = items.filter((a): a is Cmd => a.kind === "cmd");
  const exec = (a: NvimAction) => runIn(nvim, instance!, a);

  const line = search.startsWith(":") ? search.slice(1).trim() : "";
  const lineItem = line && (
    <List.Section title="Command Line">
      <List.Item
        icon={Icon.Terminal}
        title={`:${line}`}
        subtitle="Run in Neovim"
        keywords={[search]}
        actions={
          <ActionPanel>
            <Action title="Run in Neovim" icon={Icon.Play} onAction={() => exec({ kind: "ex", title: line, line })} />
          </ActionPanel>
        }
      />
    </List.Section>
  );

  const item = (a: NvimAction, i: number) => (
    <List.Item
      key={`${a.kind}${i}`}
      icon={a.kind === "map" ? Icon.Keyboard : Icon.Terminal}
      title={a.title}
      // the description's words too, so "save" finds :write and :saveas
      keywords={a.kind === "map" ? [a.lhs, ...a.keys] : a.kind === "cmd" ? [a.cmd, ...a.detail.split(/\W+/)] : []}
      accessories={[
        ...(hidden.has(actionId(a)) ? [{ icon: Icon.EyeDisabled, tooltip: "Hidden" }] : []),
        ...(a.kind === "map" ? a.keys.map((k) => ({ tag: k })) : a.kind === "cmd" ? [{ text: a.detail }] : []),
      ]}
      actions={
        <ActionPanel>
          {a.kind === "cmd" && a.nargs !== "0" ? (
            <>
              <Action.Push
                title="Type Arguments…"
                icon={Icon.Pencil}
                target={<ArgsSearch nvim={nvim} instance={instance!} cmd={a} />}
              />
              {(a.nargs === "?" || a.nargs === "*") && (
                <Action title="Run Without Arguments" icon={Icon.Play} onAction={() => exec({ ...a, args: "" })} />
              )}
              <Action
                title="Finish Typing in Neovim"
                icon={Icon.Terminal}
                shortcut={{ modifiers: ["cmd"], key: "return" }}
                onAction={() => exec(a)}
              />
            </>
          ) : (
            <Action title="Run in Neovim" icon={Icon.Play} onAction={() => exec(a)} />
          )}
          {a.kind === "map" && <Action.CopyToClipboard title="Copy Shortcut" content={a.keys.join(", ")} />}
          <Action
            title={hidden.has(actionId(a)) ? "Unhide" : "Hide from List"}
            icon={hidden.has(actionId(a)) ? Icon.Eye : Icon.EyeDisabled}
            shortcut={{ modifiers: ["ctrl"], key: "h" }}
            onAction={() => toggle(actionId(a))}
          />
          <Action
            title={showHidden ? "Stop Showing Hidden Actions" : "Show Hidden Actions"}
            icon={Icon.Eye}
            shortcut={{ modifiers: ["ctrl", "shift"], key: "h" }}
            onAction={() => setShowHidden(!showHidden)}
          />
        </ActionPanel>
      }
    />
  );

  return (
    <List
      isLoading={isLoading || loadingActions}
      searchBarPlaceholder="Search Neovim actions, or type : and a command"
      searchBarAccessory={dropdown}
      filtering={{ keepSectionOrder: true }}
      onSearchTextChange={setSearch}
    >
      {!isLoading && !instance ? (
        <NoNeovim />
      ) : (
        <>
          {lineItem}
          <List.Section title="Shortcuts">{maps.map(item)}</List.Section>
          <List.Section title="Commands">{cmds.map(item)}</List.Section>
        </>
      )}
    </List>
  );
}

// Type a command's arguments in the search bar and press Enter. For a command
// that takes a file (:write, :saveas, :edit, ...) the project's files are
// listed below as you type; :saveas starts with the current file's name.
// Relative names are relative to Neovim's folder, as in Neovim.
function ArgsSearch({ nvim, instance, cmd }: { nvim: string; instance: Instance; cmd: Cmd }) {
  const current = instance.file ? path.relative(instance.cwd, instance.file) : "";
  const [text, setText] = useState(cmd.cmd === "saveas" ? current : "");
  const { data: files, isLoading } = usePromise(
    async (cwd: string, wanted: boolean) => (wanted ? projectFiles(cwd) : []),
    [instance.cwd, !!cmd.file],
  );
  const required = cmd.nargs === "1" || cmd.nargs === "+";
  const args = text.trim();
  const runWith = (a: string) => runIn(nvim, instance, { ...cmd, args: a });
  const matches = cmd.file && args ? bestMatches(args, files ?? [], 50).filter((f) => f !== args) : [];

  return (
    <List
      isLoading={isLoading}
      navigationTitle={cmd.title}
      searchBarPlaceholder={`${cmd.title} ${required ? "needs" : "takes"} ${cmd.file ? "a file name" : "arguments"}`}
      searchText={text}
      onSearchTextChange={setText}
      filtering={false}
    >
      {args || !required ? (
        <List.Item
          icon={Icon.Play}
          title={`${cmd.title}${args ? ` ${args}` : ""}`}
          subtitle={cmd.detail}
          actions={
            <ActionPanel>
              <Action title="Run in Neovim" icon={Icon.Play} onAction={() => runWith(args)} />
            </ActionPanel>
          }
        />
      ) : (
        <List.EmptyView
          icon={Icon.Pencil}
          title={`Type the ${cmd.file ? "file name" : "arguments"} for ${cmd.title}`}
        />
      )}
      {matches.length > 0 && (
        <List.Section title="Project Files">
          {matches.map((f) => (
            <List.Item
              key={f}
              icon={Icon.Document}
              title={f}
              actions={
                <ActionPanel>
                  <Action title={`${cmd.title} ${f}`} icon={Icon.Play} onAction={() => runWith(f)} />
                  <Action title="Use as Start of Name" icon={Icon.Pencil} onAction={() => setText(f)} />
                </ActionPanel>
              }
            />
          ))}
        </List.Section>
      )}
    </List>
  );
}
