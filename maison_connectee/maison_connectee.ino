/*
  Maison connectée - ESP32S NodeMCU - Étape 2 : ordres du site, lampe auto, liste de badges
  Météo : DHT11 (P4) + photorésistance GL5528 (P34)
  Accès : RFID RC522 (P5, P18, P19, P23, P22) + servo (P13)
  Lampe : P27 (LED ou module relais)
  Alerte : buzzer actif (P26)
  Indicateurs : LED verte (P25) + LED rouge (P33)

  Bibliothèques à installer :
   - MFRC522 (par GithubCommunity)
   - DHT sensor library (par Adafruit) + Adafruit Unified Sensor
   - ESP32Servo (par Kevin Harrington)
  Carte à choisir : "ESP32 Dev Module"
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ---------- Wi-Fi et serveur ----------
const char* WIFI_SSID     = "AL-AMINE";
const char* WIFI_PASSWORD = "00000000";

// Test local : "http://192.168.1.100:3000"
// En ligne   : "https://mon-site.onrender.com"
const char* SERVER_URL = "http://192.168.1.100:3000";

#define TIMEOUT_HTTP_MS 2000

// ---------- Broches ----------
#define PIN_SS        5
#define PIN_RST       22
#define PIN_DHT       4
#define PIN_LDR       34
#define PIN_SERVO     13
#define PIN_LAMPE     27
#define PIN_BUZZER    26
#define PIN_LED_VERT  25
#define PIN_LED_ROUGE 33

// ---------- Réglages ----------
#define LAMPE_NIVEAU_ON      HIGH     // mettez LOW si votre module relais s'active à l'état bas
#define SEUIL_ALLUMER        800      // lampe auto : allumer sous cette valeur
#define SEUIL_ETEINDRE       1500     // lampe auto : éteindre au-dessus de cette valeur
#define SEUIL_NUIT           800
#define ANGLE_FERME          0
#define ANGLE_OUVERT         90
#define DUREE_OUVERTURE_MS   5000
#define INTERVALLE_LDR_MS    1000
#define INTERVALLE_METEO_MS  60000
#define INTERVALLE_ORDRES_MS 3000
#define INTERVALLE_BADGES_MS 45000
#define INTERVALLE_RETRY_MS  10000    // attente après un échec serveur
#define DUREE_LED_MS         2000
#define NB_REFUS_ALARME      3
#define FENETRE_REFUS_MS     60000
#define DUREE_ALARME_MS      10000
#define MAX_BADGES           50

// Liste de secours, utilisée tant que la liste du serveur n'a pas été téléchargée
const char* BADGES_SECOURS[] = { "35 4F 35 03" };
const int NB_BADGES_SECOURS = sizeof(BADGES_SECOURS) / sizeof(BADGES_SECOURS[0]);

// ---------- Objets ----------
MFRC522 rfid(PIN_SS, PIN_RST);
DHT dht(PIN_DHT, DHT11);
Servo servo;

// ---------- Variables d'état ----------
// Porte
unsigned long tOuverture = 0;
bool porteOuverte = false;

// LED
unsigned long tLed = 0;
bool ledActive = false;

// Buzzer non bloquant
int bipsRestants = 0;
int bipDureeMs = 100;
int bipPauseMs = 80;
bool buzzerAllume = false;
unsigned long tBuzzer = 0;

// Alarme (3 refus)
bool alarmeActive = false;
unsigned long tAlarmeDebut = 0;
int nbRefus = 0;
unsigned long tPremierRefus = 0;

// Lampe
String modeLampe = "auto";              // "auto", "on" ou "off" (donné par le site)
bool lampeAllumee = false;
int lumiere = 0;
unsigned long tLdr = 0;

// Serveur
unsigned long tMeteo = 0;
unsigned long tOrdres = 0;
unsigned long tBadges = 0;
unsigned long delaiOrdres = INTERVALLE_ORDRES_MS;
unsigned long delaiBadges = 0;

// Liste des badges gardée en mémoire
String listeBadges[MAX_BADGES];
int nbBadges = 0;
bool listeTelechargee = false;

// ---------- Wi-Fi ----------
void connecterWiFi(unsigned long timeoutMs) {
  Serial.print("Connexion a ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long debut = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - debut < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connecte ! IP : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi indisponible : mode local (la porte fonctionne quand meme)");
  }
}

// Requête GET ou POST. Renvoie le code HTTP (ou -1). Le corps de la réponse va dans *reponse.
int requete(bool post, const char* route, const String& json, String* reponse) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  String url = String(SERVER_URL) + route;
  WiFiClient clientSimple;
  WiFiClientSecure clientSecurise;
  HTTPClient http;

  bool ok;
  if (url.startsWith("https")) {
    clientSecurise.setInsecure();
    ok = http.begin(clientSecurise, url);
  } else {
    ok = http.begin(clientSimple, url);
  }
  if (!ok) return -1;

  http.setConnectTimeout(TIMEOUT_HTTP_MS);
  http.setTimeout(TIMEOUT_HTTP_MS);

  int code;
  if (post) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(json);
  } else {
    code = http.GET();
  }

  if (code == 200 && reponse) *reponse = http.getString();
  if (code != 200) Serial.printf("Erreur %s %s -> %d\n", post ? "POST" : "GET", route, code);
  http.end();
  return code;
}

// ---------- Buzzer non bloquant ----------
void demarrerBips(int nombre, int dureeMs, int pauseMs) {
  bipsRestants = nombre;
  bipDureeMs = dureeMs;
  bipPauseMs = pauseMs;
  buzzerAllume = false;
  tBuzzer = millis() - pauseMs;
}

void gererBuzzer() {
  unsigned long maintenant = millis();

  if (alarmeActive) {
    if (maintenant - tAlarmeDebut >= DUREE_ALARME_MS) {
      alarmeActive = false;
      digitalWrite(PIN_BUZZER, LOW);
      buzzerAllume = false;
    } else if (maintenant - tBuzzer >= 300) {
      tBuzzer = maintenant;
      buzzerAllume = !buzzerAllume;
      digitalWrite(PIN_BUZZER, buzzerAllume);
    }
    return;
  }

  if (bipsRestants > 0 || buzzerAllume) {
    if (!buzzerAllume && maintenant - tBuzzer >= (unsigned long)bipPauseMs) {
      digitalWrite(PIN_BUZZER, HIGH);
      buzzerAllume = true;
      tBuzzer = maintenant;
    } else if (buzzerAllume && maintenant - tBuzzer >= (unsigned long)bipDureeMs) {
      digitalWrite(PIN_BUZZER, LOW);
      buzzerAllume = false;
      bipsRestants--;
      tBuzzer = maintenant;
    }
  }
}

// ---------- LED ----------
void allumerLed(int pin) {
  digitalWrite(PIN_LED_VERT, LOW);
  digitalWrite(PIN_LED_ROUGE, LOW);
  digitalWrite(pin, HIGH);
  tLed = millis();
  ledActive = true;
}

void gererLed() {
  if (ledActive && millis() - tLed >= DUREE_LED_MS) {
    digitalWrite(PIN_LED_VERT, LOW);
    digitalWrite(PIN_LED_ROUGE, LOW);
    ledActive = false;
  }
}

// ---------- Lampe ----------
void commanderLampe(bool allumer) {
  lampeAllumee = allumer;
  digitalWrite(PIN_LAMPE, allumer ? LAMPE_NIVEAU_ON : !LAMPE_NIVEAU_ON);
}

void gererLampe() {
  if (millis() - tLdr < INTERVALLE_LDR_MS) return;
  tLdr = millis();
  lumiere = analogRead(PIN_LDR);

  bool voulu = lampeAllumee;
  if (modeLampe == "on") {
    voulu = true;
  } else if (modeLampe == "off") {
    voulu = false;
  } else {                                      // automatique avec deux seuils (hystérésis)
    if (lumiere < SEUIL_ALLUMER) voulu = true;
    else if (lumiere > SEUIL_ETEINDRE) voulu = false;
  }

  if (voulu != lampeAllumee) {
    commanderLampe(voulu);
    Serial.printf("Lampe %s (mode %s, luminosite %d)\n",
                  voulu ? "ALLUMEE" : "ETEINTE", modeLampe.c_str(), lumiere);
  }
}

// ---------- Badges ----------
String lireUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

bool badgeAutorise(const String& uid) {
  if (!listeTelechargee) {                      // pas encore de liste du serveur : liste de secours
    for (int i = 0; i < NB_BADGES_SECOURS; i++) {
      if (uid.equalsIgnoreCase(BADGES_SECOURS[i])) return true;
    }
    return false;
  }
  for (int i = 0; i < nbBadges; i++) {
    if (uid.equalsIgnoreCase(listeBadges[i])) return true;
  }
  return false;
}

// Réponse attendue : {"badges":["35 4F 35 03","AA BB CC DD"]}
bool lireListeBadges(const String& corps) {
  int debut = corps.indexOf('[');
  int fin = corps.indexOf(']');
  if (debut < 0 || fin < debut) return false;

  int n = 0;
  int pos = debut;
  while (n < MAX_BADGES) {
    int a = corps.indexOf('"', pos);
    if (a < 0 || a > fin) break;
    int b = corps.indexOf('"', a + 1);
    if (b < 0 || b > fin) break;
    listeBadges[n++] = corps.substring(a + 1, b);
    pos = b + 1;
  }
  nbBadges = n;
  listeTelechargee = true;
  return true;
}

bool telechargerBadges() {
  String corps;
  if (requete(false, "/api/badges-actifs", "", &corps) != 200) return false;
  if (!lireListeBadges(corps)) return false;
  Serial.printf("Liste des badges mise a jour : %d badge(s)\n", nbBadges);
  return true;
}

// ---------- Porte ----------
void ouvrirPorte() {
  servo.write(ANGLE_OUVERT);
  porteOuverte = true;
  tOuverture = millis();
  Serial.println("Porte ouverte");
}

void fermerPorte() {
  servo.write(ANGLE_FERME);
  porteOuverte = false;
  Serial.println("Porte fermée");
}

void envoyerAcces(const String& uid, bool autorise, const char* cause) {
  String json = "{\"uid\":\"" + uid + "\"" +
                ",\"autorise\":" + (autorise ? "true" : "false") +
                ",\"cause\":\"" + cause + "\"}";
  int code = requete(true, "/api/acces", json, nullptr);
  Serial.printf("POST /api/acces -> %d\n", code);
}

void declencherAlarme() {
  Serial.println("!!! ALARME : 3 refus consecutifs !!!");
  alarmeActive = true;
  tAlarmeDebut = millis();
  tBuzzer = millis();
  bipsRestants = 0;
  int code = requete(true, "/api/alarme", "{\"type\":\"3_refus\"}", nullptr);
  Serial.printf("POST /api/alarme -> %d\n", code);
}

void gererRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = lireUID();
  Serial.println("Badge détecté : " + uid);

  if (badgeAutorise(uid)) {
    Serial.println("Accès AUTORISÉ");
    allumerLed(PIN_LED_VERT);
    nbRefus = 0;
    if (alarmeActive) {
      alarmeActive = false;
      digitalWrite(PIN_BUZZER, LOW);
      buzzerAllume = false;
    }
    demarrerBips(1, 100, 0);
    ouvrirPorte();
    envoyerAcces(uid, true, "badge");
  } else {
    Serial.println("Accès REFUSÉ");
    allumerLed(PIN_LED_ROUGE);
    demarrerBips(3, 80, 80);

    unsigned long maintenant = millis();
    if (nbRefus == 0 || maintenant - tPremierRefus > FENETRE_REFUS_MS) {
      nbRefus = 1;
      tPremierRefus = maintenant;
    } else {
      nbRefus++;
    }

    envoyerAcces(uid, false, "badge");

    if (nbRefus >= NB_REFUS_ALARME) {
      nbRefus = 0;
      declencherAlarme();
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ---------- Météo ----------
void envoyerMeteo() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  Serial.println("----- Météo -----");
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Erreur de lecture du DHT11 (vérifiez le câblage)");
    return;
  }
  Serial.printf("Température : %.1f °C\n", temp);
  Serial.printf("Humidité    : %.0f %%\n", hum);
  Serial.printf("Luminosité  : %d (%s)\n", lumiere, lumiere < SEUIL_NUIT ? "NUIT" : "JOUR");

  String json = "{\"temperature\":" + String(temp, 1) +
                ",\"humidite\":" + String(hum, 0) +
                ",\"luminosite\":" + String(lumiere) + "}";
  int code = requete(true, "/api/meteo", json, nullptr);
  Serial.printf("POST /api/meteo -> %d\n", code);
}

// ---------- Ordres du site ----------
// Réponse attendue : {"modeLampe":"auto","ouvrirPorte":false}
bool lireOrdres() {
  String corps;
  if (requete(false, "/api/ordres", "", &corps) != 200) return false;

  String nouveauMode = modeLampe;
  if (corps.indexOf("\"modeLampe\":\"on\"") >= 0) nouveauMode = "on";
  else if (corps.indexOf("\"modeLampe\":\"off\"") >= 0) nouveauMode = "off";
  else if (corps.indexOf("\"modeLampe\":\"auto\"") >= 0) nouveauMode = "auto";

  if (nouveauMode != modeLampe) {
    modeLampe = nouveauMode;
    Serial.println("Mode lampe : " + modeLampe);
    tLdr = 0;                                   // applique le nouveau mode tout de suite
  }

  if (corps.indexOf("\"ouvrirPorte\":true") >= 0) {
    Serial.println("Ordre du site : ouverture de la porte");
    allumerLed(PIN_LED_VERT);
    demarrerBips(1, 100, 0);
    ouvrirPorte();
    envoyerAcces("", true, "site");
  }
  return true;
}

bool envoyerEtatReel() {
  String json = "{\"lampeAllumee\":" + String(lampeAllumee ? "true" : "false") +
                ",\"modeActuel\":\"" + modeLampe + "\"" +
                ",\"porteOuverte\":" + (porteOuverte ? "true" : "false") + "}";
  return requete(true, "/api/etat-reel", json, nullptr) == 200;
}

void gererServeur() {
  if (WiFi.status() != WL_CONNECTED) return;    // hors Wi-Fi : rien à tenter, la porte reste locale

  unsigned long maintenant = millis();

  if (maintenant - tOrdres >= delaiOrdres) {
    tOrdres = maintenant;
    bool ok = lireOrdres();
    if (ok) ok = envoyerEtatReel();
    delaiOrdres = ok ? INTERVALLE_ORDRES_MS : INTERVALLE_RETRY_MS;   // évite de bloquer le lecteur si le serveur est injoignable
  }

  if (maintenant - tBadges >= delaiBadges) {
    tBadges = maintenant;
    delaiBadges = telechargerBadges() ? INTERVALLE_BADGES_MS : INTERVALLE_RETRY_MS;
  }
}

// ---------- Programme principal ----------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LAMPE, OUTPUT);
  commanderLampe(false);                        // lampe éteinte au démarrage

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_LED_VERT, OUTPUT);
  pinMode(PIN_LED_ROUGE, OUTPUT);
  digitalWrite(PIN_LED_VERT, LOW);
  digitalWrite(PIN_LED_ROUGE, LOW);

  dht.begin();
  lumiere = analogRead(PIN_LDR);

  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO, 500, 2400);
  servo.write(ANGLE_FERME);

  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial();

  connecterWiFi(15000);                         // abandonne après 15 s : la porte reste utilisable

  tMeteo = millis() - INTERVALLE_METEO_MS + 5000;   // première météo 5 s après le démarrage
  Serial.println("Système prêt. Passez un badge.");
}

void loop() {
  gererRFID();
  gererLampe();
  gererBuzzer();
  gererLed();

  if (porteOuverte && millis() - tOuverture >= DUREE_OUVERTURE_MS) {
    fermerPorte();
  }

  gererServeur();

  if (millis() - tMeteo >= INTERVALLE_METEO_MS) {
    tMeteo = millis();
    envoyerMeteo();
  }
}