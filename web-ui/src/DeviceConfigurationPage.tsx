import {
  ActionIcon,
  Alert,
  Badge,
  Button,
  Card,
  Divider,
  Group,
  Modal,
  NumberInput,
  Select,
  SimpleGrid,
  Stack,
  Switch,
  Text,
  TextInput,
  ThemeIcon,
  Title,
} from "@mantine/core";
import { showNotification } from "@mantine/notifications";
import {
  IconAlertTriangle,
  IconArrowLeft,
  IconCpu,
  IconDeviceFloppy,
  IconEdit,
  IconPlus,
  IconRefresh,
  IconTrash,
} from "@tabler/icons-react";
import {
  useEffect,
  useMemo,
  useState,
} from "react";

type Props = {
  onBack: () => void;
};

export type DeviceType =
  | "pca9685"
  | "mcp23017"
  | "pcf8574";

export type DeviceConfiguration = {
  id: string;
  name: string;
  type: DeviceType;
  enabled: boolean;
  address: number;
  firstVpin: number;
  pinCount: number;
  frequency?: number;
  interruptPin?: number | null;
};

export type DeviceConfigurationDocument = {
  version: number;
  devices: DeviceConfiguration[];
};

export function isDeviceConfigurationDocument(
  value: unknown
): value is DeviceConfigurationDocument {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<DeviceConfigurationDocument>;
  return Array.isArray(candidate.devices) && candidate.devices.every(item => {
    if (!item || typeof item !== "object") return false;
    const device = item as Partial<DeviceConfiguration>;
    return typeof device.id === "string" &&
      typeof device.name === "string" &&
      typeof device.type === "string" &&
      Object.prototype.hasOwnProperty.call(DEVICE_DEFINITIONS, device.type) &&
      typeof device.enabled === "boolean" &&
      Number.isInteger(device.address) &&
      Number.isInteger(device.firstVpin) &&
      Number.isInteger(device.pinCount);
  });
}

type DeviceForm = {
  id: string | null;
  name: string;
  type: DeviceType;
  enabled: boolean;
  address: string;
  firstVpin: number;
  frequency: number;
  interruptPin: number | null;
};

type DeviceDefinition = {
  type: DeviceType;
  label: string;
  description: string;
  pinCount: number;
  addressMin: number;
  addressMax: number;
  defaultAddress: number;
  defaultFirstVpin: number;
  color: string;
};

const STORAGE_KEY =
  "dcc-express-lite.device-configuration-draft.v1";

const DEVICE_DEFINITIONS: Record<
  DeviceType,
  DeviceDefinition
> = {
  pca9685: {
    type: "pca9685",
    label: "PCA9685",
    description: "16-channel PWM and servo controller",
    pinCount: 16,
    addressMin: 0x40,
    addressMax: 0x7d,
    defaultAddress: 0x40,
    defaultFirstVpin: 100,
    color: "violet",
  },
  mcp23017: {
    type: "mcp23017",
    label: "MCP23017",
    description: "16-channel digital input/output expander",
    pinCount: 16,
    addressMin: 0x20,
    addressMax: 0x27,
    defaultAddress: 0x20,
    defaultFirstVpin: 164,
    color: "cyan",
  },
  pcf8574: {
    type: "pcf8574",
    label: "PCF8574",
    description: "8-channel digital input/output expander",
    pinCount: 8,
    addressMin: 0x20,
    addressMax: 0x27,
    defaultAddress: 0x22,
    defaultFirstVpin: 200,
    color: "teal",
  },
};

const DEVICE_TYPE_OPTIONS = Object.values(
  DEVICE_DEFINITIONS
).map(definition => ({
  value: definition.type,
  label: `${definition.label} · ${definition.description}`,
}));

function newId(): string {
  return globalThis.crypto?.randomUUID?.() ??
    `device-${Date.now()}-${Math.random()
      .toString(16)
      .slice(2)}`;
}

function hexAddress(address: number): string {
  return `0x${address
    .toString(16)
    .toUpperCase()
    .padStart(2, "0")}`;
}

function parseAddress(value: string): number | null {
  const trimmed = value.trim();
  if (!/^(?:0x)?[0-9a-f]{1,2}$/i.test(trimmed)) {
    return null;
  }

  const parsed = Number.parseInt(
    trimmed.replace(/^0x/i, ""),
    16
  );

  return Number.isInteger(parsed) ? parsed : null;
}

function createForm(type: DeviceType): DeviceForm {
  const definition = DEVICE_DEFINITIONS[type];

  return {
    id: null,
    name: definition.label,
    type,
    enabled: true,
    address: hexAddress(definition.defaultAddress),
    firstVpin: definition.defaultFirstVpin,
    frequency: 50,
    interruptPin: null,
  };
}

function loadDraft(): DeviceConfiguration[] {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];

    const value = JSON.parse(raw) as unknown;
    if (!Array.isArray(value)) return [];

    return value.filter(
      (item): item is DeviceConfiguration => {
        if (!item || typeof item !== "object") return false;
        const candidate = item as Partial<DeviceConfiguration>;
        return (
          typeof candidate.id === "string" &&
          typeof candidate.name === "string" &&
          typeof candidate.type === "string" &&
          candidate.type in DEVICE_DEFINITIONS &&
          typeof candidate.enabled === "boolean" &&
          Number.isInteger(candidate.address) &&
          Number.isInteger(candidate.firstVpin) &&
          Number.isInteger(candidate.pinCount)
        );
      }
    );
  } catch {
    return [];
  }
}

function saveDraft(devices: DeviceConfiguration[]): void {
  window.localStorage.setItem(
    STORAGE_KEY,
    JSON.stringify(devices)
  );
}

export default function DeviceConfigurationPage({
  onBack,
}: Props) {
  const [devices, setDevices] =
    useState<DeviceConfiguration[]>(loadDraft);
  const [form, setForm] =
    useState<DeviceForm>(() => createForm("pca9685"));
  const [dialogOpened, setDialogOpened] =
    useState(false);
  const [deleteTarget, setDeleteTarget] =
    useState<DeviceConfiguration | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [dirty, setDirty] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [detectedAddresses, setDetectedAddresses] = useState<string[]>([]);
  const [scanning, setScanning] = useState(false);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      try {
        const response = await fetch("/api/device-config", { cache: "no-store" });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const document = await response.json() as unknown;
        if (!isDeviceConfigurationDocument(document)) {
          throw new Error("Invalid device configuration response");
        }
        if (!cancelled) {
          setDevices(document.devices);
          saveDraft(document.devices);
          setDirty(false);
          setLoadError(null);
        }
      } catch (error) {
        if (!cancelled) {
          setLoadError(
            `Could not read the EX-CSB1 configuration (${error instanceof Error ? error.message : String(error)}). The local browser draft is shown.`
          );
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    })();
    return () => { cancelled = true; };
  }, []);

  const definition = DEVICE_DEFINITIONS[form.type];
  const parsedAddress = parseAddress(form.address);

  const formError = useMemo(() => {
    if (!form.name.trim()) return "Device name is required.";
    if (parsedAddress === null) {
      return "Enter a hexadecimal I²C address, for example 0x40.";
    }
    if (
      parsedAddress < definition.addressMin ||
      parsedAddress > definition.addressMax
    ) {
      return `${definition.label} address must be between ${hexAddress(
        definition.addressMin
      )} and ${hexAddress(definition.addressMax)}.`;
    }
    if (
      !Number.isInteger(form.firstVpin) ||
      form.firstVpin < 40 ||
      form.firstVpin + definition.pinCount - 1 > 32767
    ) {
      return "Choose a free VPIN range between 40 and 32767.";
    }

    const otherDevices = devices.filter(
      device => device.id !== form.id && device.enabled
    );

    if (
      form.enabled &&
      otherDevices.some(
        device => device.address === parsedAddress
      )
    ) {
      return `I²C address ${hexAddress(parsedAddress)} is already used.`;
    }

    const lastVpin =
      form.firstVpin + definition.pinCount - 1;
    const overlap = form.enabled
      ? otherDevices.find(device => {
        const deviceLast =
          device.firstVpin + device.pinCount - 1;
        return (
          form.firstVpin <= deviceLast &&
          lastVpin >= device.firstVpin
        );
      })
      : undefined;

    if (overlap) {
      return `VPIN range overlaps ${overlap.name} (${overlap.firstVpin}–${overlap.firstVpin + overlap.pinCount - 1
        }).`;
    }

    return null;
  }, [definition, devices, form, parsedAddress]);

  const replaceDevices = (
    nextDevices: DeviceConfiguration[]
  ) => {
    setDevices(nextDevices);
    saveDraft(nextDevices);
    setDirty(true);
  };

  const applyConfiguration = async () => {
    setSaving(true);
    try {
      const document: DeviceConfigurationDocument = { version: 1, devices };
      const response = await fetch("/api/device-config", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(document),
      });
      const result = await response.json().catch(() => null) as { message?: string } | null;
      if (!response.ok) throw new Error(result?.message ?? `HTTP ${response.status}`);
      setDirty(false);
      setLoadError(null);
      showNotification({
        color: "teal",
        title: "Device configuration saved",
        message: result?.message ?? "EX-CSB1 is restarting with the new HAL devices.",
      });
    } catch (error) {
      showNotification({
        color: "red",
        title: "Could not save device configuration",
        message: error instanceof Error ? error.message : String(error),
      });
    } finally {
      setSaving(false);
    }
  };

  const scanI2c = async () => {
    setScanning(true);
    try {
      const response = await fetch("/api/devices", { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const result = await response.json() as { i2cDevices?: Array<{ addressHex?: string; address?: number }> };
      const addresses = Array.isArray(result.i2cDevices)
        ? result.i2cDevices.map(device => device.addressHex ??
          (Number.isInteger(device.address) ? hexAddress(device.address as number) : "")).filter(Boolean)
        : [];
      setDetectedAddresses(addresses);
      showNotification({
        color: "blue",
        title: "I²C scan complete",
        message: addresses.length ? `Detected: ${addresses.join(", ")}` : "No I²C devices detected.",
      });
    } catch (error) {
      showNotification({
        color: "red",
        title: "I²C scan failed",
        message: error instanceof Error ? error.message : String(error),
      });
    } finally {
      setScanning(false);
    }
  };

  const openAdd = (type: DeviceType = "pca9685") => {
    setForm(createForm(type));
    setDialogOpened(true);
  };

  const openEdit = (device: DeviceConfiguration) => {
    setForm({
      id: device.id,
      name: device.name,
      type: device.type,
      enabled: device.enabled,
      address: hexAddress(device.address),
      firstVpin: device.firstVpin,
      frequency: device.frequency ?? 50,
      interruptPin: device.interruptPin ?? null,
    });
    setDialogOpened(true);
  };

  const saveDevice = () => {
    if (formError || parsedAddress === null) return;

    const nextDevice: DeviceConfiguration = {
      id: form.id ?? newId(),
      name: form.name.trim(),
      type: form.type,
      enabled: form.enabled,
      address: parsedAddress,
      firstVpin: form.firstVpin,
      pinCount: definition.pinCount,
      ...(form.type === "pca9685"
        ? { frequency: form.frequency }
        : { interruptPin: form.interruptPin }),
    };

    replaceDevices(
      form.id
        ? devices.map(device =>
          device.id === form.id ? nextDevice : device
        )
        : [...devices, nextDevice]
    );
    setDialogOpened(false);
  };

  const toggleDevice = (
    target: DeviceConfiguration,
    enabled: boolean
  ) => {
    if (enabled) {
      const addressConflict = devices.some(
        device =>
          device.id !== target.id &&
          device.enabled &&
          device.address === target.address
      );
      const targetLast =
        target.firstVpin + target.pinCount - 1;
      const rangeConflict = devices.some(device => {
        if (device.id === target.id || !device.enabled) return false;
        const deviceLast =
          device.firstVpin + device.pinCount - 1;
        return (
          target.firstVpin <= deviceLast &&
          targetLast >= device.firstVpin
        );
      });

      if (addressConflict || rangeConflict) {
        openEdit(target);
        return;
      }
    }

    replaceDevices(
      devices.map(device =>
        device.id === target.id
          ? { ...device, enabled }
          : device
      )
    );
  };

  const deleteDevice = () => {
    if (!deleteTarget) return;
    replaceDevices(
      devices.filter(device => device.id !== deleteTarget.id)
    );
    setDeleteTarget(null);
  };

  return (
    <Stack gap="md">
      <Modal
        opened={dialogOpened}
        onClose={() => setDialogOpened(false)}
        title={form.id ? "Edit device" : "Add I²C device"}
        centered
        size="lg"
        returnFocus={false}
        closeOnClickOutside={false}
        closeOnEscape={false}
        trapFocus={false}
      >        <Stack gap="md">
          <Select
            label="Driver"
            data={DEVICE_TYPE_OPTIONS}
            value={form.type}
            allowDeselect={false}
            onChange={value => {
              if (!value) return;
              const nextType = value as DeviceType;
              const nextDefinition =
                DEVICE_DEFINITIONS[nextType];
              setForm(current => ({
                ...current,
                type: nextType,
                name: current.id
                  ? current.name
                  : nextDefinition.label,
                address: hexAddress(
                  nextDefinition.defaultAddress
                ),
                firstVpin:
                  nextDefinition.defaultFirstVpin,
              }));
            }}
          />

          <TextInput
            label="Name"
            placeholder="Station servo board"
            value={form.name}
            onChange={event => {
              const value = event.currentTarget.value;
              setForm(current => ({
                ...current,
                name: value,
              }));
            }}
          />

          <SimpleGrid cols={{ base: 1, sm: 2 }}>
            <TextInput
              label="I²C address"
              description={`${hexAddress(
                definition.addressMin
              )}–${hexAddress(definition.addressMax)}`}
              value={form.address}
              onChange={event => {
                const value = event.currentTarget.value;
                setForm(current => ({
                  ...current,
                  address: value,
                }));
              }}
            />

            <NumberInput
              label="First VPIN"
              description={`${definition.pinCount} VPINs will be reserved`}
              value={form.firstVpin}
              min={40}
              max={32767 - definition.pinCount + 1}
              allowDecimal={false}
              onChange={value => {
                if (typeof value === "number") {
                  setForm(current => ({
                    ...current,
                    firstVpin: value,
                  }));
                }
              }}
            />
          </SimpleGrid>

          {form.type === "pca9685" ? (
            <NumberInput
              label="PWM frequency"
              description="Standard analogue servo frequency is 50 Hz"
              suffix=" Hz"
              value={form.frequency}
              min={24}
              max={1526}
              allowDecimal={false}
              onChange={value => {
                if (typeof value === "number") {
                  setForm(current => ({
                    ...current,
                    frequency: value,
                  }));
                }
              }}
            />
          ) : (
            <NumberInput
              label="Interrupt GPIO (optional)"
              description="Leave empty to poll the input expander"
              placeholder="Not configured"
              value={form.interruptPin ?? ""}
              min={0}
              max={39}
              allowDecimal={false}
              onChange={value =>
                setForm(current => ({
                  ...current,
                  interruptPin:
                    typeof value === "number" ? value : null,
                }))
              }
            />
          )}

          <Switch
            label="Enabled"
            description="Enabled devices will be created during EX-CSB1 startup"
            checked={form.enabled}
            onChange={event => {
              const checked = event.currentTarget.checked;
              setForm(current => ({
                ...current,
                enabled: checked,
              }));
            }}
          />

          {formError && (
            <Alert
              color="red"
              icon={<IconAlertTriangle size={18} />}
            >
              {formError}
            </Alert>
          )}

          <Group justify="flex-end">
            <Button
              variant="default"
              onClick={() => setDialogOpened(false)}
            >
              Cancel
            </Button>
            <Button
              leftSection={<IconDeviceFloppy size={17} />}
              disabled={!!formError}
              onClick={saveDevice}
            >
              Save draft
            </Button>
          </Group>
        </Stack>
      </Modal>

      <Modal
        opened={deleteTarget !== null}
        onClose={() => setDeleteTarget(null)}
        title="Delete device"
        centered
        size="sm"
        returnFocus={false}
      >
        <Stack>
          <Text>
            Remove <b>{deleteTarget?.name}</b> from the device
            configuration draft?
          </Text>
          <Group justify="flex-end">
            <Button
              variant="default"
              onClick={() => setDeleteTarget(null)}
            >
              Cancel
            </Button>
            <Button color="red" onClick={deleteDevice}>
              Delete
            </Button>
          </Group>
        </Stack>
      </Modal>

      <Card withBorder radius={5} p="md">
        <Group justify="space-between" align="center" wrap="wrap">
          <Group gap="sm" wrap="nowrap">
            <ActionIcon
              variant="subtle"
              color="gray"
              size="lg"
              aria-label="Back to home"
              onClick={onBack}
            >
              <IconArrowLeft size={22} />
            </ActionIcon>
            <ThemeIcon
              size={42}
              radius="md"
              color="blue"
              variant="light"
            >
              <IconCpu size={24} />
            </ThemeIcon>
            <div>
              <Title order={3}>Device configuration</Title>
              <Text size="sm" c="dimmed">
                External DCC-EX HAL devices and VPIN ranges
              </Text>
            </div>
          </Group>

          <Group gap="xs">
            <Badge color={dirty ? "orange" : "teal"} variant="light">
              {loading ? "Loading" : dirty ? "Unsaved changes" : "Saved on device"}
            </Badge>
            <Button
              variant="light"
              leftSection={<IconDeviceFloppy size={17} />}
              loading={saving}
              disabled={loading || !dirty}
              onClick={() => void applyConfiguration()}
            >
              Apply &amp; restart
            </Button>
            <Button
              leftSection={<IconPlus size={17} />}
              onClick={() => openAdd()}
            >
              Add device
            </Button>
          </Group>
        </Group>
      </Card>

      {loadError ? (
        <Alert color="yellow" icon={<IconAlertTriangle size={19} />}>
          {loadError}
        </Alert>
      ) : (
        <Alert color="blue" icon={<IconCpu size={19} />}>
          Changes are validated and saved to LittleFS. Applying a configuration
          restarts the EX-CSB1 so DCC-EX can create the selected HAL devices safely.
        </Alert>
      )}

      <SimpleGrid cols={{ base: 1, md: 3 }} spacing="md">
        {Object.values(DEVICE_DEFINITIONS).map(item => (
          <Card key={item.type} withBorder radius={5} p="md">
            <Group justify="space-between" align="flex-start">
              <ThemeIcon
                size={42}
                radius="md"
                color={item.color}
                variant="light"
              >
                <IconCpu size={23} />
              </ThemeIcon>
              <Badge color={item.color} variant="light">
                {item.pinCount} channels
              </Badge>
            </Group>
            <Title order={4} mt="sm">
              {item.label}
            </Title>
            <Text size="sm" c="dimmed" mt={4}>
              {item.description}
            </Text>
            <Button
              mt="md"
              size="compact-sm"
              variant="light"
              color={item.color}
              leftSection={<IconPlus size={15} />}
              onClick={() => openAdd(item.type)}
            >
              Add {item.label}
            </Button>
          </Card>
        ))}
      </SimpleGrid>

      <Card withBorder radius={5} p="md">
        <Group justify="space-between" align="center" wrap="wrap">
          <div>
            <Title order={4}>Configured devices</Title>
            <Text size="sm" c="dimmed">
              {devices.length === 0
                ? "No external device has been added yet."
                : `${devices.length} device${devices.length === 1 ? "" : "s"
                } configured.`}
            </Text>
          </div>

          <Button
            variant="light"
            color="gray"
            leftSection={<IconRefresh size={16} />}
            loading={scanning}
            onClick={() => void scanI2c()}
          >
            Scan I²C bus
          </Button>
        </Group>

        {detectedAddresses.length > 0 && (
          <Group gap="xs" mt="sm">
            <Text size="sm" c="dimmed">Detected:</Text>
            {detectedAddresses.map(address => (
              <Badge key={address} variant="light" color="blue">{address}</Badge>
            ))}
          </Group>
        )}

        <Divider my="md" />

        {devices.length === 0 ? (
          <Stack align="center" py="xl" gap="xs">
            <ThemeIcon
              size={54}
              radius="xl"
              color="gray"
              variant="light"
            >
              <IconCpu size={30} />
            </ThemeIcon>
            <Text fw={700}>No configured devices</Text>
            <Text size="sm" c="dimmed" ta="center">
              Add a servo or GPIO expander to start building the
              hardware configuration.
            </Text>
          </Stack>
        ) : (
          <Stack gap="sm">
            {devices.map(device => {
              const item = DEVICE_DEFINITIONS[device.type];
              const lastVpin =
                device.firstVpin + device.pinCount - 1;

              return (
                <Card key={device.id} withBorder radius={5} p="sm">
                  <Group
                    justify="space-between"
                    align="center"
                    wrap="wrap"
                  >
                    <Group gap="sm" wrap="nowrap">
                      <ThemeIcon
                        size={40}
                        radius="md"
                        color={device.enabled ? item.color : "gray"}
                        variant="light"
                      >
                        <IconCpu size={22} />
                      </ThemeIcon>
                      <div>
                        <Group gap="xs">
                          <Text fw={700}>{device.name}</Text>
                          <Badge
                            size="sm"
                            color={device.enabled ? "teal" : "gray"}
                            variant="light"
                          >
                            {device.enabled ? "Enabled" : "Disabled"}
                          </Badge>
                        </Group>
                        <Text size="sm" c="dimmed">
                          {item.label} · {hexAddress(device.address)} ·
                          VPIN {device.firstVpin}–{lastVpin}
                          {device.frequency
                            ? ` · ${device.frequency} Hz`
                            : ""}
                        </Text>
                      </div>
                    </Group>

                    <Group gap="xs" wrap="nowrap">
                      <Switch
                        aria-label={`Enable ${device.name}`}
                        checked={device.enabled}
                        onChange={event =>
                          toggleDevice(
                            device,
                            event.currentTarget.checked
                          )
                        }
                      />
                      <ActionIcon
                        variant="light"
                        color="blue"
                        aria-label={`Edit ${device.name}`}
                        onClick={() => openEdit(device)}
                      >
                        <IconEdit size={17} />
                      </ActionIcon>
                      <ActionIcon
                        variant="light"
                        color="red"
                        aria-label={`Delete ${device.name}`}
                        onClick={() => setDeleteTarget(device)}
                      >
                        <IconTrash size={17} />
                      </ActionIcon>
                    </Group>
                  </Group>
                </Card>
              );
            })}
          </Stack>
        )}
      </Card>
    </Stack>
  );
}
