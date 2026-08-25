export class Turnout extends HTMLElement {
    address: number = 0
    canvas: HTMLCanvasElement;
    ctx: CanvasRenderingContext2D | null;
    angle: number = 270;
    width: number;
    height: number;

    constructor() {
        super()
        this.canvas = document.createElement("canvas") as HTMLCanvasElement
        this.width = 40;
        this.height = 40
        this.canvas.width = 60
        this.canvas.height = 60
        this.ctx = this.canvas.getContext('2d')
    }

    connectedCallback() {
        this.appendChild(this.canvas)
        this.draw()
    }

    init(address: number) {
        this.address = address
    }

    draw() {
        if (this.ctx) {
            this.ctx.save()

            this.ctx.beginPath()
            this.ctx.fillStyle = "silver"
            this.ctx.fillRect(0,0, this.canvas.width, this.canvas.height)
            this.ctx.stroke()

            this.ctx.restore();
        }
    }

    

    private _isClosed: boolean = false;
    public get isClosed(): boolean {
        return this._isClosed;
    }
    public set isClosed(v: boolean) {
        this._isClosed = v;
        this.draw();
    }

    get posLeft(): number {
        return 10;
    }
    get posTop(): number {
        return 10;
    }
    get posRight(): number {
        return 50;
    }
    get posBottom(): number {
        return 50;
    }

    get centerX(): number {
        return 30;
    }
    get centerY(): number {
        return 30;
    }

    get stateColor(): string {
        return "silver"
    }
}

export class TurnoutRightElement extends Turnout {

    draw() {
        super.draw();
        if(this.ctx) {
            this.ctx.save()
            this.drawTurnout(this.ctx, this.isClosed)
            this.ctx.restore()
        }
        
    }

    public drawTurnout(ctx: CanvasRenderingContext2D, t1Closed: boolean): void {

        ctx.beginPath();
        ctx.strokeStyle = "black"
        ctx.lineWidth = 7;

        if (this.angle == 0) {
            ctx.moveTo(this.posLeft, this.centerY)
            ctx.lineTo(this.posRight, this.centerY)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posRight, this.posBottom)
        }
        else if (this.angle == 45) {
            ctx.moveTo(this.posLeft, this.posTop)
            ctx.lineTo(this.posRight, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.centerX, this.posBottom)
        }
        else if (this.angle == 90) {
            ctx.moveTo(this.centerX, this.posTop)
            ctx.lineTo(this.centerX, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posLeft, this.posBottom)
        }
        else if (this.angle == 135) {
            ctx.moveTo(this.posRight, this.posTop)
            ctx.lineTo(this.posLeft, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posLeft, this.centerY)
        }
        else if (this.angle == 180) {
            ctx.moveTo(this.posLeft, this.centerY)
            ctx.lineTo(this.posRight, this.centerY)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posLeft, this.posTop)
        }
        else if (this.angle == 225) {
            ctx.moveTo(this.posLeft, this.posTop)
            ctx.lineTo(this.posRight, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.centerX, this.posTop)
        }
        else if (this.angle == 270) {
            ctx.moveTo(this.centerX, this.posTop)
            ctx.lineTo(this.centerX, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posRight, this.posTop)
        }
        else if (this.angle == 315) {
            ctx.moveTo(this.posRight, this.posTop)
            ctx.lineTo(this.posLeft, this.posBottom)
            ctx.moveTo(this.centerX, this.centerY)
            ctx.lineTo(this.posRight, this.centerY)
        }
        ctx.stroke();


        // var color = Colors.TrackLightColor
        // switch (this.state) {
        //     case RailStates.selected: color = Colors.TrackSelectedColor
        //         break;
        //     case RailStates.occupied: color = Colors.TrackDangerColor
        //         break;
        // }
        // CLOSED
        if (t1Closed) {
            ctx.beginPath();


            ctx.strokeStyle = this.stateColor
            ctx.lineWidth = 3;

            var dx = this.width / 5
            if (this.angle == 0) {
                ctx.moveTo(this.posLeft + dx, this.centerY)
                ctx.lineTo(this.posRight - dx, this.centerY)
            }
            else if (this.angle == 45) {
                ctx.moveTo(this.posLeft + dx, this.posTop + dx)
                ctx.lineTo(this.posRight - dx, this.posBottom - dx)
            }
            else if (this.angle == 90) {
                ctx.moveTo(this.centerX, this.posTop + dx)
                ctx.lineTo(this.centerX, this.posBottom - dx)
            }
            else if (this.angle == 135) {
                ctx.moveTo(this.posRight - dx, this.posTop + dx)
                ctx.lineTo(this.posLeft + dx, this.posBottom - dx)
            }
            else if (this.angle == 180) {
                ctx.moveTo(this.posLeft + dx, this.centerY)
                ctx.lineTo(this.posRight - dx, this.centerY)
            }
            else if (this.angle == 225) {
                ctx.moveTo(this.posLeft + dx, this.posTop + dx)
                ctx.lineTo(this.posRight - dx, this.posBottom - dx)
            }
            else if (this.angle == 270) {
                ctx.moveTo(this.centerX, this.posTop + dx)
                ctx.lineTo(this.centerX, this.posBottom - dx)
            }
            else if (this.angle == 315) {
                ctx.moveTo(this.posRight - dx, this.posTop + dx)
                ctx.lineTo(this.posLeft + dx, this.posBottom - dx)
            }


            ctx.stroke();
        } else {
            ctx.beginPath();
            ctx.strokeStyle = this.stateColor
            ctx.lineWidth = 3;

            var dx = this.width / 5
            var dx2 = this.width / 5

            if (this.angle == 0) {
                ctx.moveTo(this.posLeft + dx, this.centerY)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posRight - dx2, this.posBottom - dx2)
            }
            else if (this.angle == 45) {
                ctx.moveTo(this.posLeft + dx, this.posTop + dx)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.centerX, this.posBottom - dx2)
            }
            else if (this.angle == 90) {
                ctx.moveTo(this.centerX, this.posTop + dx)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posLeft + dx2, this.posBottom - dx2)
            }
            else if (this.angle == 135) {
                ctx.moveTo(this.posRight - dx2, this.posTop + dx2)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posLeft + dx, this.centerY)
            }
            else if (this.angle == 180) {
                ctx.moveTo(this.posLeft + dx2, this.posTop + dx2)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posRight - dx, this.centerY)
            }
            else if (this.angle == 225) {
                ctx.moveTo(this.centerX, this.posTop + dx)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posRight - dx2, this.posBottom - dx2)
            }
            else if (this.angle == 270) {
                ctx.moveTo(this.posRight - dx2, this.posTop + dx2)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.centerX, this.posBottom - dx)
            }
            else if (this.angle == 315) {
                ctx.moveTo(this.posRight - dx, this.centerY)
                ctx.lineTo(this.centerX, this.centerY)
                ctx.lineTo(this.posLeft + dx2, this.posBottom - dx2)
            }
            ctx.stroke();
        }

        ctx.beginPath();
        ctx.lineWidth = 1
        ctx.strokeStyle = "black"
        ctx.fillStyle = "silver"
        ctx.arc(this.centerX, this.centerY, 3, 0, 2 * Math.PI)
        ctx.fill()
        ctx.stroke()

    }  
}

customElements.define("turnout-right-element", TurnoutRightElement)


export class TurnoutLeftElement extends TurnoutRightElement {

    constructor() {
        super()
        this.angle = 90
    }

    public drawTurnout(ctx: CanvasRenderingContext2D, t1Closed: boolean): void {
        ctx.save()
        ctx.translate(this.centerX, this.centerY);
        ctx.scale(1, -1)
        ctx.translate(-this.centerX, -this.centerY);
        super.drawTurnout(ctx, t1Closed)
        ctx.restore()
    }
}

customElements.define("turnout-left-element", TurnoutLeftElement)