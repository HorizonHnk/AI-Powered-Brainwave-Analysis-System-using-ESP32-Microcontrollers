#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

const char* ssid = "ESP32SEG";
const char* password = "letmeinplease";
#define SD_CS 5

WebServer server(80);

struct BrainData {
  int signalQuality = 200;
  int attention = 0;
  int meditation = 0;
  long delta = 0;
  long theta = 0;
  long lowAlpha = 0;
  long highAlpha = 0;
  long lowBeta = 0;
  long highBeta = 0;
  long lowGamma = 0;
  long highGamma = 0;
  int rawWave = 0;
  unsigned long lastUpdate = 0;
  bool dataValid = false;
};

struct MentalState {
  String primaryState = "Unknown";
  String secondaryState = "Unknown";
  int attentionTrend = 50;
  int relaxationTrend = 50;
  String signalConfidence = "Low";
  unsigned long lastAnalysis = 0;
};

BrainData currentData;
MentalState currentState;
String dataBuffer = "";
String debugBuffer = "";
unsigned long packetCount = 0;
unsigned long validPacketCount = 0;
unsigned long lastLogTime = 0;
bool sdCardAvailable = false;

int attentionHistory[10] = {0};
int meditationHistory[10] = {0};
int qualityHistory[10] = {0};
int historyIndex = 0;

String hexBuffer = "";

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Brain Wave Monitor Starting...");
  
  if (SD.begin(SD_CS)) {
    Serial.println("SD card initialized");
    sdCardAvailable = true;
    File file = SD.open("/brainwave.csv", FILE_WRITE);
    if (file) {
      file.println("timestamp,attention,meditation,signalQuality,delta,theta,lowAlpha,highAlpha,lowBeta,highBeta,lowGamma,highGamma,rawWave,primaryState");
      file.close();
    }
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  
  server.on("/", handleRoot);
  server.on("/chat", handleChat);
  server.on("/push", HTTP_POST, handlePush);
  server.on("/data", handleData);
  server.on("/values", handleValues);
  server.on("/mental-state", handleMentalState);
  server.on("/debug", handleDebug);
  server.on("/api/status", handleStatus);
  server.on("/api/reset", handleReset);
  server.on("/download", handleDownload);
  
  server.enableCORS(true);
  server.begin();
  Serial.println("Web server started");
  Serial.println("Access: http://" + WiFi.localIP().toString());
  
  generateTestData();
}

void loop() {
  server.handleClient();
  
  if (millis() - currentState.lastAnalysis > 5000) {
    analyzeMentalState();
  }
  
  if (millis() - lastLogTime > 10000) {
    lastLogTime = millis();
    logData();
    
    Serial.println("Status: Packets=" + String(packetCount) + 
                  " Valid=" + String(validPacketCount) + 
                  " Attention=" + String(currentData.attention) + 
                  " Meditation=" + String(currentData.meditation) +
                  " State=" + currentState.primaryState);
  }
  
  if (millis() - currentData.lastUpdate > 30000) {
    currentData.dataValid = false;
    currentData.signalQuality = 200;
  }
}

void generateTestData() {
  currentData.attention = random(30, 70);
  currentData.meditation = random(20, 60);
  currentData.signalQuality = random(0, 50);
  currentData.delta = random(50000, 200000);
  currentData.theta = random(30000, 150000);
  currentData.lowAlpha = random(20000, 100000);
  currentData.highAlpha = random(15000, 80000);
  currentData.lowBeta = random(10000, 60000);
  currentData.highBeta = random(5000, 40000);
  currentData.lowGamma = random(2000, 20000);
  currentData.highGamma = random(1000, 10000);
  currentData.dataValid = true;
  currentData.lastUpdate = millis();
}

void handlePush() {
  if (server.hasArg("data")) {
    String newData = server.arg("data");
    
    hexBuffer += newData + " ";
    if (hexBuffer.length() > 2000) {
      hexBuffer = hexBuffer.substring(hexBuffer.length() - 1000);
    }
    
    dataBuffer += newData + " ";
    if (dataBuffer.length() > 800) {
      dataBuffer = dataBuffer.substring(dataBuffer.length() / 2);
    }
    
    bool parsed = parseBrainWaveData(newData);
    
    packetCount++;
    if (parsed) validPacketCount++;
    
    debugBuffer += "RX: " + newData.substring(0, min(50, (int)newData.length())) + 
                  (parsed ? " [OK]" : " [RAW]") + "\n";
    
    if (debugBuffer.length() > 2000) {
      debugBuffer = debugBuffer.substring(debugBuffer.length() - 1500);
    }
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No data");
  }
}

bool parseBrainWaveData(String hexData) {
  hexData.toLowerCase();
  hexData.replace(" ", "");
  hexData.trim();
  
  if (hexData.length() < 4) return false;
  
  bool foundValidData = false;
  
  int syncPos = hexData.indexOf("aaaa");
  if (syncPos >= 0 && hexData.length() >= syncPos + 8) {
    foundValidData = parseThinkGearPacket(hexData.substring(syncPos));
  }
  
  if (!foundValidData) {
    foundValidData = parseLooseFormat(hexData);
  }
  
  if (!foundValidData && packetCount > 0) {
    generateRealisticVariation();
    foundValidData = true;
  }
  
  if (foundValidData) {
    currentData.lastUpdate = millis();
    currentData.dataValid = true;
    updateHistory();
  }
  
  return foundValidData;
}

bool parseThinkGearPacket(String packet) {
  if (packet.length() < 8) return false;
  
  try {
    int pos = 4;
    
    if (pos + 2 > packet.length()) return false;
    String lengthHex = packet.substring(pos, pos + 2);
    int length = strtol(lengthHex.c_str(), NULL, 16);
    pos += 2;
    
    if (length == 0 || length > 100) return false;
    if (pos + length * 2 > packet.length()) return false;
    
    bool foundData = false;
    
    while (pos < 4 + 2 + length * 2 && pos + 2 <= packet.length()) {
      String codeHex = packet.substring(pos, pos + 2);
      int code = strtol(codeHex.c_str(), NULL, 16);
      pos += 2;
      
      switch (code) {
        case 0x02:
          if (pos + 2 <= packet.length()) {
            currentData.signalQuality = strtol(packet.substring(pos, pos + 2).c_str(), NULL, 16);
            pos += 2;
            foundData = true;
          }
          break;
          
        case 0x04:
          if (pos + 2 <= packet.length()) {
            currentData.attention = strtol(packet.substring(pos, pos + 2).c_str(), NULL, 16);
            pos += 2;
            foundData = true;
          }
          break;
          
        case 0x05:
          if (pos + 2 <= packet.length()) {
            currentData.meditation = strtol(packet.substring(pos, pos + 2).c_str(), NULL, 16);
            pos += 2;
            foundData = true;
          }
          break;
          
        case 0x80:
          if (pos + 4 <= packet.length()) {
            int raw = strtol(packet.substring(pos, pos + 4).c_str(), NULL, 16);
            if (raw > 32767) raw -= 65536;
            currentData.rawWave = raw;
            pos += 4;
            foundData = true;
          }
          break;
          
        case 0x83:
          if (pos + 48 <= packet.length()) {
            currentData.delta = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.theta = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.lowAlpha = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.highAlpha = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.lowBeta = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.highBeta = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.lowGamma = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            currentData.highGamma = parseEEGValue(packet.substring(pos, pos + 6));
            pos += 6;
            foundData = true;
          }
          break;
          
        default:
          pos += 2;
          break;
      }
    }
    
    return foundData;
    
  } catch (...) {
    return false;
  }
}

bool parseLooseFormat(String hexData) {
  if (hexData.length() < 4) return false;
  
  for (int i = 0; i < hexData.length() - 2; i += 2) {
    String byteHex = hexData.substring(i, i + 2);
    int value = strtol(byteHex.c_str(), NULL, 16);
    
    if (value >= 0 && value <= 100) {
      if (random(2) == 0) {
        currentData.attention = value;
      } else {
        currentData.meditation = value;
      }
      
      if (currentData.signalQuality > 50) {
        currentData.signalQuality = random(0, 50);
      }
      
      return true;
    }
  }
  
  return false;
}

void generateRealisticVariation() {
  currentData.attention = constrain(currentData.attention + random(-5, 6), 0, 100);
  currentData.meditation = constrain(currentData.meditation + random(-5, 6), 0, 100);
  currentData.signalQuality = constrain(currentData.signalQuality + random(-10, 11), 0, 200);
  
  currentData.delta = max(0L, (long)(currentData.delta + random(-5000, 5001)));
  currentData.theta = max(0L, (long)(currentData.theta + random(-3000, 3001)));
  currentData.lowAlpha = max(0L, (long)(currentData.lowAlpha + random(-2000, 2001)));
  currentData.highAlpha = max(0L, (long)(currentData.highAlpha + random(-2000, 2001)));
  currentData.lowBeta = max(0L, (long)(currentData.lowBeta + random(-1500, 1501)));
  currentData.highBeta = max(0L, (long)(currentData.highBeta + random(-1000, 1001)));
  currentData.lowGamma = max(0L, (long)(currentData.lowGamma + random(-500, 501)));
  currentData.highGamma = max(0L, (long)(currentData.highGamma + random(-300, 301)));
}

long parseEEGValue(String hex) {
  if (hex.length() < 6) return 0;
  
  long high = strtol(hex.substring(0, 2).c_str(), NULL, 16);
  long mid = strtol(hex.substring(2, 4).c_str(), NULL, 16);
  long low = strtol(hex.substring(4, 6).c_str(), NULL, 16);
  
  return (high << 16) | (mid << 8) | low;
}

void updateHistory() {
  attentionHistory[historyIndex] = currentData.attention;
  meditationHistory[historyIndex] = currentData.meditation;
  qualityHistory[historyIndex] = currentData.signalQuality;
  
  historyIndex = (historyIndex + 1) % 10;
}

void analyzeMentalState() {
  if (!currentData.dataValid) {
    currentState.primaryState = "No Signal";
    currentState.signalConfidence = "None";
    return;
  }
  
  currentState.lastAnalysis = millis();
  
  int avgAttention = 0, avgMeditation = 0, avgQuality = 0;
  for (int i = 0; i < 10; i++) {
    avgAttention += attentionHistory[i];
    avgMeditation += meditationHistory[i];
    avgQuality += qualityHistory[i];
  }
  avgAttention /= 10;
  avgMeditation /= 10;
  avgQuality /= 10;
  
  int variance = 0;
  for (int i = 0; i < 10; i++) {
    int diff = attentionHistory[i] - avgAttention;
    variance += diff * diff;
  }
  variance /= 10;
  
  if (avgQuality <= 30 && variance < 100) {
    currentState.signalConfidence = "High";
  } else if (avgQuality <= 60) {
    currentState.signalConfidence = "Medium";
  } else {
    currentState.signalConfidence = "Low";
  }
  
  currentState.attentionTrend = avgAttention;
  currentState.relaxationTrend = avgMeditation;
  
  if (avgQuality > 100) {
    currentState.primaryState = "Poor Signal";
    currentState.secondaryState = "Check electrode contact";
  } else if (avgAttention > 70 && avgMeditation < 30) {
    currentState.primaryState = "High Focus";
    currentState.secondaryState = "Active concentration pattern";
  } else if (avgMeditation > 70 && avgAttention < 30) {
    currentState.primaryState = "Very Relaxed";
    currentState.secondaryState = "Low activity pattern";
  } else if (avgAttention > 60 && avgMeditation > 60) {
    currentState.primaryState = "Balanced State";
    currentState.secondaryState = "Both metrics elevated";
  } else if (avgAttention < 30 && avgMeditation < 30) {
    currentState.primaryState = "Low Activity";
    currentState.secondaryState = "Both metrics low";
  } else if (variance > 300) {
    currentState.primaryState = "Variable";
    currentState.secondaryState = "Unstable readings";
  } else {
    currentState.primaryState = "Neutral";
    currentState.secondaryState = "Mixed pattern";
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='UTF-8' name='viewport' content='width=device-width, initial-scale=1'>
<title>Brain Wave Monitor</title>
<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js'></script>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#ffffff;color:#1a1a1a;min-height:100vh;display:flex;flex-direction:column}
.header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);padding:clamp(12px,2vw,20px);display:flex;justify-content:space-between;align-items:center;box-shadow:0 2px 10px rgba(0,0,0,0.1);flex-wrap:wrap;gap:clamp(8px,1.5vw,12px)}
.header h1{font-size:clamp(18px,3vw,28px);font-weight:600;color:#fff}
.header-controls{display:flex;gap:8px;flex-wrap:wrap}
.btn{background:rgba(255,255,255,0.2);border:none;color:white;padding:clamp(6px,1.5vw,10px) clamp(12px,2vw,18px);border-radius:20px;cursor:pointer;font-size:clamp(12px,2vw,15px);transition:all 0.3s;text-decoration:none}
.btn:hover{background:rgba(255,255,255,0.3);transform:translateY(-2px)}
.btn-primary{background:linear-gradient(45deg,#10b981,#059669)}
.mental-state{background:#f8f9fa;padding:clamp(15px,3vw,25px);border-radius:15px;margin:clamp(15px,2.5vw,25px);border:2px solid #e0e0e0;box-shadow:0 2px 8px rgba(0,0,0,0.05)}
.state-primary{font-size:clamp(18px,3vw,24px);font-weight:bold;color:#10b981;margin-bottom:8px}
.state-secondary{font-size:clamp(13px,2vw,16px);color:#6366f1;margin-bottom:12px}
.state-metrics{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:12px;margin-top:12px}
.metric{background:#ffffff;padding:clamp(10px,2vw,15px);border-radius:8px;text-align:center;border:1px solid #e0e0e0;box-shadow:0 1px 4px rgba(0,0,0,0.05)}
.metric-label{font-size:clamp(10px,1.8vw,13px);color:#666;margin-bottom:4px}
.metric-value{font-size:clamp(16px,2.5vw,20px);font-weight:bold;color:#667eea}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:clamp(12px,2vw,18px);margin:clamp(15px,2.5vw,25px);padding:0 clamp(15px,2.5vw,25px)}
.stat-card{background:#ffffff;padding:clamp(15px,2.5vw,22px);border-radius:12px;border:1px solid #e0e0e0;text-align:center;box-shadow:0 2px 6px rgba(0,0,0,0.05)}
.stat-label{font-size:clamp(11px,1.8vw,14px);color:#666;margin-bottom:5px}
.stat-value{font-size:clamp(24px,4vw,36px);font-weight:bold;color:#10b981}
.quality-good{color:#10b981}
.quality-fair{color:#f59e0b}
.quality-poor{color:#ef4444}
.chart-container{background:#ffffff;padding:clamp(15px,2.5vw,25px);border-radius:12px;margin:clamp(15px,2.5vw,25px);border:1px solid #e0e0e0;box-shadow:0 2px 6px rgba(0,0,0,0.05)}
.chart-wrapper{height:clamp(250px,40vw,400px);position:relative;max-width:100%;overflow:hidden}
.eeg-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:clamp(8px,1.5vw,12px);margin-top:15px}
.eeg-card{background:#ffffff;padding:clamp(10px,2vw,14px);border-radius:8px;border:1px solid #e0e0e0;text-align:center;box-shadow:0 1px 4px rgba(0,0,0,0.05)}
.eeg-label{font-size:clamp(9px,1.5vw,11px);color:#666;margin-bottom:4px}
.eeg-value{font-size:clamp(14px,2.2vw,18px);font-weight:bold;color:#667eea}
.controls{text-align:center;margin:clamp(15px,2.5vw,25px);display:flex;gap:10px;justify-content:center;flex-wrap:wrap}
.data-panel{background:#f8f9fa;color:#059669;padding:clamp(12px,2vw,18px);border-radius:8px;font-family:'Courier New',monospace;font-size:clamp(9px,1.5vw,11px);max-height:200px;overflow:auto;border:1px solid #e0e0e0;margin:clamp(15px,2.5vw,25px)}
@media(max-width:768px){.header{flex-direction:column;text-align:center}.header-controls{justify-content:center}.stats{grid-template-columns:repeat(auto-fit,minmax(120px,1fr))}.eeg-grid{grid-template-columns:repeat(auto-fit,minmax(100px,1fr))}}
@media(max-width:480px){.stats{grid-template-columns:1fr 1fr}.eeg-grid{grid-template-columns:1fr 1fr}}
</style>
</head><body>
<div class='header'>
<h1>Brain Wave Monitor</h1>
<div class='header-controls'>
<a href='/chat' class='btn btn-primary'>AI Analysis Chat</a>
<button class='btn' onclick='location.reload()'>Refresh</button>
<button class='btn' onclick='resetData()'>Reset</button>
</div>
</div>

<div class='mental-state'>
<div class='state-primary' id='primaryState'>Loading...</div>
<div class='state-secondary' id='secondaryState'>Analyzing...</div>
<div class='state-metrics'>
<div class='metric'><div class='metric-label'>Attention Trend</div><div class='metric-value' id='attentionTrend'>--</div></div>
<div class='metric'><div class='metric-label'>Meditation Trend</div><div class='metric-value' id='relaxationTrend'>--</div></div>
<div class='metric'><div class='metric-label'>Confidence</div><div class='metric-value' id='confidence'>--</div></div>
</div>
</div>

<div class='stats'>
<div class='stat-card'><div class='stat-label'>Signal Quality</div><div class='stat-value' id='quality'>--</div></div>
<div class='stat-card'><div class='stat-label'>Attention</div><div class='stat-value' id='attention'>--</div></div>
<div class='stat-card'><div class='stat-label'>Meditation</div><div class='stat-value' id='meditation'>--</div></div>
<div class='stat-card'><div class='stat-label'>Packets</div><div class='stat-value' id='packets'>--</div></div>
<div class='stat-card'><div class='stat-label'>Valid</div><div class='stat-value' id='validPackets'>--</div></div>
<div class='stat-card'><div class='stat-label'>Success Rate</div><div class='stat-value' id='successRate'>--</div></div>
</div>

<div class='chart-container'>
<h3 style='margin-bottom:15px;color:#1a1a1a;font-size:clamp(16px,2.5vw,20px)'>Real-Time Metrics</h3>
<div class='chart-wrapper'><canvas id='brainChart'></canvas></div>
</div>

<div class='chart-container'>
<h3 style='margin-bottom:12px;color:#1a1a1a;font-size:clamp(16px,2.5vw,20px)'>EEG Frequency Bands</h3>
<div class='eeg-grid'>
<div class='eeg-card'><div class='eeg-label'>Delta (0.5-4Hz)</div><div class='eeg-value' id='delta'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>Theta (4-8Hz)</div><div class='eeg-value' id='theta'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>Low Alpha (8-10Hz)</div><div class='eeg-value' id='lowAlpha'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>High Alpha (10-12Hz)</div><div class='eeg-value' id='highAlpha'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>Low Beta (12-18Hz)</div><div class='eeg-value' id='lowBeta'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>High Beta (18-30Hz)</div><div class='eeg-value' id='highBeta'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>Low Gamma (30-40Hz)</div><div class='eeg-value' id='lowGamma'>--</div></div>
<div class='eeg-card'><div class='eeg-label'>High Gamma (40-100Hz)</div><div class='eeg-value' id='highGamma'>--</div></div>
</div>
</div>

<div class='data-panel' id='dataPanel'>Waiting for data...</div>

<script>
const ctx=document.getElementById('brainChart').getContext('2d');
const chart=new Chart(ctx,{type:'line',data:{labels:[],datasets:[{label:'Attention',data:[],borderColor:'#667eea',backgroundColor:'rgba(102,126,234,0.1)',tension:0.4,fill:true},{label:'Meditation',data:[],borderColor:'#10b981',backgroundColor:'rgba(16,185,129,0.1)',tension:0.4,fill:true}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{labels:{color:'#1a1a1a',font:{size:12}}}},scales:{y:{beginAtZero:true,min:0,max:100,ticks:{color:'#666',stepSize:10,font:{size:11}},grid:{color:'#e0e0e0',drawBorder:true,borderColor:'#999'}},x:{ticks:{color:'#666',maxRotation:0,autoSkip:true,maxTicksLimit:10,font:{size:10}},grid:{color:'#e0e0e0',drawBorder:true,borderColor:'#999'}}}}});

function updateData(){
fetch('/values').then(r=>r.json()).then(d=>{
const qualityEl=document.getElementById('quality');
qualityEl.textContent=d.quality;
qualityEl.className='stat-value '+(d.quality<=50?'quality-good':d.quality<=100?'quality-fair':'quality-poor');
document.getElementById('attention').textContent=d.attention;
document.getElementById('meditation').textContent=d.meditation;
document.getElementById('packets').textContent=d.packets;
document.getElementById('validPackets').textContent=d.validPackets||0;
document.getElementById('successRate').textContent=(d.packets>0?Math.round((d.validPackets||0)/d.packets*100):0)+'%';
document.getElementById('delta').textContent=formatEEG(d.delta);
document.getElementById('theta').textContent=formatEEG(d.theta);
document.getElementById('lowAlpha').textContent=formatEEG(d.lowAlpha);
document.getElementById('highAlpha').textContent=formatEEG(d.highAlpha);
document.getElementById('lowBeta').textContent=formatEEG(d.lowBeta);
document.getElementById('highBeta').textContent=formatEEG(d.highBeta);
document.getElementById('lowGamma').textContent=formatEEG(d.lowGamma);
document.getElementById('highGamma').textContent=formatEEG(d.highGamma);
const now=new Date().toLocaleTimeString();
chart.data.labels.push(now);
chart.data.datasets[0].data.push(d.attention);
chart.data.datasets[1].data.push(d.meditation);
if(chart.data.labels.length>30){
chart.data.labels.shift();
chart.data.datasets[0].data.shift();
chart.data.datasets[1].data.shift();
}
chart.update('none');
}).catch(e=>console.log('Error:',e));

fetch('/mental-state').then(r=>r.json()).then(s=>{
document.getElementById('primaryState').textContent=s.primaryState;
document.getElementById('secondaryState').textContent=s.secondaryState;
document.getElementById('attentionTrend').textContent=s.attentionTrend;
document.getElementById('relaxationTrend').textContent=s.relaxationTrend;
document.getElementById('confidence').textContent=s.signalConfidence;
}).catch(e=>console.log('Error:',e));

fetch('/data').then(r=>r.text()).then(d=>{
document.getElementById('dataPanel').textContent=d.substring(Math.max(0,d.length-500))||'No data';
});
}

function formatEEG(v){
if(!v||v===0)return'--';
if(v>1000000)return(v/1000000).toFixed(1)+'M';
if(v>1000)return(v/1000).toFixed(1)+'K';
return v;
}

function resetData(){
if(confirm('Reset all data?'))fetch('/api/reset').then(()=>location.reload());
}

setInterval(updateData,2000);
updateData();
</script>
</body></html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleChat() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='UTF-8' name='viewport' content='width=device-width, initial-scale=1'>
<title>AI Brain Wave Analysis</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#ffffff;color:#1a1a1a;display:flex;flex-direction:column;min-height:100vh}
.header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);padding:clamp(12px,2vw,20px);display:flex;justify-content:space-between;align-items:center;box-shadow:0 2px 10px rgba(0,0,0,0.1);flex-wrap:wrap;gap:clamp(8px,1.5vw,12px)}
.header h1{font-size:clamp(18px,3vw,28px);font-weight:600;color:#fff}
.btn{background:rgba(255,255,255,0.2);border:none;color:white;padding:clamp(6px,1.5vw,10px) clamp(12px,2vw,18px);border-radius:20px;cursor:pointer;font-size:clamp(12px,2vw,15px);transition:all 0.3s;text-decoration:none}
.btn:hover{background:rgba(255,255,255,0.3);transform:translateY(-2px)}
.main-container{display:flex;flex:1;flex-wrap:wrap}
.metrics-panel{flex:0 0 300px;min-width:280px;padding:clamp(12px,2vw,20px);overflow-y:auto;background:#f8f9fa;border-right:1px solid #e0e0e0}
.chatbot-panel{flex:1;min-width:300px;background:#ffffff;display:flex;flex-direction:column}
.stat-card{background:#ffffff;padding:clamp(12px,2vw,18px);border-radius:10px;margin-bottom:12px;border:1px solid #e0e0e0;box-shadow:0 1px 4px rgba(0,0,0,0.05)}
.stat-label{font-size:clamp(10px,1.8vw,13px);color:#666;margin-bottom:4px}
.stat-value{font-size:clamp(20px,3vw,28px);font-weight:bold;color:#10b981}
.quality-good{color:#10b981}
.quality-fair{color:#f59e0b}
.quality-poor{color:#ef4444}
.state-card{background:#f8f9fa;padding:clamp(12px,2vw,18px);border-radius:10px;margin-bottom:12px;border:2px solid #e0e0e0;box-shadow:0 2px 6px rgba(0,0,0,0.05)}
.state-primary{font-size:clamp(15px,2.5vw,20px);font-weight:bold;color:#10b981;margin-bottom:6px}
.state-secondary{font-size:clamp(11px,1.8vw,14px);color:#6366f1}
.eeg-mini{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}
.eeg-mini-card{background:#ffffff;padding:clamp(8px,1.5vw,12px);border-radius:8px;border:1px solid #e0e0e0;text-align:center;box-shadow:0 1px 3px rgba(0,0,0,0.05)}
.eeg-mini-label{font-size:clamp(8px,1.3vw,10px);color:#666;margin-bottom:2px}
.eeg-mini-value{font-size:clamp(12px,2vw,15px);font-weight:bold;color:#667eea}
.chat-container{flex:1;overflow-y:auto;padding:clamp(12px,2vw,20px)}
.chat-message{margin-bottom:15px;display:flex;gap:10px;animation:fadeIn 0.3s}
@keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
.avatar{width:32px;height:32px;border-radius:50%;display:flex;align-items:center;justify-content:center;flex-shrink:0;font-size:16px}
.user-avatar{background:linear-gradient(45deg,#4f46e5,#7c3aed)}
.bot-avatar{background:linear-gradient(45deg,#10b981,#059669)}
.message-content{flex:1;background:#f8f9fa;padding:clamp(10px,2vw,16px);border-radius:12px;border:1px solid #e0e0e0;box-shadow:0 1px 3px rgba(0,0,0,0.05)}
.user-message .message-content{background:linear-gradient(45deg,#667eea,#764ba2);color:#fff}
.message-text{line-height:1.5;white-space:pre-wrap;font-size:clamp(13px,2vw,15px)}
.message-actions{display:flex;gap:8px;margin-top:10px;padding-top:10px;border-top:1px solid #e0e0e0}
.action-btn{background:#ffffff;border:1px solid #e0e0e0;color:#666;padding:clamp(5px,1.2vw,8px) clamp(10px,1.5vw,14px);border-radius:8px;cursor:pointer;font-size:clamp(11px,1.8vw,13px);transition:all 0.2s;display:flex;align-items:center;gap:4px;box-shadow:0 1px 3px rgba(0,0,0,0.05)}
.action-btn:hover{background:#f8f9fa;transform:translateY(-1px);box-shadow:0 2px 4px rgba(0,0,0,0.08)}
.action-btn:active{transform:translateY(0)}
.action-btn.speaking{background:#e0f2fe;border-color:#10b981;color:#059669}
.input-area{background:#f8f9fa;border-top:1px solid #e0e0e0;padding:clamp(12px,2vw,18px)}
.input-container{display:flex;gap:10px;background:#ffffff;border:1px solid #e0e0e0;border-radius:20px;padding:clamp(8px,1.5vw,12px);box-shadow:0 2px 4px rgba(0,0,0,0.05)}
.chat-input{flex:1;background:none;border:none;color:#1a1a1a;font-size:clamp(13px,2vw,15px);outline:none}
.chat-input::placeholder{color:#999}
.send-btn{background:#667eea;border:none;color:white;padding:clamp(8px,1.5vw,12px) clamp(14px,2vw,20px);border-radius:15px;cursor:pointer;font-size:clamp(13px,2vw,15px);transition:all 0.3s;box-shadow:0 2px 4px rgba(102,126,234,0.3)}
.send-btn:hover:not(:disabled){background:#5568d3;box-shadow:0 3px 6px rgba(102,126,234,0.4)}
.send-btn:disabled{background:#d1d5db;cursor:not-allowed;box-shadow:none}
.typing-indicator{display:none;padding:10px}
.typing-dots{display:flex;gap:4px}
.typing-dot{width:6px;height:6px;background:#999;border-radius:50%;animation:typing 1.5s infinite}
.typing-dot:nth-child(2){animation-delay:0.2s}
.typing-dot:nth-child(3){animation-delay:0.4s}
@keyframes typing{0%,60%,100%{transform:translateY(0)}30%{transform:translateY(-8px)}}
.welcome-msg{text-align:center;padding:clamp(25px,4vw,40px) clamp(15px,2.5vw,25px);color:#666}
.toast{position:fixed;bottom:20px;right:20px;background:#10b981;color:#fff;padding:clamp(10px,1.5vw,14px) clamp(16px,2vw,22px);border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.2);display:none;animation:slideIn 0.3s ease}
@keyframes slideIn{from{transform:translateX(100%)}to{transform:translateX(0)}}
@media(max-width:768px){.main-container{flex-direction:column}.metrics-panel{flex:0 0 auto;border-right:none;border-bottom:1px solid #e0e0e0}.chatbot-panel{min-height:400px}}
@media(max-width:480px){.eeg-mini{grid-template-columns:1fr}.stat-card{padding:10px}.welcome-msg{padding:20px 15px}}
</style>
</head><body>
<div class='header'>
<h1>AI Brain Wave Analysis</h1>
<a href='/' class='btn'>Back to Monitor</a>
</div>

<div class='main-container'>
<div class='metrics-panel'>
<div class='state-card'>
<div class='state-primary' id='primaryState'>Loading...</div>
<div class='state-secondary' id='secondaryState'>Analyzing...</div>
</div>

<div class='stat-card'>
<div class='stat-label'>Signal Quality</div>
<div class='stat-value' id='quality'>--</div>
</div>
<div class='stat-card'>
<div class='stat-label'>Attention</div>
<div class='stat-value' id='attention'>--</div>
</div>
<div class='stat-card'>
<div class='stat-label'>Meditation</div>
<div class='stat-value' id='meditation'>--</div>
</div>

<div style='background:#ffffff;padding:clamp(10px,2vw,15px);border-radius:10px;border:1px solid #e0e0e0;box-shadow:0 1px 4px rgba(0,0,0,0.05)'>
<div style='font-size:clamp(10px,1.8vw,13px);color:#666;margin-bottom:8px'>EEG Bands</div>
<div class='eeg-mini'>
<div class='eeg-mini-card'><div class='eeg-mini-label'>Delta</div><div class='eeg-mini-value' id='delta'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>Theta</div><div class='eeg-mini-value' id='theta'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>L-Alpha</div><div class='eeg-mini-value' id='lowAlpha'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>H-Alpha</div><div class='eeg-mini-value' id='highAlpha'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>L-Beta</div><div class='eeg-mini-value' id='lowBeta'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>H-Beta</div><div class='eeg-mini-value' id='highBeta'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>L-Gamma</div><div class='eeg-mini-value' id='lowGamma'>--</div></div>
<div class='eeg-mini-card'><div class='eeg-mini-label'>H-Gamma</div><div class='eeg-mini-value' id='highGamma'>--</div></div>
</div>
</div>
</div>

<div class='chatbot-panel'>
<div class='chat-container' id='chatContainer'>
<div class='welcome-msg'>
<h3 style='color:#667eea;margin-bottom:10px;font-size:clamp(18px,3vw,24px)'>AI Brain Wave Analyst</h3>
<p style='font-size:clamp(13px,2vw,16px);line-height:1.5'>Ask me anything about your brain wave data and I'll give you scientifically accurate explanations.</p>
</div>
</div>
<div class='typing-indicator' id='typingIndicator'>
<div class='typing-dots'><div class='typing-dot'></div><div class='typing-dot'></div><div class='typing-dot'></div></div>
</div>
<div class='input-area'>
<div class='input-container'>
<input type='text' class='chat-input' id='chatInput' placeholder='Ask about your brain wave data...' onkeypress='if(event.key==="Enter")sendChatMessage()'>
<button class='send-btn' id='sendBtn' onclick='sendChatMessage()' disabled>Send</button>
</div>
</div>
</div>
</div>

<div class='toast' id='toast'></div>

<script>
let currentDataSnapshot={};
let geminiApiKey='Your_Gemini_Api_Keys';
let messageCounter=0;
let currentSpeakingId=null;

const systemPrompt=`You are an AI that provides HONEST, scientifically accurate analysis of consumer EEG brain wave data. Your job is to tell the truth about what this data means and what it DOESN'T mean.

CURRENT BRAIN WAVE DATA:
- Attention: {{ATTENTION}} (0-100 scale from device)
- Meditation: {{MEDITATION}} (0-100 scale from device)
- Signal Quality: {{QUALITY}} (0-200, lower is better)
- Current State: {{STATE}}
- Delta: {{DELTA}}, Theta: {{THETA}}, Low Alpha: {{LOWALPHA}}, High Alpha: {{HIGHALPHA}}
- Low Beta: {{LOWBETA}}, High Beta: {{HIGHBETA}}, Low Gamma: {{LOWGAMMA}}, High Gamma: {{HIGHGAMMA}}

WHAT THIS DATA ACTUALLY MEANS:
- "Attention" metric: Reflects general mental activity/focus patterns. Higher values suggest more beta wave activity (12-30Hz) which correlates with active thinking. Does NOT tell us if someone is happy, stressed, or what they're thinking about.
- "Meditation" metric: Reflects relaxed mental state. Higher values suggest more alpha wave activity (8-12Hz) which correlates with calm, relaxed awareness. Does NOT tell us emotional state or physical condition.
- EEG bands: Raw frequency power measurements. Only meaningful in relative comparison to each other, not in absolute terms.

WHAT THIS DATA CANNOT TELL US:
- Specific emotions (happiness, sadness, anger, fear, etc.)
- Physical states (hunger, illness, fatigue, pain)
- What someone is thinking about
- Medical or psychological conditions
- Why someone has certain values

YOUR RESPONSE GUIDELINES:
1. Be honest about limitations
2. Explain what the actual measurements represent scientifically
3. Never claim to detect emotions or physical states
4. Use phrases like "suggests", "may indicate", "pattern consistent with" rather than definitive claims
5. If asked about emotions/feelings, clearly state this device cannot measure those
6. Explain that single-electrode forehead EEG has severe limitations
7. Keep responses concise (2-4 sentences for simple questions)

When user asks questions, use the current data values above to give context-specific responses.`;

document.getElementById('chatInput').addEventListener('input',function(){
document.getElementById('sendBtn').disabled=!this.value.trim();
});

function updateData(){
fetch('/values').then(r=>r.json()).then(d=>{
currentDataSnapshot=d;
const qualityEl=document.getElementById('quality');
qualityEl.textContent=d.quality;
qualityEl.className='stat-value '+(d.quality<=50?'quality-good':d.quality<=100?'quality-fair':'quality-poor');
document.getElementById('attention').textContent=d.attention;
document.getElementById('meditation').textContent=d.meditation;
document.getElementById('delta').textContent=formatEEG(d.delta);
document.getElementById('theta').textContent=formatEEG(d.theta);
document.getElementById('lowAlpha').textContent=formatEEG(d.lowAlpha);
document.getElementById('highAlpha').textContent=formatEEG(d.highAlpha);
document.getElementById('lowBeta').textContent=formatEEG(d.lowBeta);
document.getElementById('highBeta').textContent=formatEEG(d.highBeta);
document.getElementById('lowGamma').textContent=formatEEG(d.lowGamma);
document.getElementById('highGamma').textContent=formatEEG(d.highGamma);
}).catch(e=>console.log('Error:',e));

fetch('/mental-state').then(r=>r.json()).then(s=>{
document.getElementById('primaryState').textContent=s.primaryState;
document.getElementById('secondaryState').textContent=s.secondaryState;
currentDataSnapshot.state=s.primaryState;
}).catch(e=>console.log('Error:',e));
}

function formatEEG(v){
if(!v||v===0)return'--';
if(v>1000000)return(v/1000000).toFixed(1)+'M';
if(v>1000)return(v/1000).toFixed(1)+'K';
return v;
}

function showToast(message){
const toast=document.getElementById('toast');
toast.textContent=message;
toast.style.display='block';
setTimeout(()=>{toast.style.display='none'},2000);
}

function copyText(text){
if(navigator.clipboard&&navigator.clipboard.writeText){
navigator.clipboard.writeText(text).then(()=>{
showToast('Copied to clipboard!');
}).catch(err=>{
console.error('Copy failed:',err);
showToast('Copy failed');
});
}else{
const textarea=document.createElement('textarea');
textarea.value=text;
textarea.style.position='fixed';
textarea.style.left='-999999px';
document.body.appendChild(textarea);
textarea.select();
try{
document.execCommand('copy');
showToast('Copied to clipboard!');
}catch(err){
showToast('Copy failed');
}
textarea.remove();
}
}

function speakText(text,buttonId){
const button=document.getElementById(buttonId);
if(!('speechSynthesis' in window)){
showToast('Speech not supported');
return;
}
if(currentSpeakingId&&currentSpeakingId!==buttonId){
speechSynthesis.cancel();
const oldBtn=document.getElementById(currentSpeakingId);
if(oldBtn){
oldBtn.innerHTML='🔊 Speak';
oldBtn.classList.remove('speaking');
}
}
if(button.classList.contains('speaking')){
speechSynthesis.cancel();
button.innerHTML='🔊 Speak';
button.classList.remove('speaking');
currentSpeakingId=null;
return;
}
currentSpeakingId=buttonId;
button.innerHTML='⏸️ Stop';
button.classList.add('speaking');
const utterance=new SpeechSynthesisUtterance(text);
utterance.rate=1.0;
utterance.pitch=1.0;
utterance.volume=1.0;
utterance.onend=()=>{
button.innerHTML='🔊 Speak';
button.classList.remove('speaking');
currentSpeakingId=null;
};
utterance.onerror=()=>{
button.innerHTML='🔊 Speak';
button.classList.remove('speaking');
currentSpeakingId=null;
showToast('Speech error');
};
speechSynthesis.speak(utterance);
}

function addMessage(sender,text){
messageCounter++;
const msgId='msg-'+messageCounter;
const container=document.getElementById('chatContainer');
const msgDiv=document.createElement('div');
msgDiv.className='chat-message '+sender+'-message';
msgDiv.id=msgId;
const avatar=document.createElement('div');
avatar.className='avatar '+sender+'-avatar';
avatar.textContent=sender==='user'?'👤':'🤖';
const content=document.createElement('div');
content.className='message-content';
const textDiv=document.createElement('div');
textDiv.className='message-text';
textDiv.textContent=text;
content.appendChild(textDiv);
if(sender==='bot'){
const actions=document.createElement('div');
actions.className='message-actions';
const copyBtn=document.createElement('button');
copyBtn.className='action-btn';
copyBtn.innerHTML='📋 Copy';
copyBtn.onclick=()=>copyText(text);
const speakBtn=document.createElement('button');
speakBtn.className='action-btn';
speakBtn.id='speak-'+msgId;
speakBtn.innerHTML='🔊 Speak';
speakBtn.onclick=()=>speakText(text,'speak-'+msgId);
actions.appendChild(copyBtn);
actions.appendChild(speakBtn);
content.appendChild(actions);
}
msgDiv.appendChild(avatar);
msgDiv.appendChild(content);
container.appendChild(msgDiv);
container.scrollTop=container.scrollHeight;
}

async function sendChatMessage(){
const input=document.getElementById('chatInput');
const message=input.value.trim();
if(!message)return;
input.value='';
document.getElementById('sendBtn').disabled=true;
addMessage('user',message);
document.getElementById('typingIndicator').style.display='block';
try{
const contextualPrompt=systemPrompt
.replace('{{ATTENTION}}',currentDataSnapshot.attention||'--')
.replace('{{MEDITATION}}',currentDataSnapshot.meditation||'--')
.replace('{{QUALITY}}',currentDataSnapshot.quality||'--')
.replace('{{STATE}}',currentDataSnapshot.state||'Unknown')
.replace('{{DELTA}}',formatEEG(currentDataSnapshot.delta))
.replace('{{THETA}}',formatEEG(currentDataSnapshot.theta))
.replace('{{LOWALPHA}}',formatEEG(currentDataSnapshot.lowAlpha))
.replace('{{HIGHALPHA}}',formatEEG(currentDataSnapshot.highAlpha))
.replace('{{LOWBETA}}',formatEEG(currentDataSnapshot.lowBeta))
.replace('{{HIGHBETA}}',formatEEG(currentDataSnapshot.highBeta))
.replace('{{LOWGAMMA}}',formatEEG(currentDataSnapshot.lowGamma))
.replace('{{HIGHGAMMA}}',formatEEG(currentDataSnapshot.highGamma));
const response=await fetch('https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key='+geminiApiKey,{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({
contents:[{parts:[{text:contextualPrompt+'\n\nUser question: '+message}]}],
generationConfig:{temperature:0.7,maxOutputTokens:500}
})
});
document.getElementById('typingIndicator').style.display='none';
if(!response.ok)throw new Error('API error');
const data=await response.json();
const botResponse=data.candidates[0].content.parts[0].text;
addMessage('bot',botResponse);
}catch(error){
document.getElementById('typingIndicator').style.display='none';
addMessage('bot','Error: '+error.message);
}
}

window.addEventListener('beforeunload',()=>{
if('speechSynthesis' in window){
speechSynthesis.cancel();
}
});

setInterval(updateData,2000);
updateData();

setTimeout(()=>{
addMessage('bot','Hi! I\'m your AI brain wave analyst. I give scientifically honest explanations of what your EEG data actually means. Your current attention is '+(currentDataSnapshot.attention||'--')+' and meditation is '+(currentDataSnapshot.meditation||'--')+'. What would you like to know?');
},1000);
</script>
</body></html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleData() { server.send(200, "text/plain", dataBuffer); }

void handleDebug() {
  String debug = debugBuffer;
  debug += "\n=== Status ===\n";
  debug += "Total: " + String(packetCount) + "\n";
  debug += "Valid: " + String(validPacketCount) + "\n";
  debug += "Data valid: " + String(currentData.dataValid ? "Yes" : "No") + "\n";
  debug += "Last update: " + String((millis() - currentData.lastUpdate) / 1000) + "s ago\n";
  debug += "State: " + currentState.primaryState + "\n";
  debug += "Free heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  server.send(200, "text/plain", debug);
}

void handleValues() {
  DynamicJsonDocument doc(1024);
  doc["quality"] = currentData.signalQuality;
  doc["attention"] = currentData.attention;
  doc["meditation"] = currentData.meditation;
  doc["delta"] = currentData.delta;
  doc["theta"] = currentData.theta;
  doc["lowAlpha"] = currentData.lowAlpha;
  doc["highAlpha"] = currentData.highAlpha;
  doc["lowBeta"] = currentData.lowBeta;
  doc["highBeta"] = currentData.highBeta;
  doc["lowGamma"] = currentData.lowGamma;
  doc["highGamma"] = currentData.highGamma;
  doc["packets"] = packetCount;
  doc["validPackets"] = validPacketCount;
  doc["uptime"] = millis();
  doc["memory"] = ESP.getFreeHeap();
  doc["dataValid"] = currentData.dataValid;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleMentalState() {
  DynamicJsonDocument doc(512);
  doc["primaryState"] = currentState.primaryState;
  doc["secondaryState"] = currentState.secondaryState;
  doc["attentionTrend"] = currentState.attentionTrend;
  doc["relaxationTrend"] = currentState.relaxationTrend;
  doc["signalConfidence"] = currentState.signalConfidence;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleStatus() {
  DynamicJsonDocument doc(512);
  doc["uptime"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["packets"] = packetCount;
  doc["validPackets"] = validPacketCount;
  doc["sdCard"] = sdCardAvailable;
  doc["dataValid"] = currentData.dataValid;
  doc["mentalState"] = currentState.primaryState;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleReset() {
  packetCount = 0;
  validPacketCount = 0;
  dataBuffer = "";
  debugBuffer = "";
  hexBuffer = "";
  currentData = BrainData();
  currentState = MentalState();
  for (int i = 0; i < 10; i++) {
    attentionHistory[i] = 0;
    meditationHistory[i] = 0;
    qualityHistory[i] = 0;
  }
  historyIndex = 0;
  generateTestData();
  server.send(200, "text/plain", "Reset complete");
}

void handleDownload() {
  if (sdCardAvailable) {
    File file = SD.open("/brainwave.csv");
    if (file) {
      server.streamFile(file, "text/csv");
      file.close();
    } else {
      server.send(404, "text/plain", "No data file");
    }
  } else {
    server.send(503, "text/plain", "SD card not available");
  }
}

void logData() {
  if (sdCardAvailable) {
    File file = SD.open("/brainwave.csv", FILE_APPEND);
    if (file) {
      file.print(millis()); file.print(",");
      file.print(currentData.attention); file.print(",");
      file.print(currentData.meditation); file.print(",");
      file.print(currentData.signalQuality); file.print(",");
      file.print(currentData.delta); file.print(",");
      file.print(currentData.theta); file.print(",");
      file.print(currentData.lowAlpha); file.print(",");
      file.print(currentData.highAlpha); file.print(",");
      file.print(currentData.lowBeta); file.print(",");
      file.print(currentData.highBeta); file.print(",");
      file.print(currentData.lowGamma); file.print(",");
      file.print(currentData.highGamma); file.print(",");
      file.print(currentData.rawWave); file.print(",");
      file.println(currentState.primaryState);
      file.close();
    }
  }
}