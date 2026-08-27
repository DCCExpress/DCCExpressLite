import {
  ActionIcon,
  Alert,
  Badge,
  Button,
  Card,
  Group,
  Loader,
  ScrollArea,
  Select,
  Stack,
  Switch,
  Text,
  Title,
} from "@mantine/core";
import {
  IconAlertTriangle,
  IconCheck,
  IconDeviceFloppy,
  IconPlus,
  IconRefresh,
  IconTrash,
} from "@tabler/icons-react";
import { useEffect, useMemo, useState } from "react";

import {
  loadSignalLogicRulesWs,
  saveSignalLogicRulesWs,
} from "@/api/signalLogicWsApi";
import AppModal from "@/components/common/AppModal";
import { generateId } from "@/helpers";
import { isTurnoutElement, type LayoutView } from "@/models/editor/core/LayoutView";
import { TrackSensorElementView } from "@/models/editor/elements/TrackSensorElementView";
import { TrackSignalElementView } from "@/models/editor/elements/TrackSignalElementView";
import type {
  SignalAspect,
  SignalLogicConditionDto,
  SignalLogicDocumentDto,
  SignalLogicRuleDto,
  SignalLogicRuleGroupDto,
  SignalLogicRuntimeStateDto,
  SignalLogicValidationIssue,
} from "@domain/signalLogic";
import {
  getAllowedSignalAspects,
  migrateSignalLogicReferences,
  validateSignalLogicDocument,
} from "@domain/signalLogic";

type SignalLogicDialogProps = {
  opened: boolean;
  onClose: () => void;
  layout: LayoutView;
};

type AddressOption = {
  value: string;
  label: string;
};

type SignalOption = AddressOption & {
  address: number;
  aspect: number;
};

type ElementOption = AddressOption & { address: number };

const MAX_GROUPS = 24;
const MAX_RULES = 6;
const MAX_CONDITIONS = 6;

function uniqueOptions<T extends AddressOption>(items: T[]): T[] {
  const seen = new Set<string>();
  return items.filter(item => {
    if (seen.has(item.value)) return false;
    seen.add(item.value);
    return true;
  });
}

function newTurnoutCondition(turnoutId: string): SignalLogicConditionDto {
  return {
    id: generateId(),
    type: "turnout",
    turnoutId,
    closed: true,
  };
}

function newSensorCondition(sensorId: string): SignalLogicConditionDto {
  return {
    id: generateId(),
    type: "sensor",
    sensorId,
    active: true,
  };
}

function newRule(turnoutId: string): SignalLogicRuleDto {
  return {
    id: generateId(),
    aspect: "green",
    conditions: turnoutId ? [newTurnoutCondition(turnoutId)] : [],
  };
}

function aspectColor(aspect: SignalAspect): string {
  if (aspect === "green") return "green";
  if (aspect === "yellow") return "yellow";
  if (aspect === "white") return "gray";
  return "red";
}

export default function SignalLogicDialog({ opened, onClose, layout }: SignalLogicDialogProps) {
  const [groups, setGroups] = useState<SignalLogicRuleGroupDto[]>([]);
  const [selectedGroupId, setSelectedGroupId] = useState<string | null>(null);
  const [runtime, setRuntime] = useState<SignalLogicRuntimeStateDto>({ running: false, enabled: false });
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [migrationIssues, setMigrationIssues] = useState<SignalLogicValidationIssue[]>([]);

  const signalOptions = useMemo<SignalOption[]>(() => uniqueOptions(
    layout.getAllElements()
      .filter((element): element is TrackSignalElementView =>
        element instanceof TrackSignalElementView && element.address > 0)
      .map(signal => ({
        value: signal.id,
        label: `Signal #${signal.address}${signal.name && signal.name !== "element" ? ` · ${signal.name}` : ""}`,
        address: signal.address,
        aspect: signal.aspect,
      }))
      .sort((a, b) => a.address - b.address)
  ), [layout]);

  const turnoutOptions = useMemo<ElementOption[]>(() => uniqueOptions(
    layout.getAllElements()
      .filter(isTurnoutElement)
      .filter(turnout => turnout.turnoutAddress > 0)
      .map(turnout => ({
        value: turnout.id,
        label: `Turnout #${turnout.turnoutAddress}${turnout.name && turnout.name !== "element" ? ` · ${turnout.name}` : ""}`,
        address: turnout.turnoutAddress,
      }))
      .sort((a, b) => a.address - b.address)
  ), [layout]);

  const sensorOptions = useMemo<ElementOption[]>(() => uniqueOptions(
    layout.getAllElements()
      .filter((element): element is TrackSensorElementView =>
        element instanceof TrackSensorElementView && element.address > 0)
      .map(sensor => ({
        value: sensor.id,
        label: `Sensor #${sensor.address}${sensor.name && sensor.name !== "element" ? ` · ${sensor.name}` : ""}`,
        address: sensor.address,
      }))
      .sort((a, b) => a.address - b.address)
  ), [layout]);

  const document = useMemo<SignalLogicDocumentDto>(() => ({
    version: 2,
    enabled: runtime.enabled,
    groups: groups.map(group => ({ ...group, defaultAspect: "red" })),
  }), [groups, runtime.enabled]);

  const issues = useMemo(() => validateSignalLogicDocument(
    document,
    signalOptions.map(signal => ({ id: signal.value, address: signal.address, aspect: signal.aspect })),
    turnoutOptions.map(turnout => ({ id: turnout.value, address: turnout.address })),
    sensorOptions.map(sensor => ({ id: sensor.value, address: sensor.address }))
  ), [document, sensorOptions, signalOptions, turnoutOptions]);
  const allIssues = [...migrationIssues, ...issues];
  const hasErrors = allIssues.some(issue => issue.level === "error");
  const selectedGroup = groups.find(group => group.id === selectedGroupId) ?? groups[0] ?? null;

  const load = async (): Promise<void> => {
    setLoading(true);
    setError(null);
    setMessage(null);
    try {
      const result = await loadSignalLogicRulesWs();
      const migration = migrateSignalLogicReferences(
        result.document,
        signalOptions.map(signal => ({ id: signal.value, address: signal.address, aspect: signal.aspect })),
        turnoutOptions.map(turnout => ({ id: turnout.value, address: turnout.address })),
        sensorOptions.map(sensor => ({ id: sensor.value, address: sensor.address }))
      );
      setMigrationIssues(migration.issues);
      let loadedDocument = migration.document;
      if (migration.migratedReferences > 0 && !migration.issues.some(issue => issue.level === "error")) {
        const saved = await saveSignalLogicRulesWs(migration.document);
        loadedDocument = saved.document;
      }
      const nextGroups = loadedDocument.groups.map(group => ({ ...group, defaultAspect: "red" as const }));
      setGroups(nextGroups);
      setSelectedGroupId(nextGroups[0]?.id ?? null);
      setRuntime(result.state);
      setMessage(migration.migratedReferences > 0
        ? `${migration.migratedReferences} legacy address reference(s) migrated to stable element IDs.`
        : "Signal logic loaded. Integrity check passed.");
    } catch (loadError) {
      setError(loadError instanceof Error ? loadError.message : String(loadError));
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (opened) void load();
    // Loading is intentionally tied only to opening the dialog.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [opened]);

  const save = async (): Promise<void> => {
    setSaving(true);
    setError(null);
    setMessage(null);
    try {
      const result = await saveSignalLogicRulesWs(document);
      setGroups(result.document.groups.map(group => ({ ...group, defaultAspect: "red" as const })));
      setRuntime(result.state);
      setMessage(result.state.running ? "Rules saved. Signal logic is running." : "Rules saved. Signal logic is disabled.");
    } catch (saveError) {
      setError(saveError instanceof Error ? saveError.message : String(saveError));
    } finally {
      setSaving(false);
    }
  };

  const updateGroup = (
    groupId: string,
    update: (group: SignalLogicRuleGroupDto) => SignalLogicRuleGroupDto
  ): void => {
    setMessage(null);
    setGroups(current => current.map(group =>
      group.id === groupId ? { ...update(group), defaultAspect: "red" } : group
    ));
  };

  const updateRule = (
    groupId: string,
    ruleId: string,
    update: (rule: SignalLogicRuleDto) => SignalLogicRuleDto
  ): void => updateGroup(groupId, group => ({
    ...group,
    rules: group.rules.map(rule => rule.id === ruleId ? update(rule) : rule),
  }));

  const updateCondition = (
    groupId: string,
    ruleId: string,
    conditionId: string,
    update: (condition: SignalLogicConditionDto) => SignalLogicConditionDto
  ): void => updateRule(groupId, ruleId, rule => ({
    ...rule,
    conditions: rule.conditions.map(condition => condition.id === conditionId ? update(condition) : condition),
  }));

  const addGroup = (): void => {
    const used = new Set(groups.map(group => group.signalId));
    const signal = signalOptions.find(item => !used.has(item.value));
    if (!signal || groups.length >= MAX_GROUPS) return;
    const group: SignalLogicRuleGroupDto = {
      id: generateId(),
      signalId: signal.value,
      defaultAspect: "red",
      rules: [newRule(turnoutOptions[0]?.value ?? "")],
    };
    setGroups(current => [...current, group]);
    setSelectedGroupId(group.id);
  };

  const allowedAspects = (signalId: string): SignalAspect[] => {
    const signal = signalOptions.find(item => item.value === signalId);
    return getAllowedSignalAspects(signal?.aspect ?? 2).filter(aspect => aspect !== "red");
  };

  const availableSignalOptions = signalOptions.filter(option =>
    !groups.some(group => group.id !== selectedGroup?.id && group.signalId === option.value)
  );
  const signalAddressForId = (signalId: string): number =>
    signalOptions.find(option => option.value === signalId)?.address ?? 0;

  return (
    <AppModal
      opened={opened}
      onClose={onClose}
      title="Signal logic"
      size={1100}
      centered
      draggable
      styles={{
        content: { height: "min(820px, calc(100vh - 40px))", display: "flex", flexDirection: "column" },
        body: { flex: 1, minHeight: 0, overflow: "hidden" },
      }}
    >
      <Stack h="100%" gap="sm">
        <Group justify="space-between" wrap="wrap">
          <div>
            <Text fw={700}>Automatic signal aspects</Text>
            <Text size="sm" c="dimmed">The first matching rule wins. If none match, the signal is RED.</Text>
          </div>
          <Group gap="xs">
            <Badge color={runtime.running ? "green" : "gray"} variant="light">
              {runtime.running ? "RUNNING" : "STOPPED"}
            </Badge>
            <Switch
              checked={runtime.enabled}
              label="Enabled"
              onChange={event => setRuntime(current => ({ ...current, enabled: event.currentTarget.checked }))}
            />
          </Group>
        </Group>

        {loading && <Group gap="xs"><Loader size="xs" /><Text size="sm">Loading rules…</Text></Group>}
        {error && <Alert color="red" icon={<IconAlertTriangle size={16} />}>{error}</Alert>}
        {message && !error && <Alert color="green">{message}</Alert>}
        {allIssues.length > 0 && (
          <Alert color={hasErrors ? "red" : "yellow"} icon={<IconAlertTriangle size={16} />} py="xs">
            <Stack gap={3}>
              {allIssues.slice(0, 6).map((issue, index) => (
                <Text size="sm" key={`${issue.message}-${index}`}>{issue.message}</Text>
              ))}
              {allIssues.length > 6 && <Text size="xs">+{allIssues.length - 6} more issue(s)</Text>}
            </Stack>
          </Alert>
        )}

        <Group align="stretch" wrap="nowrap" style={{ flex: 1, minHeight: 0 }}>
          <Card withBorder p="sm" w={235} style={{ flex: "0 0 235px" }}>
            <Group justify="space-between" mb="sm">
              <Title order={5}>Signals</Title>
              <ActionIcon
                variant="light"
                onClick={addGroup}
                disabled={groups.length >= MAX_GROUPS || !signalOptions.some(option => !groups.some(group => group.signalId === option.value))}
                title="Add signal"
              >
                <IconPlus size={16} />
              </ActionIcon>
            </Group>
            <ScrollArea h="100%" mah={570}>
              <Stack gap="xs">
                {groups.length === 0 && <Text size="sm" c="dimmed">Add a signal from the layout.</Text>}
                {[...groups].sort((a, b) => signalAddressForId(a.signalId) - signalAddressForId(b.signalId)).map(group => (
                  <Button
                    key={group.id}
                    variant={selectedGroup?.id === group.id ? "filled" : "light"}
                    justify="space-between"
                    onClick={() => setSelectedGroupId(group.id)}
                  >
                    <span>Signal #{signalAddressForId(group.signalId) || "missing"}</span>
                    <Badge size="xs" variant="light">{group.rules.length}</Badge>
                  </Button>
                ))}
              </Stack>
            </ScrollArea>
          </Card>

          <ScrollArea style={{ flex: 1 }} offsetScrollbars>
            <Stack pr="sm" pb="sm">
              {!selectedGroup && (
                <Card withBorder><Text c="dimmed">Select a signal or add a new one.</Text></Card>
              )}

              {selectedGroup && (
                <>
                  <Card withBorder>
                    <Group justify="space-between" align="flex-end">
                      <Group align="flex-end">
                        <Select
                          label="Signal"
                          data={availableSignalOptions}
                          value={selectedGroup.signalId || null}
                          onChange={value => {
                            const signalId = value ?? "";
                            updateGroup(selectedGroup.id, group => {
                              const { signalAddress: _legacyAddress, ...currentGroup } = group;
                              return {
                              ...currentGroup,
                              signalId,
                              rules: group.rules.map(rule => ({
                                ...rule,
                                aspect: allowedAspects(signalId)[0] ?? "green",
                              })),
                            }; });
                          }}
                          w={260}
                        />
                        <Badge color="red" size="lg">RED fallback</Badge>
                      </Group>
                      <ActionIcon
                        color="red"
                        variant="light"
                        title="Delete signal rules"
                        onClick={() => {
                          const next = groups.filter(group => group.id !== selectedGroup.id);
                          setGroups(next);
                          setSelectedGroupId(next[0]?.id ?? null);
                        }}
                      >
                        <IconTrash size={16} />
                      </ActionIcon>
                    </Group>
                  </Card>

                  {selectedGroup.rules.map((rule, ruleIndex) => (
                    <Card key={rule.id} withBorder>
                      <Group justify="space-between" mb="sm">
                        <Group>
                          <Badge color={aspectColor(rule.aspect)}>Rule {ruleIndex + 1}</Badge>
                          <Select
                            data={allowedAspects(selectedGroup.signalId).map(aspect => ({ value: aspect, label: aspect.toUpperCase() }))}
                            value={rule.aspect}
                            onChange={value => updateRule(selectedGroup.id, rule.id, current => ({
                              ...current,
                              aspect: (value ?? "green") as SignalAspect,
                            }))}
                            w={150}
                          />
                        </Group>
                        <ActionIcon
                          color="red"
                          variant="subtle"
                          title="Delete rule"
                          onClick={() => updateGroup(selectedGroup.id, group => ({
                            ...group,
                            rules: group.rules.filter(item => item.id !== rule.id),
                          }))}
                        >
                          <IconTrash size={16} />
                        </ActionIcon>
                      </Group>

                      <Stack gap="xs">
                        {rule.conditions.length === 0 && (
                          <Text size="sm" c="orange">No condition: this rule always matches.</Text>
                        )}
                        {rule.conditions.map(condition => (
                          <Group key={condition.id} align="flex-end" wrap="wrap">
                            {condition.type === "turnout" ? (
                              <>
                                <Select
                                  label="Turnout"
                                  data={turnoutOptions}
                                  value={condition.turnoutId || null}
                                  onChange={value => updateCondition(selectedGroup.id, rule.id, condition.id, current =>
                                    current.type === "turnout" ? (() => {
                                      const { turnoutAddress: _legacyAddress, ...next } = current;
                                      return { ...next, turnoutId: value ?? "" };
                                    })() : current)}
                                  w={210}
                                />
                                <Select
                                  label="State"
                                  data={[{ value: "true", label: "Closed" }, { value: "false", label: "Thrown" }]}
                                  value={String(condition.closed)}
                                  onChange={value => updateCondition(selectedGroup.id, rule.id, condition.id, current =>
                                    current.type === "turnout" ? { ...current, closed: value === "true" } : current)}
                                  w={140}
                                />
                              </>
                            ) : (
                              <>
                                <Select
                                  label="Sensor"
                                  data={sensorOptions}
                                  value={condition.sensorId || null}
                                  onChange={value => updateCondition(selectedGroup.id, rule.id, condition.id, current =>
                                    current.type === "sensor" ? (() => {
                                      const { sensorAddress: _legacyAddress, ...next } = current;
                                      return { ...next, sensorId: value ?? "" };
                                    })() : current)}
                                  w={210}
                                />
                                <Select
                                  label="State"
                                  data={[{ value: "true", label: "Active" }, { value: "false", label: "Inactive" }]}
                                  value={String(condition.active)}
                                  onChange={value => updateCondition(selectedGroup.id, rule.id, condition.id, current =>
                                    current.type === "sensor" ? { ...current, active: value === "true" } : current)}
                                  w={140}
                                />
                              </>
                            )}
                            <ActionIcon
                              color="red"
                              variant="subtle"
                              title="Delete condition"
                              onClick={() => updateRule(selectedGroup.id, rule.id, current => ({
                                ...current,
                                conditions: current.conditions.filter(item => item.id !== condition.id),
                              }))}
                            >
                              <IconTrash size={16} />
                            </ActionIcon>
                          </Group>
                        ))}

                        <Group gap="xs">
                          <Button
                            size="xs"
                            variant="light"
                            leftSection={<IconPlus size={14} />}
                            disabled={rule.conditions.length >= MAX_CONDITIONS || turnoutOptions.length === 0}
                            onClick={() => updateRule(selectedGroup.id, rule.id, current => ({
                              ...current,
                              conditions: [...current.conditions, newTurnoutCondition(turnoutOptions[0]?.value ?? "")],
                            }))}
                          >
                            Turnout
                          </Button>
                          <Button
                            size="xs"
                            variant="light"
                            leftSection={<IconPlus size={14} />}
                            disabled={rule.conditions.length >= MAX_CONDITIONS || sensorOptions.length === 0}
                            onClick={() => updateRule(selectedGroup.id, rule.id, current => ({
                              ...current,
                              conditions: [...current.conditions, newSensorCondition(sensorOptions[0]?.value ?? "")],
                            }))}
                          >
                            Sensor
                          </Button>
                        </Group>
                      </Stack>
                    </Card>
                  ))}

                  <Button
                    variant="light"
                    leftSection={<IconPlus size={16} />}
                    disabled={selectedGroup.rules.length >= MAX_RULES}
                    onClick={() => updateGroup(selectedGroup.id, group => ({
                      ...group,
                      rules: [...group.rules, newRule(turnoutOptions[0]?.value ?? "")],
                    }))}
                  >
                    Add rule
                  </Button>
                </>
              )}
            </Stack>
          </ScrollArea>
        </Group>

        <Group justify="space-between">
          <Text size="xs" c="dimmed">Limits: {MAX_GROUPS} signals · {MAX_RULES} rules/signal · {MAX_CONDITIONS} conditions/rule</Text>
          <Group>
            <Button
              variant="light"
              color={hasErrors ? "red" : "teal"}
              leftSection={<IconCheck size={16} />}
              onClick={() => setMessage(hasErrors
                ? `Integrity check found ${allIssues.filter(issue => issue.level === "error").length} error(s).`
                : allIssues.length > 0
                  ? `Integrity check passed with ${allIssues.length} warning(s).`
                  : "Integrity check passed. Every referenced layout element exists.")}
            >
              Validate rules
            </Button>
            <Button variant="light" leftSection={<IconRefresh size={16} />} loading={loading} onClick={() => void load()}>
              Reload
            </Button>
            <Button leftSection={<IconDeviceFloppy size={16} />} loading={saving} disabled={hasErrors} onClick={() => void save()}>
              Save
            </Button>
          </Group>
        </Group>
      </Stack>
    </AppModal>
  );
}
