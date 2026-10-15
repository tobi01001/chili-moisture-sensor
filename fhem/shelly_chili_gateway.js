// Shelly Gen2+ script (e.g. Plus Plug S): BLE gateway chili sensor (BTHome v2) -> FHEM.
// Settings: Bluetooth enabled, script set to "run on startup".
// FHEM side: define WEBchili FHEMWEB 8084 global ; attr WEBchili csrfToken none (LAN only!)
var FHEM = "http://192.168.1.10:8084/fhem";  // adjust
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
    else break;
  }
  return r;
}

BLE.Scanner.Subscribe(function (ev, res) {
  if (ev !== BLE.Scanner.SCAN_RESULT || !res.local_name) return;
  var name = res.local_name;
  if (name.indexOf("Chili_") !== 0) return;
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
  if (r.moist === undefined || last[name] === r.pid) return;
  last[name] = r.pid;
  var p = "moisture " + r.moist + ": supply_mv " + r.mv + ": usb " + r.usb + ": water " + r.water;
  var cmd = '{handleChiliSensor("' + name + '", "' + p + '")}';
  Shelly.call("HTTP.GET", { url: FHEM + "?XHR=1&cmd=" + enc(cmd), timeout: 5 }, function (x, ec) {
    if (ec) print("FHEM error", ec);
  });
});

BLE.Scanner.Start({ duration_ms: BLE.Scanner.INFINITE_SCAN, active: false });
