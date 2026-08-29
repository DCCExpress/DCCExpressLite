import {
  ActionIcon,
  Box,
  Group,
  NumberInput,
  Tooltip,
} from "@mantine/core";

import {
  IconBinary,
} from "@tabler/icons-react";

import {
  useState,
} from "react";

import BitCalculatorDialog from "@/components/tools/BitCalculatorDialog";

import type { BaseElementView } from "../../models/editor/core/BaseElementView";
import type { IEditableProperty } from "../../models/editor/elements/PropertyDescriptor";
import { TrackSignalElementView } from "../../models/editor/elements/TrackSignalElementView";
import ElementPreview from "../../models/editor/rendering/ElementPreviewRenderer";
import {
  createSignalPreview,
  type SignalPreviewColor,
} from "./previewFactories";
import type { SelectedElementUpdateHandler } from "./propertyPanelTypes";

type SignalAspectPropertyEditorProps = {
  prop: IEditableProperty;
  selectedElement: BaseElementView;
  onUpdateSelectedElement: SelectedElementUpdateHandler;
};

type SignalValueKey =
  | "valueGreen"
  | "valueRed"
  | "valueYellow"
  | "valueWhite";

type SignalAspectRow = {
  color: SignalPreviewColor;
  label: string;
  valueKey: SignalValueKey;
  send: (
    signal: TrackSignalElementView
  ) => void;
  minAspect?: number;
};

type CalculatorTarget = {
  valueKey: SignalValueKey;
  label: string;
} | null;

const rows: SignalAspectRow[] = [
  {
    color: 1,
    label: "Green",
    valueKey: "valueGreen",
    send: signal =>
      signal.sendGreen(),
  },
  {
    color: 2,
    label: "Red",
    valueKey: "valueRed",
    send: signal =>
      signal.sendRed(),
  },
  {
    color: 3,
    label: "Yellow",
    valueKey: "valueYellow",
    send: signal =>
      signal.sendYellow(),
    minAspect: 3,
  },
  {
    color: 4,
    label: "White",
    valueKey: "valueWhite",
    send: signal =>
      signal.sendWhite(),
    minAspect: 4,
  },
];

export default function SignalAspectPropertyEditor({
  selectedElement,
  onUpdateSelectedElement,
}: SignalAspectPropertyEditorProps) {
  const signal =
    selectedElement as TrackSignalElementView;

  const [
    calculatorTarget,
    setCalculatorTarget,
  ] =
    useState<CalculatorTarget>(
      null
    );

  const openCalculator = (
    row: SignalAspectRow
  ) => {
    setCalculatorTarget({
      valueKey: row.valueKey,
      label: row.label,
    });
  };

  const closeCalculator = () => {
    setCalculatorTarget(null);
  };

  const applyCalculatorValue = (
    value: number
  ) => {
    if (!calculatorTarget) {
      return;
    }

    signal[
      calculatorTarget.valueKey
    ] = value;

    onUpdateSelectedElement(
      signal
    );
  };

  const calculatorValue =
    calculatorTarget
      ? Number(
          signal[
            calculatorTarget
              .valueKey
          ]
        ) || 0
      : 0;

  return (
    <>
      <Group>
        {rows
          .filter(
            row =>
              !row.minAspect ||
              signal.aspect >=
                row.minAspect
          )
          .map(row => (
            <Group
              key={row.label}
              gap="xs"
              wrap="nowrap"
            >
              <Box className="route-turnout-preview-button">
                <ElementPreview
                  style={{
                    width: "50%",
                  }}
                  element={createSignalPreview(
                    selectedElement,
                    row.color
                  )}
                  label={
                    row.label
                  }
                  width={40}
                  height={40}
                  translateX={-10}
                  onClick={() =>
                    row.send(signal)
                  }
                />
              </Box>

              <NumberInput
                style={{
                  flex: 1,
                }}
                value={
                  signal[
                    row.valueKey
                  ]
                }
                min={0}
                max={255}
                allowDecimal={
                  false
                }
                allowNegative={
                  false
                }
                onChange={value => {
                  const numberValue =
                    typeof value ===
                    "number"
                      ? value
                      : Number(
                          value
                        );

                  signal[
                    row.valueKey
                  ] =
                    Number.isFinite(
                      numberValue
                    )
                      ? Math.max(
                          0,
                          Math.min(
                            255,
                            Math.trunc(
                              numberValue
                            )
                          )
                        )
                      : 0;

                  onUpdateSelectedElement(
                    signal
                  );
                }}
              />

              <Tooltip
                label={`Bit calculator · ${row.label}`}
              >
                <ActionIcon
                  variant="light"
                  size="lg"
                  onClick={() =>
                    openCalculator(
                      row
                    )
                  }
                  aria-label={`Open bit calculator for ${row.label}`}
                >
                  <IconBinary
                    size={18}
                  />
                </ActionIcon>
              </Tooltip>
            </Group>
          ))}
      </Group>

      <BitCalculatorDialog
        opened={
          calculatorTarget !==
          null
        }
        initialValue={
          calculatorValue
        }
        onClose={
          closeCalculator
        }
        onApply={
          applyCalculatorValue
        }
      />
    </>
  );
}
