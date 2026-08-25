export enum DccDirections {
    reverse = 0,
    forward = 1
}

export enum ApiCommands {
    getLoco = "getLoco",
    setLoco = "setLoco",
    locoInfo = "locoInfo",
    setLocoFunction = "setLocoFunction",
    getTurnout = "getTurnout",
    setTurnout = "setTurnout",
    turnoutInfo = "turnoutInfo",
    getBasicAcessory = "getBasicAcessory",
    setBasicAccessory = "setBasicAccessory",
    basicAccessoryInfo = "basicAccessoryInfo",

    setBlock = "setBlock",
    // getBlock = "getBlock",
    fetchBlocks = "fetchBlocks",
    blockInfo = "blockInfo",

    getRBusInfo = "getRBusInfo",
    rbusInfo = "rbusInfo",

    alert = "alert",
    response = "response",
    systemInfo = "systemInfo",
    powerInfo = "powerInfo",
    setTrackPower = "setPower",
    emergencyStop = "emergencyStop",

    UnsuccessfulOperation = "UnsuccessfulOperation",
    saveSettings = "saveSettings",
    getSettings = "getSettings",
    settingsInfo = "settingsInfo",
    timeInfo = "timeInfo",
    setTimeSettings = "setTimeSettings",
    saveCommandCenter = "saveCommandCenter",
    getSensor = "getSensor",
    sensorInfo = "sensorInfo",
    setOutput = "setOutput",
    getOutput = "getOutput",
    outputInfo = "outputInfo",
    setProgPower = "setProgPower",
    writeDccExDirectCommand = "writeDirectCommand",

    dccExDirectCommandResponse = "dccExDirectCommandResponse",
    wsSensorInfo = "wsSensorInfo",
    dccRaw = "dccRaw",
    dccexraw = "dccexraw",
    rawInfo = "rawInfo",
    ack = "ack"

}

export interface iData {
    type: ApiCommands,
    data: Object,
}

export interface iLocoFunction {
    id: number,
    name: string,
    momentary: boolean,
    isOn: boolean
}

export interface iSetLocoFunction {
    address: number,
    id: number,
    isOn: boolean,
}

export interface iLocomotive {
    id?: string;
    name: string;
    address: number;
    imageUrl: string;
    speedMode: string;
    speed: number;
    direction: DccDirections
    functions: iLocoFunction[]
    functionMap: number
}

export interface iLoco {
    address: number,
    speed: number,
    direction: number,
    funcMap: number,
}

export interface iPowerInfo {
    info?: number,
    current?: number,
    emergencyStop?: boolean,                  // 0x01 // The emergency stop is switched on
    trackVoltageOn?: boolean,                // 0x02 // The track voltage is switched off.
    shortCircuit?: boolean,                   // 0x04 // Short-circuit
    programmingModeActive?: boolean,          // 0x20 // The programming mode is active    
    //cc: iCommandCenter
}

export interface FunctionButton extends HTMLButtonElement {
    function?: iLocoFunction;
    fn: number;
}



export interface iStartup {
    power: string;
    init: string;
}

export interface iConfig {
    startup: iStartup;
}

export interface iTurnout {
    id: number,
    address: number,
    name: string,
    isLeft: boolean,
    isInverted: boolean,
    isAccessory: boolean,
    isClosed: boolean
}