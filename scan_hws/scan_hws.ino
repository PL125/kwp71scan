const int K_IN = 0;
const int K_OUT = 1;

boolean initialized = false;  // 5baud init status
byte bc = 1;                   // block counter

/* Supported PIDs */
#define ENGINE_RPM 0x0C
#define VEHICLE_SPEED 0x0D
#define ENGINE_COOLANT_TEMP 0x05
#define MAF_AIRFLOW 0x10
#define THROTTLE_POS 0x11


const byte WAIT = 8;        // wait time
const int TIME_OUT = 1000;  // loop time out
const byte EOM = 3;         // end of block data

void setup() {
  pinMode(K_OUT, OUTPUT);
  pinMode(K_IN, INPUT);

  Serial.begin(4800);

  //clear rx buffer
  clear_buffer();
}

void loop() {
  delay(1000);
  // Wake up DIAG unit
  wake_up();
  
  // put your main code here, to run repeatedly:
  delay(3000);

  //init
  if ( !initialized ) {
    bc = 1; //reset block counter
    kw_init();
  }

  //Get information

}

/* kw-71 init */
void kw_init() {
  int b = 0;
  byte kw1, kw2, kw3, kw4, kw5;

  clear_buffer();
  //  serial_tx_off(); //disable UART so we can "bit-Bang" the slow init.
  //  serial_rx_off();

  delay(2600); //k line should be free of traffic for at least two secconds.

  // drive K line high for 300ms
  digitalWrite(K_OUT, HIGH);
  delay(300);

  //send Motronic address(0x10)
  bitbang(0x10);

  // switch now to 4800 bauds
  Serial.begin(4800);
  Serial.begin(115200);   //for Debug

  // wait for 0x55 from the ECU (up to 300ms) ECUから0x55を待つ（最大300ms）
  //since our time out for reading is 125ms, we will try it three times
  byte tryc = 0;
  while (b != 0x55 && tryc < 6) {
    b = read_byte();
    tryc++;
  }
  if (b != 0x55) {
    initialized = false;
    return -1;
  }
  // wait for kw1 and kw2
  kw1 = read_byte();
  kw2 = read_byte();
  kw3 = read_byte();
  kw4 = read_byte();
  kw5 = read_byte();

  delay(WAIT);
  //response to ECU
  if (kw2 != 0x00) {
    send_byte(kw2 ^ 0xFF);
  }

  //recieve ECU hardware version
  if (! rcv_block()) {
    initialized = false;
    Serial.println("H/W init fail"); //DEBUG
    clear_buffer();
    return - 1;
  }

  //recieve ECU Software version
  if (! rcv_block()) {
    initialized = false;
    Serial.println("S/W init fail"); //DEBUG
    clear_buffer();
    return - 1;
  }

  //recieve ECU Software version
  if (! rcv_block()) {
    initialized = false;
    Serial.println("??? init fail"); //DEBUG
    clear_buffer();
    return - 1;
  }

  // init OK!
  initialized = false;
  return 0;
}




// データ受信を行う。
bool rcv_block() {
  byte bsize = 0x00;  //block data size
  byte t = 0;
  while (t != TIME_OUT  && (Serial.available() == 0)) {  //wait data
    delay(1);
    t++;
  }

  // In kw-71, the first byte of block data is the number of data bytes
  bsize = read_byte();
  delay(WAIT);
  send_byte( bsize ^ 0xFF );  //return byte

  byte b[24];
  for (byte i = 0; i < bsize; i++) {
    b[i] = read_byte();

    //03 = last は返信しない
    if ( i != (bsize - 1) ) {
      send_byte( b[i] ^ 0xFF );
    }
  }

  //最終0x03を受け取れていたら正常とみなす
  if( b[(bsize - 1)] == EOM ) {
    bc = b[0];
    send_ack();
  Serial.println("rcv_block true"); //DEBUG
    return true;
  }

  Serial.println("rcv_block false"); //DEBUG
  return false;
}

void send_ack() {
  send_byte( 0x03 );
  read_byte();
  send_byte( bc + 1 );
  read_byte();
  send_byte( 0x09 );
  read_byte();
  send_byte( 0x03 );
}

void clear_buffer() {
  byte b;
  Serial.println(Serial.available()); //debug
  while (Serial.available() > 0) {
    b = Serial.read();
    Serial.println("clear: " + b); //DEBUG
  }
}

void bitbang(byte b) {
  // send byte at 5 bauds
  // start bit
  digitalWrite(K_OUT, LOW);
  delay(200);
  // data
  for (byte mask = 0x01; mask; mask <<= 1) {
    if (b & mask) { // choose bit
      digitalWrite(K_OUT, HIGH); // send 1
    } else {
      digitalWrite(K_OUT, LOW); // send 0
    }
    delay(200);
  }
  // stop bit(200ms) + 190 ms delay
  digitalWrite(K_OUT, HIGH);
  delay(390);
}

void serial_rx_off() {
  UCSR0B &= ~(_BV(RXEN0));  //disable UART RX
  delay(15);                 //allow time for buffers to flush
}

void serial_tx_off() {
  UCSR0B &= ~(_BV(TXEN0));  //disable UART TX
  delay(15);                 //allow time for buffers to flush
}

void serial_rx_on() {
  Serial.begin(4800);   //setting enable bit didn't work, so do beginSerial
  Serial.begin(115200);
}

int read_byte() {
  int b = -1;
  byte t = 0;
  while (t != 125  && (b = Serial.read()) == -1) {
    delay(1);
    t++;
  }
  if (t >= 125) { //Read Time Out
    b = 0;
  }
  if ( b == 0xFF) {
    Serial.println("read_byte " + String(b, HEX)); //DEBUG
    b = read_byte();
  }
  Serial.println("b:" + String(b, HEX)); //DEBUG
  return b;
}

void send_byte(byte b) {
  serial_rx_off();
  Serial.write(b);
  delay(WAIT);    // ISO requires 5-20 ms delay between bytes.
  serial_rx_on();
}

void wake_up() {
  serial_tx_off();
  serial_rx_off();
  // drive K line high for 300ms
  digitalWrite(K_OUT, HIGH);
  delay(300);
  digitalWrite(K_OUT, LOW);
  delay(1800);
  // stop bit + 60 ms delay
  digitalWrite(K_OUT, HIGH);
  delay(260);
}
