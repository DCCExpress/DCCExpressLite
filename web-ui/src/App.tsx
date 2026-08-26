import {
  Alert,
  Badge,
  Box,
  Button,
  Card,
  Divider,
  Group,
  Loader,
  PasswordInput,
  SimpleGrid,
  Stack,
  Text,
  TextInput,
  ThemeIcon,
  Title,
} from "@mantine/core";
import { showNotification } from "@mantine/notifications";
import {
  IconAlertTriangle,
  IconArrowLeft,
  IconCheck,
  IconDeviceFloppy,
  IconEdit,
  IconHome,
  IconMap,
  IconRefresh,
  IconRouter,
  IconSettings,
  IconTrain,
  IconWifi,
} from "@tabler/icons-react";
import { useCallback, useEffect, useState } from "react";

import type { Loco } from "@domain/types";

import { getLocos } from "@/api/domainApi";
import LocoDialog from "@/components/LocoDialog";
import LocoPanel from "@/layout/LocoPanel";
import LiteLayoutPage from "./LiteLayoutPage";
import { getDefaultWsUrl } from "@/services/defaultWsUrl";
import { wsApi } from "@/services/wsApi";
import {
  wsClient,
  type WsConnectionStatus,
} from "@/services/wsClient";

type LoadState = "idle" | "loading" | "ready" | "error";
type Page = "home" | "drive" | "layout" | "settings";

type NetworkSettingsDto = {
  configured: boolean;
  ssid: string;
  hasPassword: boolean;
  hostname: string;
  mode: "access-point" | "station";
  currentIp: string;
  connectionUrl: string;
  restartPending: boolean;
};

type ApiResponse = {
  ok: boolean;
  message: string;
};

function statusColor(status: WsConnectionStatus): string {
  switch (status) {
    case "connected":
      return "teal";
    default:
      return "red";
  }
}

function formatStatus(status: WsConnectionStatus): string {
  switch (status) {
    case "connected":
      return "Online";
    case "connecting":
      return "Connecting";
    case "reconnecting":
      return "Reconnecting";
    case "error":
      return "Connection error";
    default:
      return "Offline";
  }
}

function pageFromHash(): Page {
  const page = window.location.hash.replace("#", "");
  return page === "drive" || page === "layout" || page === "settings" ? page : "home";
}

function AppHeader({ status, version }: { status: WsConnectionStatus; version: string }) {
  return (
    <Group justify="space-between" align="center" wrap="nowrap">
      <Group gap="sm" wrap="nowrap">
        <ThemeIcon size={44} radius="md" variant="gradient" gradient={{ from: "cyan", to: "blue" }}>
          <IconTrain size={25} />
        </ThemeIcon>
        <div>
          <Title order={3}>DCCExpress Lite</Title>
          <Group gap={6} wrap="nowrap">
            <Text size="xs" c="dimmed">EX-CSB1 command station</Text>
            <Badge size="xs" variant="light" color="violet">v{version}</Badge>
          </Group>
        </div>
      </Group>

      <Badge
        color={statusColor(status)}
        variant={status === "connected" ? "light" : "filled"}
        size="lg"
        className={status === "connected" ? "" : "lite-ws-alert"}
      >
        {formatStatus(status)}
      </Badge>
    </Group>
  );
}

function PageTitle({ icon, title, subtitle }: { icon: React.ReactNode; title: string; subtitle: string }) {
  return (
    <Group gap="sm" wrap="nowrap">
      <ThemeIcon size={42} radius="md" variant="light" color="cyan">
        {icon}
      </ThemeIcon>
      <div>
        <Title order={3}>{title}</Title>
        <Text size="sm" c="dimmed">{subtitle}</Text>
      </div>
    </Group>
  );
}

function ConnectionBanner({ networkInfo }: { networkInfo: NetworkSettingsDto | null }) {
  const currentIp = networkInfo?.currentIp || window.location.hostname || "192.168.4.1";
  const connectionUrl = networkInfo?.connectionUrl || `http://${currentIp}`;
  const modeLabel = networkInfo?.mode === "station" ? "Local network" : "DCCEX hotspot";

  return (
    <Card className="connection-banner" radius="lg" p="sm" withBorder>
      <Group justify="space-between" align="center" wrap="nowrap">
        <Group gap="sm" wrap="nowrap">
          <ThemeIcon size={38} radius="md" color="cyan" variant="light">
            <IconWifi size={21} />
          </ThemeIcon>
          <div>
            <Text size="xs" c="dimmed">CONNECT TO THIS ADDRESS</Text>
            <Text component="a" href={connectionUrl} className="connection-address" fw={800}>
              {connectionUrl}
            </Text>
          </div>
        </Group>
        <Badge variant="light" color={networkInfo?.mode === "station" ? "teal" : "blue"}>
          {modeLabel}
        </Badge>
      </Group>
    </Card>
  );
}

function HomePage({
  status,
  version,
  locoCount,
  onNavigate,
  onOpenLocoEditor,
}: {
  status: WsConnectionStatus;
  version: string;
  locoCount: number;
  onNavigate: (page: Page) => void;
  onOpenLocoEditor: () => void;
}) {
  return (
    <Stack gap="lg">
      <Card className="hero-card" radius="xl" p="xl">
        <Stack gap="lg">
          <AppHeader status={status} version={version} />
          <div>
            <Text className="eyebrow">YOUR RAILWAY, READY TO MOVE</Text>
            <Title className="hero-title">Command the layout from your pocket.</Title>
            <Text c="dimmed" mt="sm" maw={540}>
              Drive locomotives, manage the command station and connect the EX-CSB1 to your local network.
            </Text>
          </div>
          <Button
            size="lg"
            radius="md"
            leftSection={<IconTrain size={20} />}
            onClick={() => onNavigate("drive")}
          >
            Open locomotive control
          </Button>
        </Stack>
      </Card>

      <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="md">
        <Card className="action-card" withBorder radius="xl" p="lg" onClick={() => onNavigate("drive")}>
          <ThemeIcon size={48} radius="lg" color="cyan" variant="light">
            <IconTrain size={27} />
          </ThemeIcon>
          <Title order={4} mt="md">Locomotives</Title>
          <Text size="sm" c="dimmed" mt={4}>
            {locoCount > 0 ? `${locoCount} locomotive${locoCount === 1 ? "" : "s"} available` : "Open throttle and function controls"}
          </Text>
        </Card>

        <Card className="action-card" withBorder radius="xl" p="lg" onClick={() => onNavigate("layout")}>
          <ThemeIcon size={48} radius="lg" color="teal" variant="light">
            <IconMap size={27} />
          </ThemeIcon>
          <Title order={4} mt="md">Layout editor</Title>
          <Text size="sm" c="dimmed" mt={4}>
            Build the track plan and operate turnouts
          </Text>
        </Card>

        <Card className="action-card" withBorder radius="xl" p="lg" onClick={onOpenLocoEditor}>
          <ThemeIcon size={48} radius="lg" color="violet" variant="light">
            <IconEdit size={27} />
          </ThemeIcon>
          <Title order={4} mt="md">Locomotive editor</Title>
          <Text size="sm" c="dimmed" mt={4}>
            Add locomotives and configure addresses, images and functions
          </Text>
        </Card>

        <Card className="action-card" withBorder radius="xl" p="lg" onClick={() => onNavigate("settings")}>
          <ThemeIcon size={48} radius="lg" color="indigo" variant="light">
            <IconRouter size={27} />
          </ThemeIcon>
          <Title order={4} mt="md">Network settings</Title>
          <Text size="sm" c="dimmed" mt={4}>
            Join the EX-CSB1 to your local Wi-Fi network
          </Text>
        </Card>
      </SimpleGrid>

      <Card withBorder radius="xl" p="lg" className="status-card">
        <Group justify="space-between">
          <Group gap="sm">
            <IconWifi size={21} />
            <div>
              <Text fw={700}>Live command link</Text>
              <Text size="xs" c="dimmed">WebSocket connection to this device</Text>
            </div>
          </Group>
          <Badge
            color={statusColor(status)}
            variant={status === "connected" ? "dot" : "filled"}
            className={status === "connected" ? "" : "lite-ws-alert"}
          >
            {formatStatus(status)}
          </Badge>
        </Group>
      </Card>
    </Stack>
  );
}

function SettingsPage({ onBack, status }: { onBack: () => void; status: WsConnectionStatus }) {
  const [settings, setSettings] = useState<NetworkSettingsDto | null>(null);
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const loadSettings = useCallback(async () => {
    setLoading(true);
    setError(null);

    try {
      const response = await fetch("/api/settings/network", { cache: "no-store" });

      if (!response.ok) throw new Error("The device did not return its network settings.");

      const data = await response.json() as NetworkSettingsDto;
      setSettings(data);
      setSsid(data.ssid);
      setPassword("");
    } catch (loadError) {
      setError(loadError instanceof Error ? loadError.message : "Could not load network settings.");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void loadSettings();
  }, [loadSettings]);

  const saveSettings = async () => {
    setSaving(true);
    setError(null);

    try {
      const body = new URLSearchParams({ ssid: ssid.trim(), password });
      const response = await fetch("/api/settings/network", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body,
      });
      const result = await response.json() as ApiResponse;

      if (!response.ok || !result.ok) throw new Error(result.message || "Could not save network settings.");

      showNotification({
        color: "teal",
        title: "Network saved",
        message: "The EX-CSB1 is restarting. Reconnect through your local Wi-Fi and open dccex.local.",
        autoClose: 9000,
      });
      setSettings(current => current ? { ...current, configured: true, ssid: ssid.trim(), hasPassword: true, restartPending: true } : current);
    } catch (saveError) {
      setError(saveError instanceof Error ? saveError.message : "Could not save network settings.");
      setSaving(false);
    }
  };

  const resetSettings = async () => {
    if (!window.confirm("Clear the saved network and restart in DCCEX hotspot mode?")) return;

    setSaving(true);
    setError(null);

    try {
      const response = await fetch("/api/settings/network/reset", { method: "POST" });
      const result = await response.json() as ApiResponse;

      if (!response.ok || !result.ok) throw new Error(result.message || "Could not clear network settings.");

      showNotification({
        color: "teal",
        title: "Hotspot mode restored",
        message: "The EX-CSB1 is restarting. Join its DCCEX hotspot again.",
        autoClose: 9000,
      });
      setSettings(current => current ? { ...current, configured: false, ssid: "", hasPassword: false, restartPending: true } : current);
      setSsid("");
      setPassword("");
    } catch (resetError) {
      setError(resetError instanceof Error ? resetError.message : "Could not clear network settings.");
      setSaving(false);
    }
  };

  return (
    <Stack gap="lg">
      <Group justify="space-between">
        <Button variant="subtle" color="gray" leftSection={<IconArrowLeft size={18} />} onClick={onBack} className="back-button">
          Back to home
        </Button>
        <Badge
          color={statusColor(status)}
          variant={status === "connected" ? "light" : "filled"}
          className={status === "connected" ? "" : "lite-ws-alert"}
        >
          {formatStatus(status)}
        </Badge>
      </Group>

      <PageTitle icon={<IconSettings size={24} />} title="Settings" subtitle="Device and local network" />

      <Card withBorder radius="xl" p="lg">
        <Stack gap="md">
          <Group justify="space-between">
            <div>
              <Title order={4}>Local Wi-Fi network</Title>
              <Text size="sm" c="dimmed">Saved securely on the EX-CSB1</Text>
            </div>
            <ThemeIcon size={42} radius="md" variant="light" color={settings?.configured ? "teal" : "gray"}>
              <IconWifi size={23} />
            </ThemeIcon>
          </Group>

          {loading ? (
            <Group justify="center" py="xl"><Loader /></Group>
          ) : (
            <>
              {settings?.configured && (
                <Alert color="teal" icon={<IconCheck size={18} />} title="Local network configured">
                  Currently saved: <strong>{settings.ssid}</strong>
                </Alert>
              )}

              {error && (
                <Alert color="red" icon={<IconAlertTriangle size={18} />} title="Network settings">
                  {error}
                </Alert>
              )}

              <TextInput
                label="Wi-Fi name (SSID)"
                placeholder="Your local network"
                value={ssid}
                onChange={event => setSsid(event.currentTarget.value)}
                maxLength={32}
                leftSection={<IconRouter size={17} />}
                disabled={saving}
                required
              />

              <PasswordInput
                label="Wi-Fi password"
                placeholder={settings?.hasPassword ? "Leave blank to keep the saved password" : "8–63 characters"}
                description={settings?.hasPassword ? "The saved password is never sent back to the browser." : undefined}
                value={password}
                onChange={event => setPassword(event.currentTarget.value)}
                minLength={8}
                maxLength={63}
                disabled={saving}
              />

              <Alert color="blue" variant="light" icon={<IconWifi size={18} />}>
                After saving, the device restarts and this hotspot disappears. Join your local Wi-Fi, then open <strong>http://dccex.local</strong> or the IP shown on the EX-CSB1 display.
              </Alert>

              <Button
                size="md"
                leftSection={<IconDeviceFloppy size={19} />}
                loading={saving}
                disabled={!ssid.trim() || (!settings?.hasPassword && password.length < 8)}
                onClick={() => void saveSettings()}
              >
                Save network and restart
              </Button>

              <Divider label="Recovery" labelPosition="center" />

              <Button variant="light" color="red" disabled={saving || !settings?.configured} onClick={() => void resetSettings()}>
                Return to DCCEX hotspot mode
              </Button>
            </>
          )}
        </Stack>
      </Card>
    </Stack>
  );
}

export default function App() {
  const [page, setPage] = useState<Page>(pageFromHash);
  const [status, setStatus] = useState<WsConnectionStatus>(wsClient.getStatus());
  const [locos, setLocos] = useState<Loco[]>([]);
  const [loadState, setLoadState] = useState<LoadState>("idle");
  const [networkInfo, setNetworkInfo] = useState<NetworkSettingsDto | null>(null);
  const [locoEditorOpened, setLocoEditorOpened] = useState(false);
  const [version, setVersion] = useState("development");

  const loadLocos = useCallback(async () => {
    setLoadState("loading");

    try {
      setLocos(await getLocos());
      setLoadState("ready");
    } catch (error) {
      console.error(error);
      setLoadState("error");
    }
  }, []);

  const loadLocosForEditor = useCallback(async (): Promise<Loco[]> => {
    const response = await fetch("/api/locos", { cache: "no-store" });
    if (!response.ok) throw new Error("Could not load locomotives from the EX-CSB1.");
    return response.json() as Promise<Loco[]>;
  }, []);

  const saveLocosForEditor = useCallback(async (nextLocos: Loco[]): Promise<void> => {
    const response = await fetch("/api/locos", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(nextLocos),
    });
    if (!response.ok) throw new Error("Could not save locomotives to the EX-CSB1.");
  }, []);

  useEffect(() => {
    wsApi.connect(getDefaultWsUrl());
    const closeSocketForPageExit = () => wsApi.disconnect();
    const unsubscribeStatus = wsClient.subscribeStatus(nextStatus => {
      setStatus(nextStatus);
      if (nextStatus === "connected") void loadLocos();
    });

    const onHashChange = () => setPage(pageFromHash());
    window.addEventListener("hashchange", onHashChange);
    window.addEventListener("pagehide", closeSocketForPageExit);

    void fetch("/api/settings/network", { cache: "no-store" })
      .then(response => response.ok ? response.json() as Promise<NetworkSettingsDto> : null)
      .then(data => setNetworkInfo(data))
      .catch(() => undefined);

    void fetch("/version.json", { cache: "no-store" })
      .then(response => response.ok ? response.json() as Promise<{ version?: string }> : null)
      .then(data => {
        if (data?.version) setVersion(data.version);
      })
      .catch(() => undefined);

    return () => {
      window.removeEventListener("hashchange", onHashChange);
      window.removeEventListener("pagehide", closeSocketForPageExit);
      unsubscribeStatus();
      wsApi.disconnect();
    };
  }, [loadLocos]);

  const navigate = (nextPage: Page) => {
    window.location.hash = nextPage === "home" ? "" : nextPage;
    setPage(nextPage);
    window.scrollTo({ top: 0, behavior: "smooth" });
  };

  const renderPage = () => {
    if (page === "settings") return <SettingsPage onBack={() => navigate("home")} status={status} />;

    if (page === "layout") return <LiteLayoutPage version={version} locos={locos} onBack={() => navigate("home")} onOpenLocoEditor={() => setLocoEditorOpened(true)} onDataImported={loadLocos} />;

    if (page === "drive") {
      return (
        <Stack gap="lg">
          <Group justify="space-between">
            <Button variant="subtle" color="gray" leftSection={<IconArrowLeft size={18} />} onClick={() => navigate("home")} className="back-button">
              Back to home
            </Button>
            <Badge
              color={statusColor(status)}
              variant={status === "connected" ? "light" : "filled"}
              className={status === "connected" ? "" : "lite-ws-alert"}
            >
              {formatStatus(status)}
            </Badge>
          </Group>
          <Group justify="space-between" align="center">
            <PageTitle icon={<IconTrain size={24} />} title="Locomotives" subtitle="Throttle, direction and functions" />
            <Button variant="light" color="violet" leftSection={<IconEdit size={17} />} onClick={() => setLocoEditorOpened(true)}>
              Edit locomotives
            </Button>
          </Group>
          {loadState === "loading" && locos.length === 0 ? (
            <Card withBorder radius="xl" p="xl"><Stack align="center"><Loader /><Text c="dimmed">Loading locomotives…</Text></Stack></Card>
          ) : (
            <Box className="mobile-loco-panel"><LocoPanel locos={locos} /></Box>
          )}
          {loadState === "error" && (
            <Button variant="light" leftSection={<IconRefresh size={16} />} onClick={() => void loadLocos()}>Retry loading locomotives</Button>
          )}
        </Stack>
      );
    }

    return <HomePage status={status} version={version} locoCount={locos.length} onNavigate={navigate} onOpenLocoEditor={() => setLocoEditorOpened(true)} />;
  };

  return (
    <Box className={`mobile-shell${page === "layout" ? " layout-shell" : ""}`}>
      <Stack gap="md">
        {page === "home" && <ConnectionBanner networkInfo={networkInfo} />}
        <Box className="mobile-content">{renderPage()}</Box>
      </Stack>
      {page !== "layout" && (
        <Card className="bottom-nav" radius="xl" withBorder p={6}>
          <Button variant={page === "home" ? "light" : "subtle"} color={page === "home" ? "cyan" : "gray"} leftSection={<IconHome size={18} />} onClick={() => navigate("home")}>Home</Button>
          <Button variant={page === "drive" ? "light" : "subtle"} color={page === "drive" ? "cyan" : "gray"} leftSection={<IconTrain size={18} />} onClick={() => navigate("drive")}>Drive</Button>
          <Button variant="subtle" color="gray" leftSection={<IconMap size={18} />} onClick={() => navigate("layout")}>Layout</Button>
          <Button variant={page === "settings" ? "light" : "subtle"} color={page === "settings" ? "cyan" : "gray"} leftSection={<IconSettings size={18} />} onClick={() => navigate("settings")}>Settings</Button>
        </Card>
      )}
      <LocoDialog
        opened={locoEditorOpened}
        onClose={() => setLocoEditorOpened(false)}
        onSaved={() => void loadLocos()}
        loadLocos={loadLocosForEditor}
        saveLocos={saveLocosForEditor}
      />
    </Box>
  );
}
