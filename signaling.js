const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8999 });
const clients = new Map();
wss.on('connection', (ws) => {
  console.log("new connection");

  ws.on('message', (messageAsString) => {
  const message = JSON.parse(messageAsString);
      console.log(message);
      console.log(message.type);
      switch (message.type) {

        case 'registerme':
          console.log("registering"); 
          clients.set(message.id, ws);
          break;
        default:
          console.log("default");
          console.log(message);
          to = clients.get(message.to);
          to.send(JSON.stringify(message));
  
      }
      
    })
    ws.on("close", () => {
        console.log("disconnected",clients.get(ws))
        clients.delete(ws);
      });
})
