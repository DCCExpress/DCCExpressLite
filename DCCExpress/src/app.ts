import { Api } from "./api";
import { ApiCommands, iData, iConfig, iTurnout } from "./dcc";
import { LocoPanel } from "./locoPanel";
import { iDccRaw, wsClient } from "./ws";

console.log(LocoPanel)
console.log(Api)

export class App {
    cp: any;
    static instance: App | null
    config: iConfig = {
        startup: {
            power: "<1 MAIN>",
            init: "<s>\n<T 1 DCC 1>"
        }
    };

    onMessage(msg: string) {

    }

    constructor() {
        App.instance = this
        this.cp = document.createElement('loco-panel') as LocoPanel
        document.body.appendChild(this.cp)

        wsClient.onOpen = () => {
            //wsClient.sendRaw("<s>")

            wsClient.sendRaw(this.config.startup.power)
            wsClient.sendRaw(this.config.startup.init)
            setTimeout(() => {
                this.cp.init()
            }, 100)
        }

        wsClient.onClosed = () => {
            //alert("closed")
        }

        wsClient.onMessage = (msg: iData) => {
            // if (msg.type == ApiCommands.rawInfo) {
            //     const raw = (msg.data as iDccRaw).raw
            //     if (raw == "<!E>") {
            //         this.cp.powerInfo.emergencyStop = true;
            //         this.cp.power = this.cp.powerInfo
            //         this.cp.updateUI()
            //         return;
            //     }
            // }

            this.cp.processMessage(msg)

        }

        // fetch("data/config.json").then((res) => res.json).then((json) => {
        //     this.conf = json as unknown as iConfig


        // }).finally(() => {
        //     wsClient.connect()
        //     wsClient.sendRaw(this.conf.startup.power)
        //     wsClient.sendRaw(this.conf.startup.init)
        // });


        fetch("config.json")
            .then((res) => {
                if (!res.ok) throw new Error(`HTTP error ${res.status}`);
                return res.json();
            })
            .then((json) => {
                this.config = json;
            })
            .catch((err) => console.error("Hiba a config.json beolvasásakor:", err))
            .finally(() => {
                wsClient.connect()

            });


        // fetch("turnouts.json").then((res) => {
        //     if (!res.ok) throw new Error(`HTTP error ${res.status}`);
        //     return res.json();

        // }).then((json) => {
        //     this.turnouts = json

        // }).catch((err) => {

        // });      
    }

    sendRaw(raw: any) {
        const json = {
            type: "dccexraw",
            data: { raw: raw }
        };
        wsClient.socket.send(JSON.stringify(json));
    }

}

const app = new App()