import type { OutputCommandModeDto } from "@domain/layout/layoutDto";

import { wsApi } from "./wsApi";

export const OUTPUT_COMMAND_MODE_OPTIONS = [
  {
    value: "accessory",
    label: "DCC accessory · <a address 0|1>",
  },
  {
    value: "vpin",
    label: "DCC-EX VPIN · <z ±vpin>",
  },
] satisfies Array<{ value: OutputCommandModeDto; label: string }>;

export function normalizeOutputCommandMode(
  value: unknown
): OutputCommandModeDto {
  return value === "vpin" ? "vpin" : "accessory";
}

export function sendBinaryOutput(
  mode: OutputCommandModeDto,
  address: number,
  active: boolean
): boolean {
  if (!Number.isInteger(address) || address <= 0 || address > 32767) {
    return false;
  }

  if (mode === "vpin") {
    return wsApi.setVpin(address, active);
  }

  return wsApi.setBasicAccessory(address, active);
}

export function sendTurnoutOutput(
  mode: OutputCommandModeDto,
  address: number,
  closed: boolean
): boolean {
  if (mode === "vpin") {
    return sendBinaryOutput(mode, address, closed);
  }

  return wsApi.setTurnout(address, closed);
}
