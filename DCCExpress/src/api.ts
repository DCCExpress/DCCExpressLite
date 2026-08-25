import { App } from "./app";
import { ApiCommands, DccDirections, iData, iTurnout } from "./dcc";
import { iDccRaw, wsClient } from "./ws";

declare const Android: {
    connect(ip: string, port: number): void;
    send(data: string): void;
    disconnect(): void;
};

export function onTcpMessage(msg: string) {
    if (App.instance) {
        App.instance.cp.processMessage(msg)
    }
}

export class Api {

    static tcpSend(raw: string) {
        Android.send(raw);
    }

    static tcpConnect(ip: string, port: number) {
        Android.connect(ip, port);
    }
    static tcpDisconnect() {
        Android.disconnect();
    }

    static sendRaw(raw: string) {
        wsClient.send({ type: ApiCommands.dccexraw, data: { raw: raw } as iDccRaw } as iData)
    }
    static setLoco(address: number, speed: number, direction: DccDirections) {
        Api.sendRaw(`<t ${address} ${speed} ${direction}>`)
    }

    static getLocoInfo(address: number) {
        Api.sendRaw(`<t ${address}>`)
    }

    static setLocoFunction(address: number, fn: number, on: boolean) {
        Api.sendRaw(`<F ${address} ${fn} ${on ? 1 : 0}>`)
    }

    static emergencyStop() {
        Api.sendRaw(`<!>`)
    }

    static getSupportedLocos() {
        Api.sendRaw(`<c>`)
    }

    static setTurnout(to: iTurnout) {
        Api.sendRaw(`<T ${to.address} ${to.isClosed ? 0 : 1}>`)
    }

    static getAllTurnout() {
        Api.sendRaw('<T>')
    }

}