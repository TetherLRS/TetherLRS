# TetherLRS tool

Configuration and firmware tool for TetherLRS radios, as a desktop app (Tauri 2) and as a web app (Chrome or Edge,
Web Serial). One Svelte 5 + TypeScript code base: in the browser the serial port is Web Serial, in the desktop app
a small Rust bridge (`src-tauri/src/serial.rs`) gives the same port interface. Flashing uses esptool-js in both.

- `npm run dev`: web app at http://localhost:1420
- `npm run tauri dev`: desktop app
- `npm run build`: static web app in `build/`
- `npx vitest run`: tests, against a demo Tx that answers like the firmware's CLI

The tool talks to the ground radio (Tx) through its CLI, entered with SiK's `+++`. On USB serial adapters it
looks for TetherLRS radios by itself; it never sends anything to flight controller ports.
