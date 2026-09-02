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
  IconDeviceFloppy,
  IconPlus,
  IconRefresh,
  IconTrash,
} from "@tabler/icons-react";

import {
  useEffect,
  useMemo,
  useState,
} from "react";

import {
  loadSignalLogicRulesWs,
  saveSignalLogicRulesWs,
} from "@/api/signalLogicWsApi";

import AppModal from "@/components/common/AppModal";
import { generateId } from "@/helpers";

import {
  isTurnoutElement,
  type LayoutView,
} from "@/models/editor/core/LayoutView";

import {
  TrackSensorElementView,
} from "@/models/editor/elements/TrackSensorElementView";

import {
  TrackSignalElementView,
} from "@/models/editor/elements/TrackSignalElementView";

import type {
  SignalLogicConditionDto,
  SignalLogicDocumentDto,
  SignalLogicKnownSignal,
  SignalLogicRuleDto,
  SignalLogicRuleGroupDto,
  SignalLogicRuntimeStateDto,
  SignalLogicValidationIssue,
} from "@domain/signalLogic";

import {
  migrateSignalLogicReferences,
  validateSignalLogicDocument,
} from "@domain/signalLogic";

type SignalLogicDialogProps = {
  opened: boolean;
  onClose: () => void;
  layout: LayoutView;
};

type Option = {
  value: string;
  label: string;
};

type SignalOption =
  Option &
  Omit<SignalLogicKnownSignal, "states"> & {
    states: NonNullable<SignalLogicKnownSignal["states"]>;
  };

type ElementOption = Option & {
  address: number;
};

const MAX_GROUPS = 24;
const MAX_RULES = 6;
const MAX_CONDITIONS = 6;

function newTurnoutCondition(
  turnoutId: string
): SignalLogicConditionDto {
  return {
    id: generateId(),
    type: "turnout",
    turnoutId,
    closed: true,
  };
}

function newSensorCondition(
  sensorId: string
): SignalLogicConditionDto {
  return {
    id: generateId(),
    type: "sensor",
    sensorId,
    active: true,
  };
}

function stateOptions(
  signal: SignalOption | undefined
): Option[] {
  return (
    signal?.states.map(state => ({
      value: state.id,
      label: state.label,
    })) ?? []
  );
}

export default function SignalLogicDialog({
  opened,
  onClose,
  layout,
}: SignalLogicDialogProps) {
  const [
    groups,
    setGroups,
  ] =
    useState<SignalLogicRuleGroupDto[]>([]);

  const [
    selectedGroupId,
    setSelectedGroupId,
  ] =
    useState<string | null>(null);

  const [
    runtime,
    setRuntime,
  ] =
    useState<SignalLogicRuntimeStateDto>({
      running: false,
      enabled: false,
    });

  const [loading, setLoading] =
    useState(false);

  const [saving, setSaving] =
    useState(false);

  const [message, setMessage] =
    useState<string | null>(null);

  const [error, setError] =
    useState<string | null>(null);

  const [
    migrationIssues,
    setMigrationIssues,
  ] =
    useState<SignalLogicValidationIssue[]>([]);

  const signalOptions =
    useMemo<SignalOption[]>(
      () =>
        layout
          .getAllElements()
          .filter(
            (
              element
            ): element is TrackSignalElementView =>
              element instanceof
                TrackSignalElementView &&
              element.signalOutput.address > 0
          )
          .map(signal => ({
            value: signal.id,
            label:
              `Signal #${signal.signalOutput.address}` +
              (
                signal.name &&
                signal.name !== "element"
                  ? ` · ${signal.name}`
                  : ""
              ),
            id: signal.id,
            address:
              signal.signalOutput.address,
            states:
              signal.signalOutput.states.map(
                state => ({
                  id: state.id,
                  label: state.label,
                })
              ),
          }))
          .sort(
            (a, b) =>
              a.address - b.address
          ),
      [layout]
    );

  const turnoutOptions =
    useMemo<ElementOption[]>(
      () =>
        layout
          .getAllElements()
          .filter(isTurnoutElement)
          .filter(
            turnout =>
              turnout.turnoutAddress > 0
          )
          .map(turnout => ({
            value: turnout.id,
            label:
              `Turnout #${turnout.turnoutAddress}` +
              (
                turnout.name &&
                turnout.name !== "element"
                  ? ` · ${turnout.name}`
                  : ""
              ),
            address:
              turnout.turnoutAddress,
          }))
          .sort(
            (a, b) =>
              a.address - b.address
          ),
      [layout]
    );

  const sensorOptions =
    useMemo<ElementOption[]>(
      () =>
        layout
          .getAllElements()
          .filter(
            (
              element
            ): element is TrackSensorElementView =>
              element instanceof
                TrackSensorElementView &&
              element.address > 0
          )
          .map(sensor => ({
            value: sensor.id,
            label:
              `Sensor #${sensor.address}` +
              (
                sensor.name &&
                sensor.name !== "element"
                  ? ` · ${sensor.name}`
                  : ""
              ),
            address: sensor.address,
          }))
          .sort(
            (a, b) =>
              a.address - b.address
          ),
      [layout]
    );

  const document =
    useMemo<SignalLogicDocumentDto>(
      () => ({
        version: 3,
        enabled: runtime.enabled,
        groups,
      }),
      [groups, runtime.enabled]
    );

  const issues =
    useMemo(
      () =>
        validateSignalLogicDocument(
          document,
          signalOptions.map(
            signal => ({
              id: signal.id,
              address: signal.address,
              states: signal.states,
            })
          ),
          turnoutOptions.map(
            turnout => ({
              id: turnout.value,
              address: turnout.address,
            })
          ),
          sensorOptions.map(
            sensor => ({
              id: sensor.value,
              address: sensor.address,
            })
          )
        ),
      [
        document,
        signalOptions,
        turnoutOptions,
        sensorOptions,
      ]
    );

  const allIssues = [
    ...migrationIssues,
    ...issues,
  ];

  const hasErrors =
    allIssues.some(
      issue => issue.level === "error"
    );

  const selectedGroup =
    groups.find(
      group =>
        group.id === selectedGroupId
    ) ??
    groups[0] ??
    null;

  const knownSignals =
    signalOptions.map(signal => ({
      id: signal.id,
      address: signal.address,
      states: signal.states,
    }));

  const load = async (): Promise<void> => {
    setLoading(true);
    setError(null);
    setMessage(null);

    try {
      const result =
        await loadSignalLogicRulesWs();

      const migration =
        migrateSignalLogicReferences(
          result.document,
          knownSignals,
          turnoutOptions.map(
            turnout => ({
              id: turnout.value,
              address: turnout.address,
            })
          ),
          sensorOptions.map(
            sensor => ({
              id: sensor.value,
              address: sensor.address,
            })
          )
        );

      setMigrationIssues(
        migration.issues
      );

      let loadedDocument =
        migration.document;

      if (
        migration.migratedReferences > 0 &&
        !migration.issues.some(
          issue => issue.level === "error"
        )
      ) {
        const saved =
          await saveSignalLogicRulesWs(
            migration.document
          );

        loadedDocument =
          saved.document;
      }

      setGroups(
        loadedDocument.groups
      );

      setSelectedGroupId(
        loadedDocument.groups[0]?.id ??
          null
      );

      setRuntime(result.state);

      setMessage(
        migration.migratedReferences > 0
          ? `${migration.migratedReferences} legacy reference(s) migrated to dynamic signal state IDs.`
          : "Signal logic loaded."
      );
    } catch (loadError) {
      setError(
        loadError instanceof Error
          ? loadError.message
          : String(loadError)
      );
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (opened) {
      void load();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [opened]);

  const save =
    async (): Promise<void> => {
      setSaving(true);
      setError(null);
      setMessage(null);

      try {
        const result =
          await saveSignalLogicRulesWs(
            document
          );

        setGroups(
          result.document.groups
        );

        setRuntime(result.state);

        setMessage(
          result.state.running
            ? "Rules saved. Signal logic is running."
            : "Rules saved. Signal logic is disabled."
        );
      } catch (saveError) {
        setError(
          saveError instanceof Error
            ? saveError.message
            : String(saveError)
        );
      } finally {
        setSaving(false);
      }
    };

  const updateGroup = (
    groupId: string,
    update: (
      group: SignalLogicRuleGroupDto
    ) => SignalLogicRuleGroupDto
  ): void => {
    setMessage(null);

    setGroups(current =>
      current.map(group =>
        group.id === groupId
          ? update(group)
          : group
      )
    );
  };

  const updateRule = (
    groupId: string,
    ruleId: string,
    update: (
      rule: SignalLogicRuleDto
    ) => SignalLogicRuleDto
  ): void =>
    updateGroup(
      groupId,
      group => ({
        ...group,
        rules: group.rules.map(
          rule =>
            rule.id === ruleId
              ? update(rule)
              : rule
        ),
      })
    );

  const updateCondition = (
    groupId: string,
    ruleId: string,
    conditionId: string,
    update: (
      condition:
        SignalLogicConditionDto
    ) => SignalLogicConditionDto
  ): void =>
    updateRule(
      groupId,
      ruleId,
      rule => ({
        ...rule,
        conditions:
          rule.conditions.map(
            condition =>
              condition.id ===
              conditionId
                ? update(condition)
                : condition
          ),
      })
    );

  const addGroup = (): void => {
    if (groups.length >= MAX_GROUPS) {
      return;
    }

    const used =
      new Set(
        groups.map(
          group => group.signalId
        )
      );

    const signal =
      signalOptions.find(
        item => !used.has(item.id)
      );

    const firstState =
      signal?.states[0];

    if (!signal || !firstState) {
      return;
    }

    const group:
      SignalLogicRuleGroupDto = {
        id: generateId(),
        signalId: signal.id,
        defaultStateId:
          firstState.id,
        rules: [],
      };

    setGroups(current => [
      ...current,
      group,
    ]);

    setSelectedGroupId(group.id);
  };

  const deleteGroup = (
    groupId: string
  ): void => {
    setGroups(current => {
      const next =
        current.filter(
          group =>
            group.id !== groupId
        );

      setSelectedGroupId(
        next[0]?.id ?? null
      );

      return next;
    });
  };

  const addRule = (
    group: SignalLogicRuleGroupDto
  ): void => {
    if (
      group.rules.length >=
      MAX_RULES
    ) {
      return;
    }

    const signal =
      signalOptions.find(
        item =>
          item.id ===
          group.signalId
      );

    const stateId =
      signal?.states[0]?.id ??
      "";

    const firstTurnout =
      turnoutOptions[0]?.value ??
      "";

    updateGroup(
      group.id,
      current => ({
        ...current,
        rules: [
          ...current.rules,
          {
            id: generateId(),
            stateId,
            conditions:
              firstTurnout
                ? [
                    newTurnoutCondition(
                      firstTurnout
                    ),
                  ]
                : [],
          },
        ],
      })
    );
  };

  const signalAddressForId = (
    signalId: string
  ): number =>
    signalOptions.find(
      signal =>
        signal.id === signalId
    )?.address ?? 0;

  const availableSignals =
    signalOptions.filter(
      option =>
        !groups.some(
          group =>
            group.id !==
              selectedGroup?.id &&
            group.signalId ===
              option.id
        )
    );

  return (
    <AppModal
      opened={opened}
      onClose={onClose}
      title="Signal logic"
      size={1150}
      centered
      draggable
      styles={{
        content: {
          height:
            "min(840px, calc(100vh - 40px))",
          display: "flex",
          flexDirection: "column",
        },
        body: {
          flex: 1,
          minHeight: 0,
          overflow: "hidden",
        },
      }}
    >
      <Stack h="100%" gap="sm">
        <Group
          justify="space-between"
          wrap="wrap"
        >
          <div>
            <Text fw={700}>
              Automatic signal states
            </Text>
            <Text
              size="sm"
              c="dimmed"
            >
              First matching rule wins.
              If none match, the configured
              default state is used.
            </Text>
          </div>

          <Group gap="xs">
            <Badge
              color={
                runtime.running
                  ? "green"
                  : "gray"
              }
              variant="light"
            >
              {runtime.running
                ? "RUNNING"
                : "STOPPED"}
            </Badge>

            <Switch
              checked={
                runtime.enabled
              }
              label="Enabled"
              onChange={event => {
                const enabled =
                  event.currentTarget.checked;

                setRuntime(
                  current => ({
                    ...current,
                    enabled,
                  })
                );
              }}
            />

            <ActionIcon
              variant="light"
              title="Reload"
              onClick={() =>
                void load()
              }
            >
              <IconRefresh size={16} />
            </ActionIcon>

            <Button
              size="xs"
              leftSection={
                <IconDeviceFloppy
                  size={16}
                />
              }
              loading={saving}
              disabled={hasErrors}
              onClick={() =>
                void save()
              }
            >
              Save
            </Button>
          </Group>
        </Group>

        {loading && (
          <Group gap="xs">
            <Loader size="xs" />
            <Text size="sm">
              Loading rules…
            </Text>
          </Group>
        )}

        {error && (
          <Alert
            color="red"
            icon={
              <IconAlertTriangle
                size={16}
              />
            }
          >
            {error}
          </Alert>
        )}

        {message && !error && (
          <Alert color="green">
            {message}
          </Alert>
        )}

        {allIssues.length > 0 && (
          <Alert
            color={
              hasErrors
                ? "red"
                : "yellow"
            }
            icon={
              <IconAlertTriangle
                size={16}
              />
            }
          >
            <Stack gap={3}>
              {allIssues
                .slice(0, 8)
                .map(
                  (
                    issue,
                    index
                  ) => (
                    <Text
                      size="sm"
                      key={`${issue.message}-${index}`}
                    >
                      {issue.message}
                    </Text>
                  )
                )}
            </Stack>
          </Alert>
        )}

        <Group
          align="stretch"
          wrap="nowrap"
          style={{
            flex: 1,
            minHeight: 0,
          }}
        >
          <Card
            withBorder
            p="sm"
            w={245}
            style={{
              flex:
                "0 0 245px",
            }}
          >
            <Group
              justify="space-between"
              mb="sm"
            >
              <Title order={5}>
                Signals
              </Title>

              <ActionIcon
                variant="light"
                title="Add signal"
                onClick={addGroup}
                disabled={
                  groups.length >=
                    MAX_GROUPS ||
                  !signalOptions.some(
                    option =>
                      !groups.some(
                        group =>
                          group.signalId ===
                          option.id
                      )
                  )
                }
              >
                <IconPlus size={16} />
              </ActionIcon>
            </Group>

            <ScrollArea
              h="100%"
              mah={590}
            >
              <Stack gap="xs">
                {groups.length === 0 && (
                  <Text
                    size="sm"
                    c="dimmed"
                  >
                    Add a signal from
                    the layout.
                  </Text>
                )}

                {[...groups]
                  .sort(
                    (a, b) =>
                      signalAddressForId(
                        a.signalId
                      ) -
                      signalAddressForId(
                        b.signalId
                      )
                  )
                  .map(group => (
                    <Button
                      key={group.id}
                      variant={
                        selectedGroup?.id ===
                        group.id
                          ? "filled"
                          : "light"
                      }
                      justify="space-between"
                      onClick={() =>
                        setSelectedGroupId(
                          group.id
                        )
                      }
                    >
                      <span>
                        Signal #
                        {signalAddressForId(
                          group.signalId
                        ) ||
                          "missing"}
                      </span>

                      <Badge
                        size="xs"
                        variant="light"
                      >
                        {group.rules.length}
                      </Badge>
                    </Button>
                  ))}
              </Stack>
            </ScrollArea>
          </Card>

          <ScrollArea
            style={{ flex: 1 }}
            offsetScrollbars
          >
            <Stack pr="sm" pb="sm">
              {!selectedGroup && (
                <Card withBorder>
                  <Text c="dimmed">
                    Select a signal or
                    add a new one.
                  </Text>
                </Card>
              )}

              {selectedGroup && (() => {
                const selectedSignal =
                  signalOptions.find(
                    signal =>
                      signal.id ===
                      selectedGroup.signalId
                  );

                const states =
                  stateOptions(
                    selectedSignal
                  );

                return (
                  <>
                    <Card withBorder>
                      <Group
                        justify="space-between"
                        align="flex-end"
                        wrap="wrap"
                      >
                        <Group
                          align="flex-end"
                          wrap="wrap"
                        >
                          <Select
                            label="Signal"
                            data={
                              availableSignals
                            }
                            value={
                              selectedGroup.signalId ||
                              null
                            }
                            onChange={value => {
                              const signalId =
                                value ?? "";

                              const signal =
                                signalOptions.find(
                                  item =>
                                    item.id ===
                                    signalId
                                );

                              const firstStateId =
                                signal?.states[0]
                                  ?.id ?? "";

                              updateGroup(
                                selectedGroup.id,
                                group => ({
                                  ...group,
                                  signalId,
                                  defaultStateId:
                                    firstStateId,
                                  rules:
                                    group.rules.map(
                                      rule => ({
                                        ...rule,
                                        stateId:
                                          firstStateId,
                                      })
                                    ),
                                })
                              );
                            }}
                            w={280}
                          />

                          <Select
                            label="Default state"
                            data={states}
                            value={
                              selectedGroup.defaultStateId ||
                              null
                            }
                            onChange={value =>
                              updateGroup(
                                selectedGroup.id,
                                group => ({
                                  ...group,
                                  defaultStateId:
                                    value ?? "",
                                })
                              )
                            }
                            w={220}
                          />
                        </Group>

                        <ActionIcon
                          color="red"
                          variant="light"
                          title="Delete signal rules"
                          onClick={() =>
                            deleteGroup(
                              selectedGroup.id
                            )
                          }
                        >
                          <IconTrash
                            size={16}
                          />
                        </ActionIcon>
                      </Group>
                    </Card>

                    {selectedGroup.rules.map(
                      (
                        rule,
                        ruleIndex
                      ) => (
                        <Card
                          key={rule.id}
                          withBorder
                        >
                          <Group
                            justify="space-between"
                            mb="sm"
                          >
                            <Group>
                              <Badge
                                variant="light"
                                color="blue"
                              >
                                Rule{" "}
                                {ruleIndex + 1}
                              </Badge>

                              <Select
                                data={states}
                                value={
                                  rule.stateId ||
                                  null
                                }
                                onChange={value =>
                                  updateRule(
                                    selectedGroup.id,
                                    rule.id,
                                    current => ({
                                      ...current,
                                      stateId:
                                        value ??
                                        "",
                                    })
                                  )
                                }
                                placeholder="Signal state"
                                w={220}
                              />
                            </Group>

                            <ActionIcon
                              color="red"
                              variant="subtle"
                              title="Delete rule"
                              onClick={() =>
                                updateGroup(
                                  selectedGroup.id,
                                  group => ({
                                    ...group,
                                    rules:
                                      group.rules.filter(
                                        item =>
                                          item.id !==
                                          rule.id
                                      ),
                                  })
                                )
                              }
                            >
                              <IconTrash
                                size={16}
                              />
                            </ActionIcon>
                          </Group>

                          <Stack gap="xs">
                            {rule.conditions.map(
                              condition => (
                                <Group
                                  key={
                                    condition.id
                                  }
                                  align="flex-end"
                                  wrap="wrap"
                                >
                                  <Select
                                    label="Type"
                                    value={
                                      condition.type
                                    }
                                    data={[
                                      {
                                        value:
                                          "turnout",
                                        label:
                                          "Turnout",
                                      },
                                      {
                                        value:
                                          "sensor",
                                        label:
                                          "Sensor",
                                      },
                                    ]}
                                    onChange={value => {
                                      if (
                                        value ===
                                        "sensor"
                                      ) {
                                        updateCondition(
                                          selectedGroup.id,
                                          rule.id,
                                          condition.id,
                                          () =>
                                            newSensorCondition(
                                              sensorOptions[0]
                                                ?.value ??
                                                ""
                                            )
                                        );
                                      } else {
                                        updateCondition(
                                          selectedGroup.id,
                                          rule.id,
                                          condition.id,
                                          () =>
                                            newTurnoutCondition(
                                              turnoutOptions[0]
                                                ?.value ??
                                                ""
                                            )
                                        );
                                      }
                                    }}
                                    w={130}
                                  />

                                  {condition.type ===
                                  "sensor" ? (
                                    <>
                                      <Select
                                        label="Sensor"
                                        data={
                                          sensorOptions
                                        }
                                        value={
                                          condition.sensorId ||
                                          null
                                        }
                                        onChange={value =>
                                          updateCondition(
                                            selectedGroup.id,
                                            rule.id,
                                            condition.id,
                                            current => ({
                                              id: current.id,
                                              type: "sensor",
                                              sensorId:
                                                value ?? "",
                                              active:
                                                current.type === "sensor"
                                                  ? current.active
                                                  : false,
                                              ...(current.type === "sensor" &&
                                              current.sensorAddress !== undefined
                                                ? {
                                                    sensorAddress:
                                                      current.sensorAddress,
                                                  }
                                                : {}),
                                            })
                                          )
                                        }
                                        w={260}
                                      />

                                      <Select
                                        label="State"
                                        value={
                                          condition.active
                                            ? "active"
                                            : "inactive"
                                        }
                                        data={[
                                          {
                                            value:
                                              "active",
                                            label:
                                              "Active",
                                          },
                                          {
                                            value:
                                              "inactive",
                                            label:
                                              "Inactive",
                                          },
                                        ]}
                                        onChange={value =>
                                          updateCondition(
                                            selectedGroup.id,
                                            rule.id,
                                            condition.id,
                                            current => ({
                                              id: current.id,
                                              type: "sensor",
                                              sensorId:
                                                current.type === "sensor"
                                                  ? current.sensorId
                                                  : "",
                                              active:
                                                value === "active",
                                              ...(current.type === "sensor" &&
                                              current.sensorAddress !== undefined
                                                ? {
                                                    sensorAddress:
                                                      current.sensorAddress,
                                                  }
                                                : {}),
                                            })
                                          )
                                        }
                                        w={130}
                                      />
                                    </>
                                  ) : (
                                    <>
                                      <Select
                                        label="Turnout"
                                        data={
                                          turnoutOptions
                                        }
                                        value={
                                          condition.turnoutId ||
                                          null
                                        }
                                        onChange={value =>
                                          updateCondition(
                                            selectedGroup.id,
                                            rule.id,
                                            condition.id,
                                            current => ({
                                              id: current.id,
                                              type: "turnout",
                                              turnoutId:
                                                value ?? "",
                                              closed:
                                                current.type === "turnout"
                                                  ? current.closed
                                                  : false,
                                              ...(current.type === "turnout" &&
                                              current.turnoutAddress !== undefined
                                                ? {
                                                    turnoutAddress:
                                                      current.turnoutAddress,
                                                  }
                                                : {}),
                                            })
                                          )
                                        }
                                        w={260}
                                      />

                                      <Select
                                        label="State"
                                        value={
                                          condition.closed
                                            ? "closed"
                                            : "thrown"
                                        }
                                        data={[
                                          {
                                            value:
                                              "closed",
                                            label:
                                              "Closed",
                                          },
                                          {
                                            value:
                                              "thrown",
                                            label:
                                              "Thrown",
                                          },
                                        ]}
                                        onChange={value =>
                                          updateCondition(
                                            selectedGroup.id,
                                            rule.id,
                                            condition.id,
                                            current => ({
                                              id: current.id,
                                              type: "turnout",
                                              turnoutId:
                                                current.type === "turnout"
                                                  ? current.turnoutId
                                                  : "",
                                              closed:
                                                value === "closed",
                                              ...(current.type === "turnout" &&
                                              current.turnoutAddress !== undefined
                                                ? {
                                                    turnoutAddress:
                                                      current.turnoutAddress,
                                                  }
                                                : {}),
                                            })
                                          )
                                        }
                                        w={130}
                                      />
                                    </>
                                  )}

                                  <ActionIcon
                                    color="red"
                                    variant="subtle"
                                    title="Delete condition"
                                    onClick={() =>
                                      updateRule(
                                        selectedGroup.id,
                                        rule.id,
                                        current => ({
                                          ...current,
                                          conditions:
                                            current.conditions.filter(
                                              item =>
                                                item.id !==
                                                condition.id
                                            ),
                                        })
                                      )
                                    }
                                  >
                                    <IconTrash
                                      size={15}
                                    />
                                  </ActionIcon>
                                </Group>
                              )
                            )}

                            <Group>
                              <Button
                                size="xs"
                                variant="light"
                                leftSection={
                                  <IconPlus
                                    size={14}
                                  />
                                }
                                disabled={
                                  rule.conditions
                                    .length >=
                                  MAX_CONDITIONS
                                }
                                onClick={() =>
                                  updateRule(
                                    selectedGroup.id,
                                    rule.id,
                                    current => ({
                                      ...current,
                                      conditions: [
                                        ...current.conditions,
                                        newTurnoutCondition(
                                          turnoutOptions[0]
                                            ?.value ??
                                            ""
                                        ),
                                      ],
                                    })
                                  )
                                }
                              >
                                Add condition
                              </Button>
                            </Group>
                          </Stack>
                        </Card>
                      )
                    )}

                    <Button
                      variant="light"
                      leftSection={
                        <IconPlus
                          size={16}
                        />
                      }
                      disabled={
                        selectedGroup.rules
                          .length >=
                        MAX_RULES
                      }
                      onClick={() =>
                        addRule(
                          selectedGroup
                        )
                      }
                    >
                      Add rule
                    </Button>
                  </>
                );
              })()}
            </Stack>
          </ScrollArea>
        </Group>
      </Stack>
    </AppModal>
  );
}
