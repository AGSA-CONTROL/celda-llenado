/* Servidor puente LLENADO v17: sirve los .html y reenvía mensajes entre
     - la tienda (tablet u otra laptop)  <->  el HMI (laptop de la rueda)
     - el carro AGV (su propia página)    <->  el HMI
   Uso:  npm install   (una sola vez)   y luego   npm start   (o: node servidor_llenado_v17.js)   */
const http = require("http"), fs = require("fs"), path = require("path"), os = require("os");
const { WebSocketServer, WebSocket } = require("ws");
const dgram = require("dgram");
const PORT = process.env.PORT || 3000, DIR = __dirname;
const NUBE = process.env.RENDER_EXTERNAL_URL || "";
/* PUENTE del carro físico: el ESP32 se conecta a ESTA laptop como siempre (misma red WiFi, se encuentra solo)
   y la laptop pasa sus mensajes al servidor de la nube, donde están el HMI, la tienda y la página del carro.
   Se activa con:  set PUENTE_NUBE=wss://celda-llenado.onrender.com   (lo hace INICIAR_PUENTE_CARRO.bat) */
const PUENTE_NUBE = NUBE ? "" : (process.env.PUENTE_NUBE || "");     // en Render (la nube) trae el link fijo, ej. https://celda-llenado.onrender.com

/* ---------- archivos .html ---------- */
const server = http.createServer((req, res) => {
  let nombre = "";
  try { nombre = path.basename(decodeURIComponent((req.url || "/").split("?")[0])); } catch (_) {}
  if (nombre === "") {
    const lista = fs.readdirSync(DIR).filter(f => f.endsWith(".html")).map(f => `<li><a href="/${f}">${f}</a></li>`).join("");
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    return res.end(`<meta charset="utf-8"><title>LLENADO</title><h1>LLENADO</h1><ul>${lista}</ul>`);
  }
  if (nombre === "sala_eliminada.html") return paginaSalaEliminada(req, res);
  if (!nombre.endsWith(".html")) { res.writeHead(404); return res.end("No encontrado"); }
  fs.readFile(path.join(DIR, nombre), (e, d) => {
    if (e) { res.writeHead(404); return res.end("No encontrado"); }
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
    res.end(d);
  });
});

/* Página que se ve cuando eliminan la sala en la que estabas: eliges otra y regresas */
function paginaSalaEliminada(req, res) {
  const q = new URL(req.url, "http://x").searchParams;
  const sl = nombreSala(q.get("sala"));
  let volver = path.basename(q.get("volver") || "");
  if (!/^[\w-]+\.html$/.test(volver) || !fs.existsSync(path.join(DIR, volver))) volver = "";
  res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
  res.end(`<!doctype html><html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Sala eliminada</title>
<style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0c0f14;color:#eef2f7;font-family:"Segoe UI",system-ui,sans-serif}
.c{background:#161b23;border:1px solid #2c3544;border-radius:14px;padding:28px;max-width:420px;width:calc(100% - 32px)}h1{font-size:22px;margin:0 0 8px}p{color:#98a3b3;margin:0 0 18px}
.r{display:flex;gap:8px}input{flex:1;font:inherit;padding:10px;border-radius:10px;border:1px solid #3b4659;background:#0b0e12;color:#eef2f7}
button{font:inherit;font-weight:700;padding:10px 16px;border-radius:10px;border:0;background:#33c469;color:#06210f;cursor:pointer}</style></head><body>
<div class="c"><h1>🗑 La sala "${sl}" se eliminó</h1><p>Todo lo que estaba en esa sala se desconectó. Escribe la sala a la que quieres entrar.</p>
<div class="r"><input id="n" maxlength="20" placeholder="nombre de la sala" value="principal"><button id="b">Entrar</button></div></div>
<script>
const limpia = x => String(x||"").toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g,"").replace(/[^a-z0-9_-]/g,"").slice(0,20);
try { if (localStorage.getItem("celda_sala") === ${JSON.stringify(sl)}) localStorage.removeItem("celda_sala"); } catch(_){}
function ir(){ const n = limpia(document.getElementById("n").value) || "principal"; try { localStorage.setItem("celda_sala", n); } catch(_){}
  location.href = ${JSON.stringify(volver ? "/" + volver : "/")} + "?sala=" + encodeURIComponent(n); }
document.getElementById("b").onclick = ir; document.getElementById("n").onkeydown = e => { if (e.key === "Enter") ir(); };
</script></body></html>`);
}

/* ---------- mensajes ---------- */
/* SALAS: cada equipo trabaja en su propia sala (su propia celda): su HMI, su tienda y su carro.
   La sala va en el link:  ...AGV_Carro_v17.html?sala=marbella   (sin ?sala= es la sala "principal").
   El carro ESP32 real pide PERMISO para entrar a una sala: lo aprueban en la página del carro de esa sala. */
const wss = new WebSocketServer({ server, maxPayload: 16 * 1024 });
const salas = new Map();
const nombreSala = x => (String(x || "").toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "").replace(/[^a-z0-9_-]/g, "").slice(0, 20)) || "principal";
function sala(nombre) {
  const n = nombreSala(nombre);
  if (!salas.has(n)) salas.set(n, {
    nombre: n, hmi: null, agv: null,    // agv = el carro ACTIVO (al que el HMI le habla)
    agvEsp: null,                       // el ESP32 real (ya aprobado en esta sala)
    agvPaginas: [],                     // páginas del carro en modo Simulador (la última es la más nueva)
    ultimaDisp: null,                   // ¿se puede pedir? — lo decide el HMI
    tiendas: new Set(), vistas: new Set(), pedidos: new Map(),
    pendientes: new Map(),              // id -> ws de carros ESP32 que piden permiso
    aprobados: new Set(),               // ids de carros con permiso (si se reconectan entran solos)
    rechazados: new Map(),              // id -> hora del rechazo (no vuelve a preguntar en 2 min)
    vigias: new Set()                   // el letrero "Sala:" de cada página abierta (para avisar si se elimina)
  });
  return salas.get(n);
}
const enviar = (ws, o) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); };
const avisarVistas = (S, o) => S.vistas.forEach(v => enviar(v, o));
const avisarTiendas = S => S.tiendas.forEach(t => enviar(t, { tipo: "hmi", en_linea: !!S.hmi }));
const avisarHmi = S => enviar(S.hmi, { tipo: "tiendas", n: S.tiendas.size });
const CTL_VISTA = ["paro", "salir", "modoDest", "vel", "modoSim"];   // lo que una vista puede pedirle al carro
const AGV_A_HMI = ["agv_estado", "agv_evento"];     // la página del AGV solo puede mandar esto al HMI
const idValido = id => typeof id === "string" && /^[\w-]{1,40}$/.test(id);
const LOCALES = ["127.0.0.1", "::1", "::ffff:127.0.0.1"];
const PIDE_PERMISO = !!NUBE;     // en la nube el carro real pide permiso; en la laptop (misma red) entra solo
const textoCorto = (x, n) => String(x || "").replace(/[^\w .:#()-]/g, "").slice(0, n);

/* Lista de carros reales de la sala -> a TODAS las páginas del carro de esa sala (Simulador y Carro ESP32) */
function avisarCarros(S) {
  const o = {
    tipo: "carros_sala", sala: S.nombre, pide_permiso: PIDE_PERMISO,
    pendientes: [...S.pendientes.values()].map(w => ({ id: w.carroId, nombre: w.carroNombre })),
    conectado: S.agvEsp ? { id: S.agvEsp.carroId, nombre: S.agvEsp.carroNombre } : null
  };
  S.agvPaginas.forEach(p => enviar(p, o)); S.vistas.forEach(v => enviar(v, o));
}
function actualizarAgv(S) {
  const nuevo = S.agvPaginas.length ? S.agvPaginas[S.agvPaginas.length - 1] : S.agvEsp;
  if (nuevo === S.agv) return;
  S.agv = nuevo;
  if (S.agv) enviar(S.agv, { tipo: "hola_ok", hmi: !!S.hmi, activo: true, sala: S.nombre });
  [S.agvEsp, ...S.agvPaginas].forEach(c => { if (c && c !== S.agv) enviar(c, { tipo: "agv_espera", msg: S.agv === S.agvEsp ? "" : "Ahora manda la página del carro en modo Simulador." }); });
  enviar(S.hmi, { tipo: "agv", en_linea: !!S.agv, sim: !!S.agv && S.agv !== S.agvEsp });
  console.log("[" + S.nombre + "] " + (S.agv ? "Carro activo: " + (S.agv === S.agvEsp ? "ESP32 real" : "página en modo Simulador") : "Sin carro AGV"));
}
/* El carro ESP32 ya tiene permiso: entra a la sala */
function entrarCarro(S, ws) {
  S.pendientes.delete(ws.carroId);
  if (S.agvEsp && S.agvEsp !== ws) { enviar(S.agvEsp, { tipo: "agv_permiso", estado: "fuera", sala: S.nombre, msg: "Entró otro carro a la sala." }); try { S.agvEsp.close(); } catch (_) {} }
  S.agvEsp = ws; ws.aprobado = true;
  enviar(ws, { tipo: "agv_permiso", estado: "aprobado", sala: S.nombre });
  avisarVistas(S, { tipo: "agv", en_linea: true });
  if (PUENTE_NUBE) abrirPuente();
  console.log("[" + S.nombre + "] Carro ESP32 (" + ws.carroNombre + ") dentro de la sala");
  actualizarAgv(S);
  if (S.agv !== ws) enviar(ws, { tipo: "hola_ok", hmi: !!S.hmi, activo: false, sala: S.nombre });
  avisarCarros(S);
}
function sacarCarro(S, ws) {          // ws deja de ser el carro de la sala (se desconectó o se lo quitaron)
  if (S.agvEsp !== ws) return;
  S.agvEsp = null; avisarVistas(S, { tipo: "agv", en_linea: false });
  if (PUENTE_NUBE) cerrarPuente();
  actualizarAgv(S);
}

/* ---------- lista de salas y ELIMINAR sala ---------- */
function listaSalas() {
  return [...salas.values()].map(S => ({
    nombre: S.nombre, paginas: S.vigias.size, hmi: !!S.hmi, tiendas: S.tiendas.size,
    carro_fisico: !!S.agvEsp, pide_permiso: S.pendientes.size
  })).sort((a, b) => a.nombre.localeCompare(b.nombre));
}
const todosVigias = () => { const r = []; salas.forEach(S => S.vigias.forEach(v => r.push(v))); return r; };
const avisarListaSalas = () => { const o = { tipo: "salas", lista: listaSalas() }; todosVigias().forEach(v => enviar(v, o)); };
function salaVacia(S) {
  return !S.hmi && !S.tiendas.size && !S.vistas.size && !S.agvPaginas.length && !S.agvEsp && !S.pendientes.size && !S.vigias.size;
}
function limpiarSala(S) { if (salas.get(S.nombre) === S && salaVacia(S)) salas.delete(S.nombre); }
function eliminarSala(n) {
  const S = salas.get(n); if (!S) return false;
  salas.delete(n);                                     // primero se borra: nadie más entra a esta sala vieja
  S.pedidos.forEach((r, id) => {
    if (r.ultimo && (r.ultimo.fase === "listo" || r.ultimo.fase === "error")) return;
    enviar(r.ws, { tipo: "estado", id, fase: "error", msg: "Se eliminó la sala \"" + n + "\". Tu pedido se canceló." });
  });
  S.vigias.forEach(v => enviar(v, { tipo: "sala_eliminada", sala: n }));
  const carros = [S.agvEsp, ...S.pendientes.values()].filter(Boolean);
  carros.forEach(c => enviar(c, { tipo: "agv_permiso", estado: "fuera", sala: n, msg: "La sala se eliminó." }));
  setTimeout(() => {
    [S.hmi, ...S.tiendas, ...S.vistas, ...S.agvPaginas, ...carros, ...S.vigias].forEach(w => { if (w) { w.sala = null; try { w.close(); } catch (_) {} } });
  }, 300);
  console.log("[" + n + "] SALA ELIMINADA");
  return true;
}

function cerrarPedido(S, id) { setTimeout(() => S.pedidos.delete(id), 10 * 60 * 1000); }

wss.on("connection", (ws, req) => {
  const local = LOCALES.includes(req.socket.remoteAddress);
  ws.rol = null;
  ws.on("message", raw => {
    let m; try { m = JSON.parse(raw); } catch (_) { return; }
    if (!m || typeof m !== "object") return;

    /* El letrero "Sala:" de cada página: vigila su sala, pide la lista y puede eliminar salas */
    if (m.tipo === "vigilar" && !ws.rol) {
      const S = ws.sala = sala(m.sala); ws.rol = "vigia"; S.vigias.add(ws);
      enviar(ws, { tipo: "salas", lista: listaSalas() }); avisarListaSalas();
      return;
    }
    if (ws.rol === "vigia") {
      if (m.tipo === "salas_lista") enviar(ws, { tipo: "salas", lista: listaSalas() });
      else if (m.tipo === "eliminar_sala") {
        const n = nombreSala(m.sala);
        if (!eliminarSala(n)) enviar(ws, { tipo: "error", msg: "Esa sala ya no existe." });
        setTimeout(avisarListaSalas, 400);
      }
      return;
    }

    if (m.tipo === "hola") {
      if (ws.rol) return;                 // cada conexión dice "hola" una sola vez
      const S = ws.sala = sala(m.sala);
      if (m.rol === "hmi") {
        if (!local && !NUBE) { enviar(ws, { tipo: "error", msg: "El HMI solo puede conectarse desde la laptop del dispensador." }); return ws.close(); }
        if (S.hmi && S.hmi !== ws && S.hmi.readyState === 1) {
          if (!NUBE) { enviar(ws, { tipo: "error", msg: "Ya hay otro HMI conectado." }); return ws.close(); }
          enviar(S.hmi, { tipo: "error", msg: "Se abrió el HMI de la sala \"" + S.nombre + "\" en otra ventana: esta se desconecta." }); try { S.hmi.close(); } catch (_) {}
        }
        S.hmi = ws; ws.rol = "hmi";
        enviar(ws, { tipo: "hola_ok", tiendas: S.tiendas.size, agv: !!S.agv, sim: !!S.agv && S.agv !== S.agvEsp, sala: S.nombre }); avisarTiendas(S);
        [S.agvEsp, ...S.agvPaginas].forEach(c => enviar(c, { tipo: "hmi", en_linea: true })); avisarVistas(S, { tipo: "hmi", en_linea: true });
        console.log("[" + S.nombre + "] HMI conectado");
      } else if (m.rol === "agv") {
        ws.rol = "agv";
        if (m.pagina) {
          S.agvPaginas.push(ws); console.log("[" + S.nombre + "] Página del carro conectada (modo Simulador)");
          actualizarAgv(S);
          if (S.agv !== ws) enviar(ws, { tipo: "hola_ok", hmi: !!S.hmi, activo: false, sala: S.nombre });
          avisarCarros(S);
        } else {                          // el carro ESP32 real
          ws.carroId = idValido(m.id) ? m.id : "esp-" + String(req.socket.remoteAddress).replace(/[^\w]/g, "").slice(-12);
          ws.carroNombre = textoCorto(m.nombre, 30) || "Carro ESP32";
          console.log("[" + S.nombre + "] Carro ESP32 " + ws.carroId + " pide entrar desde " + String(req.socket.remoteAddress).replace("::ffff:", ""));
          if (!PIDE_PERMISO || S.aprobados.has(ws.carroId)) return entrarCarro(S, ws);
          const r = S.rechazados.get(ws.carroId);
          if (r && Date.now() - r < 2 * 60 * 1000) { enviar(ws, { tipo: "agv_permiso", estado: "rechazado", sala: S.nombre }); return; }
          const viejo = S.pendientes.get(ws.carroId); if (viejo && viejo !== ws) { try { viejo.close(); } catch (_) {} }
          S.pendientes.set(ws.carroId, ws);
          enviar(ws, { tipo: "agv_permiso", estado: "esperando", sala: S.nombre });
          avisarCarros(S);
        }
      } else if (m.rol === "agv_vista") {
        ws.rol = "agv_vista"; S.vistas.add(ws);
        enviar(ws, { tipo: "hola_ok", hmi: !!S.hmi, agv: !!S.agvEsp, sala: S.nombre });
        avisarCarros(S);
        console.log("[" + S.nombre + "] Página del carro conectada (modo Carro ESP32)");
      } else {
        ws.rol = "tienda"; S.tiendas.add(ws);
        enviar(ws, { tipo: "hola_ok", hmi: !!S.hmi, sala: S.nombre }); avisarHmi(S);
        if (S.ultimaDisp) enviar(ws, S.ultimaDisp);
      }
      return;
    }

    const S = ws.sala; if (!S) return;
    /* Permiso para el carro real: lo da cualquier página del carro de la sala */
    if ((ws.rol === "agv_vista" || (ws.rol === "agv" && S.agvPaginas.includes(ws))) && m.tipo === "permiso_carro") {
      const id = String(m.id || "");
      if (m.quitar) {                                    // sacar de la sala al carro que ya estaba
        const c = S.agvEsp; if (!c || c.carroId !== id) return;
        S.aprobados.delete(id); S.rechazados.set(id, Date.now());
        enviar(c, { tipo: "agv_permiso", estado: "fuera", sala: S.nombre, msg: "Te sacaron de la sala." });
        c.aprobado = false; sacarCarro(S, c); avisarCarros(S);
        console.log("[" + S.nombre + "] Carro " + id + " sacado de la sala");
        return;
      }
      const c = S.pendientes.get(id); if (!c) return avisarCarros(S);
      if (m.ok) { S.aprobados.add(id); S.rechazados.delete(id); entrarCarro(S, c); }
      else {
        S.pendientes.delete(id); S.rechazados.set(id, Date.now());
        enviar(c, { tipo: "agv_permiso", estado: "rechazado", sala: S.nombre });
        avisarCarros(S); console.log("[" + S.nombre + "] Carro " + id + " rechazado");
      }
      return;
    }

    if (ws.rol === "tienda") {
      if (m.tipo === "pedido") {
        const p = m.p;
        const ok = idValido(m.id) && Array.isArray(p) && p.length === 3 && p.every(x => Number.isInteger(x) && x >= 0) && p[0] + p[1] + p[2] === 3;
        if (!ok) return enviar(ws, { tipo: "estado", id: String(m.id).slice(0, 40), fase: "error", msg: "Pedido no válido." });
        if (!S.hmi) return enviar(ws, { tipo: "estado", id: m.id, fase: "error", msg: "El dispensador no está disponible en este momento." });
        if (S.pedidos.has(m.id)) return;
        S.pedidos.set(m.id, { ws, ultimo: null });
        enviar(S.hmi, { tipo: "pedido", id: m.id, p });
        console.log("[" + S.nombre + "] Pedido", m.id, p.join(","));
      } else if (m.tipo === "consulta" && idValido(m.id)) {   // la tienda se reconectó: recupera el estado
        const r = S.pedidos.get(m.id);
        if (!r) return enviar(ws, { tipo: "estado", id: m.id, fase: "error", msg: "No encuentro tu pedido." });
        r.ws = ws; if (r.ultimo) enviar(ws, r.ultimo);
      }
    } else if (ws.rol === "agv") {
      if (!AGV_A_HMI.includes(m.tipo)) return;
      if (ws !== S.agvEsp && !S.agvPaginas.includes(ws)) return;   // carro real sin permiso: no habla
      if (ws === S.agv) enviar(S.hmi, m);                    // al HMI solo le habla el carro activo
      if (ws === S.agvEsp && S.vistas.size) avisarVistas(S, m); // la página en modo "Carro real" siempre ve al ESP32
      if (ws === S.agvEsp && puente && puente.readyState === 1) puente.send(JSON.stringify(m));   // y se lo pasa a la nube
    } else if (ws.rol === "agv_vista") {
      if (m.tipo === "agv_ctl" && CTL_VISTA.includes(m.cmd)) {
        if (!S.agvEsp) return enviar(ws, { tipo: "error", msg: "No hay carro ESP32 en la sala \"" + S.nombre + "\"." });
        enviar(S.agvEsp, { tipo: "agv_ctl", cmd: m.cmd, on: !!m.on, v: Number(m.v) || 0 });
      }
    } else if (ws.rol === "hmi" && ws === S.hmi && m.tipo === "disponibilidad") {     // a TODAS las tiendas de la sala
      S.ultimaDisp = { tipo: "disponibilidad", pedir: !!m.pedir, agv: String(m.agv || "").slice(0, 20), msg: String(m.msg || "").slice(0, 160) };
      S.tiendas.forEach(t => enviar(t, S.ultimaDisp));
    } else if (ws.rol === "hmi" && ws === S.hmi && m.tipo === "agv_cmd") {
      enviar(S.agv, m);
    } else if (ws.rol === "hmi" && ws === S.hmi && m.tipo === "estado") {
      const r = S.pedidos.get(m.id);
      if (!r) return;
      if (r.ultimo && (r.ultimo.fase === "listo" || r.ultimo.fase === "error")) return;   // pedido ya cerrado
      r.ultimo = m; enviar(r.ws, m);
      if (m.fase === "listo" || m.fase === "error") cerrarPedido(S, m.id);
    }
  });

  ws.on("close", () => {
    const S = ws.sala; if (!S) return;
    if (ws.rol === "vigia") { S.vigias.delete(ws); limpiarSala(S); avisarListaSalas(); return; }
    setTimeout(() => { limpiarSala(S); avisarListaSalas(); }, 50);
    if (ws === S.hmi) {                                  // el HMI se cayó: la cola en memoria se perdió
      S.hmi = null; S.ultimaDisp = null;
      S.pedidos.forEach((r, id) => {
        if (r.ultimo && (r.ultimo.fase === "listo" || r.ultimo.fase === "error")) return;
        r.ultimo = { tipo: "estado", id, fase: "error", msg: "Se perdió la conexión con el dispensador. Tu pedido se canceló." };
        enviar(r.ws, r.ultimo); cerrarPedido(S, id);
      });
      avisarTiendas(S); [S.agvEsp, ...S.agvPaginas].forEach(c => enviar(c, { tipo: "hmi", en_linea: false })); avisarVistas(S, { tipo: "hmi", en_linea: false }); console.log("[" + S.nombre + "] HMI desconectado");
    } else if (ws.rol === "agv") {
      const k = S.agvPaginas.indexOf(ws); if (k >= 0) S.agvPaginas.splice(k, 1);
      if (S.pendientes.get(ws.carroId) === ws) S.pendientes.delete(ws.carroId);
      if (ws === S.agvEsp) { console.log("[" + S.nombre + "] Carro ESP32 desconectado"); sacarCarro(S, ws); }
      actualizarAgv(S); avisarCarros(S);
    } else if (S.vistas.delete(ws)) {
    } else if (S.tiendas.delete(ws)) avisarHmi(S);
  });
});

/* ---------- PUENTE: carro ESP32 (aquí, por la red local)  <->  servidor de la nube ---------- */
let puente = null, puenteReintento = null;
/* En la laptop hay un solo carro ESP32 (el de la red local), en la sala que tenga escrita */
const espLocal = () => { for (const S of salas.values()) if (S.agvEsp) return S.agvEsp; return null; };
function abrirPuente() {
  clearTimeout(puenteReintento);
  const esp = espLocal();
  if (!esp || (puente && puente.readyState <= 1)) return;
  console.log("Puente: conectando el carro con la nube (" + PUENTE_NUBE + ")…");
  let p;
  try { p = new WebSocket(PUENTE_NUBE); } catch (e) { console.log("Puente: dirección no válida: " + e.message); return; }
  puente = p;
  p.on("open", () => { p.send(JSON.stringify({ tipo: "hola", rol: "agv", sala: esp.sala.nombre, id: esp.carroId, nombre: esp.carroNombre })); console.log("Puente: el carro ya está en línea en la nube."); });
  p.on("message", raw => {                            // órdenes del HMI / de la página del carro -> al ESP32
    let m; try { m = JSON.parse(raw); } catch (_) { return; }
    if (m && typeof m === "object") enviar(espLocal(), m);
  });
  p.on("close", () => {
    if (puente === p) puente = null;
    if (espLocal()) { console.log("Puente: se cortó la conexión con la nube, reintentando…"); puenteReintento = setTimeout(abrirPuente, 3000); }
  });
  p.on("error", e => console.log("Puente: " + e.message + " (¿la laptop tiene internet?)"));
}
function cerrarPuente() {
  clearTimeout(puenteReintento);
  if (puente) { const p = puente; puente = null; try { p.close(); } catch (_) {} }
}
setInterval(() => { if (puente && puente.readyState === 1) { try { puente.ping(); } catch (_) {} } }, 25000);

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
    console.log(`\nServidor listo en la NUBE (v17). Link fijo (agrega ?sala=tunombre al final para tu propia sala):\n  HMI:       ${NUBE}/HMI_Llenado_v17.html\n  Tienda:    ${NUBE}/Tienda_LaCosecha_v17.html\n  Carro AGV: ${NUBE}/AGV_Carro_v17.html\n  En el panel del ESP32, en "Servidor", escribe: ${NUBE.replace(/^https?:\/\//, "")}  puerto 443`);
    return;
  }
  if (PUENTE_NUBE) console.log(`\nMODO PUENTE: el carro ESP32 se conecta a esta laptop y se pasa a ${PUENTE_NUBE}\n  Abre el HMI, la tienda y el carro desde el link de la nube (no desde localhost).`);
  console.log(`\nServidor listo (v17).\n  HMI (en esta laptop):  http://localhost:${PORT}/HMI_Llenado_v17.html`);
  Object.values(os.networkInterfaces()).flat().filter(i => i && i.family === "IPv4" && !i.internal).forEach(i => {
    console.log(`  Tienda (otra laptop o tablet):  http://${i.address}:${PORT}/Tienda_LaCosecha_v17.html`);
    console.log(`  Carro AGV (ver y manejar el carro):  http://${i.address}:${PORT}/AGV_Carro_v17.html`);
    console.log(`      (El carro ESP32 encuentra esta laptop solo. Si no, escribe en su panel: ${i.address}  puerto ${PORT})`);
  });
});
