export class WebSocketClient {
    constructor() {

        this.ip = document.location.hostname;
        if (this.ip == '127.0.0.1') {
            this.ip = "192.168.1.143"
        }

        this.socket = null;
        this.handlers = {};
    }

    connect() {
        this.socket = new WebSocket(`ws://${this.ip}/ws`);

        this.socket.onopen = () => {
            console.log("✅ WebSocket connected");
        };

        this.socket.onmessage = (event) => {
            try {
                const msg = JSON.parse(event.data);
                if (msg.type && this.handlers[msg.type]) {
                    this.handlers[msg.type](msg.data);
                } else {
                    console.warn("📥 Unhandled message type:", msg);
                }
            } catch (e) {
                console.error("❌ Invalid JSON from WebSocket:", event.data);
            }
        };

        this.socket.onclose = () => {
            console.warn("🔌 WebSocket disconnected, retrying in 3s...");
            setTimeout(() => this.connect(), 3000);
        };
    }

    send(data) {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        } else {
            console.warn("Cannot send, WebSocket not open");
        }
    }

    sendRaw(raw) {
        this.send({ type: "dccexraw", data: { raw: raw } })
    }
    on(type, handler) {
        this.handlers[type] = handler;
    }
}