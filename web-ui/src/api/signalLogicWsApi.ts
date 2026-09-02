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

  return `${lines.join("\n")}\n`;
}

function parseSignalLogicJsonl(
  content: string
): SignalLogicDocumentDto {
  let enabled = false;
  let sawMeta = false;

  const groups:
    SignalLogicDocumentDto["groups"] = [];

  for (const rawLine of content.split(/\r?\n/u)) {
    const line = rawLine.trim();

    if (!line) {
      continue;
    }

    const row = JSON.parse(line) as {
      kind?: unknown;
      version?: unknown;
      enabled?: unknown;
      group?: unknown;
    };

    if (row.kind === "meta") {
      sawMeta = true;
      enabled = row.enabled === true;
      continue;
    }

    if (row.kind === "group" && row.group) {
      const normalized =
        normalizeSignalLogicDocument({
          version: 3,
          enabled,
          groups: [row.group],
        });

      if (normalized.groups[0]) {
        groups.push(normalized.groups[0]);
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

  const content = await response.text();

  if (!content.trim()) {
    return {
      version: 3,
      enabled: false,
      groups: [],
    };
  }

  return parseSignalLogicJsonl(content);
}

async function uploadJsonl(
  document: SignalLogicDocumentDto
): Promise<void> {
  const content =
    serializeSignalLogicJsonl(document);

  const form = new FormData();

  form.append(
    "file",
    new Blob(
      [content],
      {
        type: "application/x-ndjson",
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
 * One-time compatibility fallback.
 *
 * Old firmware stored one large /signal-rules.json document and exposed it
 * over WebSocket. If no JSONL file exists yet, load that document once,
 * immediately convert it to JSONL, then all later operations use streamed
 * HTTP file I/O.
 */
async function loadLegacyAndMigrate():
  Promise<SignalLogicLoadResult> {
  const response = await requestWsCommand(
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

  return {
    document,
    issues: response.issues ?? [],
    created: response.created ?? false,
    state: response.state ?? {
      running: document.enabled,
      enabled: document.enabled,
    },
    message:
      "Legacy signal rules were migrated to JSONL.",
  };
}

export async function loadSignalLogicRulesWs():
  Promise<SignalLogicLoadResult> {
  const document = await readJsonl();

  if (!document) {
    return loadLegacyAndMigrate();
  }

  return {
    document,
    issues: [],
    created: false,
    state: {
      running: document.enabled,
      enabled: document.enabled,
    },
  };
}

export async function saveSignalLogicRulesWs(
  document: SignalLogicDocumentDto
): Promise<SignalLogicLoadResult> {
  await uploadJsonl(document);

  return {
    document,
    issues: [],
    created: false,
    state: {
      running: document.enabled,
      enabled: document.enabled,
    },
  };
}
