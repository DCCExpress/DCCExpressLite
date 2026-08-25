export class DataManager {
    constructor(fileName) {
        this.fileName = fileName
        this.items = []

        this.ip = document.location.hostname
        if(this.ip == "127.0.0.1") {
            this.ip = '192.168.1.143'
        }
    }

    async load() {
        try {
            const res = await fetch(`http://${this.ip}/${this.fileName}`)
            if(res.ok) {
            this.items = await res.json()
            } else {
                alert(`${this.fileName} does not exists!`)
            }
            this.render()
        } catch (e) {
            alert("Failed to load data:\n" + e)
        }
    }

    async save() {
        try {
            await fetch(`http://${this.ip}/upload`, {
                method: "POST",
                body: this.createFormData()

            })
        } catch (e) {
            alert("Failed to save data:\n" + e)
        }
    }

    createFormData() {
        const form = new FormData();
        const blob = new Blob([JSON.stringify(this.items, null, 2)], { type: 'application/json' });
        form.append("file", blob, this.fileName);
        form.append("target", "");
        return form;
    }

    new(item) {
        this.items.push(item);
        this.save();
        this.render();
    }

    edit(id, item) {
        const index = this.items.findIndex(i => i.id === id);
        if (index >= 0) {
            this.items[index] = item;
            this.save();
            this.render();
        }
    }

    delete(id) {
        this.items = this.items.filter(i => i.id !== id);
        this.save();
        this.render();
    }

    render() {
    }

    showModal(id) {
    }
}