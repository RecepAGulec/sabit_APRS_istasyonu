/* 
   GPS olmadan APRS ile bâzı telemetre verilerinin göndermesi için arduino yazılımı.
   Yazılım, esas olarak koordinatları belli bir yerin bâzı meteorolojik verilerinin APRS sstemi
   üzerinden öğrenilmesi için tasarlanmıştır.
   APRS işaretlerinin gönderlmesi için SA818 modülü kullanılacak.
   - Billyg örneğinde koordinatlar standart şekilde giriliyor, bir NMEA çevirici vasıtası ile
   koordinat NMEA formatına çevriliyordu. Kod tasarrufu açısından koorninat doğrudan NMEA biçiminde
   girilecek şekle getirildi.
   - SA818 modülüne kumanda, Jerome LOYET'in DRA818 kütüphânesi ile gerçekleştirilmiştir.
   https://github.com/fatpat/arduino-dra818
   - Bosch BMP280 algılayıcısı sıcaklık ve basınç ölçümü için kullanılmıştır.
   Devre, bir Li-Ion pil ile beslenmektedir. 
   COMMENT kısmıda Li-ion pil gerilimi, eğer Li-Ion pil bir güneş pili ile dolfurulacaksa panel gerilimii
   hava basıncı ve sıcaklığı değerleri gönderilecek.
   
   TA2EI Recep Aydın GÜLEÇ
   https//qsl.net/ta2ei
   0.8 sürümünde:
   1) BMP280 ile sıcaklık ve basınç ölçümü
   2) Li-Ion pil ve panel gerilimi ölçümü yapılmaktadır.
   Bu sürüm problemsiz çalışmakdadır.

*/
// Arduino APRS Tracker (aat) with Arduino Pro Mini 3.3V/8 MHz
// Fork of my https://github.com/billygr/arduino-aprs-tracker for stationary use (without GPS)
// Update the lat/lon variables with your location

#include <SimpleTimer.h>
#include <LibAPRS.h>
#include <stdio.h>
#include <SoftwareSerial.h>
#include "DRA818.h" // 
#include <Wire.h>
#include <Adafruit_BMP280.h>

// Single shot button
#define BUTTON_PIN 8
#define PD         9                    // SA818'in PD bacağına
#define RX        11                    // SA818'in TX'ine bağlanacak
#define TX        12                    // SA818'in RX'ine bağlanacak
#define PTT       10                    // SA818'in PTT'sine bağlanacak
#define BMP280_ADDRESS (0x76)
// LibAPRS
#define OPEN_SQUELCH false              // Squelch dikkate alınmayacak
#define ADC_REFERENCE REF_5V            // Referans değeri 5v.

// APRS ayarları
char APRS_CALLSIGN[] = "TA2EI";         // Çağrı işaretiniz. KENDİ ÇAĞRI İŞARETİNİZİ GİRİN!
const int APRS_SSID = 1;                // SSID'niz (çağrı işaretinizden sonra görünecek rakkam)
char APRS_SYMBOL = '-';                 // APRS sistemlerinde görünecek sembolünüz (ev, araba vs. gibi)


// Timer
#define TIMER_DISABLED -1
SimpleTimer timer;
char aprs_update_timer_id = TIMER_DISABLED;
bool send_aprs_update = false;
#define CONV_BUF_SIZE 16               // buffer for conversions
static char conv_buf[CONV_BUF_SIZE];

// Gerilim değişkenleri
float temp = 0.0;
float pr1 = 9970.0;                     // A2 girişindeki PR1 direncinin değeri (OHM)                        
float pr2 = 9960.0;                     // A2 girişindeki PR2 direncinin değeri (OHM)   
float pinput_voltage = 0.0;

float br1 = 9940.0;                     // A1 girişindeki BR1 direncinin değeri (OHM)   
float br2 = 9960.0;                     // A1 girişindeki BR2 direncinin değeri (OHM)   
float binput_voltage = 0.0;
int bvyuz;
int pvyuz;
// gerilim değişkenleri buraya kadar

int isicaklik;                          // Sıcaklık
long basinc;                            // Basınç

SoftwareSerial *dra_serial;             // SA818 (DRA818) modülü için seri bağlantı
DRA818 *dra;                            // DRA818 için tanımlama
Adafruit_BMP280 bmp;                    // BMP280 için tanımlama

// SETUP başlancıcı ///////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);
  pinMode(PTT, OUTPUT);                 // PTT'ye kumanda edecek bacak çıkış olarak ayarlanıyor
  digitalWrite(PTT, LOW);               // PTT pasif
  bmp.begin();                          // BMP280 algılayıcısı başlatılıyor                                      
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setCallsign(APRS_CALLSIGN, APRS_SSID);
  APRS_setSymbol(APRS_SYMBOL);
  aprs_update_timer_id = timer.setInterval(15L * 60L * 1000L, setAprsUpdateFlag); // 15L 15 dakikaya tekabül ediyor.
  
  dra_serial = new SoftwareSerial(RX, TX); // Instantiate the Software Serial Object.
  pinMode(PD, OUTPUT);                     // Power control of the DRA818
  digitalWrite(PD, HIGH);                  //
  delay(2000);
  dra = DRA818::configure(dra_serial, DRA818_VHF, 144.800, 144.800, 4, 8, 0, 0, DRA818_12K5, true, true, true, &Serial);
  if (!dra) {
    // Serial.println("DRA modulu calismiyor");
  }
  digitalWrite(PD, LOW);                  // Paket gönderilmiyorken  SA818 modülü bekleme moduna alınıyor
}
//SETUP sonu //////////////////////////////////////////////////////////////////////////////

//XXXXXXXXXXXXXXXXXXXXXXX ANA DÖNGÜ XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
///////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  if (digitalRead(BUTTON_PIN) == 1) {
    while (digitalRead(BUTTON_PIN) == 1) {}; //debounce
    locationUpdate();  //
  }

  if (send_aprs_update) {
    Serial.println(F("APRS UPDATE"));
    locationUpdate();
    send_aprs_update = false;
  }
  timer.run();
}
//XXXXXXXXXXXXXXXXXX ANA DÖNGÜNÜN SONU XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
////////////////////////////////////////////////////////////////////////////////////////////
void aprs_msg_callback(struct AX25Msg *msg) {
}
///////////****************************////////////////////////////////////////////////////
void locationUpdate() {
    digitalWrite(PD, HIGH);                       // SA818 modülü çalışma kipine geçiriliyor 
    delay(2000);
    bolc();                                       // Li-Ion pilin gerilimini ölç
    polc();                                       // Güneş panelinin gerilimini ölç
    bmp_olc();                                    // Basınç ve sıcaklığı ölç
  // Gerilim, sıcaklık ve basınç verilerini hazırlama kısmı //////////////////////////////
  // Verileri hazırlama bölümü için
  // https://forum.arduino.cc/t/including-variables-in-text-with-aprs/597188 adresinden faydalanılmıştır.
  //                           111111111122222222223333333333444444
  //                 0123456789012345678901234567890123456789012345
  char comment [] = "HB:xxxxhPa S:xxx°C Bv:x.xxV Pv:x.xxV R:1150m";  
  // char comment [] = "Pil: x.xxV HB:xxxxhPa S:xxx°C R:1150m";  
// BASINÇ verilerini hazırlama //////////////////////////////////////////////////////
  if (basinc < 1000) {                          // Eğer basınç 1000'den küçük ise
    comment[3] =   ' ';                         // Binler hânesine boşluk yaz.
  }
  else {                                        // Değilse
  comment[3] =  (basinc / 1000) + '0';          // Binler hânesini yaz
  comment[4] =  ((basinc / 100) % 10) + '0';    // yüzler hânesi
  comment[5] =  ((basinc / 10) % 10) + '0';     // onlar hânesi
  comment[6] =  (basinc % 10) + '0';            // birler hânesi
}
  // SICAKLIK verilerini hazırlama ////////////////////////////////////////////////
  if (isicaklik >= 0) {                         // artı dereceler için
      if (isicaklik < 100) {                    // Sıcaklık iki hâneli ise
      comment[13] =  ' ';                       // yüzler hânesine boşluk yaz
      }
      else {                                    // Değilse
      comment[13] =  (isicaklik / 100) + '0';   // Yüzler hânesini yaz
       }       
       if (isicaklik < 10) {                     // Eğer sıcaklık tek hâneli ise
       comment [14] = ' ';                       // Onlar hânesine boşluk yaz
       }
       else {                                    // Değilse
       comment[14] = ((isicaklik / 10) % 10) + '0';    // onlar hânesini yaz.
       }      
       comment[15] =  (isicaklik % 10) + '0';         // birler hânesi
  }
  else {
    // eksi dereceler için
    comment[13] = '-'; // minus sign
    comment[14] = ((-isicaklik) / 10) + '0';      // onlar hânesi
    comment[15] = ((-isicaklik) % 10) + '0';      // birler hânesi
  } 
  // PİL (Bv) verilerini hazırlama ////////////////////////////////////////////////////////
  comment[23] =  (bvyuz / 100) + '0';              // yüzler hânesi
  comment[25] = ((bvyuz / 10) % 10) + '0';         // onlar hânesi
  comment[26] =  (bvyuz % 10) + '0';               // birler hânesi

  // GÜNEŞ PANEL (Pv) verilerini hazırlama //////////////////////////////////////////////
  comment[32] =  (pvyuz / 100) + '0';              // yüzler hânesi
  comment[34] = ((pvyuz / 10) % 10) + '0';         // onlar hânesi
  comment[35] =  (pvyuz % 10) + '0';               // birler hânesi
  // Verileri COMMENT için hazırlama kısmının sonu /////////////////////////////////////

  APRS_setLat("3950.34N");                        // NMEA formatında istasyonun enlem değeri KENDİ İSTASYONUNUZ DEĞERLERİNİ YAZIN!
  APRS_setLon("03233.07E");                       // NMEA formatında istasyonun boylam değeri KENDİ İSTASYONUNUZ DEĞERLERİNİ YAZIN!
  digitalWrite(PTT, HIGH);                        // Paketi göndermeden önce PTT'yi devreye sok
  delay(200);                                     // 200mS bekle

  APRS_sendLoc(comment, strlen(comment));         // Paketi göndermeye başla
  // read TX LED pin and wait till TX has finished (PB5) digital write 13 LED_BUILTIN
  while (bitRead(PORTB, 5));                      //
  delay(500);  //ek
  digitalWrite(PTT, LOW);                         // Gönderme bitti, PTT'yi devreden çıkar.
  digitalWrite(PD, LOW);                          // SA818 modülü beklemeye alınıyor.
}
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void setAprsUpdateFlag() {
  send_aprs_update = true;
}
///////////////// EKLEMELER ////////////////////////////////////////////////////////
// Güneş Paneli gerilimini ölçme yordamı ///////////////////////////////////////////

  void polc() {
  int analog_value = analogRead(A2);
  temp = (analog_value * 5) / 1024.0;
  pinput_voltage = temp / (pr2 / (pr1 + pr2));
  pvyuz = (int(pinput_voltage *100));
  delay(100);
  }
// Güneş Paneli gerilimini ölçme yordamının sonu /////////////////////////////
////// Li-Ion pil gerilimini ölçme yordamı ///////////////////////////////////
void bolc() {
  int analog_value = analogRead(A1);
  temp = (analog_value * 5) / 1024.0;
  binput_voltage = temp / (br2 / (br1 + br2));
  bvyuz = (int(binput_voltage * 100));
  delay(100);
}
// Li-Ion pil gerilimini ölçme yordamının sonu ///////////////////////////////
///// BMP280 ile basınç ve sıcaklık ölçme yordamı ////////////////////////////
void bmp_olc() {
  isicaklik = bmp.readTemperature();
  basinc = ((bmp.readPressure() / 100)+131); //
 
  delay(500);
// /////////////////////
  if (isnan(isicaklik) || isnan(basinc))  {   // Sistem düzgün çalışıyorsa bu satırı
     Serial.println(F("BMP280 okunamadı!"));   // ve bu satırı silebilirsiniz.
  }
/////////////////////////////////////////////////  
}
///// BMP280 ile basınç ve sıcaklık ölçme yordamının sonu ////////////////////
//////////////////////////////////////////////////////////////////////////////
