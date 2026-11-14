// Panel del carro AGV-01 (celda de llenado v17): carro + mantenimiento + modo Físico/Simulador.
// Se sirve desde memoria (PROGMEM) en la ruta / y en cualquier otra URL (portal cautivo).
#pragma once
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0c0f14">
<title>AGV-01 · Carro de entrega</title>
<style>

:root{
  --bg:#0c0f14; --panel:#161b23; --panel2:#202733; --inset:#0b0e12; --border:#2c3544; --border2:#3b4659;
  --text:#eef2f7; --muted:#98a3b3; --dim:#5f6b7c;
  --yellow:#f5a623; --green:#33c469; --red:#e5484d; --blue:#3aafd9;
  --radius:12px;
  --mono:Consolas,"SF Mono",Menlo,"Liberation Mono",monospace;
  --sans:"Segoe UI",system-ui,-apple-system,Roboto,sans-serif;
}
*{box-sizing:border-box}
html,body{margin:0;height:100%}
body{background:var(--bg);color:var(--text);font-family:var(--sans);font-size:15px;display:flex;flex-direction:column}
button,input,select{font:inherit;color:inherit}
[hidden]{display:none!important}
:focus-visible{outline:3px solid var(--yellow);outline-offset:2px}

/* barra superior */
#top{display:flex;align-items:center;gap:14px;padding:10px 18px;background:var(--panel);border-bottom:1px solid var(--border);flex-wrap:wrap}
.marca{display:flex;align-items:center;gap:10px}
.marca svg{width:34px;height:34px;color:var(--yellow)}
.marca b{display:block;font-size:20px;font-weight:800;letter-spacing:.5px}
.marca small{display:block;font-size:11px;color:var(--muted);font-family:var(--mono);letter-spacing:1.5px}
.tabs{display:flex;gap:6px;margin-left:10px}
.tab{padding:9px 16px;border-radius:10px;border:1px solid var(--border);background:var(--inset);color:var(--muted);font-weight:700;cursor:pointer}
.tab.on{background:rgba(245,166,35,.14);border-color:var(--yellow);color:var(--yellow)}
.spacer{flex:1}
.chip{display:inline-flex;align-items:center;gap:7px;padding:6px 12px;border-radius:999px;border:1px solid var(--border2);font-size:12px;font-weight:700;letter-spacing:1px;color:var(--muted);background:var(--inset);white-space:nowrap}
.chip i{width:8px;height:8px;border-radius:50%;background:var(--red)}
.chip.ok{color:var(--text)} .chip.ok i{background:var(--green);box-shadow:0 0 8px var(--green)}
.chipModo{cursor:pointer;background:repeating-linear-gradient(135deg,#f5a623 0 10px,#d48a0f 10px 20px);color:#1b1204;border:0;font-weight:800}
.chipModo.fisico{background:var(--panel2);color:var(--text);border:1px solid var(--border2)}
#reloj{font-family:var(--mono);font-size:18px;font-weight:700}
.paro{padding:10px 18px;border-radius:10px;border:0;background:var(--red);color:#fff;font-weight:800;cursor:pointer;box-shadow:0 0 16px rgba(229,72,77,.45)}
.paro.cont{background:var(--green);color:#06210f;box-shadow:none}

main{flex:1;min-height:0;overflow:auto;padding:16px 18px}
h2{margin:0 0 4px;font-size:24px}
.sub{margin:0 0 14px;color:var(--muted);max-width:110ch;line-height:1.45}
.grp{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:16px}
.grp h3{margin:0 0 12px;font-size:12px;letter-spacing:1.6px;color:var(--muted);text-transform:uppercase;display:flex;align-items:center;gap:8px}
.grp h3::before{content:"";width:4px;height:14px;border-radius:2px;background:var(--yellow)}
.btn{padding:11px 14px;border-radius:10px;border:1px solid var(--border2);background:var(--panel2);font-weight:700;cursor:pointer}
.btn:hover:not(:disabled){border-color:var(--muted)}
.btn:disabled{opacity:.4;cursor:default}
.btn.red{background:var(--red);border-color:var(--red);color:#fff}
.btn.yellow{background:var(--yellow);border-color:var(--yellow);color:#1b1204}
.row{display:flex;gap:8px;flex-wrap:wrap} .row .btn{flex:1}
.fld{display:flex;flex-direction:column;gap:6px}
.fld label{font-size:11px;letter-spacing:1.4px;color:var(--muted);font-weight:700;text-transform:uppercase}
select,input[type=text]{background:var(--inset);border:1px solid var(--border2);border-radius:10px;padding:10px 12px;min-width:0}
input[type=range]{width:100%;accent-color:var(--blue)}
.nota{font-size:13px;color:var(--muted);line-height:1.45;margin:0}

/* vista del carro */
.carroGrid{display:grid;grid-template-columns:minmax(0,1fr) minmax(320px,380px);gap:16px;align-items:start}
.mapa{padding:12px}
#svg{width:100%;height:auto;display:block;max-height:calc(100vh - 190px)}
.lado{display:flex;flex-direction:column;gap:12px}
.estado{font-size:20px;font-weight:800;padding:14px 16px;border-radius:10px;background:var(--inset);border:1px solid var(--border);border-left:5px solid var(--dim);line-height:1.2}
.estado small{display:block;font-size:13px;font-weight:500;color:var(--muted);margin-top:4px}
.estado.carga{border-left-color:var(--green)} .estado.carga b{color:var(--green)}
.estado.ruta{border-left-color:var(--yellow)} .estado.ruta b{color:var(--yellow)}
.estado.paro{border-left-color:var(--red)} .estado.paro b{color:var(--red)}
.tag{font-family:var(--mono);font-size:12px;color:var(--muted);margin-top:10px}
.vasos{display:flex;gap:10px;align-items:flex-end;margin-top:6px}
.vasos span{width:34px;height:44px;clip-path:polygon(8% 0,92% 0,80% 100%,20% 100%);background:var(--inset);box-shadow:inset 0 0 0 2px var(--border2)}
.vasos span.lleno{background:var(--c)}
.qr{display:flex;gap:14px;align-items:center}
.qr svg{width:104px;height:104px;flex:none;background:#fff;border-radius:6px;padding:6px}
.qr b{display:block;font-size:22px;color:var(--yellow)}
.qr small{display:block;color:var(--muted);font-size:13px}
.cuenta{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;text-align:center}
.cuenta div{background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:8px 4px}
.cuenta b{display:block;font-size:22px;font-family:var(--mono)}
.cuenta small{color:var(--muted);font-size:11px}
.cuenta .piso b{color:var(--red)}
.ctl{display:grid;gap:12px}
#registro{list-style:none;margin:0;padding:0;max-height:220px;overflow:auto;font-size:13px}
#registro li{padding:5px 0;border-bottom:1px solid #1d242f;color:var(--muted)}
#registro li b{font-family:var(--mono);color:var(--dim);font-weight:500;margin-right:8px}
#svg .alm rect.edif{fill:#1d2530;stroke:#3b4659;stroke-width:2}
#svg .alm.dest rect.edif{stroke:var(--yellow);stroke-width:3}
#svg .alm.dest .tit{fill:var(--yellow)}
#svg .alm.prox rect.edif{stroke:var(--yellow);stroke-width:2.5;stroke-dasharray:8 6}
#svg .alm.prox .tit{fill:var(--yellow)}
.destBtns{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.destBtns button{padding:12px 4px;border-radius:10px;border:2px solid var(--border2);background:var(--inset);font-weight:800;font-size:15px;cursor:pointer;line-height:1.15;color:var(--text)}
.destBtns button small{display:block;font-size:11px;font-weight:600;color:var(--muted);margin-top:3px}
.destBtns button.on{border-color:var(--yellow);background:rgba(245,166,35,.14);color:var(--yellow)}
#svg .alm .tit{fill:var(--text);font:700 16px var(--sans)}
#svg .alm .num{fill:var(--muted);font:600 12px var(--mono)}
#svg .parada line{stroke:#c9d1db;stroke-width:3}
#svg .sens{fill:#3b4659}
#svg .sens.on{fill:#ff4d4d}

/* comunicación */
.commGrid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(380px,1fr));align-items:start}
.wide{grid-column:1/-1}
.cardHead{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:12px}
.cardHead h3{margin:0}
.pill{font-size:11px;font-weight:800;letter-spacing:1px;padding:5px 10px;border-radius:999px;border:1px solid var(--border2);color:var(--muted);display:inline-flex;gap:6px;align-items:center}
.pill::before{content:"";width:7px;height:7px;border-radius:50%;background:var(--red)}
.pill.ok{color:var(--green);border-color:rgba(51,196,105,.45)} .pill.ok::before{background:var(--green)}
.modoBtns{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.modoBtn{display:flex;flex-direction:column;align-items:flex-start;gap:3px;text-align:left;padding:12px 14px;border-radius:10px;background:var(--inset);border:2px solid var(--border);cursor:pointer}
.modoBtn b{font-size:17px} .modoBtn small{font-size:12px;color:var(--muted);line-height:1.3}
.modoBtn.on{border-color:var(--yellow);background:rgba(245,166,35,.12)} .modoBtn.on b{color:var(--yellow)}
.connRow{display:grid;grid-template-columns:1fr 140px auto;gap:10px;align-items:end}
.ultima{font-family:var(--mono);font-size:13px;color:var(--muted);background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:9px 12px;margin-top:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
pre.log{margin:10px 0 0;font-family:var(--mono);font-size:12px;color:var(--muted);background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:10px;max-height:220px;overflow:auto;white-space:pre-wrap}
pre.proto{max-height:none}

#pie{background:var(--panel);border-top:1px solid var(--border);font-family:var(--mono);font-size:13px;color:var(--muted);padding:8px 18px}
#toast{position:fixed;left:50%;bottom:44px;transform:translateX(-50%);background:var(--panel2);border:1px solid var(--border2);padding:12px 18px;border-radius:10px;font-weight:600;box-shadow:0 10px 30px rgba(0,0,0,.4);opacity:0;transition:opacity .25s;pointer-events:none;max-width:90vw}
#toast.on{opacity:1}
@media (max-width:1100px){.carroGrid{grid-template-columns:1fr}.connRow{grid-template-columns:1fr}}
@media (prefers-reduced-motion:reduce){*{transition-duration:.01ms!important}}

/* ---- mantenimiento y ajustes para el ESP32 ---- */
.btn.green{background:var(--green);border-color:var(--green);color:#06210f}
.btn.sm{padding:8px 12px;font-size:.85rem}
.btn.dst-b{display:flex;flex-direction:column;align-items:center;gap:2px;padding:12px 4px;font-size:1.05rem}
.btn.dst-b small{font-size:11px;font-weight:600;color:var(--muted);letter-spacing:.5px}
.btn.dst-b.on{border-color:var(--yellow);color:var(--yellow)}
.badge{display:inline-block;padding:5px 10px;border-radius:999px;font-size:11px;font-weight:800;letter-spacing:1px;border:1px solid var(--border2);color:var(--muted);background:var(--inset);white-space:nowrap}
.badge.work{color:var(--yellow);border-color:var(--yellow)}
.badge.lost{color:var(--red);border-color:var(--red)}
.cardHead .badge,.cardHead .chip{font-size:11px}
.banner{border-radius:10px;padding:10px 14px;margin:0 0 12px;font-weight:700;background:rgba(229,72,77,.14);border:1px solid var(--red);color:#ffc3c9}
.banner[hidden]{display:none}
#bnSin{background:rgba(245,166,35,.13);border-color:var(--yellow);color:#ffe0b0}
.bars{display:flex;gap:6px;align-items:flex-end;height:110px}
.bc{flex:1;display:flex;flex-direction:column;align-items:center;gap:4px;min-width:0}
.bar{width:100%;height:84px;background:var(--inset);border:1px solid var(--border);border-radius:6px;display:flex;align-items:flex-end;overflow:hidden}
.bar i{display:block;width:100%;background:linear-gradient(0deg,var(--blue),var(--green));transition:height .12s}
.bc.bad .bar i{background:var(--red)}
.bc span{font-size:11px;color:var(--muted);font-family:var(--mono)}
.lane{position:relative;height:14px;border-radius:7px;background:var(--inset);border:1px solid var(--border);margin:16px 0 4px}
.lane:after{content:"";position:absolute;left:50%;top:-4px;bottom:-4px;width:2px;background:var(--muted);opacity:.6}
.lane i{position:absolute;top:50%;width:16px;height:16px;border-radius:50%;background:var(--yellow);transform:translate(-50%,-50%);left:50%;transition:left .12s}
.lanelbl{display:flex;justify-content:space-between;font-size:11px;color:var(--muted)}
.sens-ok{margin-top:8px;font-size:13px;color:var(--muted);display:flex;flex-wrap:wrap;gap:4px 12px}
.sens-ok .bad{color:var(--red);font-weight:700}
.kv{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.kv div{background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:8px 10px}
.kv small{display:block;color:var(--muted);font-size:11px;margin-bottom:2px}
.kv b{font-family:var(--mono);font-size:15px}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
.m{background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:8px 10px}
.m small{display:block;color:var(--muted);font-size:11px;margin-bottom:2px}
.m b{font-family:var(--mono);font-size:15px}
.line{display:flex;align-items:center;gap:8px;margin-top:8px;flex-wrap:wrap}
.line>span{min-width:86px;color:var(--muted);font-size:13px}
.line input[type=range]{flex:1;min-width:120px}
.line b{min-width:40px;text-align:right;font-family:var(--mono)}
.hist{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:6px}
.hist li{display:flex;justify-content:space-between;gap:8px;background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:7px 10px;font-size:13px}
.hist li span{color:var(--muted);font-family:var(--mono);font-size:12px}
.hist li.empty{justify-content:center;color:var(--muted)}
.pidF{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:10px}
.pidF label{display:block;font-size:11px;color:var(--muted);margin-bottom:3px;text-transform:uppercase;letter-spacing:1px}
input[type=number],input[type=text]{background:var(--inset);border:1px solid var(--border2);border-radius:10px;padding:9px 10px;color:var(--text);font:inherit;width:100%;min-width:0}
select{font:inherit}
.chip b{color:var(--text)}
.chip.on{border-color:var(--yellow);color:var(--yellow)}
.chip.on b{color:var(--yellow)}
.pill.ok{color:var(--green)}
.ultima{font-family:var(--mono);font-size:13px;color:var(--muted);background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:9px 12px;margin-top:10px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.carroGrid .grp.ctl{gap:10px}
/* v12: modo de trabajo, QR de cada estación y lectura del lector */
#svg .qrE rect.fondo{fill:#fff}
#svg .qrE text{fill:#98a3b3;font:700 11px var(--mono)}
#svg .qrE.leido rect.fondo{fill:#f5a623}
#svg .qrE.leido text{fill:#f5a623}
#svg .ruta-hecha{stroke:#f5a623;stroke-width:4;fill:none;stroke-linecap:round;opacity:.9}
.modoNota{margin-top:10px}
.dst{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:8px}

</style>
</head>
<body>
<header id="top">
  <div class="marca">
    <svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="8" width="18" height="9" rx="2.5" fill="none" stroke="currentColor" stroke-width="2"/><circle cx="7.5" cy="18.5" r="1.8" fill="currentColor"/><circle cx="16.5" cy="18.5" r="1.8" fill="currentColor"/><path d="M8 8V5.5h8V8" fill="none" stroke="currentColor" stroke-width="2"/></svg>
    <div><b>AGV-01</b><small>CARRO DE ENTREGA · ESP32</small></div>
  </div>
  <nav class="tabs" aria-label="Vistas">
    <button class="tab on" type="button" data-v="vCarro">Carro</button>
    <button class="tab" type="button" data-v="vMant">Mantenimiento</button>
  </nav>
  <div class="spacer"></div>
  <button type="button" class="chip chipModo fisico" id="chipModo" title="Modo de trabajo del carro">MODO —</button>
  <span class="chip" id="chipEsp"><i></i>ESP32</span>
  <span class="chip" id="chipSrv"><i></i>SERVIDOR</span>
  <span class="chip" id="chipHmi"><i></i>HMI</span>
  <div id="reloj">--:--:--</div>
  <button type="button" class="paro" id="btnParo">⛔ DETENER CARRO</button>
</header>

<main>
  <div class="banner" id="bnErr" hidden>Error de pines/PWM: los motores están bloqueados. Revisa el cableado y reinicia el ESP32.</div>
  <div class="banner" id="bnSin" hidden>Sin conexión con el ESP32. Reconectando…</div>

  <!-- ===================== CARRO ===================== -->
  <section id="vCarro">
    <h2>Carro AGV · entrega a almacenes</h2>
    <p class="sub">Espera en la posición de carga mientras la rueda arma el lote; los vasos aprobados caen al carro. Con el lote completo sale, sigue la línea hasta su almacén, entrega y regresa a carga.</p>
    <div class="carroGrid">
      <section class="grp mapa"><svg id="svg" viewBox="0 0 1000 600" role="img" aria-label="Circuito del carro con la posición de carga, el lector QR y los tres almacenes"></svg></section>
      <div class="lado">
        <section class="grp">
          <h3>Estado del carro</h3>
          <div class="estado" id="estado"><b>—</b><small></small></div>
          <div class="tag">Vasos en el carro</div>
          <div class="vasos" id="vasos"><span></span><span></span><span></span></div>
        </section>
        <section class="grp">
          <h3>Destino del lote</h3>
          <div class="qr"><svg id="qr" viewBox="0 0 25 25" aria-label="Código del lote"></svg>
            <div><b id="destTxt">—</b><small id="destSub">El destino se toma del lote al salir de carga.</small></div></div>
        </section>
        <section class="grp ctl">
          <h3>Control</h3>
          <div class="fld"><label>¿A qué almacén va el lote?</label>
            <div class="destBtns" id="destBtns">
              <button type="button" data-d="1">Almacén 1<small>QR-E1</small></button>
              <button type="button" data-d="2">Almacén 2<small>QR-E2</small></button>
              <button type="button" data-d="3">Almacén 3<small>QR-E3</small></button>
            </div>
            <select id="modoDest" hidden><option value="0">Sin elegir</option><option value="1">1</option><option value="2">2</option><option value="3">3</option></select>
            <p class="nota" style="margin-top:6px">Se detiene en cada estación y lee su QR; descarga solo al leer el QR del almacén elegido.</p></div>
          <div class="fld"><label for="vel">Velocidad del carro: <span id="velTxt">60</span> %</label><input type="range" id="vel" min="20" max="100" step="5" value="60"></div>
          <div class="row"><button class="btn" id="btnEnviar" type="button">Enviar carro ahora</button></div>
          <p class="nota" id="notaCtl"></p>
        </section>
        <section class="grp">
          <h3>Entregas</h3>
          <div class="cuenta"><div><b id="c1">0</b><small>Almacén 1</small></div><div><b id="c2">0</b><small>Almacén 2</small></div><div><b id="c3">0</b><small>Almacén 3</small></div><div class="piso"><b id="cPiso">0</b><small>Vasos al piso</small></div></div>
        </section>
        <section class="grp">
          <h3>Registro</h3>
          <ul id="registro"></ul>
        </section>
      </div>
    </div>
  </section>

  <!-- ===================== MANTENIMIENTO ===================== -->
  <section id="vMant" hidden>
    <h2>Mantenimiento</h2>
    <p class="sub">Pruebas y configuración del carro. Los movimientos a mano solo funcionan con el robot detenido y sin lote en curso.</p>
    <div class="commGrid">

      <section class="grp wide" id="cardModo">
        <div class="cardHead"><h3>Modo de trabajo</h3><span class="pill" id="modoPill">—</span></div>
        <div class="modoBtns">
          <button type="button" class="modoBtn" data-sim="0"><b>Físico</b><small>El carro sigue la línea real y lee los QR de la pista</small></button>
          <button type="button" class="modoBtn" data-sim="1"><b>Simulador</b><small>Motores apagados: el viaje, los QR y la descarga se simulan</small></button>
        </div>
        <p class="nota modoNota">En los dos modos el ESP32 se comunica de verdad con el HMI y la tienda. El modo solo se puede cambiar con el carro en carga y detenido, y se queda guardado al reiniciar.</p>
      </section>

      <section class="grp wide">
        <div class="cardHead"><h3>Enviar a estación (pruebas)</h3><span class="badge" id="stDestino">SIN DESTINO</span></div>
        <div class="dst">
          <button type="button" class="btn dst-b" data-e="0">Base<small>HOME</small></button>
          <button type="button" class="btn dst-b" data-e="1">E1<small>Almacén 1</small></button>
          <button type="button" class="btn dst-b" data-e="2">E2<small>Almacén 2</small></button>
          <button type="button" class="btn dst-b" data-e="3">E3<small>Almacén 3</small></button>
          <button type="button" class="btn dst-b" data-e="4">E4<small>Prueba</small></button>
        </div>
        <div class="row" style="margin-top:12px">
          <button class="btn green" id="btnSeg" type="button">Iniciar seguimiento</button>
        </div>
        <p class="nota" style="margin-top:10px">Elige una estación: el robot sigue la línea, pasa de largo las demás y se detiene en el QR de la elegida. Si es E1 a E4 corre la rampa. Con "Iniciar seguimiento" a secas se detiene en el primer QR que lea.</p>
      </section>

      <section class="grp">
        <div class="cardHead"><h3>Sensores de línea (QTR-8A)</h3><span class="badge" id="stLinea">—</span></div>
        <div class="bars" id="bars"></div>
        <div class="lane"><i id="lanePos"></i></div>
        <div class="lanelbl"><span>Izquierda</span><span>Centro</span><span>Derecha</span></div>
        <div class="sens-ok" id="estadoSensores"></div>
        <div class="row" style="margin-top:12px"><button class="btn yellow" id="btnCal" type="button">Recalibrar sensores (5 s)</button></div>
        <div class="kv" style="margin-top:12px">
          <div><small>Sensor IR de marca</small><b id="irTxt">blanco</b></div>
          <div><small>Zona de estación</small><b id="zonaTxt">no</b></div>
          <div><small>Línea visible</small><b id="lineaVis">–</b></div>
        </div>
      </section>

      <section class="grp">
        <div class="cardHead"><h3>Servomotores de la rampa</h3><span class="badge" id="stRampa">REPOSO</span></div>
        <div class="line"><span>Compuerta</span>
          <input type="range" id="svC" min="0" max="90" value="0"><b id="svCv">0°</b></div>
        <div class="row" style="margin-top:6px">
          <button class="btn green" id="btnCompA" type="button">Abrir</button>
          <button class="btn red" id="btnCompC" type="button">Cerrar</button></div>
        <div class="line"><span>Elevación</span>
          <input type="range" id="svE" min="0" max="90" value="0"><b id="svEv">0°</b></div>
        <div class="row" style="margin-top:6px">
          <button class="btn green" id="btnElevU" type="button">Subir</button>
          <button class="btn red" id="btnElevD" type="button">Bajar</button></div>
        <div class="row" style="margin-top:12px"><button class="btn yellow" id="btnDesc" type="button">Probar secuencia de descarga</button></div>
        <p class="nota" style="margin-top:10px">Los sliders mueven el servo al soltarlos. Úsalos para ajustar la posición de cerrado (0°) y abierto (90°) de tu mecanismo. Solo con el robot detenido.</p>
      </section>

      <section class="grp">
        <div class="cardHead"><h3>Lector QR (GM65)</h3><span class="chip" id="chipQrB">Bytes del lector: <b id="qrBytesTxt">–</b></span></div>
        <div class="ultima" id="qrUltimo">Ninguna lectura todavía</div>
        <ul class="hist" id="hist" style="margin-top:10px"><li class="empty">Sin lecturas todavía</li></ul>
        <div class="line" style="margin-top:12px"><span>Baudios</span>
          <select id="qrBaud"><option value="9600">9600</option><option value="115200">115200</option><option value="38400">38400</option></select>
          <button class="btn" id="btnBaud" type="button">Cambiar</button></div>
        <div class="row" style="margin-top:10px"><button class="btn" id="btnQrTest" type="button">Probar lector (QR-E1 por puente)</button></div>
        <p class="nota" style="margin-top:10px">Si "Bytes del lector" se queda en 0 al escanear, el lector no manda nada: revisa modo serial (no USB teclado), TX/RX, 5 V y baudrios.</p>
      </section>

      <section class="grp">
        <div class="cardHead"><h3>Prueba de motores</h3><span class="badge" id="stPrueba">LISTO</span></div>
        <p class="nota">Potencia directa a cada motor (sin PID ni encoders) durante 1.5 s. Ruedas en el aire.</p>
        <div class="line"><span>Potencia</span><input id="pwmTest" type="range" min="40" max="255" value="120"><b id="pwmTestVal">120</b></div>
        <div class="line"><span>Izquierdo</span>
          <button class="btn green sm" data-m="l1" type="button">Adelante</button>
          <button class="btn red sm" data-m="l-1" type="button">Atrás</button></div>
        <div class="line"><span>Derecho</span>
          <button class="btn green sm" data-m="r1" type="button">Adelante</button>
          <button class="btn red sm" data-m="r-1" type="button">Atrás</button></div>
        <div class="line"><span>Ambos</span>
          <button class="btn green sm" data-m="b1" type="button">Adelante</button>
          <button class="btn yellow sm" id="btnTestStop" type="button">Parar</button></div>
        <div class="kv" style="margin-top:12px">
          <div><small>Pulsos acumulados izq</small><b id="stTotL">–</b></div>
          <div><small>Pulsos acumulados der</small><b id="stTotR">–</b></div>
        </div>
      </section>

      <section class="grp">
        <h3>PID de línea</h3>
        <div class="pidF">
          <div><label for="kp">Kp</label><input id="kp" type="number" step="0.1" inputmode="decimal"></div>
          <div><label for="ki">Ki</label><input id="ki" type="number" step="0.01" inputmode="decimal"></div>
          <div><label for="kd">Kd</label><input id="kd" type="number" step="0.1" inputmode="decimal"></div>
          <div><label for="base">Vel. base</label><input id="base" type="number" inputmode="numeric"></div>
          <div><label for="max">Vel. máx.</label><input id="max" type="number" inputmode="numeric"></div>
        </div>
        <button class="btn green" id="btnPid" type="button">Aplicar PID de línea</button>
      </section>

      <section class="grp">
        <h3>PID de velocidad (encoder)</h3>
        <div class="pidF">
          <div><label for="kpv">Kp</label><input id="kpv" type="number" step="0.1" inputmode="decimal"></div>
          <div><label for="kiv">Ki</label><input id="kiv" type="number" step="0.01" inputmode="decimal"></div>
          <div><label for="kdv">Kd</label><input id="kdv" type="number" step="0.01" inputmode="decimal"></div>
        </div>
        <button class="btn green" id="btnVelPid" type="button">Aplicar PID de velocidad</button>
      </section>

      <section class="grp wide">
        <div class="cardHead"><h3>Red y servidor</h3><span class="pill" id="redPill">SIN CONEXIÓN</span></div>
        <div class="connRow">
          <div class="fld"><label for="ssid">Red WiFi del carro (SSID)</label><input type="text" id="ssid" placeholder="Nombre de la red o hotspot"></div>
          <div class="fld"><label for="pass">Contraseña</label><input type="text" id="pass" placeholder="(no se muestra la guardada)"></div>
          <div class="fld"><label for="srv">Servidor (IP de la laptop o link de la nube)</label><input type="text" id="srv" placeholder="vacío = lo busca solo · nube: celda-llenado.onrender.com"></div>
          <div class="fld"><label for="port">Puerto</label><input type="number" id="port" value="3000" inputmode="numeric"></div>
        </div>
        <div class="row" style="margin-top:10px"><button class="btn green" id="btnRed" type="button">Guardar y reconectar</button></div>
        <div class="ultima" id="redTxt">—</div>
        <p class="nota" style="margin-top:10px">Escribe la red WiFi de la laptop de la rueda (2.4 GHz). Deja "Servidor" VACÍO: el carro encuentra la laptop solo. Escribe la IP solo si después de 1 minuto SERVIDOR sigue en rojo. El ESP32 siempre mantiene su propia red "SeguidorLineaPID" para este panel.</p>
      </section>

      <section class="grp">
        <h3>Telemetría</h3>
        <div class="grid">
          <div class="m"><small>Posición</small><b id="stPos">–</b></div>
          <div class="m"><small>Error</small><b id="stErr">–</b></div>
          <div class="m"><small>PWM izq / der</small><b id="stPwm">–</b></div>
          <div class="m"><small>Pulsos izq</small><b id="stEncL">–</b></div>
          <div class="m"><small>Pulsos der</small><b id="stEncR">–</b></div>
          <div class="m"><small>Robot</small><b id="stRun">–</b></div>
        </div>
      </section>

    </div>
  </section>
</main>
<footer id="pie">●</footer>
<div id="toast" role="status" aria-live="polite"></div>

<script>
"use strict";
/* =====================================================================
   AGV-01 · Panel del carro (servido por el ESP32)
   Todo el estado viene del ESP32 por WebSocket (/ws), mensaje agv_estado
   cada ~150 ms, y eventos agv_evento. La página solo muestra y manda
   comandos: el carro funciona aunque el panel esté cerrado.
   ===================================================================== */
const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
const esc = s => String(s).replace(/[&<>"']/g, c => ({ "&":"&amp;", "<":"&lt;", ">":"&gt;", '"':"&quot;", "'":"&#39;" }[c]));
const COLOR = ["#8a4060", "#d08a44", "#f2c12e"];
const PARADAS = { carga:[280, 330], qr:[400, 470], a1:[650, 470], a2:[850, 290], a3:[560, 110] };
const RUTA = "M 280 330 L 280 400 Q 280 470 350 470 L 780 470 Q 850 470 850 400 L 850 180 Q 850 110 780 110 L 350 110 Q 280 110 280 180 Z";
const TXT = {
  CARGA:["EN POSICIÓN DE CARGA", "Esperando el lote de la rueda"],
  SALIENDO:["LOTE CARGADO", "Siguiendo la línea rumbo al almacén"],
  DESCARGANDO:["DESCARGANDO", "Entregando el lote"],
  REGRESANDO:["REGRESANDO", "Siguiendo la línea de vuelta a la posición de carga"]
};
const NOM_QR = { "QR-HOME":"Base (HOME)", "QR-E1":"Estación 1", "QR-E2":"Estación 2", "QR-E3":"Estación 3", "QR-E4":"Estación 4" };
const CORTO = ["HOME", "E1", "E2", "E3", "E4"];
const st = { estado:"CARGA", s:0, lado:0, linea:"010", vasos:[], lote:null, pausado:false, entregas:[0, 0, 0], piso:0 };
let ruta = null, L = 0, S = {};
let ws = null, wsAbierto = false, ultimoDato = 0, tel = null, camposListos = false;
let ultQr = "", ultQrT = 0, tToast = null;
const hist = [], qrCache = {};
const hora = () => new Date().toLocaleTimeString("es-MX", { hour12:false });

/* ---------- utilidades ---------- */
function toast(m){ const t = $("#toast"); t.textContent = m; t.classList.add("on"); clearTimeout(tToast); tToast = setTimeout(() => t.classList.remove("on"), 3200); }
function log(m){
  const li = document.createElement("li"), h = hora();
  li.innerHTML = `<b>${h}</b>`; li.append(m);
  const r = $("#registro"); r.prepend(li); while(r.children.length > 60) r.lastChild.remove();
}
function enviar(o){
  if(ws && ws.readyState === 1) ws.send(JSON.stringify(o));
  else toast("Sin conexión con el ESP32.");
}

/* =====================================================================
   DIBUJO DEL CIRCUITO (mismo trazo y estaciones que la versión anterior)
   ===================================================================== */
const qrCod = code => qrCache[code] || (qrCache[code] = qrSvg(code));
function qrSvg(texto){
  let h = 2166136261; for(const ch of texto){ h ^= ch.charCodeAt(0); h = Math.imul(h, 16777619); }
  const rnd = () => { h ^= h << 13; h ^= h >>> 17; h ^= h << 5; return (h >>> 0) / 4294967296; };
  const N = 25, finder = (x, y) => [[0,0],[N-7,0],[0,N-7]].some(([fx, fy]) => x >= fx && x < fx + 7 && y >= fy && y < fy + 7);
  let r = "";
  [[0,0],[N-7,0],[0,N-7]].forEach(([x, y]) => {
    r += `<rect x="${x}" y="${y}" width="7" height="7" fill="#111"/><rect x="${x+1}" y="${y+1}" width="5" height="5" fill="#fff"/><rect x="${x+2}" y="${y+2}" width="3" height="3" fill="#111"/>`;
  });
  for(let y = 0; y < N; y++) for(let x = 0; x < N; x++){
    if(finder(x, y)) continue;
    if(rnd() < 0.48) r += `<rect x="${x}" y="${y}" width="1.02" height="1.02" fill="#111"/>`;
  }
  return r;
}
/* cuadrito QR pegado junto a la parada (mismo texto que el código real: QR-HOME, QR-E1…) */
function qrEst(n, x, y, tx, ty){
  return `<g class="qrE" id="qrE${n}"><rect class="fondo" x="${x}" y="${y}" width="44" height="44" rx="3"/>
    <svg x="${x+3}" y="${y+3}" width="38" height="38" viewBox="0 0 25 25">${qrCod("QR-" + n)}</svg>
    <text x="${tx}" y="${ty}" text-anchor="middle">QR-${n}</text></g>`;
}
function dibujar(){
  const alm = (n, x, y, w, h, tx, ty) =>
    `<g class="alm" id="alm${n}"><rect class="edif" x="${x}" y="${y}" width="${w}" height="${h}" rx="6"/>
      <path d="M${x} ${y+16}h${w}" stroke="#3b4659" stroke-width="2"/>
      ${[0,1,2].map(k => `<rect x="${x+12+k*((w-24)/3)}" y="${y+26}" width="${(w-24)/3-8}" height="${h-36}" rx="3" fill="#141920" stroke="#2c3544"/>`).join("")}
      <text class="tit" x="${tx}" y="${ty}" text-anchor="middle">Almacén ${n}</text>
      <text class="num" x="${tx}" y="${ty+17}" text-anchor="middle" id="almN${n}">0 lotes</text></g>`;
  $("#svg").innerHTML = `
    <defs><pattern id="pisoPat" width="40" height="40" patternUnits="userSpaceOnUse"><path d="M40 0H0V40" fill="none" stroke="#1b222c" stroke-width="1"/></pattern></defs>
    <rect width="1000" height="600" fill="url(#pisoPat)"/>
    <g transform="translate(120 230)">
      <circle r="78" fill="#1a2029" stroke="#3b4659" stroke-width="3"/><circle r="56" fill="#29313d" stroke="#4a5568" stroke-width="2"/>
      <circle r="12" fill="#5b6779"/><circle cx="0" cy="-44" r="11" fill="#07090c" stroke="#c9d1db" stroke-width="2"/><circle cx="0" cy="44" r="11" fill="#07090c" stroke="#c9d1db" stroke-width="2"/>
      <text y="-92" text-anchor="middle" fill="#eef2f7" font-size="16" font-weight="700" font-family="system-ui,sans-serif">Rueda de Ginebra</text>
    </g>
    <path d="M150 300 L258 322" stroke="#f5a623" stroke-width="4" stroke-dasharray="8 7" fill="none"/>
    <path d="M258 322 l-12 -9 m12 9 l-14 4" stroke="#f5a623" stroke-width="4" fill="none" stroke-linecap="round"/>
    <text x="150" y="350" text-anchor="middle" fill="#98a3b3" font-size="13" font-family="system-ui,sans-serif">Caída libre de</text>
    <text x="150" y="366" text-anchor="middle" fill="#98a3b3" font-size="13" font-family="system-ui,sans-serif">vasos aprobados</text>
    <path id="ruta" d="${RUTA}" fill="none" stroke="#05070a" stroke-width="16" stroke-linejoin="round"/>
    <path d="${RUTA}" fill="none" stroke="#2a313c" stroke-width="1.5" stroke-dasharray="10 12"/>
    <text x="565" y="300" text-anchor="middle" fill="#5f6b7c" font-size="14" font-family="system-ui,sans-serif">Línea guía · el carro la sigue con 3 sensores infrarrojos</text>
    <g class="parada"><line x1="258" y1="330" x2="302" y2="330"/><text x="312" y="335" fill="#33c469" font-size="15" font-weight="700" font-family="system-ui,sans-serif">CARGA</text></g>
    <path id="rutaHecha" class="ruta-hecha" d="${RUTA}"/>
    ${qrEst("HOME", 204, 284, 226, 342)}
    ${qrEst("E1", 628, 398, 650, 392)}
    ${qrEst("E2", 778, 268, 800, 326)}
    ${qrEst("E3", 538, 140, 560, 198)}
    <g class="parada"><line x1="650" y1="448" x2="650" y2="492"/></g>
    <g class="parada"><line x1="828" y1="290" x2="872" y2="290"/></g>
    <g class="parada"><line x1="560" y1="88" x2="560" y2="132"/></g>
    ${alm(1, 590, 500, 120, 70, 790, 530)}
    ${alm(2, 885, 225, 105, 130, 937, 375)}
    ${alm(3, 500, 8, 120, 62, 690, 36)}
    <g id="carro">
      <rect x="-30" y="-22" width="60" height="44" rx="9" fill="#2b3442" stroke="#c9d1db" stroke-width="2"/>
      <rect x="-24" y="-27" width="16" height="7" rx="2" fill="#0d1117"/><rect x="8" y="-27" width="16" height="7" rx="2" fill="#0d1117"/>
      <rect x="-24" y="20" width="16" height="7" rx="2" fill="#0d1117"/><rect x="8" y="20" width="16" height="7" rx="2" fill="#0d1117"/>
      <rect x="-22" y="-15" width="36" height="30" rx="4" fill="#11161d" stroke="#4a5568"/>
      <circle id="v0" cx="-13" cy="-6" r="6" fill="#1b222c"/><circle id="v1" cx="3" cy="-6" r="6" fill="#1b222c"/><circle id="v2" cx="-5" cy="7" r="6" fill="#1b222c"/>
      <circle class="sens" id="s0" cx="26" cy="-10" r="3.6"/><circle class="sens" id="s1" cx="27" cy="0" r="3.6"/><circle class="sens" id="s2" cx="26" cy="10" r="3.6"/>
      <rect x="18" y="-6" width="6" height="12" rx="1.5" fill="#3aafd9"/>
    </g>`;
  ruta = $("#ruta"); L = ruta.getTotalLength();
  Object.entries(PARADAS).forEach(([k, [x, y]]) => {
    let mejor = 0, d0 = Infinity;
    for(let s = 0; s <= L; s += 2){ const p = ruta.getPointAtLength(s), d = (p.x - x) ** 2 + (p.y - y) ** 2; if(d < d0){ d0 = d; mejor = s; } }
    S[k] = mejor;
  });
  if(S.carga > L / 2) S.carga = 0;
  st.s = S.carga;
}


/* El ESP32 manda la posición cada ~150 ms. Para que el carro se vea avanzar suave por la línea
   (igual que el simulador) el panel la interpola, sin pasarse nunca de la parada a la que va. */
const VISTA = { s:0, sTel:0, tTel:0, movia:false, t0:performance.now() };
function viajando(){ return (st.estado === "SALIENDO" || st.estado === "REGRESANDO") && !st.pausado; }
function limiteViaje(){
  if(st.estado === "SALIENDO" && st.lote) return S["a" + st.lote.destino] ?? L;
  if(st.estado === "REGRESANDO") return L;
  return Infinity;
}
function telemetriaPos(m){
  let s = Number(m.s) || 0;
  if(st.estado === "REGRESANDO" && s < VISTA.sTel - L / 2) s += L;          // dio la vuelta
  VISTA.movia = Math.abs(s - VISTA.sTel) > 0.5;
  VISTA.sTel = s; VISTA.tTel = performance.now();
}
function animar(ahora){
  let obj = VISTA.sTel;
  if(viajando() && VISTA.movia){
    const vel = 40 + ((tel && tel.vel) || 60) * 2.2;                        // misma fórmula que el ESP32
    obj += vel * Math.min(0.25, (ahora - VISTA.tTel) / 1000);
    obj = Math.min(obj, limiteViaje());
  }
  if(st.estado === "CARGA") obj = 0;
  if(Math.abs(obj - VISTA.s) > L / 3 || st.estado === "CARGA" && VISTA.s > L - 5) VISTA.s = obj;
  else VISTA.s += (obj - VISTA.s) * 0.18;
  dibujarCarro(ahora);
  requestAnimationFrame(animar);
}
function dibujarCarro(ahora){
  if(!ruta) return;
  const s = ((VISTA.s % L) + L) % L, p = ruta.getPointAtLength(s), q = ruta.getPointAtLength((s + 2) % L);
  const ang = Math.atan2(q.y - p.y, q.x - p.x), nx = -Math.sin(ang), ny = Math.cos(ang);
  const sim = !!(tel && tel.sim);
  /* en simulador se ve el pequeño vaivén del seguidor de línea, igual que en la página de pruebas */
  const lado = viajando() && sim ? 3.2 * Math.sin((ahora || 0) / 380) : st.lado;
  $("#carro").setAttribute("transform", `translate(${(p.x + nx * lado).toFixed(1)} ${(p.y + ny * lado).toFixed(1)}) rotate(${(ang * 180 / Math.PI).toFixed(1)})`);
  const hecha = $("#rutaHecha");
  if(hecha){ const a = viajando() || st.estado === "DESCARGANDO" ? Math.min(VISTA.s, L) : 0; hecha.setAttribute("stroke-dasharray", `${a.toFixed(1)} ${L}`); }
  const l = sim && viajando() ? (lado > 1.6 ? "011" : lado < -1.6 ? "110" : "010") : (st.linea.length === 3 ? st.linea : "010");
  for(let i = 0; i < 3; i++) $("#s" + i).classList.toggle("on", l[i] === "1");
  for(let i = 0; i < 3; i++){ const g = st.vasos[i]; $("#v" + i).setAttribute("fill", g ? COLOR[g - 1] : "#1b222c"); }
}

/* =====================================================================
   VISTA DEL CARRO
   ===================================================================== */
function renderCarro(){
  const e = $("#estado");
  let [t1, t2] = TXT[st.estado] || ["—", ""], cls = st.estado === "CARGA" ? "carga" : "ruta";
  if(st.pausado){ t1 = "CARRO DETENIDO"; t2 = "Presiona CONTINUAR para seguir."; cls = "paro"; }
  else if(st.lote && st.estado === "SALIENDO") t2 = `Rumbo al Almacén ${st.lote.destino}`;
  else if(st.lote && st.estado === "DESCARGANDO") t2 = `Entregando en el Almacén ${st.lote.destino}`;
  else if(st.estado === "CARGA" && st.vasos.length) t2 = `Recibiendo vasos aprobados: ${st.vasos.length}`;
  e.className = "estado " + cls; e.querySelector("b").textContent = t1; e.querySelector("small").textContent = t2;
  [...$("#vasos").children].forEach((sp, i) => { const g = st.vasos[i]; sp.classList.toggle("lleno", !!g); sp.style.setProperty("--c", g ? COLOR[g - 1] : "transparent"); });

  const enAlmacen = st.lote && (st.estado === "DESCARGANDO" || st.estado === "REGRESANDO");
  $("#destTxt").textContent = st.lote ? (enAlmacen ? `ALMACÉN ${st.lote.destino}` : `Lote ${st.lote.n}`) : "—";
  $("#destSub").textContent = !st.lote ? "El destino se toma del lote al salir de carga." : `Lote ${st.lote.n} · se detiene al leer QR-E${st.lote.destino}`;
  $("#qr").innerHTML = st.lote ? qrCod(`QR-E${st.lote.destino}`) : "";
  for(let n = 1; n <= 3; n++){
    $("#alm" + n).classList.toggle("dest", !!(st.lote && st.lote.destino === n && enAlmacen));
    const prox = tel ? (tel.modoDest ? Number(tel.modoDest) : Number(tel.sigRot) || 0) : 0;
    $("#alm" + n).classList.toggle("prox", !st.lote && prox === n);
    $("#almN" + n).textContent = `${st.entregas[n - 1]} lote${st.entregas[n - 1] === 1 ? "" : "s"}`;
    $("#c" + n).textContent = st.entregas[n - 1];
  }
  $("#cPiso").textContent = st.piso;
  $("#btnEnviar").disabled = !tel || st.estado !== "CARGA" || st.pausado;
  $("#notaCtl").textContent = st.estado === "CARGA"
    ? "En posición de carga. \"Enviar carro ahora\" lo manda aunque la rueda no haya terminado el lote."
    : "El carro está en viaje: el envío manual se habilita al regresar a carga.";
  dibujarCarro();
}

/* =====================================================================
   MANTENIMIENTO (telemetría completa)
   ===================================================================== */
function registrarQr(qr, haceMs){
  const inicio = Date.now() - (haceMs >= 0 ? haceMs : 0);
  if(qr === ultQr && inicio - ultQrT < 4000) return;
  ultQr = qr; ultQrT = inicio;
  hist.unshift({ qr, t: new Date(inicio).toLocaleTimeString("es-MX", { hour12:false }) });
  if(hist.length > 6) hist.pop();
  $("#hist").innerHTML = hist.map(e => `<li><b>${esc(NOM_QR[e.qr.toUpperCase()] || "Código desconocido")}</b><span>${esc(e.qr)} · ${e.t}</span></li>`).join("");
}
function renderMant(m){
  const dst = typeof m.destino === "number" ? m.destino : -1;
  const sd = $("#stDestino");
  if(m.running && dst >= 0){ sd.textContent = `YENDO A ${CORTO[dst]}`; sd.className = "badge work"; }
  else if(st.lote){ sd.textContent = "LOTE EN CURSO"; sd.className = "badge work"; }
  else { sd.textContent = "SIN DESTINO"; sd.className = "badge"; }
  $$(".dst-b").forEach(b => b.classList.toggle("on", Number(b.dataset.e) === dst && !!m.running));
  $("#btnSeg").textContent = m.running ? "Detener seguimiento" : "Iniciar seguimiento";
  $("#btnSeg").className = "btn " + (m.running ? "red" : "green");

  const val = m.val || [], ok = m.ok || [];
  const cont = $("#bars");
  if(cont.children.length !== val.length){
    cont.innerHTML = "";
    val.forEach((_, k) => { const c = document.createElement("div"); c.className = "bc"; c.id = "bc" + k; c.innerHTML = `<div class="bar"><i id="bf${k}"></i></div><span>S${k + 1}</span>`; cont.appendChild(c); });
  }
  let res = "";
  val.forEach((v, j) => {
    $("#bf" + j).style.height = Math.max(0, Math.min(100, v / 10)) + "%";
    $("#bc" + j).className = "bc" + (ok[j] ? "" : " bad");
    if(!ok[j]) res += `<span class="bad">S${j + 1} revisar</span>`;
  });
  $("#estadoSensores").innerHTML = res || "<span>Los 8 sensores están bien tras la calibración</span>";
  $("#lanePos").style.left = Math.max(0, Math.min(100, (m.posicion || 0) / 70)) + "%";
  $("#stLinea").textContent = st.linea;
  $("#lineaVis").textContent = m.lineaVisible ? "Sí" : "No";
  $("#irTxt").textContent = m.irNegro ? "NEGRO" : "blanco";
  $("#zonaTxt").textContent = m.zonaLenta ? "SÍ (va lento)" : "no";

  const r = $("#stRampa");
  if(!m.compuertaOk || !m.elevacionOk){ r.textContent = "SERVO NO DETECTADO"; r.className = "badge lost"; }
  else { r.textContent = m.rampaFase || "REPOSO"; r.className = "badge" + (m.rampaActiva ? " work" : ""); }

  if(m.qr) registrarQr(m.qr, m.qrHaceMs);
  $("#qrUltimo").textContent = m.qr ? `${m.qr} · ${NOM_QR[String(m.qr).toUpperCase()] || "código desconocido"}` : "Ninguna lectura todavía";
  $("#qrBytesTxt").textContent = m.qrBytes !== undefined ? m.qrBytes : "–";
  $("#chipQrB").classList.toggle("on", (m.qrBytes || 0) > 0);

  $("#stPrueba").textContent = m.prueba ? "PROBANDO" : "LISTO";
  $("#stPrueba").className = "badge" + (m.prueba ? " work" : "");
  if(m.encTotL !== undefined){ $("#stTotL").textContent = m.encTotL; $("#stTotR").textContent = m.encTotR; }
  $("#stPos").textContent = m.posicion;
  $("#stErr").textContent = m.error;
  $("#stPwm").textContent = `${m.pwmL} / ${m.pwmR}`;
  $("#stEncL").textContent = m.encL;
  $("#stEncR").textContent = m.encR;
  $("#stRun").textContent = m.running ? "SIGUIENDO" : (m.pausado ? "PAUSADO" : "DETENIDO");

  $$("#destBtns button").forEach(b => b.classList.toggle("on", Number(b.dataset.d) === (m.modoDest || 0)));
  const sim = !!m.sim;
  $("#chipModo").textContent = sim ? "MODO SIMULADOR" : "MODO FÍSICO";
  $("#chipModo").classList.toggle("fisico", !sim);
  $("#modoPill").textContent = sim ? "SIMULADOR" : "FÍSICO"; $("#modoPill").classList.add("ok");
  $$("#cardModo .modoBtn").forEach(b => b.classList.toggle("on", (b.dataset.sim === "1") === sim));
  $("#redPill").textContent = m.srv ? "SERVIDOR CONECTADO" : "SIN SERVIDOR";
  $("#redPill").classList.toggle("ok", !!m.srv);
  $("#redTxt").textContent = `Wi-Fi "${m.ssid || "—"}": ${m.sta ? "conectado · IP del carro " + m.ip : "sin conexión"} · Servidor ${m.srvHost ? m.srvHost + ":" + m.srvPort + (m.srvAuto ? " (encontrado solo)" : "") : "buscando…"}: ${m.srv ? "conectado" : "sin conexión"} · Sala "${m.sala || "principal"}": ${({esperando:"esperando permiso (apruébalo en la página del carro de esa sala)",aprobado:"dentro de la sala",rechazado:"rechazado",fuera:"fuera de la sala"})[m.permiso] || "—"} · HMI: ${m.hmi ? "en línea" : "sin conexión"}`;

  if(!camposListos){
    camposListos = true;
    $("#modoDest").value = String(m.modoDest || 0);
    $("#vel").value = m.vel || 60; $("#velTxt").textContent = m.vel || 60;
    $("#kp").value = m.kp; $("#ki").value = m.ki; $("#kd").value = m.kd;
    $("#base").value = m.base; $("#max").value = m.max;
    $("#kpv").value = m.kpv; $("#kiv").value = m.kiv; $("#kdv").value = m.kdv;
    $("#ssid").value = m.ssid || ""; $("#srv").value = m.srvAuto ? "" : (m.srvHost || ""); $("#port").value = m.srvPort || 3000;
  }
}

/* =====================================================================
   LLEGADAS DEL ESP32
   ===================================================================== */
function recibirEstado(m){
  tel = m; ultimoDato = Date.now();
  Object.assign(st, {
    estado: m.estado || "CARGA", s: m.s || 0, lado: m.lado || 0, linea: m.linea || "010",
    vasos: m.vasos || [], lote: m.lote || null, pausado: !!m.pausado,
    entregas: m.entregas || [0, 0, 0], piso: m.piso || 0
  });
  telemetriaPos(m);
  $("#bnErr").hidden = m.pinesOk !== false;
  render(); renderMant(m);
  pintarConexion();
}
function recibirEvento(m){
  const lt = m.lote, d = m.almacen;
  switch(m.ev){
    case "salio":       log(`🚚 Sale con el lote ${lt ? lt.n : ""} hacia el Almacén ${lt ? lt.destino : "?"}.`); break;
    case "qr": {
      const n = m.estacion === 0 ? "HOME" : m.estacion >= 1 ? "E" + m.estacion : "";
      log(`🔳 QR leído: ${m.codigo || (n ? "QR-" + n : "?")}${d ? " (Almacén " + d + ")" : ""}.`);
      const g = n && $("#qrE" + n);
      if(g){ g.classList.add("leido"); setTimeout(() => g.classList.remove("leido"), 1800); }
      break;
    }
    case "llegada":     log(`✔ Llegó al Almacén ${d}: descargando.`); break;
    case "entregado":   log(`✔ Lote entregado en el Almacén ${d}. Regresa a carga.`); break;
    case "en_carga":    log("🚚 En la posición de carga: listo para el siguiente lote."); break;
    case "piso":        toast("⚠ ¡Un vaso aprobado cayó al piso!"); log("⚠ Vaso aprobado al piso: el carro no estaba en carga."); break;
    case "no_disponible": toast("El carro no pudo salir."); log("⚠ El carro no pudo salir (sin pines/PWM listos)."); break;
    default: break;
  }
}
function render(){ renderCarro(); }

/* =====================================================================
   CONEXIÓN CON EL ESP32 (la página la sirve el propio ESP32)
   ===================================================================== */
function conectar(){
  const host = /^\d+\.\d+\.\d+\.\d+$/.test(location.hostname) ? location.host : "192.168.4.1";
  ws = new WebSocket("ws://" + host + "/ws");
  ws.onopen = () => { wsAbierto = true; ultimoDato = Date.now(); pintarConexion(); };
  ws.onclose = () => { wsAbierto = false; pintarConexion(); setTimeout(conectar, 1500); };
  ws.onmessage = e => {
    let m; try{ m = JSON.parse(e.data); }catch(_){ return; }
    if(m.tipo === "agv_estado") recibirEstado(m);
    else if(m.tipo === "agv_evento") recibirEvento(m);
  };
}
function pintarConexion(){
  const vivo = wsAbierto && Date.now() - ultimoDato < 2500;
  $("#chipEsp").classList.toggle("ok", vivo);
  $("#chipSrv").classList.toggle("ok", !!(tel && tel.srv));
  $("#chipHmi").classList.toggle("ok", !!(tel && tel.hmi));
  $("#bnSin").hidden = vivo;
  $("#pie").textContent = `● ${tel && tel.sim ? "MODO SIMULADOR" : "MODO FÍSICO"} · ${vivo ? "ESP32 en línea" : "ESP32 sin datos"} · ${tel && tel.srv ? "servidor conectado" : "sin servidor"} · ${tel && tel.hmi ? "HMI en línea" : "HMI sin conexión"} · ${(TXT[st.estado] || ["", ""])[0].toLowerCase()}`;
  $("#btnParo").textContent = st.pausado ? "▶ CONTINUAR" : "⛔ DETENER CARRO";
  $("#btnParo").classList.toggle("cont", st.pausado);
  $("#btnEnviar").disabled = !vivo || st.estado !== "CARGA" || st.pausado;
}

/* =====================================================================
   COMANDOS
   ===================================================================== */
function bind(){
  $$(".tab").forEach(t => t.addEventListener("click", () => {
    $$(".tab").forEach(x => x.classList.toggle("on", x === t));
    $("#vCarro").hidden = t.dataset.v !== "vCarro"; $("#vMant").hidden = t.dataset.v !== "vMant";
  }));
  $("#btnParo").addEventListener("click", () => enviar({ cmd:"paro", on: !st.pausado }));

  // Modo de trabajo (Físico / Simulador)
  $("#chipModo").addEventListener("click", () => { $$(".tab").find(t => t.dataset.v === "vMant").click(); $("#cardModo").scrollIntoView({ behavior:"smooth" }); });
  $$("#cardModo .modoBtn").forEach(b => b.addEventListener("click", () => {
    const on = b.dataset.sim === "1";
    if(tel && !!tel.sim === on) return;
    if(st.estado !== "CARGA" || (tel && tel.running)){ toast("Para cambiar de modo el carro debe estar en carga y detenido."); return; }
    enviar({ cmd:"modoSim", on }); toast(on ? "Cambiando a SIMULADOR…" : "Cambiando a FÍSICO…");
  }));

  // Carro
  $("#modoDest").addEventListener("change", e => enviar({ cmd:"modoDest", v:Number(e.target.value) }));
  $$("#destBtns button").forEach(b => b.addEventListener("click", () => {
    enviar({ cmd:"modoDest", v:Number(b.dataset.d) });
    toast(`El lote irá al Almacén ${b.dataset.d}`);
  }));
  $("#vel").addEventListener("input", e => { $("#velTxt").textContent = e.target.value; });
  $("#vel").addEventListener("change", e => enviar({ cmd:"vel", v:Number(e.target.value) }));
  $("#btnEnviar").addEventListener("click", () => {
    if(!(tel && tel.hmi) || confirm("¿Enviar el carro ahora? Si la rueda está armando un lote, los vasos que falten caerán al piso.")) enviar({ cmd:"salir" });
  });

  // Estaciones de prueba
  $$(".dst-b").forEach(b => b.addEventListener("click", () => enviar({ cmd:"goto", est:Number(b.dataset.e) })));
  $("#btnSeg").addEventListener("click", () => enviar({ cmd: tel && tel.running ? "stop" : "start" }));
  $("#btnCal").addEventListener("click", () => enviar({ cmd:"calibrate" }));

  // Servos
  $("#svC").addEventListener("input", e => { $("#svCv").textContent = e.target.value + "°"; });
  $("#svC").addEventListener("change", e => enviar({ cmd:"servo", c:"c", grados:Number(e.target.value) }));
  $("#svE").addEventListener("input", e => { $("#svEv").textContent = e.target.value + "°"; });
  $("#svE").addEventListener("change", e => enviar({ cmd:"servo", c:"e", grados:Number(e.target.value) }));
  $("#btnCompA").addEventListener("click", () => enviar({ cmd:"compuertaOpen" }));
  $("#btnCompC").addEventListener("click", () => enviar({ cmd:"compuertaClose" }));
  $("#btnElevU").addEventListener("click", () => enviar({ cmd:"elevacionUp" }));
  $("#btnElevD").addEventListener("click", () => enviar({ cmd:"elevacionDown" }));
  $("#btnDesc").addEventListener("click", () => enviar({ cmd:"descargar" }));

  // QR
  $("#btnBaud").addEventListener("click", () => enviar({ cmd:"qrBaud", v:Number($("#qrBaud").value) }));
  $("#btnQrTest").addEventListener("click", () => enviar({ cmd:"qrTest" }));

  // Motores
  $("#pwmTest").addEventListener("input", e => { $("#pwmTestVal").textContent = e.target.value; });
  $$("[data-m]").forEach(b => b.addEventListener("click", () => {
    const p = Number($("#pwmTest").value) || 0, k = b.dataset.m;
    const l = (k === "l1" || k === "b1") ? p : (k === "l-1" || k === "b-1") ? -p : 0;
    const r = (k === "r1" || k === "b1") ? p : (k === "r-1") ? -p : 0;
    enviar({ cmd:"testMotor", l, r });
  }));
  $("#btnTestStop").addEventListener("click", () => enviar({ cmd:"testStop" }));

  // PID
  $("#btnPid").addEventListener("click", () => enviar({ cmd:"setPID", kp:parseFloat($("#kp").value), ki:parseFloat($("#ki").value), kd:parseFloat($("#kd").value), base:parseInt($("#base").value, 10), max:parseInt($("#max").value, 10) }));
  $("#btnVelPid").addEventListener("click", () => enviar({ cmd:"setVelPID", kp:parseFloat($("#kpv").value), ki:parseFloat($("#kiv").value), kd:parseFloat($("#kdv").value) }));

  // Red
  $("#btnRed").addEventListener("click", () => {
    enviar({ cmd:"red", ssid:$("#ssid").value.trim(), pass:$("#pass").value, srv:$("#srv").value.trim(), port:Number($("#port").value) || 3000 });
    toast("Guardado. El carro se reconecta…");
  });
}

/* ---------- arranque ---------- */
(function iniciar(){
  dibujar();
  bind();
  render();
  conectar();
  setInterval(pintarConexion, 500);
  requestAnimationFrame(animar);
  setInterval(() => { $("#reloj").textContent = hora(); }, 500);
  $("#reloj").textContent = hora();
  log("Panel listo. Esperando datos del carro…");
})();

</script>
</body>
</html>
)HTMLPAGE";
