
export type SignalAspect = "red" | "yellow" | "green" | "white";

export const SIGNAL_ASPECTS = [
  "red",
  "yellow",
  "green",
  "white",
] as const satisfies readonly SignalAspect[];

export type SignalLogicTurnoutConditionDto = {
  id: string;
  type: "turnout";
  turnoutId: string;
  /** Kept only while importing a version 1 address-based document. */
  turnoutAddress?: number;
  closed: boolean;
};

export type SignalLogicSensorConditionDto = {
  id: string;
  type: "sensor";
  sensorId: string;
  /** Kept only while importing a version 1 address-based document. */
  sensorAddress?: number;
  active: boolean;
};

export type SignalLogicConditionDto =
  | SignalLogicTurnoutConditionDto
  | SignalLogicSensorConditionDto;

export type SignalLogicRuleDto = {
  id: string;
  conditions: SignalLogicConditionDto[];
  aspect: SignalAspect;
};

export type SignalLogicRuleGroupDto = {
  id: string;
  signalId: string;
  /** Kept only while importing a version 1 address-based document. */
  signalAddress?: number;
  defaultAspect: SignalAspect;
  rules: SignalLogicRuleDto[];
};

export type SignalLogicDocumentDto = {
  version: 2;
  enabled: boolean;
  groups: SignalLogicRuleGroupDto[];
};

export type SignalLogicRuntimeStateDto = {
  running: boolean;
  enabled: boolean;
};

export type SignalLogicKnownSignal = {
  id: string;
  address: number;
  aspect: number;
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
  version: 2,
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
  aspect?: unknown;
  conditions?: unknown;
};

type RawGroup = {
  id?: unknown;
  signalAddress?: unknown;
  signalId?: unknown;
  defaultAspect?: unknown;
  rules?: unknown;
};

export function getAllowedSignalAspects(signalAspect: number): SignalAspect[] {
  if (signalAspect >= 4) {
    return ["red", "green", "yellow", "white"];
  }

  if (signalAspect >= 3) {
    return ["red", "green", "yellow"];
  }

  return ["red", "green"];
}

export function isSignalAspect(value: unknown): value is SignalAspect {
  return typeof value === "string" && (SIGNAL_ASPECTS as readonly string[]).includes(value);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function getConditionId(
  condition: RawCondition,
  groupIndex: number,
  ruleIndex: number,
  conditionIndex: number
): string {
  return typeof condition.id === "string" && condition.id.length > 0
    ? condition.id
    : `signal-condition-${groupIndex + 1}-${ruleIndex + 1}-${conditionIndex + 1}`;
}

function normalizeCondition(
  input: unknown,
  groupIndex: number,
  ruleIndex: number,
  conditionIndex: number
): SignalLogicConditionDto {
  const condition: RawCondition = isRecord(input) ? input : {};
  const id = getConditionId(
    condition,
    groupIndex,
    ruleIndex,
    conditionIndex
  );

  if (condition.type === "sensor") {
    return {
      id,
      type: "sensor",
      sensorId: typeof condition.sensorId === "string" ? condition.sensorId : "",
      ...(Number.isFinite(Number(condition.sensorAddress))
        ? { sensorAddress: Number(condition.sensorAddress) }
        : {}),
      active: Boolean(condition.active),
    };
  }

  return {
    id,
    type: "turnout",
    turnoutId: typeof condition.turnoutId === "string" ? condition.turnoutId : "",
    ...(Number.isFinite(Number(condition.turnoutAddress))
      ? { turnoutAddress: Number(condition.turnoutAddress) }
      : {}),
    closed: Boolean(condition.closed),
  };
}

export function normalizeSignalLogicDocument(input: unknown): SignalLogicDocumentDto {
  if (!isRecord(input)) {
    return DEFAULT_SIGNAL_LOGIC_DOCUMENT;
  }

  const raw = input as { enabled?: unknown; autostart?: unknown; groups?: unknown };
  const groups = Array.isArray(raw.groups) ? raw.groups : [];
  const enabled = typeof raw.enabled === "boolean"
    ? raw.enabled
    : Boolean(raw.autostart);

  return {
    version: 2,
    enabled,
    groups: groups.map((groupInput, groupIndex) => {
      const group: RawGroup = isRecord(groupInput) ? groupInput : {};
      const rules = Array.isArray(group.rules) ? group.rules : [];

      return {
        id: typeof group.id === "string" && group.id.length > 0
          ? group.id
          : `signal-group-${groupIndex + 1}`,
        signalId: typeof group.signalId === "string" ? group.signalId : "",
        ...(Number.isFinite(Number(group.signalAddress))
          ? { signalAddress: Number(group.signalAddress) }
          : {}),
        defaultAspect: isSignalAspect(group.defaultAspect)
          ? group.defaultAspect
          : "red",
        rules: rules.map((ruleInput, ruleIndex) => {
          const rule: RawRule = isRecord(ruleInput) ? ruleInput : {};
          const conditions = Array.isArray(rule.conditions)
            ? rule.conditions
            : [];

          return {
            id: typeof rule.id === "string" && rule.id.length > 0
              ? rule.id
              : `signal-rule-${groupIndex + 1}-${ruleIndex + 1}`,
            aspect: isSignalAspect(rule.aspect)
              ? rule.aspect
              : "red",
            conditions: conditions.map((condition, conditionIndex) =>
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

function conditionSignature(condition: SignalLogicConditionDto): string {
  switch (condition.type) {
    case "sensor":
      return `sensor:${condition.sensorId}:${condition.active}`;
    case "turnout":
    default:
      return `turnout:${condition.turnoutId}:${condition.closed}`;
  }
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
      const matches = knownSignals.filter(signal => signal.address === group.signalAddress);
      if (matches.length === 1) {
        group.signalId = matches[0]!.id;
        delete group.signalAddress;
        migratedReferences++;
      } else {
        issues.push({
          level: "error",
          groupId: group.id,
          message: matches.length === 0
            ? `Legacy signal #${group.signalAddress} cannot be found on the layout.`
            : `Legacy signal #${group.signalAddress} is ambiguous because multiple layout elements use that address.`,
        });
      }
    }

    for (const rule of group.rules) {
      for (const condition of rule.conditions) {
        if (condition.type === "turnout" && !condition.turnoutId && (condition.turnoutAddress ?? 0) > 0) {
          const matches = knownTurnouts.filter(turnout => turnout.address === condition.turnoutAddress);
          if (matches.length === 1) {
            condition.turnoutId = matches[0]!.id;
            delete condition.turnoutAddress;
            migratedReferences++;
          } else {
            issues.push({
              level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id,
              message: matches.length === 0
                ? `Legacy turnout #${condition.turnoutAddress} cannot be found on the layout.`
                : `Legacy turnout #${condition.turnoutAddress} is ambiguous because multiple layout elements use that address.`,
            });
          }
        }

        if (condition.type === "sensor" && !condition.sensorId && (condition.sensorAddress ?? 0) > 0) {
          const matches = knownSensors.filter(sensor => sensor.address === condition.sensorAddress);
          if (matches.length === 1) {
            condition.sensorId = matches[0]!.id;
            delete condition.sensorAddress;
            migratedReferences++;
          } else {
            issues.push({
              level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id,
              message: matches.length === 0
                ? `Legacy sensor #${condition.sensorAddress} cannot be found on the layout.`
                : `Legacy sensor #${condition.sensorAddress} is ambiguous because multiple layout elements use that address.`,
            });
          }
        }
      }
    }
  }

  return { document, migratedReferences, issues };
}

export function validateSignalLogicDocument(
  document: SignalLogicDocumentDto,
  knownSignals: SignalLogicKnownSignal[] = [],
  knownTurnouts: SignalLogicKnownTurnout[] = [],
  knownSensors: SignalLogicKnownSensor[] = []
): SignalLogicValidationIssue[] {
  const issues: SignalLogicValidationIssue[] = [];
  const knownSignalById = new Map(knownSignals.map(signal => [signal.id, signal]));
  const knownTurnoutById = new Map(knownTurnouts.map(turnout => [turnout.id, turnout]));
  const knownSensorById = new Map(knownSensors.map(sensor => [sensor.id, sensor]));
  const usedSignalIds = new Set<string>();

  const reportDuplicateIds = (items: Array<{ id: string }>, kind: string) => {
    const seen = new Set<string>();
    for (const item of items) {
      if (seen.has(item.id)) issues.push({ level: "error", message: `Duplicate ${kind} element ID: ${item.id}` });
      seen.add(item.id);
    }
  };
  reportDuplicateIds(knownSignals, "signal");
  reportDuplicateIds(knownTurnouts, "turnout");
  reportDuplicateIds(knownSensors, "sensor");

  for (const group of document.groups) {
    if (!group.signalId) {
      issues.push({ level: "error", groupId: group.id, message: "Signal rule group has no valid signal element ID." });
    }

    if (group.signalId && usedSignalIds.has(group.signalId)) {
      issues.push({ level: "warning", groupId: group.id, message: "The same signal element has more than one rule group." });
    }

    usedSignalIds.add(group.signalId);
    const knownSignal = knownSignalById.get(group.signalId);

    if (knownSignals.length > 0 && !knownSignal) {
      issues.push({ level: "error", groupId: group.id, message: `Referenced signal element was deleted or has the wrong type (${group.signalId || "missing ID"}).` });
    }

    if (knownSignal) {
      const allowedAspects = getAllowedSignalAspects(knownSignal.aspect);

      if (!allowedAspects.includes(group.defaultAspect)) {
        issues.push({ level: "error", groupId: group.id, message: `Signal #${knownSignal.address} cannot use default aspect ${group.defaultAspect}.` });
      }
    }

    if (group.rules.length === 0) {
      issues.push({ level: "warning", groupId: group.id, message: `Signal #${knownSignal?.address ?? "?"} has no rules and will always use the default aspect.` });
    }

    const ruleSignatures = new Set<string>();
    const usedRuleAspects = new Set<SignalAspect>();

    for (const rule of group.rules) {
      if (usedRuleAspects.has(rule.aspect)) {
        issues.push({
          level: "warning",
          groupId: group.id,
          ruleId: rule.id,
          message: `More than one rule sets the signal to ${rule.aspect}.`,
          messageKey: "signalLogic.validation.duplicateRuleAspect",
          messageParams: { aspect: rule.aspect },
        });
      }

      usedRuleAspects.add(rule.aspect);

      if (knownSignal) {
        const allowedAspects = getAllowedSignalAspects(knownSignal.aspect);

        if (!allowedAspects.includes(rule.aspect)) {
          issues.push({ level: "error", groupId: group.id, ruleId: rule.id, message: `Signal #${knownSignal.address} cannot use rule aspect ${rule.aspect}.` });
        }
      }

      if (rule.conditions.length === 0) {
        issues.push({ level: "warning", groupId: group.id, ruleId: rule.id, message: "Rule has no conditions and will always match." });
      }

      const signature = rule.conditions.map(conditionSignature).sort().join("|");

      if (signature.length > 0 && ruleSignatures.has(signature)) {
        issues.push({ level: "warning", groupId: group.id, ruleId: rule.id, message: "Another rule has the same conditions." });
      }

      ruleSignatures.add(signature);
      const conditionKeys = new Set<string>();

      for (const condition of rule.conditions) {
        const key = `${condition.type}:${condition.type === "sensor" ? condition.sensorId : condition.turnoutId}`;

        if (conditionKeys.has(key)) {
          issues.push({ level: "warning", groupId: group.id, ruleId: rule.id, conditionId: condition.id, message: "The same input is used more than once in the same rule." });
        }

        conditionKeys.add(key);

        if (condition.type === "sensor") {
          if (!condition.sensorId) {
            issues.push({ level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id, message: "Sensor condition has no valid sensor element ID." });
          }

          if (knownSensors.length > 0 && !knownSensorById.has(condition.sensorId)) {
            issues.push({ level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id, message: `Referenced sensor was deleted or has the wrong type (${condition.sensorId || "missing ID"}).` });
          }

          continue;
        }

        if (!condition.turnoutId) {
          issues.push({ level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id, message: "Turnout condition has no valid turnout element ID." });
        }

        if (knownTurnouts.length > 0 && !knownTurnoutById.has(condition.turnoutId)) {
          issues.push({ level: "error", groupId: group.id, ruleId: rule.id, conditionId: condition.id, message: `Referenced turnout was deleted or has the wrong type (${condition.turnoutId || "missing ID"}).` });
        }
      }
    }
  }

  return issues;
}
