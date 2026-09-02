import type {
  SignalLogicDocumentDto,
  SignalLogicRuntimeStateDto,
  SignalLogicValidationIssue,
} from "@domain/signalLogic";

import {
  normalizeSignalLogicDocument,
} from "@domain/signalLogic";

import {
  requestWsCommand,
} from "./wsRequest";

export type SignalLogicLoadResult = {
  document: SignalLogicDocumentDto;
  issues: SignalLogicValidationIssue[];
  created: boolean;
  state: SignalLogicRuntimeStateDto;
  message?: string;
};

const JSONL_PATH = "/signal-rules.jsonl";

type SignalLogicJsonlMeta = {
  kind: "meta";
  version: 3;
  enabled: boolean;
};

type SignalLogicJsonlGroup = {
  kind: "group";
  group: SignalLogicDocumentDto["groups"][number];
};

type SignalLogicJsonlStamp = {
  kind: "stamp";
  savedAt: number;
};

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

function serializeSignalLogicJsonl(
  document: SignalLogicDocumentDto
): string {
  const lines: string[] = [];

  const meta: SignalLogicJsonlMeta = {
    kind: "meta",
    version: 3,
    enabled: document.enabled,
  };

  lines.push(JSON.stringify(meta));

  for (const group of document.groups) {
    const row: SignalLogicJsonlGroup = {
      kind: "group",
      group,
    };

    lines.push(JSON.stringify(row));
  }

  const stamp: SignalLogicJsonlStamp = {
    kind: "stamp",
    savedAt: Date.now(),
  };

  lines.push(JSON.stringify(stamp));

  return `${lines.join("\n")}\n`;
}

function parseSignalLogicJsonl(
  content: string
): SignalLogicDocumentDto {
  let enabled = false;
  let sawMeta = false;

  const groups:
    SignalLogicDocumentDto["groups"] = [];

  for (
    const rawLine of
      content.split(/\r?\n/u)
  ) {
    const line = rawLine.trim();

    if (!line) {
      continue;
    }

    const row =
      JSON.parse(line) as {
        kind?: unknown;
        enabled?: unknown;
        group?: unknown;
      };

    if (row.kind === "meta") {
      sawMeta = true;
      enabled = row.enabled === true;
      continue;
    }

    if (
      row.kind === "group" &&
      row.group
    ) {
      const normalized =
        normalizeSignalLogicDocument({
          version: 3,
          enabled,
          groups: [row.group],
        });

      if (normalized.groups[0]) {
        groups.push(
          normalized.groups[0]
        );
      }
    }
  }

  if (!sawMeta) {
    throw new Error(
      "The signal logic JSONL file has no meta row."
    );
  }

  return {
    version: 3,
    enabled,
    groups,
  };
}

async function readJsonl():
  Promise<SignalLogicDocumentDto | null> {
  const response = await fetch(
    `/api/files/text?path=${encodeURIComponent(JSONL_PATH)}`,
    {
      method: "GET",
      cache: "no-store",
    }
  );

  if (response.status === 404) {
    return null;
  }

  if (!response.ok) {
    throw new Error(
      "The signal logic JSONL file could not be loaded."
    );
  }

  const content =
    await response.text();

  if (!content.trim()) {
    return {
      version: 3,
      enabled: false,
      groups: [],
    };
  }

  return parseSignalLogicJsonl(
    content
  );
}

async function uploadJsonl(
  document: SignalLogicDocumentDto
): Promise<void> {
  const content =
    serializeSignalLogicJsonl(
      document
    );

  const form = new FormData();

  form.append(
    "file",
    new Blob(
      [content],
      {
        type:
          "application/x-ndjson",
      }
    ),
    "signal-rules.jsonl"
  );

  const response = await fetch(
    `/upload?path=${encodeURIComponent("/")}`,
    {
      method: "POST",
      body: form,
    }
  );

  if (!response.ok) {
    throw new Error(
      "The signal logic JSONL file could not be saved."
    );
  }
}

/**
 * Kept for compatibility/manual diagnostics.
 * Normal JSONL load/save no longer polls this every 1.5 seconds.
 */
export async function
getSignalLogicRuntimeStateWs():
  Promise<SignalLogicRuntimeStateDto> {
  const response =
    await requestWsCommand(
      "signalLogicCommand",
      {
        action: "state",
      },
      "signalLogicResponse",
      "Signal logic state request failed."
    );

  const state =
    response.state ?? {
      running: false,
      enabled: false,
    };

  publishRuntimeState(state);
  return state;
}

async function loadLegacyAndMigrate():
  Promise<SignalLogicLoadResult> {
  const response =
    await requestWsCommand(
      "signalLogicCommand",
      {
        action: "load",
      },
      "signalLogicResponse",
      "Signal logic command failed."
    );

  if (!response.document) {
    throw new Error(
      "The device did not return legacy signal logic rules."
    );
  }

  const document =
    normalizeSignalLogicDocument(
      response.document
    );

  await uploadJsonl(document);

  const state = {
    enabled: document.enabled,
    running: document.enabled,
  };

  publishRuntimeState(state);

  return {
    document,
    issues:
      response.issues ?? [],
    created:
      response.created ?? false,
    state,
    message:
      "Legacy signal rules were migrated to JSONL.",
  };
}

export async function
loadSignalLogicRulesWs():
  Promise<SignalLogicLoadResult> {
  const document =
    await readJsonl();

  if (!document) {
    return loadLegacyAndMigrate();
  }

  const state = {
    enabled: document.enabled,
    running: document.enabled,
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
  await uploadJsonl(document);

  // Firmware watches JSONL every 500 ms. Give it one complete watch cycle.
  await new Promise<void>(
    resolve =>
      window.setTimeout(
        resolve,
        650
      )
  );

  const state = {
    enabled: document.enabled,
    running: document.enabled,
  };

  publishRuntimeState(state);

  return {
    document,
    issues: [],
    created: false,
    state,
  };
}
