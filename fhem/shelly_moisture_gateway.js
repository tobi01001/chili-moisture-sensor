// Shelly Gen2+ script (e.g. Plus Plug S): BLE gateway moisture sensor (BTHome v2) -> FHEM.
// Settings: Bluetooth enabled, script set to "run on startup".
// FHEM side: define WEBmoisture FHEMWEB 8084 global ; attr WEBmoisture csrfToken none (LAN only!)
var FHEM = "http://192.168.2.18:8087/fhem";  // adjust
var last = {};  // name -> last packet id (the sensor repeats each packet)

function enc(s) {
  var o = "", ok = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
  for (var i = 0; i < s.length; i++) {
    var c = s.charAt(i);
    if (ok.indexOf(c) >= 0) o += c;
    else { var h = s.charCodeAt(i).toString(16); o += "%" + (h.length < 2 ? "0" : "") + h; }
  }
  return o;
}

function parse(d, name) {  // d: BTHome payload after UUID, starting at device info
  var r = {}, i = 1;
  while (i < d.length) {
    var id = d.charCodeAt(i);
    if (id === 0x00) { r.pid = d.charCodeAt(i + 1); i += 2; }
    else if (id === 0x01) { r.batt = d.charCodeAt(i + 1); i += 2; }
    else if (id === 0x0C) { r.mv = d.charCodeAt(i + 1) + 256 * d.charCodeAt(i + 2); i += 3; }
    else if (id === 0x0F) { r.water = d.charCodeAt(i + 1); i += 2; }
    else if (id === 0x10) { r.usb = d.charCodeAt(i + 1); i += 2; }
    else if (id === 0x2F) { r.moist = d.charCodeAt(i + 1); i += 2; }
    else if (id === 0x3D) { r.raw = d.charCodeAt(i + 1) + 256 * d.charCodeAt(i + 2); i += 3; }
    else break;
  }
  return r;
}

BLE.Scanner.Subscribe(function (ev, res) {
  // The sensor advertises only the CRC as local name (the prefix is implied).
  if (ev !== BLE.Scanner.SCAN_RESULT || !res.local_name) return;
  var crc = res.local_name;
  if (crc.length < 1 || crc.length > 5) return;
  for (var k = 0; k < crc.length; k++) if ("0123456789".indexOf(crc.charAt(k)) < 0) return;
  var device = "Moisture_" + crc;
  var ad = res.advData, i = 0, d = null;
  while (i + 1 < ad.length) {
    var len = ad.charCodeAt(i);
    if (len === 0) break;
    if (ad.charCodeAt(i + 1) === 0x16 && ad.charCodeAt(i + 2) === 0xD2 && ad.charCodeAt(i + 3) === 0xFC)
      d = ad.slice(i + 4, i + 1 + len);
    i += len + 1;
  }
  if (d === null) return;
  var r = parse(d);
  if (r.moist === undefined || last[device] === r.pid) return;
  last[device] = r.pid;
  var p = "moi " + r.moist + ": raw " + r.raw + ": sup " + r.mv + ": usb " + r.usb + ": wtr " + r.water;
  var cmd = '{handleMoistureSensor("' + device + '", "' + p + '")}';
  Shelly.call("HTTP.GET", { url: FHEM + "?XHR=1&cmd=" + enc(cmd), timeout: 5 }, function (x, ec) {
    if (ec) print("FHEM error", ec);
  });
});

BLE.Scanner.Start({ duration_ms: BLE.Scanner.INFINITE_SCAN, active: false });
