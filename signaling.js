const WebSocket = require('ws');
server = require("express");
app = server();
require("ejs");
app.set("view ejgine", "ejs");

const wss = new WebSocket.Server({ port: 8999 });
const clients = new Map();
app.get('/',(req,res)=>{
  res.render("../client.ejs");
})
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
          if(!to){
            console.log("user with id ",message.to," not found");
            return;
          }
          to.send(JSON.stringify(message));
  
      }
      
    })
    ws.on("close", () => {
        console.log("disconnected",clients.get(ws))
        clients.delete(ws);
      });
})
app.listen(3001);
