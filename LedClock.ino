//////////////////
//-- LEDCLOCK --//-- Edulab Rennes 2 Juin 2024 --//
//////////////////-- Inspiré de Neopixel-7-Segment-Digital-Clock par Sayantan Pal
//////////////////-- Avec l'aimable contribution du code (heure été/hiver) d'Emmanuel Costes

#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

//////////////////////
//-- CONFIG PERSO --//-- WIFI personnel

// Définir le SSID et le mot de passe du Wi-Fi
const char *ssid = "Edulab AP";
const char *password = "openlabrennes2";

//-- CONFIG PERSO --//-- Branchement, électronique et matériel

#define PIXEL_PIN 15          // Broche où est connectée la LED (15 = D8)
#define LDR_PIN A0            // Broche du capteur de lumière (LDR)
#define PIXEL_PER_SEGMENT 5   // Nombre de LEDs par segment
#define PIXEL_DIGITS 4        // Nombre de chiffres à 7 segments
#define PIXEL_DASH 0          // Nombre de divisions (1 division entre hr et min || 2 divisions en hr, min, sec)
#define AUTO_BRIGHT 1         // Luminosité automatique (1 = on || 0 = off)
#define SENSOR_VALUE_MAX 900  // Valeur max de lumière captée dans la pièce de l'horloge (LDR)
#define SENSOR_VALUE_MIN 600  // Valeur mini de lumière captée dans la pièce de l'horloge (LDR)
#define BRIGHT_MAX 128        // Luminosité max pour l'affichage de l'heure (en journée)
#define BRIGHT_MIN 64         // Luminosité mini pour l'affichage de l'heure (de nuit)
#define BRIGHT_ZERO 1         // Si la luminosité est absente alors elle passe à ce seuil (sécurité)

//-- CONFIG PERSO --//-- Paramétrage de l'horloge

// Luminosité par défaut
int Brightness = 32;

// Fréquence de mise à jour
int period = 1;  // Intervalle de mise à jour en secondes

// Couleur par défaut (violet : rouge 255, vert 0, bleu 255)
uint8_t colorR = 255;  // rouge (255 = 100%)
uint8_t colorG = 0;    // vert (255 = 100%)
uint8_t colorB = 255;  // bleu (255 = 100%)

// Définir le décalage GMT
float gmtOffsetHours = 1;  // Définir ici votre décalage GMT en heures (ex. GMT+5.5 = 5.5)
int ChangeHeure = 1;       // Changement d'heure été/hiver automatique (1 = on || 0 = off)

// Format de l'heure
int timeFormat = 24;  // 12 = format 12 heures || 24 = format 24 heures

//-- FIN DE LA CONFIG --//
//////////////////////////

int gmtOffset = gmtOffsetHours * 3600;  // Conversion du décalage horaire en secondes
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset, 60000);  // Utilisation de la variable gmtOffset

Adafruit_NeoPixel strip((PIXEL_PER_SEGMENT * 7 * PIXEL_DIGITS) + (PIXEL_DASH * 2), PIXEL_PIN, NEO_GRB + NEO_KHZ800);

unsigned long time_now = 0;
int Second, Minute, Hour;

// Tableau des chiffres
byte digits[12] = {
  //abcdefg
  0b1111110,  // 0
  0b0011000,  // 1
  0b0110111,  // 2
  0b0111101,  // 3
  0b1011001,  // 4
  0b1101101,  // 5
  0b1101111,  // 6
  0b0111000,  // 7
  0b1111111,  // 8
  0b1111101,  // 9
  0b1100110,  // C
  0b1100011,  // F
};

// Fonction pour vérifier l'heure d'été en France
bool _RTCete(uint8_t anneeUTC, uint8_t moisUTC, uint8_t jourUTC, uint8_t heureUTC) {
  // En France :
  // L'heure d'hiver passe à l'heure d'été le dernier dimanche de mars à 1:00 UTC (à 2:00 heure locale, il est 3:00)
  // L'heure d'été passe à l'heure d'hiver le dernier dimanche d'octobre à 1:00 UTC (à 3:00 heure locale, il est 2:00)
  const uint8_t MARS = 3;
  const uint8_t OCTOBRE = 10;
  if (moisUTC == MARS) {
    uint8_t dernierDimancheMars = 31 - ((5 + anneeUTC + (anneeUTC >> 2)) % 7);
    return jourUTC > dernierDimancheMars || (jourUTC == dernierDimancheMars && heureUTC != 0);
  }
  if (moisUTC == OCTOBRE) {
    uint8_t dernierDimancheOctobre = 31 - ((2 + anneeUTC + (anneeUTC >> 2)) % 7);
    return jourUTC < dernierDimancheOctobre || (jourUTC == dernierDimancheOctobre && heureUTC == 0);
  }
  return MARS < moisUTC && moisUTC < OCTOBRE;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  strip.begin();
  strip.clear();

  WiFi.begin(ssid, password);
  Serial.print("Connexion en cours ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connecté !");
  timeClient.begin();
  delay(100);
  Serial.println();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {  // vérifier l'état de la connexion Wi-Fi
    if (AUTO_BRIGHT == 1) {
      int sensor_val = analogRead(LDR_PIN);
      //Serial.println(sensor_val);
      Brightness = map(sensor_val, SENSOR_VALUE_MIN, SENSOR_VALUE_MAX, BRIGHT_MIN, BRIGHT_MAX);
      if (Brightness <= 1) Brightness = BRIGHT_ZERO;
      //Serial.println(Brightness);
    }

    timeClient.update();
    unsigned long unix_epoch = timeClient.getEpochTime();  // obtenir l'heure Unix
    time_t epoch_time = static_cast<time_t>(unix_epoch);   // convertir en time_t
    struct tm *tm_struct = gmtime(&epoch_time);            // convertir en structure tm UTC

    // Vérifier le changement d'heure en France
    bool is_ete = _RTCete(tm_struct->tm_year % 100, tm_struct->tm_mon + 1, tm_struct->tm_mday, tm_struct->tm_hour);
    int ChangeHeureOffset;
    if (ChangeHeure == 1) {        // le changement d'heure est il activé ?   
      if (is_ete) {
        ChangeHeureOffset = 1;     // GMT + 1 si l'heure est en été
      } else {
        ChangeHeureOffset = 0;     // GMT + 0 si l'heure est en hivers
      }
    }
    tm_struct->tm_hour += ChangeHeureOffset;
    mktime(tm_struct);  // normaliser la structure tm

    Second = tm_struct->tm_sec;  // obtenir les secondes
    Minute = tm_struct->tm_min;  // obtenir les minutes
    Hour = tm_struct->tm_hour;   // obtenir les heures

    if (timeFormat == 12) {     // 12 ou 24 H ?
      if (Hour > 12) {
        Hour -= 12;
      }
    }
  }

  while (millis() > time_now + period * 1000) {
    time_now = millis();
    disp_Time();  // Afficher l'heure
  }
}

void disp_Time() {
  static int lastSecond = -1;
  if (Second != lastSecond) {
    lastSecond = Second;
    writeDigit(0, Hour / 10);     // Afficher le premier chiffre de l'heure
    writeDigit(1, Hour % 10);     // Afficher le second chiffre de l'heure
    writeDigit(2, Minute / 10);   // Afficher le premier chiffre des minutes
    writeDigit(3, Minute % 10);   // Afficher le second chiffre des minutes
    writeDigit(4, Second / 10);   // Afficher le premier chiffre des secondes
    writeDigit(5, Second % 10);   // Afficher le second chiffre des secondes
    disp_Dash();                  // Faire clignotter les points
    strip.show();                 // Afficher !!
  }
}


void disp_Dash() {
  static bool lastSecondEven = false;
  bool currentSecondEven = (Second % 2 == 0);

  if (currentSecondEven != lastSecondEven) {
    lastSecondEven = currentSecondEven;
    int dot, dash;
    if (PIXEL_DASH != 0) {
      for (int i = 0; i < 2; i++) {
        dot = 2 * (PIXEL_PER_SEGMENT * 7) + i;
        for (int j = 0; j < PIXEL_DASH; j++) {
          dash = dot + j * (2 * (PIXEL_PER_SEGMENT * 7) + 2);
          currentSecondEven ? strip.setPixelColor(dash, strip.Color(colorR * Brightness / 255, colorG * Brightness / 255, colorB * Brightness / 255)) : strip.setPixelColor(dash, strip.Color(0, 0, 0));  // Couleur configurable pour l'affichage du tiret
        }
      }
    }
    strip.show();
  }
}

void writeDigit(int index, int val) {
  byte digit = digits[val];
  int margin;
  if (index == 0 || index == 1) margin = 0;
  if (index == 2 || index == 3) margin = 1;
  if (index == 4 || index == 5) margin = 2;
  if (PIXEL_DASH == 0) margin = 0;
  for (int i = 6; i >= 0; i--) {
    int ChangeHeureOffset = index * (PIXEL_PER_SEGMENT * 7) + i * PIXEL_PER_SEGMENT + 0 * 2;
    uint32_t color;
    if (digit & 0x01 != 0) {
      // Gestion de la couleur ici
      color = strip.Color(colorR * Brightness / 255, colorG * Brightness / 255, colorB * Brightness / 255);  // Couleur configurable pour l'affichage des chiffres
    } else {
      color = strip.Color(0, 0, 0);
    }

    for (int j = ChangeHeureOffset; j < ChangeHeureOffset + PIXEL_PER_SEGMENT; j++) {
      strip.setPixelColor(j, color);
    }
    digit = digit >> 1;
  }
}
