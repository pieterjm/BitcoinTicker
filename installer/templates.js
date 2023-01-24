const stringifyNice = (data) => JSON.stringify(data, null, 2)
const templates = Object.freeze({
    elements: {
        fileName: "elements.json",
        value: stringifyNice([
            {
                "name": "ssid",
                "type": "ACInput",
                "value": "",
                "label": "Wi-Fi SSID",
                "pattern": "",
                "placeholder": "",
                "style": "",
                "apply": "text"
            },
            {
                "name": "wifipassword",
                "type": "ACInput",
                "value": "",
                "label": "Wi-Fi Password",
                "pattern": "",
                "placeholder": "",
                "style": "",
                "apply": "text"
            },
            {
                "name": "gertyurl",
                "type": "ACInput",
                "value": "",
                "label": "Gerty URL",
                "pattern": "",
                "placeholder": "",
                "style": "",
                "apply": "text"
            },
            {
                "name": "updatefrequency",
                "type": "ACInput",
                "value": "",
                "label": "Screen update frequency in ms",
                "pattern": "",
                "placeholder": "",
                "style": "",
                "apply": "text"
            }            
        ])
    }
})

