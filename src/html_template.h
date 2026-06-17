#ifndef HTML_TEMPLATE_H
#define HTML_TEMPLATE_H

#include <Arduino.h>
#include <WiFi.h>

String getHtml(float pPsi, float pBar, float v, bool mOverride, bool mOn, unsigned long mStart, float maxPressureThreshold, int pressureUnit, float hysteresis, unsigned long sensorInterval, unsigned long tsIntervalSeconds, unsigned long bfIntervalMinutes, float offsetVoltage) {
    String html = "<html><body>";
    html += "<h1>Pressure Sensor (" + String(HOSTNAME) + ")</h1>";
    html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Pressure: " + String(pPsi, 2) + " PSI (" + String(pBar, 2) + " Bar)</p>";
    html += "<p>Valve: <span id='valveStatus'>" + String(mOverride ? (mOn ? "Manual Open" : "Manual Closed") : "Auto") + "</span></p>";
    
    long remaining = 0;
    if (mOverride) {
        remaining = 10000L - (long)(millis() - mStart);
        if (remaining < 0) remaining = 0;
    }

    html += "<p id='timer'>" + String(mOverride && remaining > 0 ? "Manual mode time remaining: " + String(remaining / 1000) + " s" : "") + "</p>";
    
    html += "<script>";
    if (mOverride && remaining > 0) {
        html += "let remaining = " + String(remaining) + ";";
        html += "const timerEl = document.getElementById('timer');";
        html += "const interval = setInterval(() => {";
        html += "  remaining -= 1000;";
        html += "  if (remaining <= 0) {";
        html += "    clearInterval(interval);";
        html += "    location.reload();";
        html += "  } else {";
        html += "    timerEl.innerText = 'Manual mode time remaining: ' + Math.floor(remaining / 1000) + ' s';";
        html += "  }";
        html += "}, 1000);";
    }
    html += "</script>";

    html += "<form action='/api' method='POST'>";
    html += "<input type='hidden' name='cmd' value='manual_on'>";
    html += "<input type='submit' value='Manual Open'></form>";
    html += "<form action='/api' method='POST'>";
    html += "<input type='hidden' name='cmd' value='manual_off'>";
    html += "<input type='submit' value='Manual Close/Auto'></form><br>";

    html += "<p>Settings:</p>";
    html += "<form action='/api' method='POST'>";
    html += "Max Pressure Threshold (PSI): <input type='number' step='0.1' name='pressure' value='" + String(maxPressureThreshold, 1) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "Pressure Unit: <select name='pUnit'><option value='0'" + String(pressureUnit == 0 ? " selected" : "") + ">PSI</option><option value='1'" + String(pressureUnit == 1 ? " selected" : "") + ">Bar</option></select> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "Hysteresis (PSI): <input type='number' step='0.1' name='hysteresis' value='" + String(hysteresis, 1) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "Sensor Interval (ms): <input type='number' name='sInterval' value='" + String(sensorInterval) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "ThingSpeak Interval (s, min 15): <input type='number' name='tsInterval' value='" + String(tsIntervalSeconds) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "Brewfather Interval (min, min 15): <input type='number' name='bfInterval' value='" + String(bfIntervalMinutes) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "<form action='/api' method='POST'>";
    html += "Voltage Offset (V): <input type='number' step='0.001' name='offset' value='" + String(offsetVoltage, 3) + "'> <input type='submit' value='Set'><br>";
    html += "</form>";
    html += "</body></html>";
    return html;
}

#endif
