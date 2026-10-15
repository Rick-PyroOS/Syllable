import { Action, ActionPanel, Color, Icon, List } from "@raycast/api";
import { usePromise } from "@raycast/utils";
import path from "node:path";
import { buffers, switchTo } from "./nvim";
import { inNeovim, NoNeovim, useInstance } from "./shared";

// The files open in Neovim (its tabs), most recently used first.
export default function Command() {
  const { nvim, instance, dropdown, isLoading } = useInstance();
  const { data, isLoading: loadingTabs } = usePromise(
    async (sock?: string) => (sock ? buffers(nvim, sock) : []),
    [instance?.sock],
  );
  const tabs = [...(data ?? [])].sort((a, b) => b.lastused - a.lastused);

  return (
    <List
      isLoading={isLoading || loadingTabs}
      searchBarPlaceholder="Switch to an open file"
      searchBarAccessory={dropdown}
    >
      {!isLoading && !instance ? (
        <NoNeovim />
      ) : (
        tabs.map((t) => (
          <List.Item
            key={t.buf}
            icon={Icon.Document}
            title={path.basename(t.path)}
            subtitle={path.relative(instance!.cwd, path.dirname(t.path)) || undefined}
            accessories={[
              ...(t.modified ? [{ tag: { value: "unsaved", color: Color.Orange } }] : []),
              ...(t.current ? [{ tag: "current" }] : []),
            ]}
            actions={
              <ActionPanel>
                <Action
                  title="Switch to Tab"
                  icon={Icon.ArrowRight}
                  onAction={() => inNeovim(() => switchTo(nvim, instance!.sock, t.buf))}
                />
                <Action.ShowInFinder path={t.path} />
                <Action.CopyToClipboard title="Copy Path" content={t.path} />
              </ActionPanel>
            }
          />
        ))
      )}
    </List>
  );
}
