"use strict";(()=>{var f=class{sendRaw(e){this.send({type:"dccexraw",data:{raw:e}})}constructor(){}connect(){let e=document.location.protocol==="https:"?"wss:":"ws:",t=document.location.host;document.location.hostname=="127.0.0.1"&&(t="192.168.1.143");let o=`${e}//${t}/ws`;console.log(`Connecting to ${o}`),this.socket=new WebSocket(o),this.socket.onopen=()=>{console.log("WebSocket connection established."),this.onOpen&&this.onOpen()},this.socket.onmessage=s=>{try{let n=s.data.toString().replace(/[\n\r\t]/g,""),i=JSON.parse(n);this.onMessage(i)}catch{console.error("Invalid message format received:",s.data)}},this.socket.onclose=()=>{this.onClosed&&this.onClosed(),console.warn("WebSocket connection closed. Reconnecting..."),setTimeout(()=>this.connect(),1e3)},this.socket.onerror=s=>{this.onError&&this.onError(),console.error("WebSocket error:",s)},this.task&&clearInterval(this.task),this.task=setInterval(()=>{c.getSupportedLocos()},1e3)}send(e){this.socket&&this.socket.readyState===WebSocket.OPEN?this.socket.send(JSON.stringify(e)):console.log("Cannot send message. WebSocket is not open.")}onMessage(e){console.log("Processing message:",e)}},l=new f;var c=class d{static tcpSend(e){Android.send(e)}static tcpConnect(e,t){Android.connect(e,t)}static tcpDisconnect(){Android.disconnect()}static sendRaw(e){l.send({type:"dccexraw",data:{raw:e}})}static setLoco(e,t,o){d.sendRaw(`<t ${e} ${t} ${o}>`)}static getLocoInfo(e){d.sendRaw(`<t ${e}>`)}static setLocoFunction(e,t,o){d.sendRaw(`<F ${e} ${t} ${o?1:0}>`)}static emergencyStop(){d.sendRaw("<!>")}static getSupportedLocos(){d.sendRaw("<c>")}static setTurnout(e){d.sendRaw(`<T ${e.address} ${e.isClosed?0:1}>`)}static getAllTurnout(){d.sendRaw("<T>")}};var b=class extends HTMLElement{constructor(){super();this.address=0;this.angle=270;this._isClosed=!1;this.canvas=document.createElement("canvas"),this.width=40,this.height=40,this.canvas.width=60,this.canvas.height=60,this.ctx=this.canvas.getContext("2d")}connectedCallback(){this.appendChild(this.canvas),this.draw()}init(t){this.address=t}draw(){this.ctx&&(this.ctx.save(),this.ctx.beginPath(),this.ctx.fillStyle="silver",this.ctx.fillRect(0,0,this.canvas.width,this.canvas.height),this.ctx.stroke(),this.ctx.restore())}get isClosed(){return this._isClosed}set isClosed(t){this._isClosed=t,this.draw()}get posLeft(){return 10}get posTop(){return 10}get posRight(){return 50}get posBottom(){return 50}get centerX(){return 30}get centerY(){return 30}get stateColor(){return"silver"}},u=class extends b{draw(){super.draw(),this.ctx&&(this.ctx.save(),this.drawTurnout(this.ctx,this.isClosed),this.ctx.restore())}drawTurnout(e,t){if(e.beginPath(),e.strokeStyle="black",e.lineWidth=7,this.angle==0?(e.moveTo(this.posLeft,this.centerY),e.lineTo(this.posRight,this.centerY),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posRight,this.posBottom)):this.angle==45?(e.moveTo(this.posLeft,this.posTop),e.lineTo(this.posRight,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.centerX,this.posBottom)):this.angle==90?(e.moveTo(this.centerX,this.posTop),e.lineTo(this.centerX,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posLeft,this.posBottom)):this.angle==135?(e.moveTo(this.posRight,this.posTop),e.lineTo(this.posLeft,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posLeft,this.centerY)):this.angle==180?(e.moveTo(this.posLeft,this.centerY),e.lineTo(this.posRight,this.centerY),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posLeft,this.posTop)):this.angle==225?(e.moveTo(this.posLeft,this.posTop),e.lineTo(this.posRight,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.centerX,this.posTop)):this.angle==270?(e.moveTo(this.centerX,this.posTop),e.lineTo(this.centerX,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posRight,this.posTop)):this.angle==315&&(e.moveTo(this.posRight,this.posTop),e.lineTo(this.posLeft,this.posBottom),e.moveTo(this.centerX,this.centerY),e.lineTo(this.posRight,this.centerY)),e.stroke(),t){e.beginPath(),e.strokeStyle=this.stateColor,e.lineWidth=3;var o=this.width/5;this.angle==0?(e.moveTo(this.posLeft+o,this.centerY),e.lineTo(this.posRight-o,this.centerY)):this.angle==45?(e.moveTo(this.posLeft+o,this.posTop+o),e.lineTo(this.posRight-o,this.posBottom-o)):this.angle==90?(e.moveTo(this.centerX,this.posTop+o),e.lineTo(this.centerX,this.posBottom-o)):this.angle==135?(e.moveTo(this.posRight-o,this.posTop+o),e.lineTo(this.posLeft+o,this.posBottom-o)):this.angle==180?(e.moveTo(this.posLeft+o,this.centerY),e.lineTo(this.posRight-o,this.centerY)):this.angle==225?(e.moveTo(this.posLeft+o,this.posTop+o),e.lineTo(this.posRight-o,this.posBottom-o)):this.angle==270?(e.moveTo(this.centerX,this.posTop+o),e.lineTo(this.centerX,this.posBottom-o)):this.angle==315&&(e.moveTo(this.posRight-o,this.posTop+o),e.lineTo(this.posLeft+o,this.posBottom-o)),e.stroke()}else{e.beginPath(),e.strokeStyle=this.stateColor,e.lineWidth=3;var o=this.width/5,s=this.width/5;this.angle==0?(e.moveTo(this.posLeft+o,this.centerY),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posRight-s,this.posBottom-s)):this.angle==45?(e.moveTo(this.posLeft+o,this.posTop+o),e.lineTo(this.centerX,this.centerY),e.lineTo(this.centerX,this.posBottom-s)):this.angle==90?(e.moveTo(this.centerX,this.posTop+o),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posLeft+s,this.posBottom-s)):this.angle==135?(e.moveTo(this.posRight-s,this.posTop+s),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posLeft+o,this.centerY)):this.angle==180?(e.moveTo(this.posLeft+s,this.posTop+s),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posRight-o,this.centerY)):this.angle==225?(e.moveTo(this.centerX,this.posTop+o),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posRight-s,this.posBottom-s)):this.angle==270?(e.moveTo(this.posRight-s,this.posTop+s),e.lineTo(this.centerX,this.centerY),e.lineTo(this.centerX,this.posBottom-o)):this.angle==315&&(e.moveTo(this.posRight-o,this.centerY),e.lineTo(this.centerX,this.centerY),e.lineTo(this.posLeft+s,this.posBottom-s)),e.stroke()}e.beginPath(),e.lineWidth=1,e.strokeStyle="black",e.fillStyle="silver",e.arc(this.centerX,this.centerY,3,0,2*Math.PI),e.fill(),e.stroke()}};customElements.define("turnout-right-element",u);var v=class extends u{constructor(){super(),this.angle=90}drawTurnout(e,t){e.save(),e.translate(this.centerX,this.centerY),e.scale(1,-1),e.translate(-this.centerX,-this.centerY),super.drawTurnout(e,t),e.restore()}};customElements.define("turnout-left-element",v);console.log(u);var p=class extends HTMLElement{constructor(){super();this.locomotives=[];this.buttons={};this._data="";this.turnouts=[];this._powerInfo={current:0,emergencyStop:!1,info:0,programmingModeActive:!1,shortCircuit:!1,trackVoltageOn:!1};let t=this.attachShadow({mode:"open"});t.innerHTML=`
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
                /* Mozdonyk\xE9p st\xEDlus */
                #locoImage {
                    height: 64px;
                    max-width: 400px;
                    cursor:pointer;
                    
                }


                /* Gombcsoport az ir\xE1nygombokhoz (20% - 60% - 20%) */
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
        
                /* Sebess\xE9g gombok (5 oszlop, azaz 20%-os eloszt\xE1s) */
                .speed-group {
                    display: grid;
                    grid-template-columns: repeat(6, 1fr);
                    width: 100%;
                    gap: 4px;
                    margin-bottom: 4px;
                }
        
                /* Gombok \xE1ltal\xE1nos st\xEDlusa */
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
                    grid-template-rows: repeat(6, 1fr); /* 12 egyenl\u0151 sor */
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
        
                /* Akt\xEDv \xE1llapot */
                button:active {
                    background:  rgb(25, 25, 25);
                    transform: scale(0.99);
                }
        
                /* Funkci\xF3 gombok */
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

                /* Modal h\xE1tt\xE9r  */
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

                /* Kijel\xF6lt mozdony */
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

                /* Modal bez\xE1r\xF3 gomb */
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
                        <div id="locoName">#11 CS\xD6RG\u0150</div>
                    </div>
                    <div id="infoRight">INFO R</div>
                </div>

                <div id="locoInfo" style="display: flex">
                    <div id="locoInfoSpeed">10</div>
                    <div id="locoInfoPower" style="color: #555555; font-size: 0.6em;display: flex; flex-direction: column; justify-content: flex-end; ">10</div>
                </div>

                <div class="control-group">
                    <button id="btnPower" class="btn btn-primary flex-fill py-3">\u26A1</button>
                    <button id="btnRoutes" class="btn btn-warning flex-fill py-3">\u{1F500}</button>
                    <button id="btnTurnouts" class="btn btn-success flex-fill py-3">\u{1F50C}</button>
                    <button id="btnEmergency" class="btn btn-danger flex-fill py-3">\u{1F6D1}</button>
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
                

                <!-- K\xDCL\xD6N SOR: Ir\xE1ny\xEDt\xE1s (20% - 60% - 20%) -->
                <div class="direction-group">
                    <button id="btnReverse">&lt;&lt;</button>
                    <button id="btnStop">STOP</button>
                    <button id="btnForward">&gt;&gt;</button>
                </div>
        
                <!-- K\xDCL\xD6N SOR: Sebess\xE9g gombok (5 oszlop) -->
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
        `,this.locoImage=t.getElementById("locoImage"),this.locoName=t.getElementById("locoName"),this.locoInfoSpeedElement=t.getElementById("locoInfoSpeed"),this.locoInfoPowerElement=t.getElementById("locoInfoPower"),this.locoModeInfoElement=t.getElementById("locoModeInfo"),this.locoImage.addEventListener("click",()=>this.openLocoModal()),t.getElementById("closeModal")?.addEventListener("click",()=>this.closeModal()),this.btnPower=t.getElementById("btnPower"),this.btnPower.onclick=n=>{this.openPowerModal()},this.btnEmergency=t.getElementById("btnEmergency"),this.btnEmergency.onclick=n=>{c.emergencyStop()},this.btnReverse=t.getElementById("btnReverse"),this.btnReverse.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.currentLoco.speed,0)},this.btnStop=t.getElementById("btnStop"),this.btnStop.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,0,this.currentLoco.direction)},this.btnForward=t.getElementById("btnForward"),this.btnForward.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.currentLoco.speed,1)},this.btnSpeed5=t.getElementById("btnSpeed5"),this.btnSpeed5.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(5),this.currentLoco.direction)},this.btnSpeed10=t.getElementById("btnSpeed10"),this.btnSpeed10.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(10),this.currentLoco.direction)},this.btnSpeed20=t.getElementById("btnSpeed20"),this.btnSpeed20.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(20),this.currentLoco.direction)},this.btnSpeed40=t.getElementById("btnSpeed40"),this.btnSpeed40.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(40),this.currentLoco.direction)},this.btnSpeed80=t.getElementById("btnSpeed80"),this.btnSpeed80.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(80),this.currentLoco.direction)},this.btnSpeed100=t.getElementById("btnSpeed100"),this.btnSpeed100.onclick=n=>{this.currentLoco&&c.setLoco(this.currentLoco.address,this.getSpeedPercentage(100),this.currentLoco.direction)},this.fnButtons=t.getElementById("fnButtons"),this.modal=t.getElementById("modal"),this.modal.onclick=n=>{n.target==this.modal&&this.closeModal()};let o=t.getElementById("btnTurnouts");o.onclick=n=>{this.openTurnoutsModal()};for(var s=0;s<=28;s++){let n=document.createElement("button");this.buttons[s]=n,n.fn=s,n.function=void 0,n.onpointerdown=i=>{if(this.currentLoco)if(n.function)n.function.momentary?c.setLocoFunction(this.currentLoco.address,n.fn,!0):c.setLocoFunction(this.currentLoco.address,n.fn,!n.function.isOn);else{let r=(this.currentLoco.functionMap>>n.fn&1)>0;c.setLocoFunction(this.currentLoco.address,n.fn,!r)}},n.onpointerup=i=>{n.function&&n.function.momentary&&this.currentLoco&&c.setLocoFunction(this.currentLoco.address,n.fn,!1)},n.innerHTML=`F${s}`,this.fnButtons.appendChild(n)}}init(){this.fetchLocomotives(),this.fetchTurnouts()}connectedCallback(){}async fetchLocomotives(){try{let o=await(await fetch("locos.json")).json(),s=0;if(this.locomotives=o.sort((n,i)=>n.address-i.address),this.locomotives.length>0){this.locomotives.forEach((i,r)=>{i.speed=0,i.direction=1});let n=parseInt(window.localStorage.getItem("controlPanelSelectedLocoIndex"))||0;n<this.locomotives.length?this.currentLoco=this.locomotives[n]:this.currentLoco=this.locomotives[0]}}catch(t){console.error("Error fetching locomotives:",t)}}async fetchTurnouts(){try{let o=await(await fetch("turnouts.json")).json();this.turnouts=o.sort((s,n)=>s.address-n.address),c.getAllTurnout()}catch(t){console.error("Error fetching turnouts:",t)}}openPowerModal(){let t=this.shadowRoot.getElementById("modalContent");t.innerHTML="";let o=document.createElement("div");o.className="d-grid gap-2",t.appendChild(o);let s=document.createElement("button");s.innerHTML="MAIN ON",s.className="btn btn-success",o.appendChild(s),s.onclick=h=>{l.sendRaw("<1 MAIN>")};let n=document.createElement("button");n.innerHTML="MAIN OFF",n.className="btn btn-secondary",o.appendChild(n),n.onclick=h=>{l.sendRaw("<0>")};let i=document.createElement("button");i.innerHTML="JOIN",i.className="btn btn-success",o.appendChild(i),i.onclick=h=>{l.sendRaw("<1 JOIN>")};let r=document.createElement("button");r.innerHTML="UNJOIN",r.className="btn btn-secondary",o.appendChild(r),r.onclick=h=>{l.sendRaw("<0 PROG>")},this.modal.style.display="flex"}openLocoModal(){let t=this.shadowRoot.getElementById("modalContent");t.innerHTML="",this.locomotives.forEach(o=>{let s=document.createElement("div");s.classList.add("loco-item"),s.innerHTML=`
                <div style="justify-content: center; align-items: center">
                    <img src="${o.imageUrl}" alt="${o.name}">
                </div>
                <div>#${o.address} ${o.name}</div>
            `,o.id===this.currentLoco?.id&&s.classList.add("selected"),s.addEventListener("click",()=>{let n=this.locomotives.findIndex(i=>i.address==o.address);window.localStorage.setItem("controlPanelSelectedLocoIndex",n.toString()),this.currentLoco=o,this.closeModal()}),t.appendChild(s)}),this.modal.style.display="flex"}openTurnoutsModal(){let t=this.shadowRoot.getElementById("modalContent");t.innerHTML="",this.turnouts.forEach(o=>{let s=document.createElement("div");s.classList.add("turnout-item");let n=o.isLeft?document.createElement("turnout-left-element"):document.createElement("turnout-right-element");n.id=`turnout-${o.address}`,s.appendChild(n),n.addEventListener("click",()=>{let r=this.turnouts.find(h=>o.address==h.address);if(r){r.isClosed=!r.isClosed,c.setTurnout(r);let h=this.shadowRoot.getElementById(`turnout-${r.address}`);h&&(h.isClosed=r.isClosed!=r.isInverted)}});let i=document.createElement("div");i.innerHTML=`<b>#${o.address} ${o.name}</b>`,s.appendChild(i),t.appendChild(s)}),this.modal.style.display="flex"}closeModal(){this.modal.style.display="none"}renderLocoFunctions(){this.locoImage.src=this.currentLoco.imageUrl,this.locoName.innerText=`#${this.currentLoco.address} ${this.currentLoco.name}`;for(var t=0;t<=28;t++)this.buttons[t].className="",this.buttons[t].function=void 0,this.buttons[t].innerHTML=`F${t}`;this.currentLoco?.functions?.forEach(o=>{this.buttons[o.id].className="fnbutton",this.buttons[o.id].innerHTML=`F${o.id}<br>${o.name}`,this.buttons[o.id].function=o}),this.updateUI()}get currentLoco(){return this._currentLoco}set currentLoco(t){this._currentLoco=t,t&&c.getLocoInfo(t.address),this.renderLocoFunctions()}getSpeedPercentage(t,o=126){return Math.round(o*(t/100))}getClosestSpeedThreshold(t,o=126){if(t==0)return 0;let s=[5,10,20,40,80,100],n=t/o*100;return s.reduce((i,r)=>Math.abs(n-r)<Math.abs(n-i)?r:i)}calculateRealSpeed(t){let s=t*87*3600/1e5;return Math.round(s*10)/10}calculateRealTrainSpeed(t,o=120){if(t<0||t>127)throw new Error("A DCC sebess\xE9gnek 0 \xE9s 127 k\xF6z\xF6tt kell lennie.");let s=t/127*o;return Math.round(s*10)/10}updateUI(){if(this.currentLoco){let s=this.getClosestSpeedThreshold(this.currentLoco.speed);this.btnReverse.style.backgroundColor=this.currentLoco.direction==0?"lime":"gray",this.btnStop.style.backgroundColor=this.currentLoco.speed==0?"orange":"gray",this.btnForward.style.backgroundColor=this.currentLoco.direction==1?"lime":"gray",this.btnSpeed5.style.backgroundColor=s==5?"lime":"gray",this.btnSpeed10.style.backgroundColor=s==10?"lime":"gray",this.btnSpeed20.style.backgroundColor=s==20?"lime":"gray",this.btnSpeed40.style.backgroundColor=s==40?"lime":"gray",this.btnSpeed80.style.backgroundColor=s==80?"lime":"gray",this.btnSpeed100.style.backgroundColor=s==100?"lime":"gray",this.locoInfoSpeedElement.innerHTML=this.currentLoco.speed.toString();for(var t=0;t<=28;t++){var o=(this.currentLoco.functionMap>>t&1)==1;let n=this.currentLoco.functions.find(i=>i.id==t);n&&(n.isOn=o),o?this.buttons[t].classList.add("on"):this.buttons[t].classList.remove("on")}}else this.locoInfoSpeedElement.innerHTML="Unknown Loco"}processMessage(t){if(t.type=="rawInfo"){let n=t.data.raw;for(var o=0;o<n.length;o++){var s=n[o];if(s==">")this.parse(this._data),this._data="";else if(s=="<"||s==`
`||s=="\r"){this._data="";continue}else this._data+=s}}if(t.type=="ack")switch(t.data){case"<!>":this.powerInfo.emergencyStop=!0,this.powerInfo=this.powerInfo;break}}parse(t){if(t!="# 50"){if(t.startsWith("c CurrentMAIN")){let a=t.split(" ");this.powerInfo.current=parseInt(a[2]),this.locoInfoPowerElement.innerHTML=this.powerInfo.current.toString()}else if(t.startsWith("p1")){console.log(t);let a=t.split(" ");this.powerInfo.info=1,a[1]=="MAIN"||a[1]=="A"?this.powerInfo.trackVoltageOn=!0:(a[1]=="PROG"||a[1]=="B")&&(this.powerInfo.programmingModeActive=!0),this.powerInfo=this.powerInfo}else if(t.startsWith("p0")){console.log(t);let a=t.split(" ");this.powerInfo.info=0,a.length==2?(a[1]=="MAIN"||a[1]=="A"?this.powerInfo.trackVoltageOn=!1:(a[1]=="PROG"||a[1]=="B")&&(this.powerInfo.programmingModeActive=!1),this.powerInfo=this.powerInfo):(this.powerInfo.trackVoltageOn=!1,this.powerInfo.programmingModeActive=!1)}else if(!t.startsWith("Q ")){if(!t.startsWith("q "))if(t.startsWith("l")){console.log(t);var o=t.split(" "),s=parseInt(o[1]),n=parseInt(o[3]),i=parseInt(o[4]),r=1;{var h=0;n>=2&&n<=127?(h=n-1,r=0):n>=130&&n<=255?(h=n-129,r=1):n==0?(h=0,r=0):n==128&&(h=0,r=1),this.powerInfo.emergencyStop=n==129,this.powerInfo=this.powerInfo;let a=this.locomotives.find(w=>w.address==s);a&&(a.speed=h,a.direction=r,a.functionMap=i,this.currentLoco&&this.currentLoco.address==a.address&&this.updateUI())}}else if(t.startsWith("H")){console.log(t);var o=t.split(" "),s=parseInt(o[1]),T=parseInt(o[2])==0;let m=this.turnouts.find(I=>I.address==s);m&&(m.isClosed=T!=m.isInverted)}else t.startsWith("jT")||t.startsWith("Y")||t=="X"&&console.log("(<X>) UnsuccessfulOperation !")}}}get powerInfo(){return this._powerInfo}set powerInfo(t){this._powerInfo=t,t.trackVoltageOn?this.btnPower.style.backgroundColor="green":this.btnPower.style.backgroundColor="#555555",t.emergencyStop?this.btnEmergency.style.backgroundColor="red":this.btnEmergency.style.backgroundColor="#555555"}};customElements.define("loco-panel",p);console.log(p);console.log(c);var g=class d{constructor(){this.config={startup:{power:"<1 MAIN>",init:`<s>
<T 1 DCC 1>`}};d.instance=this,this.cp=document.createElement("loco-panel"),document.body.appendChild(this.cp),l.onOpen=()=>{l.sendRaw(this.config.startup.power),l.sendRaw(this.config.startup.init),setTimeout(()=>{this.cp.init()},100)},l.onClosed=()=>{},l.onMessage=e=>{this.cp.processMessage(e)},fetch("config.json").then(e=>{if(!e.ok)throw new Error(`HTTP error ${e.status}`);return e.json()}).then(e=>{this.config=e}).catch(e=>console.error("Hiba a config.json beolvas\xE1sakor:",e)).finally(()=>{l.connect()})}onMessage(e){}sendRaw(e){let t={type:"dccexraw",data:{raw:e}};l.socket.send(JSON.stringify(t))}},Z=new g;})();
