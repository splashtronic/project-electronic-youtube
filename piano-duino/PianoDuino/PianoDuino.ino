// Piano sederhana Arduino Nano
// 8 push button (INPUT_PULLUP, aktif LOW), buzzer pasif, 1 LED

const int JUMLAH_TOMBOL = 8;
const int pinTombol[JUMLAH_TOMBOL] = {7, 6, 5, 4, 3, 2, A0, A1};
const int frekuensi[JUMLAH_TOMBOL] = {262, 294, 330, 349, 392, 440, 494, 523}; // C4 sampai C5

const int pinBuzzer = 8;
const int pinLed = LED_BUILTIN;

int tombolAktif = -1; // -1 berarti tidak ada tombol yang ditekan

void setup() {
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    pinMode(pinTombol[i], INPUT_PULLUP);
  }
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinLed, OUTPUT);
}

void loop() {
  // Cari tombol yang sedang ditekan (LOW = ditekan)
  int ditekan = -1;
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    if (digitalRead(pinTombol[i]) == LOW) {
      ditekan = i;
      break; // ambil tombol pertama yang ditemukan
    }
  }

  if (ditekan != -1) {
    // Tombol ditekan: bunyikan nada dan nyalakan LED
    tone(pinBuzzer, frekuensi[ditekan]);
    digitalWrite(pinLed, HIGH);
  } else {
    // Tidak ada tombol ditekan: matikan suara dan LED
    noTone(pinBuzzer);
    digitalWrite(pinLed, LOW);
  }
}