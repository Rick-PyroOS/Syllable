import { Action, ActionPanel, Icon, List } from "@raycast/api";
import { usePromise } from "@raycast/utils";
import path from "node:path";
import { useMemo, useState } from "react";
import { bestMatches, openFile, projectFiles } from "./nvim";
import { inNeovim, NoNeovim, useInstance } from "./shared";

// Fuzzy-find a file in Neovim's project (git's file list) and open it there.
export default function Command() {
  const { nvim, instance, dropdown, isLoading } = useInstance();
  const [query, setQuery] = useState("");
  const { data, isLoading: loadingFiles } = usePromise(
    async (cwd?: string) => (cwd ? projectFiles(cwd) : []),
    [instance?.cwd],
  );
  const shown = useMemo(() => bestMatches(query, data ?? []), [query, data]);

  return (
    <List
      isLoading={isLoading || loadingFiles}
      filtering={false}
      onSearchTextChange={setQuery}
      searchBarPlaceholder="Find a file in the project"
      searchBarAccessory={dropdown}
    >
      {!isLoading && !instance ? (
        <NoNeovim />
      ) : (
        shown.map((rel) => {
          const full = path.join(instance!.cwd, rel);
          return (
            <List.Item
              key={rel}
              icon={{ fileIcon: full }}
              title={path.basename(rel)}
              subtitle={path.dirname(rel) === "." ? undefined : path.dirname(rel)}
              actions={
                <ActionPanel>
                  <Action
                    title="Open in Neovim"
                    icon={Icon.ArrowRight}
                    onAction={() => inNeovim(() => openFile(nvim, instance!.sock, full))}
                  />
                  <Action
                    title="Open in Right Split"
                    icon={Icon.Sidebar}
                    shortcut={{ modifiers: ["shift"], key: "return" }}
                    onAction={() => inNeovim(() => openFile(nvim, instance!.sock, full, true))}
                  />
                  <Action.ShowInFinder path={full} />
                  <Action.CopyToClipboard title="Copy Path" content={full} />
                </ActionPanel>
              }
            />
          );
        })
      )}
    </List>
  );
}
