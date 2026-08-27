import type {
  SignalLogicDocumentDto,
  SignalLogicRuntimeStateDto,
  SignalLogicValidationIssue,
} from "@domain/signalLogic";
import { normalizeSignalLogicDocument } from "@domain/signalLogic";
import { requestWsCommand } from "./wsRequest";

export type SignalLogicLoadResult = {
  document: SignalLogicDocumentDto;
  issues: SignalLogicValidationIssue[];
  created: boolean;
  state: SignalLogicRuntimeStateDto;
  message?: string;
};

async function requestSignalLogic(
  action: "load" | "save" | "state",
  document?: SignalLogicDocumentDto
): Promise<SignalLogicLoadResult> {
  const response = await requestWsCommand(
    "signalLogicCommand",
    { action, ...(document ? { document } : {}) },
    "signalLogicResponse",
    "Signal logic command failed."
  );

  if (!response.document) throw new Error("The device did not return signal logic rules.");
  return {
    document: normalizeSignalLogicDocument(response.document),
    issues: response.issues ?? [],
    created: response.created ?? false,
    state: response.state ?? {
      running: response.document.enabled,
      enabled: response.document.enabled,
    },
    ...(response.message ? { message: response.message } : {}),
  };
}

export function loadSignalLogicRulesWs(): Promise<SignalLogicLoadResult> {
  return requestSignalLogic("load");
}

export function saveSignalLogicRulesWs(document: SignalLogicDocumentDto): Promise<SignalLogicLoadResult> {
  return requestSignalLogic("save", document);
}
