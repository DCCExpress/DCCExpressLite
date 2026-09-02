import React from "react";
import ReactDOM from "react-dom/client";
import {
  ColorSchemeScript,
  MantineProvider,
} from "@mantine/core";
import {
  Notifications,
} from "@mantine/notifications";
import "@mantine/core/styles.css";
import "@mantine/notifications/styles.css";
import "@/i18n";
import {
  CommandCenterProvider,
} from "@/context/CommandCenterContext";
import {
  EditorSettingsProvider,
} from "@/context/EditorSettingsContext";
import {
  GamepadProvider,
} from "@/context/GamepadContext";
import {
  installDemoRuntime,
} from "@/demo/demoBackend";
import {
  resetDemoStorageOnStartup,
} from "@/demo/demoReset";
import App from "./App";
import "./styles.css";

resetDemoStorageOnStartup();
installDemoRuntime();

ReactDOM.createRoot(
  document.getElementById("root")!
).render(
  <React.StrictMode>
    <ColorSchemeScript
      defaultColorScheme="dark"
    />
    <MantineProvider
      defaultColorScheme="dark"
    >
      <Notifications
        position="bottom-center"
        autoClose={2500}
      />

      <CommandCenterProvider>
        <EditorSettingsProvider>
          <GamepadProvider>
            <App />
          </GamepadProvider>
        </EditorSettingsProvider>
      </CommandCenterProvider>
    </MantineProvider>
  </React.StrictMode>
);
