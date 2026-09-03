import type {
  SerializedLayoutDto,
  SerializedLayoutElementDto,
} from "@domain/layout/layoutDto";

import type {
  SignalOutputConfiguration,
  SignalOutputState,
} from "@domain/layout/signalOutput";

import type {
  SignalLogicConditionDto,
  SignalLogicDocumentDto,
  SignalLogicRuleGroupDto,
  SignalLogicRuntimeStateDto,
  SignalLogicValidationIssue,
} from "@domain/signalLogic";

export type SignalLogicLoadResult = {
  document: SignalLogicDocumentDto;
  issues: SignalLogicValidationIssue[];
  created: boolean;
  state: SignalLogicRuntimeStateDto;
  message?: string;
};

type CompiledConditionSource =
  | "turnout"
  | "sensor"
  | "vpin";

type CompiledCondition = [
  source: CompiledConditionSource,
  address: number,
  value: 0 | 1,
];

type CompiledRule = {
  value: number;
  conditions: CompiledCondition[];
};

type CompiledSignal = {
  kind: "signal";
  address: number;
  mode: "extended" | "basic";
  outputs?: number;
  default: number;
  rules: CompiledRule[];
};

type CompiledMeta = {
  kind: "meta";
  version: 1;
  enabled: boolean;
};

type CompiledDocument = {
  enabled: boolean;
  signals: CompiledSignal[];
};

const API_PATH = "/api/signal-logic";

function publishRuntimeState(
  state: SignalLogicRuntimeStateDto
): void {
  window.dispatchEvent(
    new CustomEvent(
      "dcc-lite-signal-runtime-state",
      {
        detail: state,
      }
    )
  );
}

function allElements(
  layout: SerializedLayoutDto
): SerializedLayoutElementDto[] {
  return (
    layout.layers ?? []
  ).flatMap(
    layer => layer.elements ?? []
  );
}

async function loadLayout():
  Promise<SerializedLayoutDto> {
  const response =
    await fetch(
      "/api/layout",
      {
        method: "GET",
        cache: "no-store",
      }
    );

  if (!response.ok) {
    throw new Error(
      "The layout could not be loaded for signal rule compilation."
    );
  }

  return (
    await response.json()
  ) as SerializedLayoutDto;
}

function isTurnout(
  element: SerializedLayoutElementDto
): boolean {
  return String(
    element.type ?? ""
  ).startsWith("trackturnout");
}

function isSignal(
  element: SerializedLayoutElementDto
): boolean {
  const type =
    String(element.type ?? "");

  return (
    type === "tracksignal2" ||
    type === "tracksignal3" ||
    type === "tracksignal4"
  );
}

function signalConfig(
  element: SerializedLayoutElementDto
): SignalOutputConfiguration {
  const config =
    element.signalOutput;

  if (
    !config ||
    !Number.isInteger(config.address) ||
    config.address < 1
  ) {
    throw new Error(
      `Signal ${element.id ?? "?"} has no valid output configuration.`
    );
  }

  return config;
}

function stateValue(
  config: SignalOutputConfiguration,
  state: SignalOutputState
): number {
  if (config.protocol === "dccext") {
    const aspect =
      Math.trunc(Number(state.aspect));

    if (
      !Number.isInteger(aspect) ||
      aspect < 0 ||
      aspect > 255
    ) {
      throw new Error(
        `Signal aspect ${String(state.aspect)} is invalid.`
      );
    }

    return aspect;
  }

  const outputCount =
    Math.max(
      1,
      Math.min(
        16,
        Math.trunc(
          Number(config.outputCount)
        )
      )
    );

  let bits = 0;

  for (
    let index = 0;
    index < outputCount;
    index += 1
  ) {
    if (
      state.dccOutputs[index] === "G"
    ) {
      bits |= 1 << index;
    }
  }

  return bits;
}

function findState(
  config: SignalOutputConfiguration,
  stateId: string
): SignalOutputState {
  const state =
    config.states.find(
      item => item.id === stateId
    );

  if (!state) {
    throw new Error(
      `Signal state ${stateId} no longer exists.`
    );
  }

  return state;
}

function compileCondition(
  condition: SignalLogicConditionDto,
  elements: SerializedLayoutElementDto[]
): CompiledCondition {
  if (condition.type === "sensor") {
    const sensor =
      elements.find(
        element =>
          element.id ===
          condition.sensorId
      );

    const address =
      Math.trunc(
        Number(sensor?.address ?? 0)
      );

    if (
      !sensor ||
      sensor.type !== "tracksensor" ||
      address < 1 ||
      address > 32767
    ) {
      throw new Error(
        `Sensor ${condition.sensorId} is missing or has an invalid address.`
      );
    }

    return [
      "sensor",
      address,
      condition.active ? 1 : 0,
    ];
  }

  const turnout =
    elements.find(
      element =>
        element.id ===
        condition.turnoutId
    );

  if (
    !turnout ||
    !isTurnout(turnout)
  ) {
    throw new Error(
      `Turnout ${condition.turnoutId} no longer exists.`
    );
  }

  const address =
    Math.trunc(
      Number(
        turnout.turnoutAddress ??
        turnout.turnout1Address ??
        0
      )
    );

  if (
    address < 1 ||
    address > 32767
  ) {
    throw new Error(
      `Turnout ${condition.turnoutId} has an invalid address.`
    );
  }

  const closedValue =
    turnout.turnoutClosedValue ??
    turnout.turnout1ClosedValue ??
    false;

  const physicalValue =
    condition.closed
      ? closedValue
      : !closedValue;

  const source:
    CompiledConditionSource =
    String(
      turnout.outputMode ??
      "accessory"
    ) === "vpin"
      ? "vpin"
      : "turnout";

  if (
    source === "turnout" &&
    address > 2048
  ) {
    throw new Error(
      `Turnout address ${address} is outside the DCC accessory range.`
    );
  }

  return [
    source,
    address,
    physicalValue ? 1 : 0,
  ];
}

function compileDocument(
  document: SignalLogicDocumentDto,
  layout: SerializedLayoutDto
): CompiledDocument {
  const elements =
    allElements(layout);

  const signals =
    document.groups.map(
      group => {
        const signal =
          elements.find(
            element =>
              element.id ===
                group.signalId &&
              isSignal(element)
          );

        if (!signal) {
          throw new Error(
            `Signal ${group.signalId} no longer exists.`
          );
        }

        const config =
          signalConfig(signal);

        const defaultState =
          findState(
            config,
            group.defaultStateId
          );

        const compiled:
          CompiledSignal = {
          kind: "signal",
          address:
            Math.trunc(
              Number(config.address)
            ),
          mode:
            config.protocol === "dccext"
              ? "extended"
              : "basic",
          default:
            stateValue(
              config,
              defaultState
            ),
          rules:
            group.rules.map(
              rule => {
                const state =
                  findState(
                    config,
                    rule.stateId
                  );

                return {
                  value:
                    stateValue(
                      config,
                      state
                    ),
                  conditions:
                    rule.conditions.map(
                      condition =>
                        compileCondition(
                          condition,
                          elements
                        )
                    ),
                };
              }
            ),
        };

        if (
          compiled.mode === "basic"
        ) {
          compiled.outputs =
            Math.max(
              1,
              Math.min(
                16,
                Math.trunc(
                  Number(
                    config.outputCount
                  )
                )
              )
            );
        }

        return compiled;
      }
    );

  return {
    enabled: document.enabled,
    signals,
  };
}

function serializeCompiled(
  document: CompiledDocument
): string {
  const meta: CompiledMeta = {
    kind: "meta",
    version: 1,
    enabled: document.enabled,
  };

  return [
    JSON.stringify(meta),
    ...document.signals.map(
      signal =>
        JSON.stringify(signal)
    ),
    "",
  ].join("\n");
}

function parseCompiled(
  content: string
): CompiledDocument {
  let enabled = false;
  let sawMeta = false;

  const signals:
    CompiledSignal[] = [];

  for (
    const rawLine of
      content.split(/\r?\n/u)
  ) {
    const line = rawLine.trim();

    if (!line) {
      continue;
    }

    const row =
      JSON.parse(line) as
        | CompiledMeta
        | CompiledSignal
        | {
            kind?: unknown;
          };

    if (row.kind === "meta") {
      const meta =
        row as CompiledMeta;

      if (meta.version !== 1) {
        throw new Error(
          `Unsupported signal rule version: ${String(meta.version)}.`
        );
      }

      enabled =
        meta.enabled === true;

      sawMeta = true;
      continue;
    }

    if (row.kind === "signal") {
      signals.push(
        row as CompiledSignal
      );
      continue;
    }

    throw new Error(
      "The signal rule file contains an unsupported row."
    );
  }

  if (!sawMeta) {
    throw new Error(
      "The signal rule file has no meta row."
    );
  }

  return {
    enabled,
    signals,
  };
}

function findSignalByCompiled(
  compiled: CompiledSignal,
  elements: SerializedLayoutElementDto[]
): SerializedLayoutElementDto {
  const signal =
    elements.find(element => {
      if (!isSignal(element)) {
        return false;
      }

      const config =
        element.signalOutput;

      if (!config) {
        return false;
      }

      const mode =
        config.protocol === "dccext"
          ? "extended"
          : "basic";

      return (
        config.address ===
          compiled.address &&
        mode === compiled.mode
      );
    });

  if (!signal) {
    throw new Error(
      `No layout signal matches compiled address ${compiled.address}.`
    );
  }

  return signal;
}

function findStateByValue(
  config: SignalOutputConfiguration,
  value: number
): SignalOutputState {
  const state =
    config.states.find(
      candidate =>
        stateValue(
          config,
          candidate
        ) === value
    );

  if (!state) {
    throw new Error(
      `No signal state maps to output value ${value} at address ${config.address}.`
    );
  }

  return state;
}

function decompileCondition(
  condition: CompiledCondition,
  elements: SerializedLayoutElementDto[],
  id: string
): SignalLogicConditionDto {
  const [
    source,
    address,
    rawValue,
  ] = condition;

  const value =
    rawValue !== 0;

  if (source === "sensor") {
    const sensor =
      elements.find(
        element =>
          element.type === "tracksensor" &&
          element.address === address
      );

    if (!sensor?.id) {
      throw new Error(
        `No sensor at address ${address} exists in the current layout.`
      );
    }

    return {
      id,
      type: "sensor",
      sensorId: sensor.id,
      sensorAddress: address,
      active: value,
    };
  }

  const turnout =
    elements.find(element => {
      if (!isTurnout(element)) {
        return false;
      }

      const elementAddress =
        element.turnoutAddress ??
        element.turnout1Address;

      if (
        elementAddress !== address
      ) {
        return false;
      }

      const elementSource =
        String(
          element.outputMode ??
          "accessory"
        ) === "vpin"
          ? "vpin"
          : "turnout";

      return elementSource === source;
    });

  if (!turnout?.id) {
    throw new Error(
      `No ${source} turnout at address ${address} exists in the current layout.`
    );
  }

  const closedValue =
    turnout.turnoutClosedValue ??
    turnout.turnout1ClosedValue ??
    false;

  return {
    id,
    type: "turnout",
    turnoutId: turnout.id,
    turnoutAddress: address,
    closed:
      value === closedValue,
  };
}

function decompileDocument(
  compiled: CompiledDocument,
  layout: SerializedLayoutDto
): SignalLogicDocumentDto {
  const elements =
    allElements(layout);

  const groups:
    SignalLogicRuleGroupDto[] =
    compiled.signals.map(
      (compiledSignal, signalIndex) => {
        const signal =
          findSignalByCompiled(
            compiledSignal,
            elements
          );

        const config =
          signalConfig(signal);

        const defaultState =
          findStateByValue(
            config,
            compiledSignal.default
          );

        return {
          id:
            `compiled-signal-${compiledSignal.address}-${signalIndex}`,
          signalId:
            signal.id ?? "",
          signalAddress:
            compiledSignal.address,
          defaultStateId:
            defaultState.id,
          rules:
            compiledSignal.rules.map(
              (compiledRule, ruleIndex) => {
                const state =
                  findStateByValue(
                    config,
                    compiledRule.value
                  );

                return {
                  id:
                    `compiled-rule-${compiledSignal.address}-${ruleIndex}`,
                  stateId: state.id,
                  conditions:
                    compiledRule.conditions.map(
                      (
                        condition,
                        conditionIndex
                      ) =>
                        decompileCondition(
                          condition,
                          elements,
                          `compiled-condition-${compiledSignal.address}-${ruleIndex}-${conditionIndex}`
                        )
                    ),
                };
              }
            ),
        };
      }
    );

  return {
    version: 3,
    enabled: compiled.enabled,
    groups,
  };
}

async function readCompiled():
  Promise<CompiledDocument> {
  const response =
    await fetch(
      API_PATH,
      {
        method: "GET",
        cache: "no-store",
      }
    );

  if (!response.ok) {
    throw new Error(
      "The compiled signal logic rules could not be loaded."
    );
  }

  return parseCompiled(
    await response.text()
  );
}

async function writeCompiled(
  document: CompiledDocument
): Promise<void> {
  const response =
    await fetch(
      API_PATH,
      {
        method: "POST",
        headers: {
          "Content-Type":
            "application/x-ndjson",
        },
        body:
          serializeCompiled(
            document
          ),
      }
    );

  if (!response.ok) {
    const message =
      await response.text();

    throw new Error(
      message ||
      "The compiled signal logic rules could not be saved."
    );
  }
}

export async function
loadSignalLogicRulesWs():
  Promise<SignalLogicLoadResult> {
  const [
    compiled,
    layout,
  ] =
    await Promise.all([
      readCompiled(),
      loadLayout(),
    ]);

  const document =
    decompileDocument(
      compiled,
      layout
    );

  const state = {
    enabled:
      document.enabled,
    running:
      document.enabled,
  };

  publishRuntimeState(state);

  return {
    document,
    issues: [],
    created: false,
    state,
  };
}

export async function
saveSignalLogicRulesWs(
  document: SignalLogicDocumentDto
): Promise<SignalLogicLoadResult> {
  const layout =
    await loadLayout();

  const compiled =
    compileDocument(
      document,
      layout
    );

  await writeCompiled(compiled);

  const state = {
    enabled:
      document.enabled,
    running:
      document.enabled,
  };

  publishRuntimeState(state);

  return {
    document,
    issues: [],
    created: false,
    state,
  };
}
