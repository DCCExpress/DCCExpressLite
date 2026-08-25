import { DataManager } from "./datamanager.js";
import { WebSocketClient } from "./ws.js";

export const turnoutManager = new class extends DataManager {
  constructor() {
    super("turnouts.json");
    this.render()
    this.ws = new WebSocketClient()
    this.ws.connect()
    this.ws.on("ack", (d) => {
      console.log("ACK", d)
    })
    this.ws.on("rawInfo", (d) => {
      console.log("rawInfo", d)
    })
  }

  render() {
    const container = document.getElementById("itemList");
    container.innerHTML = "";

    const title = document.createElement("h2");
    title.textContent = "Turnouts";
    container.appendChild(title);

    const addButton = document.createElement("button");
    addButton.className = "btn btn-success mb-3";
    addButton.textContent = "New Turnout...";
    addButton.onclick = () => {
      const newId = Date.now();
      const address = 0
      this.new({ id: newId, name: `T${address}`, address: address, isInverted: false, isLeft: false });
    };
    container.appendChild(addButton);

    this.items.forEach(item => {
      const div = document.createElement("div");
      div.className = "item mb-2 p-2 border rounded";
      div.innerHTML = `
        <div><strong>${item.name}</strong></div>
        <div>ID: ${item.id}</div>
        <div>Address: ${item.address || 0}</div>

        <div class="form-check">
          <input class="form-check-input" type="checkbox" value="" ${item.isLeft ? 'checked' : ''} disabled>
          <label class="form-check-label">
            Left Turnout
          </label>
        </div>
        <div class="form-check">
          <input class="form-check-input" type="checkbox" value="" ${item.isInverted ? 'checked' : ''} disabled>
          <label class="form-check-label">
            Inverted
          </label>
        </div>

        <button class="btn btn-sm btn-primary me-2" onclick="turnoutManager.showModal(${item.id})">Edit</button>
        <button class="btn btn-sm btn-danger" onclick="turnoutManager.delete(${item.id})">Delete</button>
      `;
      container.appendChild(div);
    });
  }

  showModal(id) {
    const item = this.items.find(i => i.id === id);
    if (!item) return;

    const modalElement = document.getElementById("editModal");
    modalElement.querySelector("#itemName").value = item.name;
    modalElement.querySelector("#itemId").value = item.id;
    modalElement.querySelector("#itemAddress").value = item.address;
    modalElement.querySelector("#itemIsLeft").checked = item.isLeft;
    modalElement.querySelector("#itemInverted").checked = item.isInverted;
    

    //    modalEl.classList.add("show");

    const modal = new bootstrap.Modal(modalElement);
    modal.show();


    const btnOK = modalElement.querySelector(".modal-ok");
    const btnCancel = modalElement.querySelector(".modal-cancel");
    const btnClose = modalElement.querySelector(".modal-close");
    const btnTurnOn = modalElement.querySelector(".modal-turnOn");
    const btnTurnOff = modalElement.querySelector(".modal-turnOff");

    btnOK.onclick = () => {
      item.name = modalElement.querySelector("#itemName").value;
      item.address = parseInt(modalElement.querySelector("#itemAddress").value);
      item.isLeft = modalElement.querySelector("#itemIsLeft").checked;
      item.isInverted = modalElement.querySelector("#itemInverted").checked;
      
      this.edit(item.id, item);
      modal.hide()
    };

    btnCancel.onclick = () => {
      modal.hide()
    };
    btnClose.onclick = () => {
      modal.hide()
    };

    btnTurnOn.onclick = () => {
      this.test(true)
    }
    btnTurnOff.onclick = () => {
      this.test(false)
    }
  }
  test(isClosed) {
    const modalElement = document.getElementById("editModal");
    const address = modalElement.querySelector("#itemAddress").value;
    const isLeft = modalElement.querySelector("#itemIsLeft").checked;
    const inverted = modalElement.querySelector("#itemInverted").checked;


      this.ws.sendRaw(`<T ${address} ${(isClosed == !inverted) ? 0 : 1}>`)
  }


}();