/// <reference types="@raycast/api">

/* 🚧 🚧 🚧
 * This file is auto-generated from the extension's manifest.
 * Do not modify manually. Instead, update the `package.json` file.
 * 🚧 🚧 🚧 */

/* eslint-disable @typescript-eslint/ban-types */

type ExtensionPreferences = {
  /** nvim Path - The nvim binary used to talk to the running Neovim. */
  "nvimPath": string,
  /** Terminal - Brought to the front after running something in Neovim. */
  "terminal"?: import("@raycast/api").Application
}

/** Preferences accessible in all the extension's commands */
declare type Preferences = ExtensionPreferences

declare namespace Preferences {
  /** Preferences accessible in the `actions` command */
  export type Actions = ExtensionPreferences & {}
  /** Preferences accessible in the `tabs` command */
  export type Tabs = ExtensionPreferences & {}
  /** Preferences accessible in the `files` command */
  export type Files = ExtensionPreferences & {}
}

declare namespace Arguments {
  /** Arguments passed to the `actions` command */
  export type Actions = {}
  /** Arguments passed to the `tabs` command */
  export type Tabs = {}
  /** Arguments passed to the `files` command */
  export type Files = {}
}

