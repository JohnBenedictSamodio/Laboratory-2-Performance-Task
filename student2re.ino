// ============================================================
// ESP32 UART → SPI → UART COMMUNICATION SYSTEM
//
// STUDENT 2 — UART / SPI BRIDGE
//
// FUNCTION:
//
// UART RX from Student 1
//          ↓
//       Student 2
//          ↓
//      SPI MASTER
//          ↓
//       Student 3
//
// RETURN PATH:
//
// Student 3
//    ↓
// SPI ACK
//    ↓
// Student 2
//    ↓
// UART
//    ↓
// Student 1
//
// UART:
// RX = GPIO16
// TX = GPIO17
//
// SPI:
// SCLK = GPIO18
// MISO = GPIO19
// MOSI = GPIO23
// CS   = GPIO5
// ============================================================

#include <Arduino.h>
#include <SPI.h>

// ============================================================
// UART CONFIGURATION
// ============================================================

const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;

const unsigned long UART_BAUD_RATE = 115200;

// ============================================================
// SPI CONFIGURATION
// ============================================================

const int SPI_SCLK_PIN = 18;
const int SPI_MISO_PIN = 19;
const int SPI_MOSI_PIN = 23;
const int SPI_CS_PIN   = 5;

const unsigned long SPI_CLOCK = 1000000;

// ============================================================
// MESSAGE CONFIGURATION
// ============================================================

const int MAX_MESSAGE_LENGTH = 128;

// ============================================================
// SPI FRAME FORMAT
//
// BYTE 0       = 0xA5
// BYTE 1       = 0x5A
// BYTE 2       = MESSAGE LENGTH
// BYTE 3-130   = DATA
// BYTE 3+LEN   = '\n'
//
// TOTAL FRAME = 132 BYTES
// ============================================================

const int SPI_FRAME_SIZE = 132;

const uint8_t FRAME_START_1 = 0xA5;
const uint8_t FRAME_START_2 = 0x5A;

// ============================================================
// SPI OBJECT
// ============================================================

SPIClass spi(VSPI);

// ============================================================
// UART RECEIVE BUFFER
// ============================================================

char uartBuffer[MAX_MESSAGE_LENGTH + 1];
int uartLength = 0;

// ============================================================
// MESSAGE WAITING FOR SPI
// ============================================================

char pendingMessage[MAX_MESSAGE_LENGTH + 1];

bool messageWaitingForSPI = false;

// ============================================================
// SPI RECEIVE BUFFER
// ============================================================

uint8_t spiReceiveBuffer[SPI_FRAME_SIZE];

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // ----------------------------------------------------------
  // UART
  // ----------------------------------------------------------

  Serial2.begin(
    UART_BAUD_RATE,
    SERIAL_8N1,
    UART_RX_PIN,
    UART_TX_PIN
  );

  // ----------------------------------------------------------
  // SPI
  // ----------------------------------------------------------

  spi.begin(
    SPI_SCLK_PIN,
    SPI_MISO_PIN,
    SPI_MOSI_PIN,
    SPI_CS_PIN
  );

  pinMode(SPI_CS_PIN, OUTPUT);
  digitalWrite(SPI_CS_PIN, HIGH);

  // ----------------------------------------------------------
  // CLEAR BUFFERS
  // ----------------------------------------------------------

  memset(uartBuffer, 0, sizeof(uartBuffer));
  memset(pendingMessage, 0, sizeof(pendingMessage));
  memset(spiReceiveBuffer, 0, sizeof(spiReceiveBuffer));

  // ----------------------------------------------------------
  // STARTUP MESSAGE
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("==============================================");
  Serial.println(" STUDENT 2 - UART / SPI BRIDGE");
  Serial.println("==============================================");
  Serial.println("UART RX : GPIO16");
  Serial.println("UART TX : GPIO17");
  Serial.println("SPI SCLK: GPIO18");
  Serial.println("SPI MISO: GPIO19");
  Serial.println("SPI MOSI: GPIO23");
  Serial.println("SPI CS  : GPIO5");
  Serial.println();
  Serial.println("Waiting for Student 1...");
  Serial.println();
}

// ============================================================
// READ UART FROM STUDENT 1
// ============================================================

void readUART()
{
  while (Serial2.available() > 0)
  {
    char receivedChar = Serial2.read();

    // --------------------------------------------------------
    // Ignore carriage return
    // --------------------------------------------------------

    if (receivedChar == '\r')
    {
      continue;
    }

    // --------------------------------------------------------
    // End of message
    // --------------------------------------------------------

    if (receivedChar == '\n')
    {
      if (uartLength > 0)
      {
        uartBuffer[uartLength] = '\0';

        // Copy complete message
        strcpy(
          pendingMessage,
          uartBuffer
        );

        messageWaitingForSPI = true;

        Serial.println("----------------------------------------------");
        Serial.print("UART Message Received: ");
        Serial.println(pendingMessage);

        Serial.print("Length: ");
        Serial.println(uartLength);

        Serial.println("----------------------------------------------");

        // Reset UART buffer
        uartLength = 0;

        memset(
          uartBuffer,
          0,
          sizeof(uartBuffer)
        );
      }

      continue;
    }

    // --------------------------------------------------------
    // Store received character
    // --------------------------------------------------------

    if (uartLength < MAX_MESSAGE_LENGTH)
    {
      uartBuffer[uartLength] = receivedChar;
      uartLength++;
    }
    else
    {
      // Prevent buffer overflow
      uartLength = 0;

      memset(
        uartBuffer,
        0,
        sizeof(uartBuffer)
      );

      Serial.println();
      Serial.println("ERROR: UART message too long.");
      Serial.println();
    }
  }
}

// ============================================================
// BUILD SPI FRAME
// ============================================================

void buildSPIFrame(
  const char *message,
  uint8_t *frame
)
{
  // Clear entire frame
  memset(
    frame,
    0,
    SPI_FRAME_SIZE
  );

  int messageLength = strlen(message);

  if (messageLength > MAX_MESSAGE_LENGTH)
  {
    messageLength = MAX_MESSAGE_LENGTH;
  }

  // ----------------------------------------------------------
  // HEADER
  // ----------------------------------------------------------

  frame[0] = FRAME_START_1;
  frame[1] = FRAME_START_2;
  frame[2] = (uint8_t)messageLength;

  // ----------------------------------------------------------
  // MESSAGE
  // ----------------------------------------------------------

  memcpy(
    &frame[3],
    message,
    messageLength
  );

  // ----------------------------------------------------------
  // MESSAGE TERMINATOR
  // ----------------------------------------------------------

  frame[3 + messageLength] = '\n';
}

// ============================================================
// VALIDATE ACKNOWLEDGMENT
// ============================================================

bool validateACK(
  const uint8_t *frame,
  char *ackMessage
)
{
  // ----------------------------------------------------------
  // CHECK HEADER
  // ----------------------------------------------------------

  if (frame[0] != FRAME_START_1)
  {
    return false;
  }

  if (frame[1] != FRAME_START_2)
  {
    return false;
  }

  // ----------------------------------------------------------
  // CHECK LENGTH
  // ----------------------------------------------------------

  int ackLength = frame[2];

  if (ackLength <= 0)
  {
    return false;
  }

  if (ackLength > MAX_MESSAGE_LENGTH)
  {
    return false;
  }

  // ----------------------------------------------------------
  // CHECK TERMINATOR
  // ----------------------------------------------------------

  if (frame[3 + ackLength] != '\n')
  {
    return false;
  }

  // ----------------------------------------------------------
  // COPY ACKNOWLEDGMENT
  // ----------------------------------------------------------

  memcpy(
    ackMessage,
    &frame[3],
    ackLength
  );

  ackMessage[ackLength] = '\0';

  return true;
}

// ============================================================
// SEND MESSAGE THROUGH SPI AND RECEIVE ACKNOWLEDGMENT
// ============================================================

void sendMessageThroughSPI()
{
  uint8_t transmitFrame[SPI_FRAME_SIZE];

  char acknowledgment[MAX_MESSAGE_LENGTH + 1];

  // ----------------------------------------------------------
  // BUILD MESSAGE FRAME
  // ----------------------------------------------------------

  buildSPIFrame(
    pendingMessage,
    transmitFrame
  );

  // Clear SPI receive buffer
  memset(
    spiReceiveBuffer,
    0,
    sizeof(spiReceiveBuffer)
  );

  // ----------------------------------------------------------
  // SPI TRANSACTION
  // ----------------------------------------------------------

  spi.beginTransaction(
    SPISettings(
      SPI_CLOCK,
      MSBFIRST,
      SPI_MODE1
    )
  );

  digitalWrite(
    SPI_CS_PIN,
    LOW
  );

  // ----------------------------------------------------------
  // FULL-DUPLEX SPI
  //
  // TX:
  // Student 2 → Student 3
  //
  // RX:
  // Student 3 → Student 2
  // ----------------------------------------------------------

  for (int i = 0; i < SPI_FRAME_SIZE; i++)
  {
    spiReceiveBuffer[i] =
      spi.transfer(transmitFrame[i]);
  }

  digitalWrite(
    SPI_CS_PIN,
    HIGH
  );

  spi.endTransaction();

  // ----------------------------------------------------------
  // DISPLAY TRANSMITTED MESSAGE
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("SPI Message Sent:");
  Serial.println(pendingMessage);

  // ----------------------------------------------------------
  // CHECK ACKNOWLEDGMENT
  // ----------------------------------------------------------

  memset(
    acknowledgment,
    0,
    sizeof(acknowledgment)
  );

  if (validateACK(
        spiReceiveBuffer,
        acknowledgment
      ))
  {
    Serial.print("SPI Acknowledgment: ");
    Serial.println(acknowledgment);

    // --------------------------------------------------------
    // FORWARD ACKNOWLEDGMENT TO STUDENT 1 THROUGH UART
    // --------------------------------------------------------

    Serial2.write(
      (const uint8_t *)acknowledgment,
      strlen(acknowledgment)
    );

    Serial2.write('\n');

    Serial.println(
      "Acknowledgment forwarded to Student 1."
    );
  }
  else
  {
    Serial.println(
      "WARNING: Invalid SPI acknowledgment."
    );
  }

  Serial.println();

  // ----------------------------------------------------------
  // MESSAGE HAS BEEN PROCESSED
  // ----------------------------------------------------------

  messageWaitingForSPI = false;

  memset(
    pendingMessage,
    0,
    sizeof(pendingMessage)
  );
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // ALWAYS LISTEN FOR UART
  // ----------------------------------------------------------

  readUART();

  // ----------------------------------------------------------
  // SEND COMPLETE MESSAGE THROUGH SPI
  // ----------------------------------------------------------

  if (messageWaitingForSPI)
  {
    sendMessageThroughSPI();
  }
}