#ifndef HTML_TEMPLATE_H
#define HTML_TEMPLATE_H

#include <Arduino.h>
#include <WiFi.h>

String getHtml(float pPsi, float pBar, float v, bool mOverride, bool mOn, unsigned long mStart,
               float maxPressureThreshold, int pressureUnit, float hysteresis,
               unsigned long sensorInterval, unsigned long tsIntervalSeconds,
               unsigned long bfIntervalMinutes, float offsetVoltage, bool useTempSensor,
               const String& tsApiKey, const String& bfStreamId, const String& bfDeviceName,
               bool tsEnabled, bool bfEnabled,
               bool httpEnabled, const String& httpServer, const String& httpPath,
               const String& httpBodyTemplate, unsigned long httpIntervalSeconds,
               const String& deviceName) {

    long remaining = 0;
    if (mOverride) {
        remaining = 10000L - (long)(millis() - mStart);
        if (remaining < 0) remaining = 0;
    }

    String valveStatus = mOverride ? (mOn ? "Manual Open" : "Manual Closed") : "Auto";

    String html = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Fermenter Control</title>
<style>
  :root {
    --bg: #0f1117;
    --surface: #1a1d27;
    --surface2: #22263a;
    --border: #2e3348;
    --accent: #6c8ef5;
    --accent2: #a78bfa;
    --ok: #34d399;
    --warn: #fbbf24;
    --danger: #f87171;
    --text: #e2e8f0;
    --text-muted: #8892a4;
    --radius: 12px;
    --radius-sm: 8px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    min-height: 100vh;
    padding: 16px;
  }
  .container { max-width: 640px; margin: 0 auto; }

  /* HEADER */
  .header {
    background: linear-gradient(135deg, #1e2340 0%, #16192d 100%);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px 24px;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    flex-wrap: wrap;
  }
  .header-left h1 {
    font-size: 1.3rem;
    font-weight: 700;
    background: linear-gradient(90deg, var(--accent), var(--accent2));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
    margin-bottom: 2px;
  }
  .header-left p { color: var(--text-muted); font-size: 0.8rem; }

  /* STATUS CARDS */
  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 10px;
    margin-bottom: 16px;
  }
  .card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    padding: 14px 16px;
  }
  .card-label { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-muted); margin-bottom: 4px; }
  .card-value { font-size: 1.25rem; font-weight: 700; }
  .card-value.accent { color: var(--accent); }
  .card-value.ok { color: var(--ok); }
  .card-value.warn { color: var(--warn); }

  /* VALVE CONTROLS */
  .controls {
    display: flex;
    gap: 10px;
    margin-bottom: 16px;
    flex-wrap: wrap;
  }
  .btn {
    padding: 10px 20px;
    border: none;
    border-radius: var(--radius-sm);
    font-size: 0.9rem;
    font-weight: 600;
    cursor: pointer;
    transition: opacity 0.15s, transform 0.1s;
  }
  .btn:hover { opacity: 0.85; }
  .btn:active { transform: scale(0.97); }
  .btn-open {
    background: linear-gradient(135deg, #34d399, #059669);
    color: #fff;
  }
  .btn-close {
    background: linear-gradient(135deg, #f87171, #dc2626);
    color: #fff;
  }

  /* TABS */
  .tabs-nav {
    display: flex;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius) var(--radius) 0 0;
    overflow: hidden;
  }
  .tab-btn {
    flex: 1;
    padding: 13px 16px;
    background: transparent;
    border: none;
    color: var(--text-muted);
    font-size: 0.88rem;
    font-weight: 600;
    cursor: pointer;
    transition: background 0.2s, color 0.2s;
    border-bottom: 2px solid transparent;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
  }
  .tab-btn:hover { background: var(--surface2); color: var(--text); }
  .tab-btn.active {
    color: var(--accent);
    border-bottom-color: var(--accent);
    background: var(--surface2);
  }
  .tabs-body {
    background: var(--surface);
    border: 1px solid var(--border);
    border-top: none;
    border-radius: 0 0 var(--radius) var(--radius);
    padding: 20px;
    margin-bottom: 16px;
  }
  .tab-panel { display: none; }
  .tab-panel.active { display: block; }

  /* SETTINGS FORM */
  .setting-group { margin-bottom: 14px; }
  .setting-group:last-child { margin-bottom: 0; }
  .setting-label {
    display: block;
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.07em;
    color: var(--text-muted);
    margin-bottom: 6px;
  }
  .setting-row {
    display: flex;
    gap: 8px;
    align-items: stretch;
  }
  .setting-row input,
  .setting-row select {
    flex: 1;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    color: var(--text);
    padding: 9px 12px;
    font-size: 0.9rem;
    outline: none;
    transition: border-color 0.2s;
  }
  .setting-row input:focus,
  .setting-row select:focus { border-color: var(--accent); }
  .setting-row select option { background: var(--surface); }
  .btn-set {
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff;
    border: none;
    border-radius: var(--radius-sm);
    padding: 9px 18px;
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
    white-space: nowrap;
    transition: opacity 0.15s;
  }
  .btn-set:hover { opacity: 0.85; }

  .divider {
    border: none;
    border-top: 1px solid var(--border);
    margin: 16px 0;
  }
  .section-title {
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--accent2);
    margin-bottom: 12px;
    font-weight: 700;
  }

  /* TIMER */
  #timer {
    font-size: 0.82rem;
    color: var(--warn);
    min-height: 18px;
  }
</style>
</head>
<body>
<div class="container">

  <!-- HEADER -->
  <div class="header">
    <div class="header-left">
      <h1>&#127866; Fermenter Control</h1>
      <p>)rawhtml";
    html += deviceName + " &nbsp;&#183;&nbsp; " + WiFi.localIP().toString();
    html += R"rawhtml(</p>
    </div>
  </div>

  <!-- STATUS CARDS -->
  <div class="cards">
    <div class="card">
      <div class="card-label">Pressure</div>
      <div class="card-value accent">)rawhtml";
    html += String(pPsi, 2) + " PSI";
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Pressure</div>
      <div class="card-value accent">)rawhtml";
    html += String(pBar, 2) + " Bar";
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Voltage</div>
      <div class="card-value">)rawhtml";
    html += String(v, 3) + " V";
    html += R"rawhtml(</div>
    </div>
    <div class="card">
      <div class="card-label">Valve</div>
      <div class="card-value )rawhtml";
    if (!mOverride) html += "ok\">Auto";
    else if (mOn)   html += "warn\">Manual Open";
    else            html += "danger\">Manual Closed";
    html += R"rawhtml(</div>
    </div>
  </div>

  <!-- VALVE CONTROLS -->
  <div class="controls">
    <form action="/api" method="POST">
      <input type="hidden" name="cmd" value="manual_on">
      <button class="btn btn-open" type="submit">&#9654; Manual Open</button>
    </form>
    <form action="/api" method="POST">
      <input type="hidden" name="cmd" value="manual_off">
      <button class="btn btn-close" type="submit">&#9646; Close / Auto</button>
    </form>
  </div>
  <div id="timer"></div>
  <br>

  <!-- TABS -->
  <div class="tabs-nav">
    <button class="tab-btn active" onclick="switchTab('main', this)" id="tab-main">
      &#9881; Main Settings
    </button>
    <button class="tab-btn" onclick="switchTab('cloud', this)" id="tab-cloud">
      &#9729; Cloud Settings
    </button>
  </div>

  <div class="tabs-body">
    <!-- TAB: MAIN SETTINGS -->
    <div class="tab-panel active" id="panel-main">

      <div class="section-title">Pressure &amp; Valve</div>

      <div class="setting-group">
        <label class="setting-label">Max Pressure Threshold (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.1" name="pressure" value=")rawhtml";
    html += String(maxPressureThreshold, 1);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Device Name</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="devName" value=")rawhtml";
    html += deviceName;
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Pressure Unit</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="pUnit">
              <option value="0")rawhtml";
    html += (pressureUnit == 0 ? " selected" : "");
    html += R"rawhtml(>PSI</option>
              <option value="1")rawhtml";
    html += (pressureUnit == 1 ? " selected" : "");
    html += R"rawhtml(>Bar</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Hysteresis (PSI)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.1" name="hysteresis" value=")rawhtml";
    html += String(hysteresis, 1);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <hr class="divider">
      <div class="section-title">Sensor &amp; Calibration</div>

      <div class="setting-group">
        <label class="setting-label">Sensor Poll Interval (ms)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="sInterval" value=")rawhtml";
    html += String(sensorInterval);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Voltage Offset (V)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" step="0.001" name="offset" value=")rawhtml";
    html += String(offsetVoltage, 3);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Temperature Sensor</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="useTemp">
              <option value="0")rawhtml";
    html += (!useTempSensor ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (useTempSensor ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

    </div><!-- /panel-main -->

    <!-- TAB: CLOUD SETTINGS -->
    <div class="tab-panel" id="panel-cloud">

      <div class="section-title">ThingSpeak</div>

      <div class="setting-group">
        <label class="setting-label">Logging Enabled</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="tsEnabled">
              <option value="0")rawhtml";
    html += (!tsEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (tsEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">API Key</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="tsApiKey" value=")rawhtml";
    html += tsApiKey;
    html += R"rawhtml(" placeholder="ThingSpeak Write API Key">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Send Interval (seconds, min 15)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="tsInterval" value=")rawhtml";
    html += String(tsIntervalSeconds);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <hr class="divider">
      <div class="section-title">Brewfather</div>

      <div class="setting-group">
        <label class="setting-label">Logging Enabled</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="bfEnabled">
              <option value="0")rawhtml";
    html += (!bfEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (bfEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Stream ID</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="bfStreamId" value=")rawhtml";
    html += bfStreamId;
    html += R"rawhtml(" placeholder="Brewfather Stream ID">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Device Name in Brewfather</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="bfDeviceName" value=")rawhtml";
    html += bfDeviceName;
    html += R"rawhtml(" placeholder="e.g. Pressure_Sensor">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Send Interval (minutes, min 15)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="bfInterval" value=")rawhtml";
    html += String(bfIntervalMinutes);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <hr class="divider">
      <div class="section-title">Custom HTTP POST</div>

      <div class="setting-group">
        <label class="setting-label">HTTP POST Enabled</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <select name="httpEnabled">
              <option value="0")rawhtml";
    html += (!httpEnabled ? " selected" : "");
    html += R"rawhtml(>Disabled</option>
              <option value="1")rawhtml";
    html += (httpEnabled ? " selected" : "");
    html += R"rawhtml(>Enabled</option>
            </select>
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Server Address (http://... or https://...)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="httpServer" value=")rawhtml";
    html += httpServer;
    html += R"rawhtml(" placeholder="e.g. http://192.168.1.100:8080">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">HTTP Path (supports {volt}, {psi}, {bar}, {temp})</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="httpPath" value=")rawhtml";
    html += httpPath;
    html += R"rawhtml(" placeholder="e.g. /api/data?volt={volt}&psi={psi}">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">JSON Body Template (optional, supports {volt}, {psi}, {bar}, {temp})</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="text" name="httpBodyTemplate" value=")rawhtml";
    html += httpBodyTemplate;
    html += R"rawhtml(" placeholder="e.g. {\"voltage\":{volt},\"pressure\":{psi}}">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

      <div class="setting-group">
        <label class="setting-label">Send Interval (seconds, min 15)</label>
        <form action="/api" method="POST">
          <div class="setting-row">
            <input type="number" name="httpInterval" value=")rawhtml";
    html += String(httpIntervalSeconds);
    html += R"rawhtml(">
            <button class="btn-set" type="submit">Set</button>
          </div>
        </form>
      </div>

    </div><!-- /panel-cloud -->
  </div><!-- /tabs-body -->

</div><!-- /container -->

<script>
function switchTab(name, btn) {
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  btn.classList.add('active');
}
)rawhtml";

    // Timer countdown script
    if (mOverride && remaining > 0) {
        html += "let rem=" + String(remaining) + ";";
        html += "const timerEl=document.getElementById('timer');";
        html += "timerEl.innerText='Manual mode: '+Math.floor(rem/1000)+'s remaining';";
        html += "const iv=setInterval(()=>{"
                "  rem-=1000;"
                "  if(rem<=0){clearInterval(iv);location.reload();}";
        html += "  else{timerEl.innerText='Manual mode: '+Math.floor(rem/1000)+'s remaining';}";
        html += "},1000);";
    }

    html += R"rawhtml(</script>
</body>
</html>)rawhtml";

    return html;
}

#endif // HTML_TEMPLATE_H
