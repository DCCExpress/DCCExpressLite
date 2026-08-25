import { Api } from "./api";
import { ApiCommands, DccDirections, FunctionButton, iData, iLocomotive, iPowerInfo, iTurnout } from "./dcc";
import { Turnout, TurnoutRightElement } from "./turnout";
import { iDccRaw, wsClient } from "./ws";

console.log(TurnoutRightElement)

enum Colors {
    secondary = "#222222"
}

export class LocoPanel extends HTMLElement {

    //     getSvgTurnoutClosed(id: number, isLeft: boolean) {
    //         return `
    //  <svg width="48" height="48" viewBox="0 0 12 12" id="turnout${id}" xmlns="http://www.w3.org/2000/svg">
    //    <g ${isLeft ? 'transform="scale(-1 1)  translate(-12 0)"' : ''} >
    //      <path
    //         id="rect2"
    //         style="fill:#4d4d4d;stroke:#000000;stroke-width:0.264583"
    //         d="m 8.4106304,0.1322915 h 2.1727026 l -4.6263137,6.0942533 0,4.3567892 H 4.121172 l 0,-4.8354783 z"
    //         />
    //      <rect
    //         style="fill:#f2f2f2;stroke:#000000;stroke-width:0.264583;stroke-dasharray:none"
    //         id="rect1"
    //         width="1.8358474"
    //         height="10.451042"
    //         x="4.121172"
    //         y="0.1322915" />
    //    </g>
    //  </svg>
    //  `
    //      }

    //      getSvgTurnoutThrown(id: number, isLeft: boolean) {
    //         return `
    //  <svg width="48" height="48" viewBox="0 0 12 12" id="turnout${id}" xmlns="http://www.w3.org/2000/svg">
    //    <g ${isLeft ? 'transform="scale(-1 1)  translate(-12 0)"' : ''} >
    //      <rect
    //         style="fill:#4d4d4d;stroke:#000000;stroke-width:0.264583;stroke-dasharray:none"
    //         id="rect1"
    //         width="1.8358474"
    //         height="10.451042"
    //         x="4.121172"
    //         y="0.1322915" />
    //      <path
    //         id="rect2"
    //         style="fill:#f2f2f2;stroke:#000000;stroke-width:0.264583"
    //         d="m 8.4106304,0.1322915 h 2.1727026 l -4.6263137,6.0942533 0,4.3567892 H 4.121172 l 0,-4.8354783 z"
    //         />

    //    </g>
    //  </svg>
    //  `
    //      }     
    locomotives: iLocomotive[] = [];
    locoImage: HTMLImageElement;
    btnReverse: HTMLButtonElement;
    btnForward: HTMLButtonElement;
    btnStop: HTMLButtonElement;
    btnSpeed10: HTMLButtonElement;
    btnSpeed20: HTMLButtonElement;
    btnSpeed40: HTMLButtonElement;
    btnSpeed80: HTMLButtonElement;
    btnSpeed100: HTMLButtonElement;
    buttons: { [fn: number]: FunctionButton } = {};
    fnButtons: HTMLDivElement;
    modal: HTMLDivElement;
    locoInfoSpeedElement: HTMLElement;
    btnSpeed5: HTMLButtonElement;
    btnEmergency: HTMLButtonElement;
    locoName: HTMLDivElement;
    locoModeInfoElement: HTMLElement;
    _data: string = "";
    //powerInfo: iPowerInfo = {current: 0, emergencyStop: false, info: 0, programmingModeActive: false, shortCircuit: false, trackVoltageOn: false}
    btnPower: HTMLButtonElement;
    locoInfoPowerElement: HTMLElement;

    turnouts: iTurnout[] = [];



    constructor() {
        super()

        const shadow = this.attachShadow({ mode: "open" });

        shadow.innerHTML = `
            <style>
                @import url("bootstrap.min.css");
                
                #container {
                    color: black;
                    padding: 10px;
                    margin: 0;
                    display: flex;
                    flex-direction: column;
                    align-items: center;
                    background: #f4f4f4;
                    border-radius: 10px;
                    box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
                }

                #infoLeft, #infoRight{
                    background: red;
                    width: 80px;
                    width: 20%;
                    text-align: center;                 
                    display: none;   
                }
                #locoImageDiv {
                    width: 90%;
                    text-align: center;
                    
                }
                /* Mozdonykép stílus */
                #locoImage {
                    height: 64px;
                    max-width: 400px;
                    cursor:pointer;
                    
                }


                /* Gombcsoport az iránygombokhoz (20% - 60% - 20%) */
                .control-group {
                    display: grid;
                    grid-template-columns: 1fr 1fr 1fr 1fr;
                    width: 100%;
                    gap: 4px;
                    margin-bottom: 0px;
                }
                .direction-group {
                    display: grid;
                    grid-template-columns: 1fr 2fr 1fr;
                    width: 100%;
                    gap: 6px;
                    margin-bottom: 4px;
                }
        
                /* Sebesség gombok (5 oszlop, azaz 20%-os elosztás) */
                .speed-group {
                    display: grid;
                    grid-template-columns: repeat(6, 1fr);
                    width: 100%;
                    gap: 4px;
                    margin-bottom: 4px;
                }
        
                /* Gombok általános stílusa */
                #container button {
                    background: rgb(100, 100, 100);
                    border: none;
                    border-radius: 4px;
                    padding: 0px;
                    color: white;
                    font-size: 16px;
                    font-weight: bold;
                    cursor: pointer;
                    transition: all 0.3s ease;
                    box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
                    width: 100%;
                    height: 48px;
                    
                }
                
                #fnButtons {
                    display: grid;
                    grid-template-columns: repeat(5, 1fr);
                    grid-template-rows: repeat(6, 1fr); /* 12 egyenlő sor */
                    width: 100%;
                    gap: 4px;
                    
                }

                #container #fnButtons button {
                    padding: 0;
                    font-size: 12px;
                }
                #container #fnButtons button.fnbutton {
                    background-color: dodgerblue;
                }

                /* Hover effekt */
                button:hover {
                    background: rgb(125, 125, 125);
                    transform: scale(1.00);
                }
        
                /* Aktív állapot */
                button:active {
                    background:  rgb(25, 25, 25);
                    transform: scale(0.99);
                }
        
                /* Funkció gombok */
                #container #fnButtons button.on {
                    background-color: rgb(77, 77, 255);
                    color: white;
                    border-color: blue;
                }
                    
                #container #fnButtons button.on:hover,
                #container #fnButtons button.on:active {
                    background-color: blue;
                    transform: none;
                }

                /* Modal háttér  */
                #modal {
                    display: none;
                    position: fixed;
                    top: 0;
                    left: 0;
                    width: 100vw;
                    height: 100vh;
                    background: rgba(0, 0, 0, 0.6);
                    justify-content: center;
                    align-items: center;
                    color: black;
                }
                #modal h3 {
                    padding: 0;
                    margin: 5px;
                }
                /* Modal tartalom */
                .modal-content {
                    width: 80%;
                    height: 460px;
                    background: white;
                    padding: 10px;
                    border-radius: 10px;
                    box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
                    text-align: center;
                    overflow-y: auto
                }

                /* Mozdony lista */
                .loco-item, .turnout-item{
                    display: flex;
                    flex-direction: column;
                    overflow: auto;
                    align-items: center;
                    padding: 10px;
                    cursor: pointer;
                    border-bottom: 1px solid #ddd;
                    transition: background 0.3s;
                }

                /* Kijelölt mozdony */
                .selected {
                    background: red !important;
                    color: white;
                    font-weight: bold;
                }

                .loco-item:hover {
                    background: #f0f0f0;
                }

                .loco-item img {
                    height: auto;
                    margin-right: 10px;
                    border-radius: 5px;
                }

                /* Modal bezáró gomb */
                #closeModal {
                    margin-top: 10px;
                    padding: 10px 20px;
                    background: red;
                    border: none;
                    color: white;
                    cursor: pointer;
                }

                #modalContent {
                    height: 100%;
                    overflow: auto;
                    border-radius: 5px;
                    border: 1px solid #ddd;
                }

                #locoInfo {
                    min-width: 100px;
                    color: #222;
                    background-color: gainsboro;
                    margin-bottom: 4px;
                    padding: 4px;
                    border-radius: 5px;
                    border: 1px solid black;
                    font-weight: bold;
                    justify-content: center;
                    text-align: center;
                    font-size: 2em;
                }

                #container #btnEmergency {
                    width: 100%;
                    background: rgb(104, 30, 30);
                    margin-bottom: 4px;
                }
                #container #btnEmergency:hover {
                    width: 100%;
                    background: rgb(138, 38, 38);
                    margin-bottom: 4px;
                }
                #container #btnEmergency:active {
                    width: 100%;
                    background: rgb(255, 0, 0);
                    margin-bottom: 4px;
                }
                #container #btnEmergency.on {
                    width: 100%;
                    background: rgb(255, 0, 0);
                    margin-bottom: 4px;
                    animation: blink 0.5s infinite alternate;
                }

                @keyframes blink {
                    from {
                        background-color: red;
                        fill: yellow;
                    }
                    to {
                        background-color: rgb(138, 38, 38);
                        fill: #000;
                    }
                }                   

                #container #locoModeInfo {
                    width: 100%;
                    background-color: gray;
                    color: white;
                    padding: 4px 0;
                    border-radius:5px;
                    margin: 4px 0;
                    text-align: center;"
                }

                .icon {
                    width: 24px;
                    height: 24px;
                    padding: 8px;
                    margin: 0;
                }
            </style>
        
            <div id="container" class="scrollable" >
                <div style="justify-content: center; width:100%; height: 100px; display: flex; border-bottom: 20px;">
                    <div id="infoLeft">INFO L</div>
                    <div id="locoImageDiv">
                        <img id="locoImage" src="" style="max-height: 60px">
                        <div id="locoName">#11 CSÖRGŐ</div>
                    </div>
                    <div id="infoRight">INFO R</div>
                </div>

                <div id="locoInfo" style="display: flex">
                    <div id="locoInfoSpeed">10</div>
                    <div id="locoInfoPower" style="color: #555555; font-size: 0.6em;display: flex; flex-direction: column; justify-content: flex-end; ">10</div>
                </div>

                <div class="control-group">
                    <button id="btnPower" class="btn btn-primary flex-fill py-3">⚡</button>
                    <button id="btnRoutes" class="btn btn-warning flex-fill py-3">🔀</button>
                    <button id="btnTurnouts" class="btn btn-success flex-fill py-3">🔌</button>
                    <button id="btnEmergency" class="btn btn-danger flex-fill py-3">🛑</button>
                </div>

                <!--

                <div id="locoModeInfo2">
                    <button class="btn d-flex justify-content-center align-items-center" style="width: 200px;">
                        <svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M5,8A4,4 0 0,1 9,12A4,4 0 0,1 5,16A4,4 0 0,1 1,12A4,4 0 0,1 5,8M12,1A4,4 0 0,1 16,5A4,4 0 0,1 12,9A4,4 0 0,1 8,5A4,4 0 0,1 12,1M12,15A4,4 0 0,1 16,19A4,4 0 0,1 12,23A4,4 0 0,1 8,19A4,4 0 0,1 12,15M19,8A4,4 0 0,1 23,12A4,4 0 0,1 19,16A4,4 0 0,1 15,12A4,4 0 0,1 19,8Z" /></svg>
                    </button>
                </div>
                

                <div style="width: 100%">
                    <button id="btnEmergency">EMERGENCY STOP</button>
                </div>
                -->
                

                <!-- KÜLÖN SOR: Irányítás (20% - 60% - 20%) -->
                <div class="direction-group">
                    <button id="btnReverse">&lt;&lt;</button>
                    <button id="btnStop">STOP</button>
                    <button id="btnForward">&gt;&gt;</button>
                </div>
        
                <!-- KÜLÖN SOR: Sebesség gombok (5 oszlop) -->
                <div class="speed-group">
                    <button id="btnSpeed5">5</button>
                    <button id="btnSpeed10">10</button>
                    <button id="btnSpeed20">20</button>
                    <button id="btnSpeed40">40</button>
                    <button id="btnSpeed80">80</button>
                    <button id="btnSpeed100">100</button>
                </div>
        
                <div id="fnButtons"></div>
        
            </div>

             <!-- Modal -->
            <div id="modal">
                <div class="modal-content">
                    <div id="modalContent"></div>
                    <button id="closeModal" style="border-radius: 8px">CLOSE</button>
                </div>
            </div>
        `;

        this.locoImage = shadow.getElementById("locoImage") as HTMLImageElement
        this.locoName = shadow.getElementById("locoName") as HTMLDivElement
        this.locoInfoSpeedElement = shadow.getElementById("locoInfoSpeed") as HTMLElement
        this.locoInfoPowerElement = shadow.getElementById("locoInfoPower") as HTMLElement
        this.locoModeInfoElement = shadow.getElementById("locoModeInfo") as HTMLElement
        this.locoImage.addEventListener("click", () => this.openLocoModal());
        shadow.getElementById("closeModal")?.addEventListener("click", () => this.closeModal());


        this.btnPower = shadow.getElementById("btnPower") as HTMLButtonElement
        this.btnPower.onclick = (e) => {
            this.openPowerModal()
        }

        this.btnEmergency = shadow.getElementById("btnEmergency") as HTMLButtonElement
        this.btnEmergency.onclick = (e: MouseEvent) => {
            Api.emergencyStop()
        }


        this.btnReverse = shadow.getElementById("btnReverse") as HTMLButtonElement
        this.btnReverse.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.currentLoco.speed, DccDirections.reverse)
            }
        }
        this.btnStop = shadow.getElementById("btnStop") as HTMLButtonElement
        this.btnStop.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, 0, this.currentLoco.direction)
            }
        }

        this.btnForward = shadow.getElementById("btnForward") as HTMLButtonElement
        this.btnForward.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.currentLoco.speed, DccDirections.forward)

            }
        }

        this.btnSpeed5 = shadow.getElementById("btnSpeed5") as HTMLButtonElement
        this.btnSpeed5.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(5), this.currentLoco.direction)
            }
        }

        this.btnSpeed10 = shadow.getElementById("btnSpeed10") as HTMLButtonElement
        this.btnSpeed10.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(10), this.currentLoco.direction)
            }
        }

        this.btnSpeed20 = shadow.getElementById("btnSpeed20") as HTMLButtonElement
        this.btnSpeed20.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(20), this.currentLoco.direction)
            }
        }

        this.btnSpeed40 = shadow.getElementById("btnSpeed40") as HTMLButtonElement
        this.btnSpeed40.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(40), this.currentLoco.direction)
            }
        }

        this.btnSpeed80 = shadow.getElementById("btnSpeed80") as HTMLButtonElement
        this.btnSpeed80.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(80), this.currentLoco.direction)
            }
        }
        this.btnSpeed100 = shadow.getElementById("btnSpeed100") as HTMLButtonElement
        this.btnSpeed100.onclick = (e) => {
            if (this.currentLoco) {
                Api.setLoco(this.currentLoco.address, this.getSpeedPercentage(100), this.currentLoco.direction)
            }
        }

        this.fnButtons = shadow.getElementById("fnButtons") as HTMLDivElement

        this.modal = shadow.getElementById("modal") as HTMLDivElement;
        this.modal.onclick = (e) => {
            if (e.target == this.modal) {
                this.closeModal()
            }
        }

        const btnTurnouts = shadow.getElementById("btnTurnouts") as HTMLButtonElement;
        btnTurnouts.onclick = (e) => {
            this.openTurnoutsModal()
        }

        for (var i = 0; i <= 28; i++) {

            const btn: FunctionButton = document.createElement('button') as FunctionButton
            this.buttons[i] = btn;
            // btn.style.backgroundColor = "cornflowerblue"
            // btn.style.backgroundColor = "#bbb"
            //btn.className = 'fnbutton'
            btn.fn = i
            btn.function = undefined;
            btn.onpointerdown = (e) => {
                if (this.currentLoco) {
                    if (btn.function) {
                        if (btn.function.momentary) {
                            Api.setLocoFunction(this.currentLoco.address, btn.fn, true)
                        } else {
                            Api.setLocoFunction(this.currentLoco.address, btn.fn, !btn.function.isOn)
                        }
                    } else {
                        const on: boolean = ((this.currentLoco.functionMap >> btn.fn) & 1) > 0
                        Api.setLocoFunction(this.currentLoco.address, btn.fn, !on)
                    }
                }
            }
            btn.onpointerup = (e) => {
                if (btn.function && btn.function.momentary && this.currentLoco) {
                    Api.setLocoFunction(this.currentLoco.address, btn.fn, false)
                }
            }

            btn.innerHTML = `F${i}`
            this.fnButtons.appendChild(btn)

        }



        // window.addEventListener('taskChangedEvent', (e: Event) => {
        // })

    }

    init() {
        this.fetchLocomotives()
        this.fetchTurnouts()
    }
    connectedCallback() {

    }


    public async fetchLocomotives() {
        try {
            const response = await fetch(`locos.json`);
            const locos = await response.json();
            let t = 0
            this.locomotives = locos.sort((a: iLocomotive, b: iLocomotive) => a.address - b.address);
            //this.render();
            if (this.locomotives.length > 0) {
                this.locomotives.forEach((l, i) => {
                    l.speed = 0
                    l.direction = DccDirections.forward
                    // setTimeout(() => {
                    // Api.getLocoInfo(l.address)
                    // }, i * 10)
                    //Api.getLocoInfo(l.address)

                })

                const i = parseInt(window.localStorage.getItem("controlPanelSelectedLocoIndex")!) || 0;

                if (i < this.locomotives.length) {
                    this.currentLoco = this.locomotives[i]
                } else {
                    this.currentLoco = this.locomotives[0]
                }
            }
        } catch (error) {
            console.error("Error fetching locomotives:", error);
        }
    }

    public async fetchTurnouts() {
        try {
            const response = await fetch(`turnouts.json`);
            const turnouts = await response.json();
            this.turnouts = turnouts.sort((a: iTurnout, b: iTurnout) => a.address - b.address);
            Api.getAllTurnout()

        } catch (error) {
            console.error("Error fetching turnouts:", error);
        }
    }


    openPowerModal() {
        const modalContent = this.shadowRoot!.getElementById("modalContent") as HTMLElement;
        modalContent.innerHTML = "";

        const div = document.createElement("div")
        div.className = "d-grid gap-2"
        modalContent.appendChild(div)

        const mainOn = document.createElement("button")
        mainOn.innerHTML = "MAIN ON"
        mainOn.className = "btn btn-success"
        div.appendChild(mainOn)
        mainOn.onclick = (e) => {
            wsClient.sendRaw("<1 MAIN>")
        }

        const mainOff = document.createElement("button")
        mainOff.innerHTML = "MAIN OFF"
        mainOff.className = "btn btn-secondary"
        div.appendChild(mainOff)
        mainOff.onclick = (e) => {
            wsClient.sendRaw("<0>")
        }

        const joinOn = document.createElement("button")
        joinOn.innerHTML = "JOIN"
        joinOn.className = "btn btn-success"
        div.appendChild(joinOn)
        joinOn.onclick = (e) => {
            wsClient.sendRaw("<1 JOIN>")
        }
        const unJoinOn = document.createElement("button")
        unJoinOn.innerHTML = "UNJOIN"
        unJoinOn.className = "btn btn-secondary"
        div.appendChild(unJoinOn)
        unJoinOn.onclick = (e) => {
            wsClient.sendRaw("<0 PROG>")
        }


        //     const html = `
        //     <div class="modal-body" style="">
        //     <div class="d-grid gap-2">
        //       <button class="btn btn-success" id="mainOn">Main ON</button>
        //       <button class="btn btn-secondary" id="mainOff">Main OFF</button>
        //       <button class="btn btn-primary" id="progOn">Prog ON</button>
        //       <button class="btn btn-dark" id="progOff">Prog OFF</button>
        //     </div>
        //   </div>        `
        //modalContent.innerHTML = html;
        this.modal.style.display = "flex"
        // const mainOn = document.getElementById("mainOn") as HTMLButtonElement
        // mainOn.onclick = (e) => {
        //     wsClient.sendRaw("<1 MAIN>")
        // }
        // const mainOff = document.getElementById("mainOff") as HTMLButtonElement
        // mainOff.onclick = (e) => {
        //     wsClient.sendRaw("<0>")
        // }


    }


    private openLocoModal() {
        const modalContent = this.shadowRoot!.getElementById("modalContent") as HTMLElement;
        modalContent.innerHTML = "";

        this.locomotives.forEach((loco) => {
            const locoItem = document.createElement("div");
            locoItem.classList.add("loco-item");
            locoItem.innerHTML = `
                <div style="justify-content: center; align-items: center">
                    <img src="${loco.imageUrl}" alt="${loco.name}">
                </div>
                <div>#${loco.address} ${loco.name}</div>
            `;

            if (loco.id === this.currentLoco?.id) {
                locoItem.classList.add("selected");
                //setTimeout(() => locoItem.scrollIntoView({ behavior: "smooth", block: "center" }), 300);
            }

            locoItem.addEventListener("click", () => {
                const i = this.locomotives.findIndex((l) => {
                    return l.address == loco.address
                })

                window.localStorage.setItem("controlPanelSelectedLocoIndex", i.toString())
                this.currentLoco = loco;
                this.closeModal()
            });
            modalContent.appendChild(locoItem);
        });

        this.modal.style.display = "flex"
    }

    openTurnoutsModal() {
        const modalContent = this.shadowRoot!.getElementById("modalContent") as HTMLElement;
        modalContent.innerHTML = "";

        this.turnouts.forEach((turnout) => {
            const tItem = document.createElement("div");
            tItem.classList.add("turnout-item");

            const elem = turnout.isLeft ? document.createElement("turnout-left-element") as Turnout : document.createElement("turnout-right-element") as Turnout
            elem.id = `turnout-${turnout.address}`
            tItem.appendChild(elem)
            elem.addEventListener("click", () => {
                const to = this.turnouts.find((t) => {
                    return turnout.address == t.address
                })
                if (to) {
                    to.isClosed = !to.isClosed
                    Api.setTurnout(to)
                    const elem = this.shadowRoot!.getElementById(`turnout-${to.address}`) as Turnout;
                    if (elem) {
                        elem.isClosed = to.isClosed != to.isInverted
                    }
                }
            })
            const title = document.createElement("div")
            title.innerHTML = `<b>#${turnout.address} ${turnout.name}</b>`
            tItem.appendChild(title)

            modalContent.appendChild(tItem);
        })

        this.modal.style.display = "flex"
    }

    closeModal() {
        this.modal.style.display = "none"
    }

    renderLocoFunctions() {
        this.locoImage.src = this.currentLoco!.imageUrl
        this.locoName.innerText = `#${this.currentLoco!.address} ${this.currentLoco!.name}`
        for (var i = 0; i <= 28; i++) {
            this.buttons[i].className = ''
            this.buttons[i].function = undefined;
            this.buttons[i].innerHTML = `F${i}`
        }

        this.currentLoco?.functions?.forEach((f) => {
            this.buttons[f.id].className = 'fnbutton'
            this.buttons[f.id].innerHTML = `F${f.id}<br>${f.name}`
            this.buttons[f.id].function = f
        })

        this.updateUI()
    }


    private _currentLoco: iLocomotive | undefined
    public get currentLoco(): iLocomotive | undefined {
        return this._currentLoco;
    }
    public set currentLoco(v: iLocomotive) {
        this._currentLoco = v;
        if (v) {
            Api.getLocoInfo(v.address)
        }
        this.renderLocoFunctions()
    }

    getSpeedPercentage(perc: number, maxSpeed: number = 126): number {
        return Math.round((maxSpeed * (perc / 100.0)));
    }
    getClosestSpeedThreshold(speed: number, maxSpeed: number = 126): number {
        if (speed == 0) {
            return 0;
        }
        // const speedThresholds = [5, 10, 40, 80, 100];
        // const speedValues = speedThresholds.map(p => Math.round((p / 100) * maxSpeed));

        // return speedValues.reduce((closest, current) =>
        //     Math.abs(currentSpeed - current) <= Math.abs(currentSpeed - closest) ? current : closest
        // );

        const percentageThresholds = [5, 10, 20, 40, 80, 100];

        // Százalékos értéket számolunk a speed alapján
        const speedPercentage = (speed / maxSpeed) * 100;

        // Megkeressük a legközelebbi százalékértéket
        return percentageThresholds.reduce((closest, current) =>
            Math.abs(speedPercentage - current) < Math.abs(speedPercentage - closest) ? current : closest
        );
    }

    calculateRealSpeed(modelSpeed: number): number {
        const scaleFactor = 87; // H0 méretarány
        const realSpeed = (modelSpeed * scaleFactor) * 3600 / 100000; // átváltás km/h-ra
        return Math.round(realSpeed * 10) / 10; // Egy tizedes jegyre kerekítés
    }

    calculateRealTrainSpeed(dccSpeed: number, realMaxSpeed: number = 120): number {
        if (dccSpeed < 0 || dccSpeed > 127) {
            throw new Error("A DCC sebességnek 0 és 127 között kell lennie.");
        }

        const realSpeed = (dccSpeed / 127) * realMaxSpeed;
        return Math.round(realSpeed * 10) / 10; // Kerekítés 1 tizedes jegyre
    }
    updateUI() {
        if (this.currentLoco) {
            const speed = this.getClosestSpeedThreshold(this.currentLoco.speed)

            this.btnReverse.style.backgroundColor = this.currentLoco.direction == DccDirections.reverse ? 'lime' : 'gray'
            this.btnStop.style.backgroundColor = this.currentLoco.speed == 0 ? 'orange' : 'gray'
            this.btnForward.style.backgroundColor = this.currentLoco.direction == DccDirections.forward ? 'lime' : 'gray'

            this.btnSpeed5.style.backgroundColor = speed == 5 ? 'lime' : 'gray'
            this.btnSpeed10.style.backgroundColor = speed == 10 ? 'lime' : 'gray'
            this.btnSpeed20.style.backgroundColor = speed == 20 ? 'lime' : 'gray'
            this.btnSpeed40.style.backgroundColor = speed == 40 ? 'lime' : 'gray'
            this.btnSpeed80.style.backgroundColor = speed == 80 ? 'lime' : 'gray'
            this.btnSpeed100.style.backgroundColor = speed == 100 ? 'lime' : 'gray'

            this.locoInfoSpeedElement.innerHTML = this.currentLoco.speed.toString()

            for (var i = 0; i <= 28; i++) {
                var on = ((this.currentLoco.functionMap >> i) & 1) == 1;
                const fn = this.currentLoco.functions.find(f => f.id == i)
                if (fn) {
                    fn.isOn = on
                }
                if (on) {
                    this.buttons[i].classList.add('on')
                } else {
                    this.buttons[i].classList.remove('on')
                }
            }



        } else {
            this.locoInfoSpeedElement.innerHTML = "Unknown Loco"
        }


    }
    public processMessage(msg: iData) {

        if (msg.type == ApiCommands.rawInfo) {

            const raw = (msg.data as iDccRaw).raw

            for (var i = 0; i < raw.length; i++) {
                var c = raw[i];
                if (c == ">") {
                    this.parse(this._data)
                    this._data = ""
                } else if (c == "<" || c == "\n" || c == "\r") {
                    this._data = ""
                    continue;
                }
                else {
                    this._data += c;
                }
            }
        }
        if (msg.type == ApiCommands.ack) {
            //console.log(msg.data)
            switch (msg.data) {
                case '<!>':
                    this.powerInfo.emergencyStop = true;
                    this.powerInfo = this.powerInfo
                    break;
            }
        }
    }

    parse(data: string) {
        if (data == "# 50") {
            //log('DccEx Data: ', data);
            return
        }

        if (data.startsWith('c CurrentMAIN')) {
            const params = data.split(" ");

            this.powerInfo.current = parseInt(params[2])
            this.locoInfoPowerElement.innerHTML = this.powerInfo.current.toString()

        }
        else if (data.startsWith('p1')) {
            console.log(data)
            const params = data.split(" ");
            this.powerInfo.info = 0b00000001
            if (params[1] == 'MAIN' || params[1] == 'A') {
                {
                    this.powerInfo.trackVoltageOn = true
                }
            } else if (params[1] == 'PROG' || params[1] == 'B') {
                this.powerInfo.programmingModeActive = true;
            }
            this.powerInfo = this.powerInfo
        }
        else if (data.startsWith('p0')) {
            console.log(data)
            const params = data.split(" ");
            this.powerInfo.info = 0b00000000
            if (params.length == 2) {
                if (params[1] == 'MAIN' || params[1] == 'A') {
                    this.powerInfo.trackVoltageOn = false
                } else if (params[1] == 'PROG' || params[1] == 'B') {
                    this.powerInfo.programmingModeActive = false;
                }
                this.powerInfo = this.powerInfo
            } else {
                this.powerInfo.trackVoltageOn = false
                this.powerInfo.programmingModeActive = false;
            }

        }
        else if (data.startsWith("Q ")) {
            // const params = data.split(" ");
            // const si = { address: parseInt(params[1]), on: true } as iSensorInfo
            // broadcastAll({ type: ApiCommands.sensorInfo, data: si } as iData)
        }
        else if (data.startsWith("q ")) {
            // const params = data.split(" ");
            // const si = { address: parseInt(params[1]), on: false } as iSensorInfo
            // broadcastAll({ type: ApiCommands.sensorInfo, data: si } as iData)
        }
        else if (data.startsWith('l')) {
            console.log(data)
            var items = data.split(" ")
            var address = parseInt(items[1])
            var speedByte = parseInt(items[3]);
            var funcMap = parseInt(items[4]);
            var direction: DccDirections = DccDirections.forward

            {
                var newSpeed = 0
                if ((speedByte >= 2) && (speedByte <= 127)) {
                    newSpeed = speedByte - 1;
                    direction = DccDirections.reverse;
                }
                else if ((speedByte >= 130) && (speedByte <= 255)) {
                    newSpeed = speedByte - 129;
                    direction = DccDirections.forward;
                }
                else if (speedByte == 0) {
                    newSpeed = 0;
                    direction = DccDirections.reverse;
                }
                else if (speedByte == 128) {
                    newSpeed = 0;
                    direction = DccDirections.forward;
                } else {
                }

                this.powerInfo.emergencyStop = speedByte == 129;
                this.powerInfo = this.powerInfo

                //                var loco: iLoco = { address: address, speed: newSpeed, direction: direction, funcMap: funcMap }

                const loco = this.locomotives.find(l => l.address == address)
                if (loco) {
                    loco.speed = newSpeed;
                    loco.direction = direction
                    loco.functionMap = funcMap



                    if (this.currentLoco && this.currentLoco.address == loco.address) {
                        this.updateUI()
                    }
                }

                // broadcastAll({ type: ApiCommands.locoInfo, data: loco } as iData)
                // log("BROADCAST DCC-EX LOCO INFO:", loco)
            }
        }
        else if (data.startsWith('H')) {
            console.log(data)
            var items = data.split(" ")
            var address = parseInt(items[1])
            var c = parseInt(items[2]) == 0;

            const turnout = this.turnouts.find((t) => t.address == address)
            if (turnout) {
                turnout.isClosed = c != turnout.isInverted
            }

        }
        else if (data.startsWith("jT")) {
            // var items = data.split(" ")
            // var address = parseInt(items[1])
            // var t: iTurnoutInfo = { address: address, isClosed: items[2] == 'C' }
            // broadcastAll({ type: ApiCommands.turnoutInfo, data: t } as iData)
        }
        else if (data.startsWith("Y")) {
            // var items = data.split(" ")

            // var address = parseInt(items[1])
            // var o: iOutputInfo = { address: address, value: items[items.length - 1] == '1' }
            // broadcastAll({ type: ApiCommands.outputInfo, data: o } as iData)
        }
        else if (data == "X") {
            console.log("(<X>) UnsuccessfulOperation !")
            // var d: iData = { type: ApiCommands.UnsuccessfulOperation, data: "DCCEx (<X>) Unsuccessful Operation!" }
            // broadcastAll(d)
        }
        else {
            // console.log("DCCExCommandCenter: Unknown data received: ", data)
            // broadcastAll({ type: ApiCommands.dccExDirectCommandResponse, data: { response: data } as iDccExDirectCommandResponse } as iData)
        }
    }

    private _powerInfo: iPowerInfo = { current: 0, emergencyStop: false, info: 0, programmingModeActive: false, shortCircuit: false, trackVoltageOn: false }
    public get powerInfo(): iPowerInfo {
        return this._powerInfo;
    }
    public set powerInfo(pi: iPowerInfo) {
        this._powerInfo = pi;
        if (pi.trackVoltageOn) {
            this.btnPower.style.backgroundColor = "green"
        } else {
            this.btnPower.style.backgroundColor = "#555555"
        }


        if (pi.emergencyStop) {
            this.btnEmergency.style.backgroundColor = "red"
        } else {
            this.btnEmergency.style.backgroundColor = "#555555"
        }

    }

    // private _powerInfo : iPowerInfo | undefined;
    // public get powerInfo() : iPowerInfo | undefined {
    //     return this._powerInfo;
    // }
    // public set powerInfo(v : iPowerInfo) {
    //     this._powerInfo = v;
    // }


}



customElements.define('loco-panel', LocoPanel)