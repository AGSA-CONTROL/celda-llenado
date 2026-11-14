/* Servidor puente LLENADO v17: sirve los .html y reenvía mensajes entre
     - la tienda (tablet u otra laptop)  <->  el HMI (laptop de la rueda)
     - el carro AGV (su propia página)    <->  el HMI
   Uso:  npm install   (una sola vez)   y luego   npm start   (o: node servidor_llenado_v17.js)   */
const http = require("http"), fs = require("fs"), path = require("path"), os = require("os");
const { WebSocketServer } = require("ws");
const dgram = require("dgram");
const PORT = process.env.PORT || 3000, DIR = __dirname;
const NUBE = process.env.RENDER_EXTERNAL_URL || "";     // en Render (la nube) trae el link fijo, ej. https://celda-llenado.onrender.com

/* ---------- archivos .html ---------- */
const server = http.createServer((req, res) => {
  let nombre = "";
  try { nombre = path.basename(decodeURIComponent((req.url || "/").split("?")[0])); } catch (_) {}
  if (nombre === "") {
    const lista = fs.readdirSync(DIR).filter(f => f.endsWith(".html")).map(f => `<li><a href="/${f}">${f}</a></li>`).join("");
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    return res.end(`<meta charset="utf-8"><title>LLENADO</title><h1>LLENADO</h1><ul>${lista}</ul>`);
  }
  if (!nombre.endsWith(".html")) { res.writeHead(404); return res.end("No encontrado"); }
  fs.readFile(path.join(DIR, nombre), (e, d) => {
    if (e) { res.writeHead(404); return res.end("No encontrado"); }
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
    res.end(d);
  });
});

/* ---------- mensajes ---------- */
const wss = new WebSocketServer({ server, maxPayload: 16 * 1024 });
let hmi = null, agv = null;
let ultimaDisp = null;          // ¿se puede pedir? (el carro está en la posición de carga) — lo decide el HMI
const tiendas = new Set();
const vistas = new Set();       // páginas que solo MIRAN y manejan el carro real (AGV_Carro en modo "Carro ESP32")
const CTL_VISTA = ["paro", "salir", "modoDest", "vel", "modoSim"];   // lo que una vista puede pedirle al carro
const avisarVistas = o => vistas.forEach(v => enviar(v, o));
const pedidos = new Map();      // id -> { ws, ultimo }  (ws de la tienda dueña y último estado enviado)
const enviar = (ws, o) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); };
const avisarTiendas = () => tiendas.forEach(t => enviar(t, { tipo: "hmi", en_linea: !!hmi }));
const avisarHmi = () => enviar(hmi, { tipo: "tiendas", n: tiendas.size });
const AGV_A_HMI = ["agv_estado", "agv_evento"];     // la página del AGV solo puede mandar esto al HMI
const idValido = id => typeof id === "string" && /^[\w-]{1,40}$/.test(id);
const LOCALES = ["127.0.0.1", "::1", "::ffff:127.0.0.1"];

function cerrarPedido(id) { setTimeout(() => pedidos.delete(id), 10 * 60 * 1000); }

wss.on("connection", (ws, req) => {
  const local = LOCALES.includes(req.socket.remoteAddress);
  ws.rol = null;
  ws.on("message", raw => {
    let m; try { m = JSON.parse(raw); } catch (_) { return; }
    if (!m || typeof m !== "object") return;

    if (m.tipo === "hola") {
      if (m.rol === "hmi") {
        if (!local && !NUBE) { enviar(ws, { tipo: "error", msg: "El HMI solo puede conectarse desde la laptop del dispensador." }); return ws.close(); }
        if (hmi && hmi !== ws && hmi.readyState === 1) {
          if (!NUBE) { enviar(ws, { tipo: "error", msg: "Ya hay otro HMI conectado." }); return ws.close(); }
          enviar(hmi, { tipo: "error", msg: "Se abrió el HMI en otra ventana: esta se desconecta." }); try { hmi.close(); } catch (_) {}
        }
        hmi = ws; ws.rol = "hmi";
        enviar(ws, { tipo: "hola_ok", tiendas: tiendas.size, agv: !!agv }); avisarTiendas();
        enviar(agv, { tipo: "hmi", en_linea: true }); avisarVistas({ tipo: "hmi", en_linea: true });
        console.log("HMI conectado");
      } else if (m.rol === "agv") {
        if (agv && agv !== ws && agv.readyState === 1) { enviar(ws, { tipo: "error", msg: "Ya hay otro carro AGV conectado (¿el ESP32 del carro o la página del carro?). Solo puede haber uno." }); return ws.close(); }
        agv = ws; ws.rol = "agv";
        enviar(ws, { tipo: "hola_ok", hmi: !!hmi });
        enviar(hmi, { tipo: "agv", en_linea: true }); avisarVistas({ tipo: "agv", en_linea: true });
        console.log("Carro AGV conectado desde " + String(req.socket.remoteAddress).replace("::ffff:", ""));
      } else if (m.rol === "agv_vista") {
        ws.rol = "agv_vista"; vistas.add(ws);
        enviar(ws, { tipo: "hola_ok", hmi: !!hmi, agv: !!agv });
        console.log("Página del carro conectada (modo Carro ESP32)");
      } else {
        ws.rol = "tienda"; tiendas.add(ws);
        enviar(ws, { tipo: "hola_ok", hmi: !!hmi }); avisarHmi();
        if (ultimaDisp) enviar(ws, ultimaDisp);
      }
      return;
    }

    if (ws.rol === "tienda") {
      if (m.tipo === "pedido") {
        const p = m.p;
        const ok = idValido(m.id) && Array.isArray(p) && p.length === 3 && p.every(x => Number.isInteger(x) && x >= 0) && p[0] + p[1] + p[2] === 3;
        if (!ok) return enviar(ws, { tipo: "estado", id: String(m.id).slice(0, 40), fase: "error", msg: "Pedido no válido." });
        if (!hmi) return enviar(ws, { tipo: "estado", id: m.id, fase: "error", msg: "El dispensador no está disponible en este momento." });
        if (pedidos.has(m.id)) return;
        pedidos.set(m.id, { ws, ultimo: null });
        enviar(hmi, { tipo: "pedido", id: m.id, p });
        console.log("Pedido", m.id, p.join(","));
      } else if (m.tipo === "consulta" && idValido(m.id)) {   // la tienda se reconectó: recupera el estado
        const r = pedidos.get(m.id);
        if (!r) return enviar(ws, { tipo: "estado", id: m.id, fase: "error", msg: "No encuentro tu pedido." });
        r.ws = ws; if (r.ultimo) enviar(ws, r.ultimo);
      }
    } else if (ws.rol === "agv") {
      if (AGV_A_HMI.includes(m.tipo)) { enviar(hmi, m); if (vistas.size) avisarVistas(m); }
    } else if (ws.rol === "agv_vista") {
      if (m.tipo === "agv_ctl" && CTL_VISTA.includes(m.cmd)) {
        if (!agv) return enviar(ws, { tipo: "error", msg: "El carro ESP32 no está conectado al servidor." });
        enviar(agv, { tipo: "agv_ctl", cmd: m.cmd, on: !!m.on, v: Number(m.v) || 0 });
      }
    } else if (ws.rol === "hmi" && m.tipo === "disponibilidad") {     // a TODAS las tiendas: pueden pedir o no
      ultimaDisp = { tipo: "disponibilidad", pedir: !!m.pedir, agv: String(m.agv || "").slice(0, 20), msg: String(m.msg || "").slice(0, 160) };
      tiendas.forEach(t => enviar(t, ultimaDisp));
    } else if (ws.rol === "hmi" && m.tipo === "agv_cmd") {
      enviar(agv, m);
    } else if (ws.rol === "hmi" && m.tipo === "estado") {
      const r = pedidos.get(m.id);
      if (!r) return;
      if (r.ultimo && (r.ultimo.fase === "listo" || r.ultimo.fase === "error")) return;   // pedido ya cerrado
      r.ultimo = m; enviar(r.ws, m);
      if (m.fase === "listo" || m.fase === "error") cerrarPedido(m.id);
    }
  });

  ws.on("close", () => {
    if (ws === hmi) {                                  // el HMI se cayó: la cola en memoria se perdió
      hmi = null; ultimaDisp = null;
      pedidos.forEach((r, id) => {
        if (r.ultimo && (r.ultimo.fase === "listo" || r.ultimo.fase === "error")) return;
        r.ultimo = { tipo: "estado", id, fase: "error", msg: "Se perdió la conexión con el dispensador. Tu pedido se canceló." };
        enviar(r.ws, r.ultimo); cerrarPedido(id);
      });
      avisarTiendas(); enviar(agv, { tipo: "hmi", en_linea: false }); avisarVistas({ tipo: "hmi", en_linea: false }); console.log("HMI desconectado");
    } else if (ws === agv) {
      agv = null; enviar(hmi, { tipo: "agv", en_linea: false }); avisarVistas({ tipo: "agv", en_linea: false }); console.log("Carro AGV desconectado");
    } else if (vistas.delete(ws)) {
    } else if (tiendas.delete(ws)) avisarHmi();
  });
});

/* ---------- el carro ESP32 encuentra este servidor SOLO ----------
   Cada 2 s se avisa por la red WiFi (UDP 3333): "CELDA_LLENADO,<puerto>". El ESP32 escucha ese aviso y
   se conecta a la IP de esta laptop sin que tengas que escribirla. */
const PUERTO_BUSQUEDA = 3333;
const udp = dgram.createSocket("udp4");
udp.on("error", e => console.log("Aviso por la red no disponible (" + e.message + "). Escribe la IP en el panel del ESP32."));
function difusiones() {
  const r = new Set(["255.255.255.255"]);
  Object.values(os.networkInterfaces()).flat().forEach(i => {
    if (!i || i.family !== "IPv4" || i.internal || !i.netmask) return;
    const ip = i.address.split(".").map(Number), m = i.netmask.split(".").map(Number);
    r.add(ip.map((b, k) => (b | (~m[k] & 255))).join("."));
  });
  return [...r];
}
if (!NUBE) udp.bind(() => {
  try { udp.setBroadcast(true); } catch (_) {}
  setInterval(() => {
    const msg = Buffer.from("CELDA_LLENADO," + PORT);
    difusiones().forEach(d => udp.send(msg, PUERTO_BUSQUEDA, d, () => {}));
  }, 2000);
});

/* ---------- latido: cada 25 s se "toca" cada conexión para que la nube no la corte ---------- */
setInterval(() => wss.clients.forEach(c => { try { c.ping(); } catch (_) {} }), 25000);

server.listen(PORT, () => {
  if (NUBE) {
    console.log(`\nServidor listo en la NUBE (v17). Link fijo:\n  HMI:       ${NUBE}/HMI_Llenado_v17.html\n  Tienda:    ${NUBE}/Tienda_LaCosecha_v17.html\n  Carro AGV: ${NUBE}/AGV_Carro_v17.html\n  En el panel del ESP32, en "Servidor", escribe: ${NUBE.replace(/^https?:\/\//, "")}  puerto 443`);
    return;
  }
  console.log(`\nServidor listo (v17).\n  HMI (en esta laptop):  http://localhost:${PORT}/HMI_Llenado_v17.html`);
  Object.values(os.networkInterfaces()).flat().filter(i => i && i.family === "IPv4" && !i.internal).forEach(i => {
    console.log(`  Tienda (otra laptop o tablet):  http://${i.address}:${PORT}/Tienda_LaCosecha_v17.html`);
    console.log(`  Carro AGV (ver y manejar el carro):  http://${i.address}:${PORT}/AGV_Carro_v17.html`);
    console.log(`      (El carro ESP32 encuentra esta laptop solo. Si no, escribe en su panel: ${i.address}  puerto ${PORT})`);
  });
});
