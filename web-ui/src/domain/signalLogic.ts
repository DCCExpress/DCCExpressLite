export type LegacySignalAspect =
  | "red"
  | "yellow"
  | "green"
  | "white";

/**
 * Compatibility alias for older callers.
 * New automation stores stateId instead.
 */
export type SignalAspect = LegacySignalAspect;

export const SIGNAL_ASPECTS = [
  "red",
  "yellow",
  "green",
  "white",
] as const satisfies readonly SignalAspect[];

/**
 * Compatibility helper for older UI/integrity code.
 * Dynamic signal automation does not use this to select rule results.
 */
export function getAllowedSignalAspects(
  signalAspect: number
): SignalAspect[] {
  if (signalAspect >= 4) {
    return ["red", "green", "yellow", "white"];
  }

  if (signalAspect >= 3) {
    return ["red", "green", "yellow"];
  }

  return ["red", "green"];
}

export type SignalLogicTurnoutConditionDto = {
  id: string;
  type: "turnout";
  turnoutId: string;
  turnoutAddress?: number;
  closed: boolean;
};

export type SignalLogicSensorConditionDto = {
  id: string;
  type: "sensor";
  sensorId: string;
  sensorAddress?: number;
  active: boolean;
};

export type SignalLogicConditionDto =
  | SignalLogicTurnoutConditionDto
  | SignalLogicSensorConditionDto;

export type SignalLogicRuleDto = {
  id: string;
  conditions: SignalLogicConditionDto[];

  /**
   * Stable ID of SignalOutputState.
   */
  stateId: string;

  /**
   * Version 2 compatibility only.
   */
  aspect?: LegacySignalAspect;
};

export type SignalLogicRuleGroupDto = {
  id: string;
  signalId: string;
  signalAddress?: number;

  /**
   * Stable state used when no rule matches.
   */
  defaultStateId: string;

  /**
   * Version 2 compatibility only.
   */
  defaultAspect?: LegacySignalAspect;

  rules: SignalLogicRuleDto[];
};

export type SignalLogicDocumentDto = {
  version: 3;
  enabled: boolean;
  groups: SignalLogicRuleGroupDto[];
};

export type SignalLogicRuntimeStateDto = {
  running: boolean;
  enabled: boolean;
};

export type SignalLogicKnownState = {
  id: string;
  label: string;
};

export type SignalLogicKnownSignal = {
  id: string;
  address: number;

  /**
   * Dynamic catalogue. Older project-integrity callers may omit it.
   */
  states?: SignalLogicKnownState[];

  /**
   * Legacy compatibility only.
   */
  aspect?: number;
};

export type SignalLogicKnownTurnout = {
  id: string;
  address: number;
};

export type SignalLogicKnownSensor = {
  id: string;
  address: number;
};

export type SignalLogicIntegrityOrphanSignalDto = {
  groupId: string;
  signalAddress: number;
  ruleCount: number;
  conditionCount: number;
};

export type SignalLogicIntegrityReportDto = {
  layoutSignalCount: number;
  ruleGroupCount: number;
  orphanSignals: SignalLogicIntegrityOrphanSignalDto[];
};

export type SignalLogicValidationIssue = {
  level: "error" | "warning";
  message: string;
  messageKey?: string;
  messageParams?: Record<string, string | number | boolean>;
  groupId?: string;
  ruleId?: string;
  conditionId?: string;
};

export const DEFAULT_SIGNAL_LOGIC_DOCUMENT: SignalLogicDocumentDto = {
  version: 3,
  enabled: false,
  groups: [],
};

type RawCondition = {
  id?: unknown;
  type?: unknown;
  turnoutAddress?: unknown;
  turnoutId?: unknown;
  closed?: unknown;
  sensorAddress?: unknown;
  sensorId?: unknown;
  active?: unknown;
};

type RawRule = {
  id?: unknown;
  stateId?: unknown;
  aspect?: unknown;
  conditions?: unknown;
};

type RawGroup = {
  id?: unknown;
  signalAddress?: unknown;
  signalId?: unknown;
  defaultStateId?: unknown;
  defaultAspect?: unknown;
  rules?: unknown;
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isLegacySignalAspect(value: unknown): value is LegacySignalAspect {
  return value === "red" ||
    value === "yellow" ||
    value === "green" ||
    value === "white";
}

function normalizeCondition(
  input: unknown,
  groupIndex: number,
  ruleIndex: number,
  conditionIndex: number
): SignalLogicConditionDto {
  const condition: RawCondition =
    isRecord(input) ? input : {};

  const id =
    typeof condition.id === "string" && condition.id.length > 0
      ? condition.id
      : `signal-condition-${groupIndex + 1}-${ruleIndex + 1}-${conditionIndex + 1}`;

  if (condition.type === "sensor") {
    return {
      id,
      type: "sensor",
      sensorId:
        typeof condition.sensorId === "string"
          ? condition.sensorId
          : "",
      ...(Number.isFinite(Number(condition.sensorAddress))
        ? { sensorAddress: Number(condition.sensorAddress) }
        : {}),
      active: Boolean(condition.active),
    };
  }

  return {
    id,
    type: "turnout",
    turnoutId:
      typeof condition.turnoutId === "string"
        ? condition.turnoutId
        : "",
    ...(Number.isFinite(Number(condition.turnoutAddress))
      ? { turnoutAddress: Number(condition.turnoutAddress) }
      : {}),
    closed: Boolean(condition.closed),
  };
}

export function normalizeSignalLogicDocument(
  input: unknown
): SignalLogicDocumentDto {
  if (!isRecord(input)) {
    return DEFAULT_SIGNAL_LOGIC_DOCUMENT;
  }

  const raw = input as {
    enabled?: unknown;
    autostart?: unknown;
    groups?: unknown;
  };

  const groups =
    Array.isArray(raw.groups)
      ? raw.groups
      : [];

  const enabled =
    typeof raw.enabled === "boolean"
      ? raw.enabled
      : Boolean(raw.autostart);

  return {
    version: 3,
    enabled,
    groups: groups.map((groupInput, groupIndex) => {
      const group: RawGroup =
        isRecord(groupInput)
          ? groupInput
          : {};

      const rules =
        Array.isArray(group.rules)
          ? group.rules
          : [];

      const defaultAspect =
        isLegacySignalAspect(group.defaultAspect)
          ? group.defaultAspect
          : undefined;

      return {
        id:
          typeof group.id === "string" && group.id.length > 0
            ? group.id
            : `signal-group-${groupIndex + 1}`,
        signalId:
          typeof group.signalId === "string"
            ? group.signalId
            : "",
        ...(Number.isFinite(Number(group.signalAddress))
          ? { signalAddress: Number(group.signalAddress) }
          : {}),
        defaultStateId:
          typeof group.defaultStateId === "string"
            ? group.defaultStateId
            : "",
        ...(defaultAspect
          ? { defaultAspect }
          : {}),
        rules: rules.map((ruleInput, ruleIndex) => {
          const rule: RawRule =
            isRecord(ruleInput)
              ? ruleInput
              : {};

          const conditions =
            Array.isArray(rule.conditions)
              ? rule.conditions
              : [];

          const aspect =
            isLegacySignalAspect(rule.aspect)
              ? rule.aspect
              : undefined;

          return {
            id:
              typeof rule.id === "string" && rule.id.length > 0
                ? rule.id
                : `signal-rule-${groupIndex + 1}-${ruleIndex + 1}`,
            stateId:
              typeof rule.stateId === "string"
                ? rule.stateId
                : "",
            ...(aspect ? { aspect } : {}),
            conditions: conditions.map(
              (condition, conditionIndex) =>
                normalizeCondition(
                  condition,
                  groupIndex,
                  ruleIndex,
                  conditionIndex
                )
            ),
          };
        }),
      };
    }),
  };
}

function conditionSignature(
  condition: SignalLogicConditionDto
): string {
  if (condition.type === "sensor") {
    return `sensor:${condition.sensorId}:${condition.active}`;
  }

  return `turnout:${condition.turnoutId}:${condition.closed}`;
}

function findStateByLegacyAspect(
  signal: SignalLogicKnownSignal,
  aspect: LegacySignalAspect | undefined
): SignalLogicKnownState | undefined {
  if (!aspect) {
    return undefined;
  }

  const wanted = aspect.toLowerCase();

  return (signal.states ?? []).find(
    state => state.label.trim().toLowerCase() === wanted
  );
}

export type SignalLogicMigrationResult = {
  document: SignalLogicDocumentDto;
  migratedReferences: number;
  issues: SignalLogicValidationIssue[];
};

export function migrateSignalLogicReferences(
  input: SignalLogicDocumentDto,
  knownSignals: SignalLogicKnownSignal[],
  knownTurnouts: SignalLogicKnownTurnout[],
  knownSensors: SignalLogicKnownSensor[]
): SignalLogicMigrationResult {
  const document = structuredClone(input);
  const issues: SignalLogicValidationIssue[] = [];
  let migratedReferences = 0;

  for (const group of document.groups) {
    if (!group.signalId && (group.signalAddress ?? 0) > 0) {
      const matches =
        knownSignals.filter(
          signal => signal.address === group.signalAddress
        );

      if (matches.length === 1) {
        group.signalId = matches[0]!.id;
        delete group.signalAddress;
        migratedReferences++;
      } else {
        issues.push({
          level: "error",
          groupId: group.id,
          message:
            matches.length === 0
              ? `Legacy signal #${group.signalAddress} cannot be found on the layout.`
              : `Legacy signal #${group.signalAddress} is ambiguous because multiple layout elements use that address.`,
        });
      }
    }

    const knownSignal =
      knownSignals.find(signal => signal.id === group.signalId);

    if (knownSignal && !group.defaultStateId) {
      const migrated =
        findStateByLegacyAspect(
          knownSignal,
          group.defaultAspect
        ) ?? (knownSignal.states ?? [])[0];

      if (migrated) {
        group.defaultStateId = migrated.id;
        delete group.defaultAspect;
        migratedReferences++;
      }
    }

    for (const rule of group.rules) {
      if (knownSignal && !rule.stateId) {
        const migrated =
          findStateByLegacyAspect(
            knownSignal,
            rule.aspect
          );

        if (migrated) {
          rule.stateId = migrated.id;
          delete rule.aspect;
          migratedReferences++;
        } else if (rule.aspect) {
          issues.push({
            level: "error",
            groupId: group.id,
            ruleId: rule.id,
            message:
              `Legacy aspect "${rule.aspect}" cannot be mapped to a state of signal #${knownSignal.address}.`,
          });
        }
      }

      for (const condition of rule.conditions) {
        if (
          condition.type === "turnout" &&
          !condition.turnoutId &&
          (condition.turnoutAddress ?? 0) > 0
        ) {
          const matches =
            knownTurnouts.filter(
              turnout =>
                turnout.address === condition.turnoutAddress
            );

          if (matches.length === 1) {
            condition.turnoutId = matches[0]!.id;
            delete condition.turnoutAddress;
            migratedReferences++;
          } else {
            issues.push({
              level: "error",
              groupId: group.id,
              ruleId: rule.id,
              conditionId: condition.id,
              message:
                matches.length === 0
                  ? `Legacy turnout #${condition.turnoutAddress} cannot be found on the layout.`
                  : `Legacy turnout #${condition.turnoutAddress} is ambiguous.`,
            });
          }
        }

        if (
          condition.type === "sensor" &&
          !condition.sensorId &&
          (condition.sensorAddress ?? 0) > 0
        ) {
          const matches =
            knownSensors.filter(
              sensor =>
                sensor.address === condition.sensorAddress
            );

          if (matches.length === 1) {
            condition.sensorId = matches[0]!.id;
            delete condition.sensorAddress;
            migratedReferences++;
          } else {
            issues.push({
              level: "error",
              groupId: group.id,
              ruleId: rule.id,
              conditionId: condition.id,
              message:
                matches.length === 0
                  ? `Legacy sensor #${condition.sensorAddress} cannot be found on the layout.`
                  : `Legacy sensor #${condition.sensorAddress} is ambiguous.`,
            });
          }
        }
      }
    }
  }

  return {
    document,
    migratedReferences,
    issues,
  };
}

export function validateSignalLogicDocument(
  document: SignalLogicDocumentDto,
  knownSignals: SignalLogicKnownSignal[] = [],
  knownTurnouts: SignalLogicKnownTurnout[] = [],
  knownSensors: SignalLogicKnownSensor[] = []
): SignalLogicValidationIssue[] {
  const issues: SignalLogicValidationIssue[] = [];

  const knownSignalById =
    new Map(
      knownSignals.map(signal => [signal.id, signal])
    );

  const knownTurnoutById =
    new Map(
      knownTurnouts.map(turnout => [turnout.id, turnout])
    );

  const knownSensorById =
    new Map(
      knownSensors.map(sensor => [sensor.id, sensor])
    );

  const usedSignalIds = new Set<string>();

  for (const group of document.groups) {
    if (!group.signalId) {
      issues.push({
        level: "error",
        groupId: group.id,
        message:
          "Signal rule group has no valid signal element ID.",
      });
    }

    if (
      group.signalId &&
      usedSignalIds.has(group.signalId)
    ) {
      issues.push({
        level: "warning",
        groupId: group.id,
        message:
          "The same signal element has more than one rule group.",
      });
    }

    usedSignalIds.add(group.signalId);

    const signal =
      knownSignalById.get(group.signalId);

    if (knownSignals.length > 0 && !signal) {
      issues.push({
        level: "error",
        groupId: group.id,
        message:
          `Referenced signal element was deleted or has the wrong type (${group.signalId || "missing ID"}).`,
      });

      continue;
    }

    if (signal?.states) {
      const stateIds =
        new Set(signal.states.map(state => state.id));

      if (
        !group.defaultStateId ||
        !stateIds.has(group.defaultStateId)
      ) {
        issues.push({
          level: "error",
          groupId: group.id,
          message:
            `Signal #${signal.address} has an invalid default state.`,
        });
      }

      for (const rule of group.rules) {
        if (
          !rule.stateId ||
          !stateIds.has(rule.stateId)
        ) {
          issues.push({
            level: "error",
            groupId: group.id,
            ruleId: rule.id,
            message:
              `Signal #${signal.address} has a rule that references a missing state.`,
          });
        }
      }
    }

    if (group.rules.length === 0) {
      issues.push({
        level: "warning",
        groupId: group.id,
        message:
          `Signal #${signal?.address ?? "?"} has no rules and will always use its default state.`,
      });
    }

    const signatures = new Set<string>();

    for (const rule of group.rules) {
      if (rule.conditions.length === 0) {
        issues.push({
          level: "warning",
          groupId: group.id,
          ruleId: rule.id,
          message:
            "Rule has no conditions and will always match.",
        });
      }

      const signature =
        rule.conditions
          .map(conditionSignature)
          .sort()
          .join("|");

      if (
        signature.length > 0 &&
        signatures.has(signature)
      ) {
        issues.push({
          level: "warning",
          groupId: group.id,
          ruleId: rule.id,
          message:
            "Another rule has the same conditions.",
        });
      }

      signatures.add(signature);

      const conditionKeys =
        new Set<string>();

      for (const condition of rule.conditions) {
        const elementId =
          condition.type === "sensor"
            ? condition.sensorId
            : condition.turnoutId;

        const key =
          `${condition.type}:${elementId}`;

        if (conditionKeys.has(key)) {
          issues.push({
            level: "warning",
            groupId: group.id,
            ruleId: rule.id,
            conditionId: condition.id,
            message:
              "The same input is used more than once in the same rule.",
          });
        }

        conditionKeys.add(key);

        if (condition.type === "sensor") {
          if (!condition.sensorId) {
            issues.push({
              level: "error",
              groupId: group.id,
              ruleId: rule.id,
              conditionId: condition.id,
              message:
                "Sensor condition has no valid sensor element ID.",
            });
          } else if (
            knownSensors.length > 0 &&
            !knownSensorById.has(condition.sensorId)
          ) {
            issues.push({
              level: "error",
              groupId: group.id,
              ruleId: rule.id,
              conditionId: condition.id,
              message:
                `Referenced sensor was deleted (${condition.sensorId}).`,
            });
          }

          continue;
        }

        if (!condition.turnoutId) {
          issues.push({
            level: "error",
            groupId: group.id,
            ruleId: rule.id,
            conditionId: condition.id,
            message:
              "Turnout condition has no valid turnout element ID.",
          });
        } else if (
          knownTurnouts.length > 0 &&
          !knownTurnoutById.has(condition.turnoutId)
        ) {
          issues.push({
            level: "error",
            groupId: group.id,
            ruleId: rule.id,
            conditionId: condition.id,
            message:
              `Referenced turnout was deleted (${condition.turnoutId}).`,
          });
        }
      }
    }
  }

  return issues;
}
