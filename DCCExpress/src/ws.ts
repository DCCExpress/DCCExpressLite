import { Api } from "./api";
import { ApiCommands } from "./dcc";

export interface iData {
    type: string,
    data: object
}

export interface iDccRaw {
    raw: string
}

export class WebSocketClient {

    socket!: WebSocket;
    onOpen?: () => void;
    onError?: () => void;
    onClosed?: () => void;
    task: NodeJS.Timeout | undefined;
    
    

    sendRaw(raw: string) {
        this.send({ type: ApiCommands.dccexraw, data: {raw: raw} as iDccRaw } as iData)
    }

    constructor() {
    }

    public connect(): void {


        const protocol = document.location.protocol === "https:" ? "wss:" : "ws:";
        let host = document.location.host;

        if(document.location.hostname == "127.0.0.1") {
            host = "192.168.1.143"
        }

        const url = `${protocol}//${host}/ws`; 
        
        // Node red listening /ws
        //const url = `ws://192.168.1.143/ws`;

        console.log(`Connecting to ${url}`);

        this.socket = new WebSocket(url);

        this.socket.onopen = () => {
            console.log("WebSocket connection established.");
            if (this.onOpen) {
                this.onOpen()
            }
        };

        this.socket.onmessage = (event) => {
            try {
                const m = event.data.toString().replace(/[\n\r\t]/g, "")
                const message: iData = JSON.parse(m);
                this.onMessage(message);
            } catch (error) {
                console.error("Invalid message format received:", event.data);
            }
        };

        this.socket.onclose = () => {
            if (this.onClosed) {
                this.onClosed()
            }
            console.warn("WebSocket connection closed. Reconnecting...");
            setTimeout(() => this.connect(), 1000); 
        };

        this.socket.onerror = (error) => {
            if (this.onError) {
                this.onError()
            }
            console.error("WebSocket error:", error);
        };


        if(this.task) {
            clearInterval(this.task)
        }

        this.task = setInterval(() => {
            Api.getSupportedLocos()
        }, 1000)


    }

    send(data: iData): void {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        } else {
            console.log("Cannot send message. WebSocket is not open.");
        }
    }

    onMessage(message: iData): void {
        console.log("Processing message:", message);
    }
}


export const wsClient = new WebSocketClient();
