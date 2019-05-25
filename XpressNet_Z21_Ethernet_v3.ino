/*
    Z21 Ethernet Emulation für die App-Steuerung via Smartphone über XpressNet.
    Original: http://pgahtow.de/wiki/index.php?title=Z21_mobile#Z21_-_Slave_am_XpressNet
    Modified: by BorgMcz www.dccmm.cz

    Version 3
    Neu:
    - customized only Arduino Uno and Leonardo base board
    - Uno UART 0 - pinRx 0, pinTx 1;  Mega UART 1 - pinRx 19, pinTx 18
 */

//----------------------------------------------------------------------------

#define WebConfig 1    //HTTP Port 80 Website zur Konfiguration

//----------------------------------------------------------------------------

#include <EEPROM.h>

#include <XpressNet.h>
XpressNetClass XpressNet;

//For use with Standard W5100 Library
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library

#define EEip 40    //Startddress im EEPROM für die IP
#define EEXNet 45   //Adresse im XNet-Bus

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[6] = {0xFE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 111);

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

#if WebConfig
  // (port 80 is default for HTTP):
  EthernetServer server(80);
#endif

#define localPort 21105      // Z21 local port to listen on
#define XNetTxRxPin 9    //Send/Receive Pin MAX

#define Z21ResetPin A1  // pin reset to defaut IP
#define RESET_ETH A5		// Connect to reset PIN in ethernet shield

//--------------------------------------------------------------

// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
byte XNetAddress = 25;    //Adresse im XpressNet
#define XBusVer 0x30      //Version XNet-Bus (default 3.0)

// buffers for receiving and sending data
#define UDP_TX_MAX_SIZE 10
unsigned char packetBuffer[UDP_TX_MAX_SIZE]; //buffer to hold incoming packet,
//--> UDP_TX_PACKET_MAX_SIZE

#define maxIP 10        //Speichergröße für IP-Adressen
#define ActTimeIP 20    //Aktivhaltung einer IP für (sec./2)
#define interval 2000   //interval at milliseconds

struct TypeActIP {
  byte ip0;    // Byte IP
  byte ip1;    // Byte IP
  byte ip2;    // Byte IP
  byte ip3;    // Byte IP
  byte BCFlag;  //BoadCastFlag 4. Byte Speichern
  byte time;  //Zeit
};
TypeActIP ActIP[maxIP];    //Speicherarray für IPs

long previousMillis = 0;        // will store last time of IP decount updated


//--------------------------------------------------------------------------------------------
void setup() {
  #if UseEnc28
    /* Disable SD card */
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
  #endif

  pinMode(Z21ResetPin, INPUT_PULLUP);
  delay(50);
  if (digitalRead(Z21ResetPin) == LOW || EEPROM.read(EEXNet) > 32) {
    EEPROM.write(EEXNet, XNetAddress);
    EEPROM.write(EEip, ip[0]);
    EEPROM.write(EEip+1, ip[1]);
    EEPROM.write(EEip+2, ip[2]);
    EEPROM.write(EEip+3, ip[3]);
  }
  XNetAddress = EEPROM.read(EEXNet);
  ip[0] = EEPROM.read(EEip);
  ip[1] = EEPROM.read(EEip+1);
  ip[2] = EEPROM.read(EEip+2);
  ip[3] = EEPROM.read(EEip+3);

  // start the Ethernet and UDP:
    pinMode(RESET_ETH, OUTPUT);
	digitalWrite(RESET_ETH, LOW);		// set reset ETH shield
	delay(2500);
	digitalWrite(RESET_ETH, HIGH);  	// set no reset ETH shield
	delay(2000);

  Ethernet.begin(mac,ip);  //IP and MAC Festlegung
  #if WebConfig
    server.begin();    //HTTP Server
  #endif
  Udp.begin(localPort);  //UDP Z21 Port

  XpressNet.start(XNetAddress, XNetTxRxPin);    //Initialisierung XNet und Send/Receive-PIN

  for (int i = 0; i < maxIP; i++)
    clearIPSlot(i);  //löschen gespeicherter aktiver IP's

}

/*
//--------------------------------------------------------------------------------------------
void notifyXNetVer(uint8_t V, uint8_t ID ) {
}

//--------------------------------------------------------------------------------------------
 void notifyXNetStatus(uint8_t LedState ) {
 }
 */

//--------------------------------------------------------------------------------------------
void loop() {

  XpressNet.receive();  //Check for XpressNet

  Ethreceive();    //Read Data on UDP Port

  XpressNet.receive();  //Check for XpressNet

  #if WebConfig
    Webconfig();    //Webserver for Configuration
  #endif


  //Nicht genutzte IP's aus Speicher löschen
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    for (int i = 0; i < maxIP; i++) {
      if (ActIP[i].ip3 != 0) {  //Slot nicht leer?
        if (ActIP[i].time > 0)
          ActIP[i].time--;    //Zeit herrunterrechnen
        else {
          clearIPSlot(i);   //clear IP DATA
        }
      }
    }
  }
}

//--------------------------------------------------------------------------------------------
void clearIPSlots() {
  for (int i = 0; i < maxIP; i++)
    clearIPSlot(i);
}

//--------------------------------------------------------------------------------------------
//Slot mit Nummer "i" löschen
void clearIPSlot(byte i) {
  ActIP[i].ip0 = 0;
  ActIP[i].ip1 = 0;
  ActIP[i].ip2 = 0;
  ActIP[i].ip3 = 0;
  ActIP[i].BCFlag = 0;
  ActIP[i].time = 0;
}

//--------------------------------------------------------------------------------------------
void clearIPSlot(byte ip0, byte ip1, byte ip2, byte ip3) {
  for (int i = 0; i < maxIP; i++) {
    if (ActIP[i].ip0 == ip0 && ActIP[i].ip1 == ip1 && ActIP[i].ip2 == ip2 && ActIP[i].ip3 == ip3)
      clearIPSlot(i);
  }
}

//--------------------------------------------------------------------------------------------
byte addIPToSlot (byte ip0, byte ip1, byte ip2, byte ip3, byte BCFlag) {
  byte Slot = maxIP;
  for (int i = 0; i < maxIP; i++) {
    if (ActIP[i].ip0 == ip0 && ActIP[i].ip1 == ip1 && ActIP[i].ip2 == ip2 && ActIP[i].ip3 == ip3) {
      ActIP[i].time = ActTimeIP;
      if (BCFlag != 0)    //Falls BC Flag übertragen wurde diesen hinzufügen!
        ActIP[i].BCFlag = BCFlag;
      return ActIP[i].BCFlag;    //BC Flag 4. Byte Rückmelden
    }
    else if (ActIP[i].time == 0 && Slot == maxIP)
      Slot = i;
  }
  ActIP[Slot].ip0 = ip0;
  ActIP[Slot].ip1 = ip1;
  ActIP[Slot].ip2 = ip2;
  ActIP[Slot].ip3 = ip3;
  ActIP[Slot].time = ActTimeIP;
  notifyXNetPower(XpressNet.getPower());
  return ActIP[Slot].BCFlag;   //BC Flag 4. Byte Rückmelden
}

//--------------------------------------------------------------------------------------------
#if WebConfig
void Webconfig() {
  EthernetClient client = server.available();
  if (client) {
    String receivedText = String(50);
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (receivedText.length() < 50) {
          receivedText += c;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          //client.println("Connection: close");  // the connection will be closed after completion of the response
          //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          //Website:
          client.println("<html><head><title>Z21</title></head><body>");
          client.println("<h1>Z21</h1><br />");
          //----------------------------------------------------------------------------------------------------
          int firstPos = receivedText.indexOf("?");
          if (firstPos > -1) {
            client.println("-> accept change after RESET!");
            byte lastPos = receivedText.indexOf(" ", firstPos);
            String theText = receivedText.substring(firstPos+3, lastPos); // 10 is the length of "?A="
             byte XNetPos = theText.indexOf("&XNet=");
            XNetAddress = theText.substring(XNetPos+6).toInt();
            byte Aip = theText.indexOf("&B=");
            byte Bip = theText.indexOf("&C=", Aip);
            byte Cip = theText.indexOf("&D=", Bip);
            byte Dip = theText.substring(Cip+3, XNetPos).toInt();
            Cip = theText.substring(Bip+3, Cip).toInt();
            Bip = theText.substring(Aip+3, Bip).toInt();
            Aip = theText.substring(0, Aip).toInt();
            ip[0] = Aip;
            ip[1] = Bip;
            ip[2] = Cip;
            ip[3] = Dip;
            if (EEPROM.read(EEXNet) != XNetAddress)
              EEPROM.write(EEXNet, XNetAddress);
            if (EEPROM.read(EEip) != Aip)
              EEPROM.write(EEip, Aip);
            if (EEPROM.read(EEip+1) != Bip)
              EEPROM.write(EEip+1, Bip);
            if (EEPROM.read(EEip+2) != Cip)
              EEPROM.write(EEip+2, Cip);
            if (EEPROM.read(EEip+3) != Dip)
              EEPROM.write(EEip+3, Dip);
          }
          //----------------------------------------------------------------------------------------------------
          client.print("<form method=get>IP-Adr.: <input type=number min=10 max=254 name=A value=");
          client.println(ip[0]);
          client.print(">.<input type=number min=0 max=254 name=B value=");
          client.println(ip[1]);
          client.print(">.<input type=number min=0 max=254 name=C value=");
          client.println(ip[2]);
          client.print(">.<input type=number min=0 max=254 name=D value=");
          client.println(ip[3]);
          client.print("><br /> XBus Adr.: <input type=number min=1 max=31 name=XNet value=");
          client.print(XNetAddress);
          client.println("><br /><br />");
          client.println("<input type=submit></form>");
          client.println("</body></html>");
          break;
        }
        if (c == '\n')
          currentLineIsBlank = true; // you're starting a new line
        else if (c != '\r')
          currentLineIsBlank = false; // you've gotten a character on the current line
      }
    }
    client.stop();  // close the connection:
  }
}
#endif

//--------------------------------------------------------------------------------------------
void Ethreceive() {
  int packetSize = Udp.parsePacket();
  if(packetSize > 0) {
    addIPToSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3], 0);
    Udp.read(packetBuffer,UDP_TX_MAX_SIZE);  // read the packet into packetBufffer
    // send a reply, to the IP address and port that sent us the packet we received
    int header = (packetBuffer[3]<<8) + packetBuffer[2];
    //    int datalen = (packetBuffer[1]<<8) + packetBuffer[0];
    byte data[16];
    boolean ok = false;
    switch (header) {
    case 0x10:
      data[0] = 0xF5;  //Seriennummer 32 Bit (little endian)
      data[1] = 0x0A;
      data[2] = 0x00;
      data[3] = 0x00;
      EthSend (0x08, 0x10, data, false, 0x00);
      break;
    case 0x1A:
      data[0] = 0x01;  //HwType 32 Bit
      data[1] = 0x02;
      data[2] = 0x02;
      data[3] = 0x00;
      data[4] = 0x20;  //FW Version 32 Bit
      data[5] = 0x01;
      data[6] = 0x00;
      data[7] = 0x00;
      EthSend (0x0C, 0x1A, data, false, 0x00);
      break;
    case 0x30:
      clearIPSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3]);
      //Antwort von Z21: keine
      break;
      case (0x40):
      switch (packetBuffer[4]) { //X-Header
      case 0x21:
        switch (packetBuffer[5]) {  //DB0
        case 0x21:
          data[0] = 0x63;
          data[1] = 0x21;
          data[2] = XBusVer;   //X-Bus Version
          data[3] = 0x12;  //ID der Zentrale
          EthSend (0x09, 0x40, data, true, 0x00);
          break;
        case 0x24:
          data[0] = 0x62;
          data[1] = 0x22;
          data[2] = XpressNet.getPower();
          //Debug.print("LAN_X_GET_STATUS ");
          //Debug.println(data[2], HEX);
          EthSend (0x08, 0x40, data, true, 0x00);
          break;
        case 0x80:
          XpressNet.setPower(csTrackVoltageOff);
          break;
        case 0x81:
          XpressNet.setPower(csNormal);
          break;
        }
        break;
      case 0x23:
        if (packetBuffer[5] == 0x11) {  //DB0
          byte CV_MSB = packetBuffer[6];
          byte CV_LSB = packetBuffer[7];
          XpressNet.readCVMode(CV_LSB+1);
        }
        break;
      case 0x24:
        if (packetBuffer[5] == 0x12) {  //DB0
          byte CV_MSB = packetBuffer[6];
          byte CV_LSB = packetBuffer[7];
          byte value = packetBuffer[8];
          XpressNet.writeCVMode(CV_LSB+1, value);
        }
        break;
      case 0x43:
        XpressNet.getTrntInfo(packetBuffer[5], packetBuffer[6]);
        break;
      case 0x53:
        XpressNet.setTrntPos(packetBuffer[5], packetBuffer[6], packetBuffer[7] & 0x0F);
        break;
      case 0x80:
        XpressNet.setPower(csEmergencyStop);
        break;
      case 0xE3:
        if (packetBuffer[5] == 0xF0) {  //DB0
          //Antwort: LAN_X_LOCO_INFO  Adr_MSB - Adr_LSB
          XpressNet.getLocoInfo(packetBuffer[6] & 0x3F, packetBuffer[7]);
          XpressNet.getLocoFunc(packetBuffer[6] & 0x3F, packetBuffer[7]);  //F13 bis F28
        }
        break;
      case 0xE4:
        if (packetBuffer[5] == 0xF8) {  //DB0
          //LAN_X_SET_LOCO_FUNCTION  Adr_MSB        Adr_LSB            Type (EIN/AUS/UM)      Funktion
          XpressNet.setLocoFunc(packetBuffer[6] & 0x3F, packetBuffer[7], packetBuffer[8] >> 6, packetBuffer[8] & B00111111);
        }
        else {
          //LAN_X_SET_LOCO_DRIVE            Adr_MSB          Adr_LSB      DB0          Dir+Speed
          XpressNet.setLocoDrive(packetBuffer[6] & 0x3F, packetBuffer[7], packetBuffer[5] & B11, packetBuffer[8]);
        }
        break;
      case 0xE6:
        if (packetBuffer[5] == 0x30) {  //DB0
          byte Option = packetBuffer[8] & B11111100;  //Option DB3
          byte Adr_MSB = packetBuffer[6] & 0x3F;  //DB1
          byte Adr_LSB = packetBuffer[7];    //DB2
          int CVAdr = packetBuffer[9] | ((packetBuffer[8] & B11) << 7);
          if (Option == 0xEC) {
            byte value = packetBuffer[10];  //DB5
          }
          if (Option == 0xE8) {
            //Nicht von der APP Unterstützt
          }
        }
        break;
      case 0xF1:
        data[0] = 0xf3;
        data[1] = 0x0a;
        data[2] = 0x01;   //V_MSB
        data[3] = 0x23;  //V_LSB
        EthSend (0x09, 0x40, data, true, 0x00);
        break;
      }
      break;
      case (0x50):
        addIPToSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3], packetBuffer[4]);
        notifyXNetPower (XpressNet.getPower());  //Zustand Gleisspannung Antworten
      break;
      case (0x51):
        data[0] = 0x00;
        data[1] = 0x00;
        data[2] = 0x00;
        data[3] = addIPToSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3], 0);
        EthSend (0x08, 0x51, data, false, 0x00);
      break;
      case (0x60):
      break;
      case (0x61):
      break;
      case (0x70):
      break;
      case (0x71):
      break;
      case (0x81):
      break;
      case (0x82):
      break;
      case (0x85):
        data[0] = 0x00;  //MainCurrent mA
        data[1] = 0x00;  //MainCurrent mA
        data[2] = 0x00;  //ProgCurrent mA
        data[3] = 0x00;  //ProgCurrent mA
        data[4] = 0x00;  //FilteredMainCurrent
        data[5] = 0x00;  //FilteredMainCurrent
        data[6] = 0x00;  //Temperature
        data[7] = 0x20;  //Temperature
        data[8] = 0x0F;  //SupplyVoltage
        data[9] = 0x00;  //SupplyVoltage
        data[10] = 0x00;  //VCCVoltage
        data[11] = 0x03;  //VCCVoltage
        data[12] = XpressNet.getPower();  //CentralState
        data[13] = 0x00;  //CentralStateEx
        data[14] = 0x00;  //reserved
        data[15] = 0x00;  //reserved
        EthSend (0x14, 0x84, data, false, 0x00);
      break;
      case (0x89):
      break;
      case (0xA0):
      break;
      case (0xA1):
      break;
      case (0xA2):
      break;
      case (0xA3):
      break;
      case (0xA4):
      break;
    default:
      data[0] = 0x61;
      data[1] = 0x82;
      EthSend (0x07, 0x40, data, true, 0x00);
    }
  }
}

//--------------------------------------------------------------------------------------------
void EthSend (unsigned int DataLen, unsigned int Header, byte *dataString, boolean withXOR, byte BC) {
  if (BC != 0x00) {
    IPAddress IPout = Udp.remoteIP();
    for (int i = 0; i < maxIP; i++) {
      if (ActIP[i].time > 0 && ActIP[i].BCFlag >= BC) {    //Noch aktiv?
        IPout[0] = ActIP[i].ip0;
        IPout[1] = ActIP[i].ip1;
        IPout[2] = ActIP[i].ip2;
        IPout[3] = ActIP[i].ip3;
        Udp.beginPacket(IPout, Udp.remotePort());    //Broadcast
        Ethwrite (DataLen, Header, dataString, withXOR);
        Udp.endPacket();
      }
    }
  }
  else {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());    //Broadcast
    Ethwrite (DataLen, Header, dataString, withXOR);
    Udp.endPacket();
  }
}

//--------------------------------------------------------------------------------------------
//Senden von Lokdaten via Ethernet
void Ethwrite (unsigned int DataLen, unsigned int Header, byte *dataString, boolean withXOR) {
  Udp.write(DataLen & 0xFF);
  Udp.write(DataLen >> 8);
  Udp.write(Header & 0xFF);
  Udp.write(Header >> 8);

  unsigned char XOR = 0;
  byte ldata = DataLen-5;  //Ohne Length und Header und XOR
  if (!withXOR)    //XOR vorhanden?
    ldata++;
  for (int i = 0; i < (ldata); i++) {
    XOR = XOR ^ *dataString;
    Udp.write(*dataString);
    dataString++;
  }
  if (withXOR)
    Udp.write(XOR);
}

//--------------------------------------------------------------------------------------------
void notifyXNetPower (uint8_t State)
{
  byte data[] = { 0x61, 0x00  };
  switch (State) {
  case csNormal: data[1] = 0x01;
    break;
  case csTrackVoltageOff: data[1] = 0x00;
    break;
  case csServiceMode: data[1] = 0x02;
    break;
  case csShortCircuit: data[1] = 0x08;
    break;
  case csEmergencyStop:
    data[0] = 0x81;
    data[1] = 0x00;
    break;
  default: return;
  }
  EthSend(0x07, 0x40, data, true, 0x01);
}

//--------------------------------------------------------------------------------------------
void notifyLokFunc(uint8_t Adr_High, uint8_t Adr_Low, uint8_t F2, uint8_t F3 ) {
}

//--------------------------------------------------------------------------------------------
void notifyLokAll(uint8_t Adr_High, uint8_t Adr_Low, boolean Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, boolean Req ) {
  byte DB2 = Steps;
  if (DB2 == 3)  //nicht vorhanden!
    DB2 = 4;
  if (Busy)
    bitWrite(DB2, 3, 1);
  byte DB3 = Speed;
  if (Direction == 1)
    bitWrite(DB3, 7, 1);
  byte data[9];
  data[0] = 0xEF;  //X-HEADER
  data[1] = Adr_High & 0x3F;
  data[2] = Adr_Low;
  data[3] = DB2;
  data[4] = DB3;
  data[5] = F0;    //F0, F4, F3, F2, F1
  data[6] = F1;    //F5 - F12; Funktion F5 ist bit0 (LSB)
  data[7] = F2;  //F13-F20
  data[8] = F3;  //F21-F28
  if (Req == false)  //kein BC
    EthSend (14, 0x40, data, true, 0x00);  //Send Power und Funktions ask App
  else EthSend (14, 0x40, data, true, 0x01);  //Send Power und Funktions to all active Apps
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  byte data[4];
  data[0] = 0x43;  //HEADER
  data[1] = Adr_High;
  data[2] = Adr_Low;
  data[3] = Pos;
  EthSend (0x09, 0x40, data, true, 0x01);
}

//--------------------------------------------------------------------------------------------
void notifyCVInfo(uint8_t State ) {
  if (State == 0x01 || State == 0x02) {  //Busy or No Data
    //LAN_X_CV_NACK
    byte data[2];
    data[0] = 0x61;  //HEADER
    data[1] = 0x13; //DB0
    EthSend (0x07, 0x40, data, true, 0x00);
  }
}

//--------------------------------------------------------------------------------------------
void notifyCVResult(uint8_t cvAdr, uint8_t cvData ) {
  //LAN_X_CV_RESULT
  byte data[5];
  data[0] = 0x64; //HEADER
  data[1] = 0x14;  //DB0
  data[2] = 0x00;  //CVAdr_MSB
  data[3] = cvAdr;  //CVAdr_LSB
  data[4] = cvData;  //Value
  EthSend (0x0A, 0x40, data, true, 0x00);
}

//--------------------------------------------------------------

//--------------------------------------------------------------------------------------------

