import type {
  TrackTurnoutElement as CommonTrackTurnoutElement,
} from "@domain/layout/elements/TrackTurnoutElement";
import {
  drawTextWithRoundedBackground,
} from "../../../../../graphics";
import {
  OUTPUT_COMMAND_MODE_OPTIONS,
  sendTurnoutOutput,
} from "../../../../../services/layoutOutput";
import type {
  DrawOptions,
} from "../../../types/EditorTypes";
import type {
  IEditableProperty,
} from "../../../elements/PropertyDescriptor";
import type {
  TrackElementViewSupportTarget,
} from "./TrackElementViewSupport";

export type TrackTurnoutElementViewSupportTarget =
  CommonTrackTurnoutElement &
  TrackElementViewSupportTarget & {
    turnoutLockedColor: string | CanvasGradient | CanvasPattern;
    turnoutUnLockedColor: string | CanvasGradient | CanvasPattern;

    drawTurnout(
      ctx: CanvasRenderingContext2D,
      closed: boolean
    ): void;

    drawSectionInfo(
      ctx: CanvasRenderingContext2D,
      options?: DrawOptions
    ): void;

    drawSelection(ctx: CanvasRenderingContext2D): void;
  };

export function drawTurnoutElement(
  element: TrackTurnoutElementViewSupportTarget,
  ctx: CanvasRenderingContext2D,
  options?: DrawOptions
): void {
  if (!element.visible) {
    return;
  }

  element.beginDraw(ctx, options);
  element.drawTurnout(ctx, element.isClosed);
  element.endDraw(ctx);

  element.beginDraw(ctx);

  if (options?.showTurnoutAddress) {
    drawTextWithRoundedBackground(
      ctx,
      element.posLeft,
      element.posBottom - 10,
      "#" + element.turnoutAddress.toString()
    );
  }

  element.drawSectionInfo(ctx, options);
  element.endDraw(ctx);

  element.drawSelection(ctx);
}

export function toggleTurnout(
  element: CommonTrackTurnoutElement
): void {
  const nextClosed = !element.turnoutClosed;
  element.turnoutClosed = nextClosed;
  sendTurnoutOutput(
    element.outputMode,
    element.turnoutAddress,
    nextClosed
  );
}

export function mouseDownTurnout(
  element: CommonTrackTurnoutElement,
  _ev: MouseEvent
): void {
  toggleTurnout(element);
}

export function getTurnoutEditableProperties(
  baseProperties: IEditableProperty[]
): IEditableProperty[] {
  return [
    ...baseProperties,
    {
      label: "Output type",
      key: "outputMode",
      type: "select",
      readonly: false,
      options: OUTPUT_COMMAND_MODE_OPTIONS,
    },
    {
      label: "Accessory address / VPIN",
      key: "turnoutAddress",
      type: "number",
      readonly: false,
      validate: () => true,
    },
    {
      label: "Closed Value",
      key: "turnoutClosedValue",
      type: "bittoggle",
      readonly: false,
      validate: () => true,
    },
  ];
}
