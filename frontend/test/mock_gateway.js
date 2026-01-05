const WebSocket = require('ws');

// Configuration
const GATEWAY_URL = 'ws://localhost:8888';
const CLIENTS_COUNT = 1;
const MESSAGES_PER_CLIENT = 10000;
const MSG_SIZE = 1024; // 1KB frame

const clients = [];

function startStressTest() {
    console.log(`Starting WS Stress Test: ${CLIENTS_COUNT} clients, ${MESSAGES_PER_CLIENT} msgs each...`);

    for (let i = 0; i < CLIENTS_COUNT; i++) {
        const ws = new WebSocket(GATEWAY_URL);

        ws.on('open', () => {
            console.log(`Client ${i} connected`);
            // Wait for ID assigment then start blasting
        });

        ws.on('message', (data) => {
            // In a real relay scenario, we might receive what we sent if backend echoes
            // Here we just measure connectivity or act as "Backend" simulator?
            // Actually, this script acts as a CLIENT connecting to Gateway.
            // But to test Frontend, we need a Mock Gateway/Backend pushing data TO Frontend.

            // Wait, "Frontend Stress" means stressing the BROWSER.
            // So we need a script that acts as GATEWAY pushing data TO Browser.
        });
    }
}

// CORRECT APPROACH: Mock Gateway Server
// Run this, then open Frontend. It will blast data to Frontend.

const { WebSocketServer } = require('ws');

const wss = new WebSocketServer({ port: 8899 }); // Different port for testing

console.log("Mock Gateway running on ws://localhost:8899");
console.log("Point your frontend to this port to stress test decoding.");

wss.on('connection', function connection(ws) {
  console.log('Frontend connected! Starting blast in 3 seconds...');

  ws.send("INFO:NAME=StressTestAgent"); // Handshake

  setTimeout(() => {
      let sent = 0;
      const start = Date.now();

      const interval = setInterval(() => {
          if (sent >= MESSAGES_PER_CLIENT) {
              clearInterval(interval);
              const end = Date.now();
              console.log(`Finished sending ${sent} frames in ${(end-start)/1000}s`);
              return;
          }

          // Construct Frame: [Len 4][ClientID 4][BackID 4][Payload...]
          // Simple Binary Frame (0x01 = Screen)
          const buffer = Buffer.alloc(12 + MSG_SIZE);
          buffer.writeUInt32BE(MSG_SIZE, 0); // Len (BE or LE? Protocol says LE usually but checked C code uses htonl (BE))
          // Wait, Gateway uses LE or BE?
          // C Code: memcpy(&net_len..); ntohl(net_len). so Network Byte Order (BE).
          // Frontend TS: view.getUint32(0, false) (false = Big Endian).
          // So BE is correct.

          buffer.writeUInt32BE(100, 4); // ClientID
          buffer.writeUInt32BE(1, 8); // BackID

          // Payload: 0x01 (Screen) + Junk
          buffer[12] = 0x01;

          ws.send(buffer);
          sent++;
      }, 1); // 1ms interval = ~1000 fps target
  }, 3000);
});
