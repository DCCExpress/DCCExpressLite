import type { Loco } from "@domain/types";
import { getLocosWs, saveLocosWs } from "./locosWsApi";

export function getLocos(): Promise<Loco[]> {
  return getLocosWs();
}

export async function saveLocos(locos: Loco[]): Promise<void> {
  await saveLocosWs(locos);
}
