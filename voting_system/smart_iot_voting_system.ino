#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <EEPROM.h>

// WiFi credentials
const char* ssid = "minhtawlwe’s iPhone";
const char* password = "972866772";

// Pin setup
const int servoPin = D4;
const int secondServoPin = D8;
const int buzzerPin = D7;
const int buttonAPin = D5;
const int buttonBPin = D6;
const int buttonCPin = D3;

ESP8266WebServer server(80);
Servo doorServo;
Servo secondServo;

// EEPROM addresses
const int EEPROM_VOTE_A_ADDR = 0;
const int EEPROM_VOTE_B_ADDR = sizeof(int);
const int EEPROM_VOTE_C_ADDR = 2 * sizeof(int);
const int EEPROM_AUTH_IP_ADDR = 3 * sizeof(int);
const int EEPROM_SECOND_SERVO_STATE_ADDR = EEPROM_AUTH_IP_ADDR + 4;
const int EEPROM_SIZE = EEPROM_SECOND_SERVO_STATE_ADDR + 1;

// Vote counts
int voteA = 0;
int voteB = 0;
int voteC = 0;

// Voting state
bool voterActive = false;
bool hasVoted = false;
unsigned long voteTime = 0;

// Second Servo State
bool secondServoOpen = false;

// Button Debounce Variables
unsigned long lastButtonPressTimeA = 0;
unsigned long lastButtonPressTimeB = 0;
unsigned long lastButtonPressTimeC = 0;
const long debounceDelay = 200;

// Countdown Timer Variables
unsigned long doorOpenTimestamp = 0;
const long votingDurationMs = 30000;

// Login Credentials
const char* http_username = "admin";
const char* http_password = "password";

// Session management
IPAddress authenticatedIP;

// Base64 Encoded Image Data
const char* photoA_base64 = "data:image/png;base64,iVBORw0KG...";
const char* photoB_base64 = "data:image/png;base64,iVBORw0KG...";
const char* photoC_base64 = "data:image/png;base64,iVBORw0KG...";

void setup() {
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);
  loadFromEEPROM();

  // Initialize pins
  pinMode(buttonAPin, INPUT_PULLUP);
  pinMode(buttonBPin, INPUT_PULLUP);
  pinMode(buttonCPin, INPUT_PULLUP);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  if (!doorServo.attach(servoPin)) {
    Serial.println("Failed to attach main door servo!");
  }
  closeDoor();

  if (!secondServo.attach(secondServoPin)) {
    Serial.println("Failed to attach second servo!");
  }
  if (secondServoOpen) {
    openSecondServo();
  } else {
    closeSecondServo();
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  unsigned long wifiTimeout = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP Address: ");
    Serial.println(WiFi.localIP());
    buzz(200);
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }

  // Route handlers
  server.on("/", HTTP_GET, handleUserPage);
  server.on("/admin", HTTP_GET, handleAdminPage);
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/doLogin", HTTP_POST, handleDoLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/test", HTTP_GET, handleTestRequest);
  server.on("/openDoor", HTTP_GET, handleOpenDoor);
  server.on("/closeDoor", HTTP_GET, handleCloseDoor);
  server.on("/toggleSecondServo", HTTP_GET, handleToggleSecondServo);
  server.on("/resetVotes", HTTP_GET, handleResetVotes);
  server.on("/voteA", HTTP_GET, handleVoteA);
  server.on("/voteB", HTTP_GET, handleVoteB);
  server.on("/voteC", HTTP_GET, handleVoteC);
  server.on("/status", HTTP_GET, handleStatusCheck);
  server.on("/open_door", HTTP_GET, handleOpenDoor);
  server.onNotFound(handleInvalid);

  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  if (voterActive) {
    if (!hasVoted) {
      if (digitalRead(buttonAPin) == LOW && (currentMillis - lastButtonPressTimeA > debounceDelay)) {
        voteA++;
        hasVoted = true;
        voteTime = currentMillis;
        buzz(100);
        Serial.printf("Button A pressed -> Vote A: %d\n", voteA);
        saveToEEPROM();
        lastButtonPressTimeA = currentMillis;
      } else if (digitalRead(buttonBPin) == LOW && (currentMillis - lastButtonPressTimeB > debounceDelay)) {
        voteB++;
        hasVoted = true;
        voteTime = currentMillis;
        buzz(100);
        Serial.printf("Button B pressed -> Vote B: %d\n", voteB);
        saveToEEPROM();
        lastButtonPressTimeB = currentMillis;
      } else if (digitalRead(buttonCPin) == LOW && (currentMillis - lastButtonPressTimeC > debounceDelay)) {
        voteC++;
        hasVoted = true;
        voteTime = currentMillis;
        buzz(100);
        Serial.printf("Button C pressed -> Vote C: %d\n", voteC);
        saveToEEPROM();
        lastButtonPressTimeC = currentMillis;
      }
    }

    if (hasVoted && (currentMillis - voteTime >= 5000)) {
      closeDoor();
    }

    if (currentMillis - doorOpenTimestamp >= votingDurationMs) {
      closeDoor();
      Serial.println("Voting duration expired. Main door closed.");
    }
  }
}

void handleUserPage() {
  int totalVotes = voteA + voteB + voteC;
  int percentA = totalVotes > 0 ? (voteA * 100) / totalVotes : 0;
  int percentB = totalVotes > 0 ? (voteB * 100) / totalVotes : 0;
  int percentC = totalVotes > 0 ? (voteC * 100) / totalVotes : 0;

  String leader = "NO LEADER";
  String leaderInitial = "N/A";
  if (totalVotes > 0) {
    if (voteA >= voteB && voteA >= voteC) {
      leader = "MIN HTAW LWE";
      leaderInitial = "ML";
    } else if (voteB >= voteA && voteB >= voteC) {
      leader = "TIN MYO ZIN NWE";
      leaderInitial = "TN";
    } else {
      leader = "Wai Wai Aung";
      leaderInitial = "WA";
    }
  }

  String totalSignals = String(totalVotes);
  String html = "<!DOCTYPE html><html><head><title>Public Voting Results</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>";
  html += "body, html { margin: 0; padding: 0; width: 100vw; height: 100vh; font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #dbe9ff, #f0f4ff); color: #2c3e50; display: flex; flex-direction: column; align-items: center; justify-content: center; overflow: hidden; }";
  html += "h1 { font-size: 4vh; margin: 0; color: #2c3e50; font-weight: 600; }";
  html += "h2 { font-size: 2.2vh; margin: 5px 0 20px; color: #7f8c8d; letter-spacing: 1px; }";
  html += ".dashboard-grid { display: flex; gap: 3vw; margin-bottom: 35px; }";
  html += ".dashboard-item { background: #ffffff; padding: 22px 35px; border-radius: 14px; box-shadow: 0 6px 18px rgba(0,0,0,0.08); text-align: center; min-width: 200px; }";
  html += ".dashboard-value { font-size: 2.8vh; font-weight: 600; color: #1f618d; }";
  html += ".dashboard-label { font-size: 1.5vh; color: #888; text-transform: uppercase; }";
  html += ".candidates-grid { display: flex; justify-content: center; gap: 4vw; }";
  html += ".candidate-item { background: #ffffff; padding: 25px 20px; border-radius: 15px; box-shadow: 0 6px 20px rgba(0,0,0,0.1); width: 180px; text-align: center; transition: transform 0.25s; }";
  html += ".hexagon-container { width: 85px; height: 85px; margin: 0 auto; clip-path: polygon(50% 0%, 93% 25%, 93% 75%, 50% 100%, 7% 75%, 7% 25%); display: flex; align-items: center; justify-content: center; color: white; font-weight: bold; font-size: 2.1vh; margin-bottom: 14px; box-shadow: 0 4px 10px rgba(0,0,0,0.2); }";
  html += ".hex-blue { background-color: #2e86de; }";
  html += ".hex-green { background-color: #27ae60; }";
  html += ".hex-gold { background-color: #f39c12; }";
  html += ".candidate-name { font-size: 1.7vh; font-weight: 600; color: #2c3e50; margin-bottom: 6px; }";
  html += ".vote-info { font-size: 1.5vh; }";
  html += ".vote-count { font-size: 2vh; font-weight: bold; display: block; color: #2c3e50; }";
  html += ".vote-percent { color: #666; font-size: 1.4vh; }";
  html += "a.admin-login { background: #34495e; color: white; padding: 10px 22px; border-radius: 8px; font-size: 1.6vh; margin-top: 35px; display: inline-block; text-decoration: none; box-shadow: 0 3px 8px rgba(0,0,0,0.2); transition: background 0.2s; }";
  html += "a.admin-login:hover { background: #2c3e50; }";
  html += "</style></head><body>";

  html += "<h1>LIVE VOTING RESULT</h1>";
  html += "<h2>EC SELECTION</h2>";
  html += "<div class='dashboard-grid'>";
  html += "<div class='dashboard-item'><div class='dashboard-value'>" + totalSignals + "</div><div class='dashboard-label'>Total Signals</div></div>";
  html += "<div class='dashboard-item'><div class='dashboard-value'>" + leaderInitial + "</div><div class='dashboard-label'>Leading Node</div></div>";
  html += "</div>";

  html += "<div class='candidates-grid'>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-blue'>ML</div>";
  html += "<div class='candidate-name'>MIN HTAW LWE</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteA) + " votes</span><span class='vote-percent'>" + String(percentA) + "%</span></div>";
  html += "</div>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-green'>TN</div>";
  html += "<div class='candidate-name'>TIN MYO ZIN NWE</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteB) + " votes</span><span class='vote-percent'>" + String(percentB) + "%</span></div>";
  html += "</div>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-gold'>WA</div>";
  html += "<div class='candidate-name'>WAI WAI AUNG</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteC) + " votes</span><span class='vote-percent'>" + String(percentC) + "%</span></div>";
  html += "</div>";
  html += "</div>";

  html += "<a href='/login' class='admin-login'>Admin Login</a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleAdminPage() {
  if (!isAuthenticated()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "");
    return;
  }

  int totalVotes = voteA + voteB + voteC;
  int percentA = totalVotes > 0 ? (voteA * 100) / totalVotes : 0;
  int percentB = totalVotes > 0 ? (voteB * 100) / totalVotes : 0;
  int percentC = totalVotes > 0 ? (voteC * 100) / totalVotes : 0;

  String leader = "NO LEADER";
  String leaderInitial = "N/A";
  if (totalVotes > 0) {
    if (voteA >= voteB && voteA >= voteC) {
      leader = "MIN HTAW LWE";
      leaderInitial = "ML";
    } else if (voteB >= voteA && voteB >= voteC) {
      leader = "TIN MYO ZIN NWE";
      leaderInitial = "TN";
    } else {
      leader = "Wai Wai Aung";
      leaderInitial = "WA";
    }
  }

  String totalSignals = String(totalVotes);
  String html = "<!DOCTYPE html><html><head><title>Admin Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>";
  html += "body, html { margin: 0; padding: 0; width: 100vw; height: 100vh; font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #e0f2f7, #c1e4ee); color: #2c3e50; display: flex; flex-direction: column; align-items: center; justify-content: center; overflow: auto; }";
  html += "h1 { font-size: 4vh; margin: 0; color: #2c3e50; font-weight: 600; }";
  html += "h2 { font-size: 2.2vh; margin: 5px 0 20px; color: #7f8c8d; letter-spacing: 1px; }";
  html += "#countdown-timer { font-size: 2.2vh; color: #d63031; font-weight: bold; margin-bottom: 25px; }";
  html += ".dashboard-grid { display: flex; gap: 3vw; margin-bottom: 35px; flex-wrap: wrap; justify-content: center;}";
  html += ".dashboard-item { background: #ffffff; padding: 22px 35px; border-radius: 14px; box-shadow: 0 6px 18px rgba(0,0,0,0.08); text-align: center; min-width: 200px; }";
  html += ".dashboard-value { font-size: 2.8vh; font-weight: 600; color: #1f618d; }";
  html += ".dashboard-label { font-size: 1.5vh; color: #888; text-transform: uppercase; }";
  html += ".candidates-grid { display: flex; justify-content: center; gap: 4vw; flex-wrap: wrap; }";
  html += ".candidate-item { background: #ffffff; padding: 25px 20px; border-radius: 15px; box-shadow: 0 6px 20px rgba(0,0,0,0.1); width: 180px; text-align: center; margin-bottom: 20px;}";
  html += ".hexagon-container { width: 85px; height: 85px; margin: 0 auto; clip-path: polygon(50% 0%, 93% 25%, 93% 75%, 50% 100%, 7% 75%, 7% 25%); display: flex; align-items: center; justify-content: center; color: white; font-weight: bold; font-size: 2.1vh; margin-bottom: 14px; box-shadow: 0 4px 10px rgba(0,0,0,0.2); }";
  html += ".hex-blue { background-color: #2e86de; }";
  html += ".hex-green { background-color: #27ae60; }";
  html += ".hex-gold { background-color: #f39c12; }";
  html += ".candidate-name { font-size: 1.7vh; font-weight: 600; color: #2c3e50; margin-bottom: 6px; }";
  html += ".vote-info { font-size: 1.5vh; }";
  html += ".vote-count { font-size: 2vh; font-weight: bold; display: block; color: #2c3e50; }";
  html += ".vote-percent { color: #666; font-size: 1.4vh; }";
  html += ".admin-controls { margin-top: 40px; display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }";
  html += ".admin-controls a { background: #34495e; color: white; padding: 12px 25px; border-radius: 8px; font-size: 1.7vh; text-decoration: none; box-shadow: 0 3px 8px rgba(0,0,0,0.2); transition: background 0.2s, transform 0.2s; white-space: nowrap; }";
  html += ".admin-controls a:hover { background: #2c3e50; transform: translateY(-2px); }";
  html += ".logout-btn { background-color: #e74c3c !important; }";
  html += ".logout-btn:hover { background-color: #c0392b !important; }";
  html += "</style></head><body>";

  html += "<h1>ADMIN DASHBOARD</h1>";
  html += "<h2>VOTING SYSTEM CONTROL</h2>";
  html += "<div id='countdown-timer'>" + String(voterActive ? "TIME LEFT: " : "VOTING NOT ACTIVE") + "</div>";
  html += "<div id='data-container' data-door-open-timestamp='" + String(doorOpenTimestamp) + "'";
  html += " data-voting-duration-ms='" + String(votingDurationMs) + "'";
  html += " data-server-current-millis='" + String(millis()) + "'";
  html += " data-voter-active='" + String(voterActive ? 1 : 0) + "'></div>";
  html += "<div class='dashboard-grid'>";
  html += "<div class='dashboard-item'><div class='dashboard-value'>" + totalSignals + "</div><div class='dashboard-label'>Total Signals</div></div>";
  html += "<div class='dashboard-item'><div class='dashboard-value'>" + leaderInitial + "</div><div class='dashboard-label'>Leading Node</div></div>";
  html += "</div>";

  html += "<div class='candidates-grid'>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-blue'>ML</div>";
  html += "<div class='candidate-name'>MIN HTAW LWE</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteA) + " votes</span><span class='vote-percent'>" + String(percentA) + "%</span></div>";
  html += "</div>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-green'>TN</div>";
  html += "<div class='candidate-name'>TIN MYO ZIN NWE</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteB) + " votes</span><span class='vote-percent'>" + String(percentB) + "%</span></div>";
  html += "</div>";
  html += "<div class='candidate-item'>";
  html += "<div class='hexagon-container hex-gold'>WA</div>";
  html += "<div class='candidate-name'>WAI WAI AUNG</div>";
  html += "<div class='vote-info'><span class='vote-count'>" + String(voteC) + " votes</span><span class='vote-percent'>" + String(percentC) + "%</span></div>";
  html += "</div>";
  html += "</div>";

  html += "<div class='admin-controls'>";
  html += "<a href='/openDoor'>Open Main Door & Start Voting</a>";
  html += "<a href='/closeDoor'>Close Main Door & Stop Voting</a>";
  html += "<a href='/toggleSecondServo'>" + String(secondServoOpen ? "Close Second Door" : "Open Second Door") + "</a>";
  html += "<a href='/resetVotes' onclick='return confirm(\"Are you sure you want to reset all votes?\");'>Reset All Votes</a>";
  html += "<a href='/logout' class='logout-btn'>Logout</a>";
  html += "</div>";

  html += "<script>";
  html += "function startCountdown() {";
  html += "  var el = document.getElementById('data-container');";
  html += "  var doorOpen = parseInt(el.getAttribute('data-door-open-timestamp'));";
  html += "  var duration = parseInt(el.getAttribute('data-voting-duration-ms'));";
  html += "  var serverMillis = parseInt(el.getAttribute('data-server-current-millis'));";
  html += "  var active = parseInt(el.getAttribute('data-voter-active'));";
  html += "  var countdownEl = document.getElementById('countdown-timer');";
  html += "  if (active === 0) { countdownEl.innerHTML = 'VOTING NOT ACTIVE'; return; }";
  html += "  var offset = new Date().getTime() - serverMillis;";
  html += "  var endTime = doorOpen + duration + offset;";
  html += "  var x = setInterval(function() {";
  html += "    var now = new Date().getTime();";
  html += "    var diff = endTime - now;";
  html += "    if (diff < 0) { clearInterval(x); countdownEl.innerHTML = 'VOTING CLOSED'; return; }";
  html += "    var m = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));";
  html += "    var s = Math.floor((diff % (1000 * 60)) / 1000);";
  html += "    countdownEl.innerHTML = 'TIME LEFT: ' + m + 'm ' + (s < 10 ? '0' + s : s) + 's';";
  html += "  }, 1000);";
  html += "}";
  html += "window.onload = startCountdown;";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleLogin() {
  String html = "<!DOCTYPE html><html><head><title>Login</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: 'Share Tech Mono', monospace; background-color: #0d0d1a; color: #00e6e6; display: flex; flex-direction: column; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }";
  html += ".login-container { background: linear-gradient(145deg, #1a1a33, #0d0d1a); padding: 20px; border-radius: 10px; box-shadow: 0 0 20px rgba(0, 230, 230, 0.4); border: 1px solid #00e6e6; max-width: 400px; width: 90%; text-align: center; }";
  html += "h1 { font-size: 2.5em; letter-spacing: 3px; color: #00e6e6; text-shadow: 0 0 10px #00e6e6; margin-bottom: 30px; }";
  html += "input[type='text'], input[type='password'] { width: calc(100% - 20px); padding: 12px; margin-bottom: 20px; border: 1px solid #00ffff; border-radius: 5px; background-color: #0d0d1a; color: #00ffff; font-size: 1em; box-shadow: inset 0 0 5px rgba(0, 230, 230, 0.2); }";
  html += "input[type='submit'] { background-color: #A020F0; color: white; padding: 12px 25px; border: none; border-radius: 5px; cursor: pointer; font-size: 1.1em; transition: background-color 0.3s ease, box-shadow 0.3s ease; box-shadow: 0 0 10px rgba(160, 32, 240, 0.5); }";
  html += "input[type='submit']:hover { background-color: #8A00C0; box-shadow: 0 0 15px rgba(160, 32, 240, 0.7); }";
  html += ".error-message { color: #ff3333; margin-top: 15px; font-weight: bold; }";
  html += "</style></head><body>";
  html += "<div class='login-container'>";
  html += "<h1>CYBER VOTE LOGIN</h1>";
  html += "<form action='/doLogin' method='POST'>";
  html += "<input type='text' name='username' placeholder='Username' required><br>";
  html += "<input type='password' name='password' placeholder='Password' required><br>";
  html += "<input type='submit' value='Login'>";
  html += "</form>";
  if (server.hasArg("login_error") && server.arg("login_error") == "1") {
    html += "<p class='error-message'>Invalid Username or Password!</p>";
  }
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleDoLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String inputUser = server.arg("username");
    String inputPass = server.arg("password");
    
    bool userMatch = (inputUser.length() == strlen(http_username));
    bool passMatch = (inputPass.length() == strlen(http_password));
    
    for (size_t i = 0; i < inputUser.length() && i < strlen(http_username); i++) {
      userMatch &= (inputUser[i] == http_username[i]);
    }
    
    for (size_t i = 0; i < inputPass.length() && i < strlen(http_password); i++) {
      passMatch &= (inputPass[i] == http_password[i]);
    }
    
    if (userMatch && passMatch) {
      authenticatedIP = server.client().localIP();
      saveToEEPROM();
      server.sendHeader("Location", "/admin");
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.sendHeader("Location", "/login?login_error=1");
  server.send(302, "text/plain", "");
}

void handleLogout() {
  authenticatedIP = IPAddress(0, 0, 0, 0);
  saveToEEPROM();
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "");
  Serial.println("User logged out.");
}

void handleTestRequest() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  Serial.println("/test triggered: Opening main door");
  openDoor();
  server.sendHeader("Location", "/admin");
  server.send(302, "text/plain", "");
}

void handleOpenDoor() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  Serial.println("/openDoor triggered: Opening main door");
  openDoor();
  server.sendHeader("Location", "/admin");
  server.send(302, "text/plain", "");
}

void handleCloseDoor() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  Serial.println("/closeDoor triggered: Closing main door");
  closeDoor();
  server.sendHeader("Location", "/admin");
  server.send(302, "text/plain", "");
}

void handleToggleSecondServo() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  if (secondServoOpen) {
    closeSecondServo();
  } else {
    openSecondServo();
  }
  server.sendHeader("Location", "/admin");
  server.send(302, "text/plain", "");
}

void handleResetVotes() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  Serial.println("/resetVotes triggered: Resetting votes");
  voteA = 0;
  voteB = 0;
  voteC = 0;
  saveToEEPROM();
  server.sendHeader("Location", "/admin");
  server.send(302, "text/plain", "");
}

void handleVoteA() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  if (voterActive && !hasVoted) {
    voteA++;
    hasVoted = true;
    voteTime = millis();
    buzz(100);
    Serial.printf("Vote A: %d\n", voteA);
    saveToEEPROM();
    server.send(200, "text/plain", "Vote A counted");
  } else {
    server.send(403, "text/plain", "Voting not allowed or already voted");
  }
}

void handleVoteB() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  if (voterActive && !hasVoted) {
    voteB++;
    hasVoted = true;
    voteTime = millis();
    buzz(100);
    Serial.printf("Vote B: %d\n", voteB);
    saveToEEPROM();
    server.send(200, "text/plain", "Vote B counted");
  } else {
    server.send(403, "text/plain", "Voting not allowed or already voted");
  }
}

void handleVoteC() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  if (voterActive && !hasVoted) {
    voteC++;
    hasVoted = true;
    voteTime = millis();
    buzz(100);
    Serial.printf("Button C pressed -> Vote C: %d\n", voteC);
    saveToEEPROM();
    server.send(200, "text/plain", "Vote C counted");
  } else {
    server.send(403, "text/plain", "Voting not allowed or already voted");
  }
}

void handleStatusCheck() {
  if (!isAuthenticated()) {
    server.send(403, "text/plain", "Access Denied: Not logged in.");
    return;
  }
  server.send(200, "text/plain", "NodeMCU is active and logged in.");
}

void handleInvalid() {
  Serial.println("Invalid request received! Triggering buzzer");
  buzz(500);
  server.send(404, "text/plain", "Invalid request");
}

void buzz(int duration_ms) {
  digitalWrite(buzzerPin, HIGH);
  delay(duration_ms);
  digitalWrite(buzzerPin, LOW);
}

void openDoor() {
  if (doorServo.attached()) {
    doorServo.write(90);
    voterActive = true;
    hasVoted = false;
    doorOpenTimestamp = millis();
    buzz(200);
    Serial.println("Main door opened, voting enabled.");
  }
}

void closeDoor() {
  if (doorServo.attached()) {
    doorServo.write(0);
    voterActive = false;
    hasVoted = false;
    buzz(150);
    Serial.println("Main door closed.");
  }
}

void openSecondServo() {
  if (secondServo.attached()) {
    secondServo.write(90);
    secondServoOpen = true;
    Serial.println("Second servo opened.");
    saveToEEPROM();
  }
}

void closeSecondServo() {
  if (secondServo.attached()) {
    secondServo.write(0);
    secondServoOpen = false;
    Serial.println("Second servo closed.");
    saveToEEPROM();
  }
}

bool isAuthenticated() {
  return (server.client().localIP() == authenticatedIP);
}

void saveToEEPROM() {
  EEPROM.put(EEPROM_VOTE_A_ADDR, voteA);
  EEPROM.put(EEPROM_VOTE_B_ADDR, voteB);
  EEPROM.put(EEPROM_VOTE_C_ADDR, voteC);

  EEPROM.write(EEPROM_AUTH_IP_ADDR + 0, authenticatedIP[0]);
  EEPROM.write(EEPROM_AUTH_IP_ADDR + 1, authenticatedIP[1]);
  EEPROM.write(EEPROM_AUTH_IP_ADDR + 2, authenticatedIP[2]);
  EEPROM.write(EEPROM_AUTH_IP_ADDR + 3, authenticatedIP[3]);

  EEPROM.write(EEPROM_SECOND_SERVO_STATE_ADDR, secondServoOpen ? 1 : 0);

  if (!EEPROM.commit()) {
    Serial.println("EEPROM commit failed!");
  }
}

void loadFromEEPROM() {
  EEPROM.get(EEPROM_VOTE_A_ADDR, voteA);
  EEPROM.get(EEPROM_VOTE_B_ADDR, voteB);
  EEPROM.get(EEPROM_VOTE_C_ADDR, voteC);

  byte ip0 = EEPROM.read(EEPROM_AUTH_IP_ADDR + 0);
  byte ip1 = EEPROM.read(EEPROM_AUTH_IP_ADDR + 1);
  byte ip2 = EEPROM.read(EEPROM_AUTH_IP_ADDR + 2);
  byte ip3 = EEPROM.read(EEPROM_AUTH_IP_ADDR + 3);
  authenticatedIP = IPAddress(ip0, ip1, ip2, ip3);

  secondServoOpen = (EEPROM.read(EEPROM_SECOND_SERVO_STATE_ADDR) == 1);

  voteA = constrain(voteA, 0, 1000);
  voteB = constrain(voteB, 0, 1000);
  voteC = constrain(voteC, 0, 1000);
}