// CASPERSKY_THE_GHOST_V1 (HEADLESS SERIAL MODE + DYNAMIC HEARTBEAT + SECURE VAULT)

#include <SPI.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "esp_system.h"
#include "FS.h"
#include "LittleFS.h"
#include <mbedtls/md.h>  // For SHA-256 calculation
#include "mbedtls/aes.h"
#include "mbedtls/esp_config.h"
#include "mbedtls/gcm.h"

// Heartbeat / status LED on GPIO 21
#define CYD_LED_STATUS 21

// HACKER_GUESTBOOK VARIABLE
String Hacker_Guestbook = "0";  // Take a nice cup of coffee and leave a message.

unsigned long previousMillis = 0;
unsigned long ledTimer = 0;
int stepCounter = 100;  
bool triggerMirror = false;

// --- DYNAMIC HUMAN HEARTBEAT VARIABLES ---
unsigned long heartbeatTimer = 0;
int heartbeatState = 0;
int currentHeartbeatInterval = 800; // Standard calm pause (ms)
unsigned long lastScanCheck = 0;
int detectedNetworks = 0;

// SSID names (leetspeak Coffee theme)
const char* hacker_phrases[] = {
  "Ju57_C0ff33", "H3xpr3ss0_C0d3", "C4ff31n3_R3v3rb", "V01d_3ch0_8r3w", "D4rk_3ch0_Dr1p", "PH4NT0M_3CH0_834N",
  "51l3nt_Gr1nd_3ch0", "D34d_B34n_R00t", "N1ght_8r3w_V01d", "4r481c4_0v3rr1d3",
  "Cr3m4_Ch05_V1", "81tt3r_3xtr4ct_X", "5t34m_P0rt_3ch0", "Fl1t3r_8y73_Dr1p",
  "D3c4f_D3n14l", "R04st_R00tk1t", "B34n_Bypass_01", "3spr3ss0_3xpl01t",
  "Gr1nd_Gu4rd14n", "C4n1st3r_Ch40s", "D4rk_Dr1p_D0pt", "P0ur_0v3r_P0rt",
  "St34m_Sh3ll_X", "8r3w_8r34ch_V2", "M0ch4_M4lw4r3", "C0ff33_C0d3x_7",
  "Cr3m4_Cr4ck3r", "F1lt3r_Ph4nt0m", "8l4ck_8y73_8r3w", "S1ph0n_S3nt1n3l",
  "Fr3nch_Pr3ss_Pwn", "3xt4rct_3rr0r", "L4tt3_L0g1c_88", "D3c4f_D3n13d",
  "Sy5_St34m_Pwn", "C0ff33_D0m41n", "R08u5t4_R00t", "K3rn3l_C0ff33", "P0rt_80_8r3w",
  "S3rv3r_5t34m", "1ntru51v3_Dr1p", "H4rdw4r3_H4z3", "5t33p_53ntr1_X", "8r3w_F0rc3",
  "Gr1nd_5h3ll_01", "D0pt_D4rk_R045t", "B34n_0v3rfl0w", "C4ppucc1n0_C0d3", "Dr1p_D3f3n53",
  "N3t_8r3w_Ph05t", "81tt3r_8y73_V3", "C0ld_8r3w_Cr4ck", "5h0t_5h3ll_V8", "T3rm1n4l_T4p",
  "V4p0r_V01d_X", "Pr355_P4ck3t", "C0ff33_C0mm4nd", "5h1ft_Sh0t_X", "8y73_B34n_L0g",
  "5iphon_5h3ll"
};

// --- AES ENCRYPTION LOGIC (Links Chip-ID + Extract to Beans, using Coffee as key) ---
String hardwareGCM(String data, bool encrypt, String decryptionKey = "", String storedData = "") {
  uint64_t chipid = ESP.getEfuseMac();
  
  // If a decryption key is passed (e.g. during vault unlock), use it.
  // Otherwise, create a key based on ChipID + Extract (during setupVault).
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)&chipid, sizeof(chipid));
  if (decryptionKey.length() > 0) {
    mbedtls_md_update(&ctx, (const unsigned char*)decryptionKey.c_str(), decryptionKey.length());
  }
  byte key[32];
  mbedtls_md_finish(&ctx, key);
  mbedtls_md_free(&ctx);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  
  unsigned char nonce[12];
  unsigned char tag[16];
  int dataLen = data.length();
  
  if (encrypt) {
    for (int i = 0; i < 12; i++) nonce[i] = (unsigned char)esp_random();
    unsigned char output[dataLen > 0 ? dataLen : 1];
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, dataLen, nonce, 12, NULL, 0,
                            (const unsigned char*)data.c_str(), output, 16, tag);
    String result = "";
    for (int i = 0; i < 12; i++) {
      char buf[3];
      sprintf(buf, "%02x", nonce[i]);
      result += buf;
    }
    for (int i = 0; i < 16; i++) {
      char buf[3];
      sprintf(buf, "%02x", tag[i]);
      result += buf;
    }
    for (int i = 0; i < dataLen; i++) {
      char buf[3];
      sprintf(buf, "%02x", output[i]);
      result += buf;
    }
    mbedtls_gcm_free(&gcm);
    return result;
  } else {
    int totalLen = storedData.length() / 2;
    if (totalLen < 28) return "ERROR: DATA_TOO_SHORT";
    unsigned char raw[totalLen];
    for (int i = 0; i < totalLen; i++) raw[i] = (unsigned char)strtol(storedData.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    memcpy(nonce, raw, 12);
    memcpy(tag, raw + 12, 16);
    int cipherLen = totalLen - 28;
    unsigned char output[cipherLen + 1];
    int ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen, nonce, 12, NULL, 0, tag, 16, raw + 28, output);
    output[cipherLen] = '\0';
    mbedtls_gcm_free(&gcm);
    if (ret == 0) return String((char*)output);
    else return "ERROR: VAULT_TAMPERED";
  }
}

unsigned long lastSsidChange = 0;
unsigned long lastMacChange = 0;
unsigned long lastTxUpdate = 0;
int8_t currentTxPower = 78;
int STATUS_MODE = 0;

// --- LEET CONVERTER ---
String convertToLeet(String text) {
  text.toUpperCase();
  text.replace("A", "4");
  text.replace("E", "3");
  text.replace("I", "1");
  text.replace("O", "0");
  text.replace("S", "5");
  text.replace("T", "7");
  text.replace("B", "8");
  text.replace("G", "6");
  return text;
}

String generateStrongPassword(int length) {
  const char lower[] = "abcdefghijklmnopqrstuvwxyz";
  const char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char digits[] = "0123456789";
  const char symbols[] = "!@#$%^&*";
  const char* allGroups[] = { lower, upper, digits, symbols };
  int sizes[] = { 26, 26, 10, 8 };
  String res = "";
  for (int i = 0; i < length; i++) {
    int groupIdx = esp_random() % 4;
    const char* selectedGroup = allGroups[groupIdx];
    res += selectedGroup[esp_random() % sizes[groupIdx]];
  }
  return res;
}

void addLog(String msg) {
  Serial.print("user@caspersky:~$ ");
  Serial.println(msg);
  if (Hacker_Guestbook != "0") {
    Serial.print(" > GUEST: ");
    Serial.println(Hacker_Guestbook);
  }
}

void onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  triggerMirror = true;
  addLog("!!! AUTH_ATTEMPT_DETECTED !!!");
  addLog("STATUS: INITIATING_MIRROR");
}

String getSHA256(String input) {
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  String hashStr = "";
  for (int i = 0; i < 32; i++) {
    char buf[3];
    sprintf(buf, "%02x", shaResult[i]);
    hashStr += buf;
  }
  return hashStr;
}

void showVaultViaSerial(String encryptedBean, String extractKey) {
  String decryptedBean = hardwareGCM("", false, extractKey, encryptedBean);
  if (decryptedBean == "ERROR: VAULT_TAMPERED" || decryptedBean == "ERROR: DATA_TOO_SHORT") {
    Serial.println("!! ALARM !! INTEGRITY_CHECK_FAILED - DATA_CORRUPTION_OR_TAMPERING_DETECTED");
    delay(5000);
    ESP.restart();
  }
  Serial.println("--- VAULT CONTENT ---");
  Serial.println(decryptedBean);
  Serial.println("---------------------");
  delay(60000);
  ESP.restart();
}

void checkVaultTrigger() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;
    String inputHash = getSHA256(input);
    if (LittleFS.exists("/vault.bin")) {
      fs::File f = LittleFS.open("/vault.bin", "r");
      String storedHash = f.readStringUntil('\n');
      storedHash.trim();
      String encryptedBean = f.readString();
      encryptedBean.trim();
      f.close();

      if (inputHash == storedHash) {
        Serial.println("MATCH! Now enter EXTRACT to decrypt the vault...");
        while (!Serial.available());
        String extractInput = Serial.readStringUntil('\n');
        extractInput.trim();
        
        showVaultViaSerial(encryptedBean, extractInput);
      } else {
        Serial.println("NO MATCH.");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(CYD_LED_STATUS, OUTPUT);
  digitalWrite(CYD_LED_STATUS, HIGH);  // Off
  randomSeed(esp_random());
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.onEvent(onStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  addLog("System initialized.");
  addLog("Dual-Cycle ACTIVATED");
  addLog("Dynamic TX-BREATHING Enabled");
  addLog("Intrusion Detection ACTIVE");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
}

// --- DYNAMIC HUMAN HEARTBEAT LOGIC ---
void updateHeartbeat() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastScanCheck >= 10000 || lastScanCheck == 0) {
    lastScanCheck = currentMillis;
    WiFi.scanNetworks(true); 
  }

  int n = WiFi.scanComplete();
  if (n >= 0) {
    detectedNetworks = n;
    WiFi.scanDelete(); 
    currentHeartbeatInterval = map(detectedNetworks, 0, 15, 1000, 200);
    currentHeartbeatInterval = constrain(currentHeartbeatInterval, 200, 1000);
  }

  switch (heartbeatState) {
    case 0:  
      if (currentMillis - heartbeatTimer >= (unsigned long)currentHeartbeatInterval) {
        digitalWrite(CYD_LED_STATUS, LOW);  
        heartbeatTimer = currentMillis;
        heartbeatState = 1;
      }
      break;

    case 1:  
      if (currentMillis - heartbeatTimer >= 80) {
        digitalWrite(CYD_LED_STATUS, HIGH);  
        heartbeatTimer = currentMillis;
        heartbeatState = 2;
      }
      break;

    case 2:  
      if (currentMillis - heartbeatTimer >= 100) {
        digitalWrite(CYD_LED_STATUS, LOW);  
        heartbeatTimer = currentMillis;
        heartbeatState = 3;
      }
      break;

    case 3:  
      if (currentMillis - heartbeatTimer >= 80) {
        digitalWrite(CYD_LED_STATUS, HIGH);  
        heartbeatTimer = currentMillis;
        heartbeatState = 0;  
      }
      break;
  }
}

void checkGuestbook() {
  static String lastKnownEntry = "0";
  if (Hacker_Guestbook != "0" && Hacker_Guestbook != lastKnownEntry) {
    addLog("--- [EXTERNAL_INTEGRITY_SYNC] ---");
    addLog("SIGNAL: GREETINGS_FROM_THE_ETHER");
    addLog("MESSAGE: " + String(Hacker_Guestbook));
    addLog("STATUS: RESPECT_LEVEL_MAXIMUM");
    addLog("--- [COFFEE_SERVED] ---");
    lastKnownEntry = Hacker_Guestbook;
  }
}

void loop() {
  checkVaultTrigger();
  checkGuestbook();
  updateHeartbeat();  
  unsigned long currentMillis = millis();
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);
  static int lastCount = 0;
  if (stationList.num > 0 && stationList.num > lastCount) {
    triggerMirror = true;
    addLog("DET: PROXIMITY_DETECTED");
  }
  lastCount = stationList.num;

  // 1. MAC-LAYER PRIVACY (Every 5 min)
  if (currentMillis - lastMacChange >= 300000 || lastMacChange == 0) {
    addLog("IRQ: MAC_SPOOF_TRIGGERED");
    WiFi.softAPdisconnect(true);
    delay(100);
    uint8_t newMAC[6];
    for (int i = 0; i < 6; i++) newMAC[i] = esp_random() % 256;
    newMAC[0] = (newMAC[0] | 0x02) & 0xFE;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_mac(WIFI_IF_AP, &newMAC[0]);
    esp_wifi_start();
    addLog("OP: STACK_REINIT_SUCCESS");
    lastMacChange = currentMillis;
  }

  unsigned long currentInterval = (STATUS_MODE == 1) ? 15000 : 60000;

  // 2. IDENTITY SCHEDULER
  if (currentMillis - lastSsidChange >= currentInterval || lastSsidChange == 0 || triggerMirror) {
    addLog("IRQ: IDENTITY_ROTATION");
    int phraseCount = sizeof(hacker_phrases) / sizeof(hacker_phrases[0]);
    String finalSSID;
    if (triggerMirror) {
      STATUS_MODE = 1;
      addLog("MODE: MAC_REFRACTION");
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      int n = WiFi.scanNetworks(false, false, false, 150);
      if (n > 0) {
        uint8_t* bssid = WiFi.BSSID(0);
        char macPrefix[9];
        sprintf(macPrefix, "%02X:%02X:%02X", bssid[0], bssid[1], bssid[2]);
        finalSSID = String(macPrefix) + "...ICU";
        addLog("MIRROR: OUI -> " + finalSSID);
      } else {
        finalSSID = hacker_phrases[esp_random() % phraseCount];
      }
      WiFi.mode(WIFI_AP_STA);
      triggerMirror = false;
    } else {
      STATUS_MODE = 0;
      addLog("MODE: POOL_ADVERTISEMENT");
      finalSSID = hacker_phrases[esp_random() % phraseCount];
    }
    WiFi.softAPdisconnect(true);
    delay(200);
    String currentPass = generateStrongPassword(22);
    int randomChannel = (esp_random() % 13) + 1;
    if (WiFi.softAP(finalSSID.c_str(), currentPass.c_str(), randomChannel, 0, 1)) {
      addLog("OP: BROADCAST_ACTIVE");
    } else {
      ESP.restart();
    }
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("NEW IDENTITY -> SSID: ");
    Serial.print(finalSSID);
    Serial.print(" | BSSID: ");
    Serial.println(macStr);
    addLog("SSID: " + finalSSID);
    lastSsidChange = currentMillis;
  }

  // 4. RF_EMISSION (SIGNAL BREATHING)
  if (currentMillis - lastTxUpdate >= 5000) {
    lastTxUpdate = currentMillis;
    currentTxPower = (esp_random() % 35) + 44;
    esp_wifi_set_max_tx_power(currentTxPower);
  }
}