// ============================================================
// THREE-ESP32 COMMUNICATION SYSTEM
//
// STUDENT 2 - UART TO SPI BRIDGE
//
// Function:
//
// Student 1
//     |
//     | UART
//     v
// Student 2
//     |
//     | SPI
//     v
// Student 3
//
// UART:
// RX = GPIO16
// TX = GPIO17
// Baud = 115200
//
// SPI MASTER:
// SCLK = GPIO18
// MISO = GPIO19
// MOSI = GPIO23
// CS   = GPIO5
//
// READY INPUT:
// GPIO32
//
// MAXIMUM MESSAGE LENGTH:
// 128 characters
//
// Arduino ESP32 Core 3.3.x
// ============================================================

#include <Arduino.h>
#include <SPI.h>


// ============================================================
// UART CONFIGURATION
// ============================================================

const uint32_t UART_BAUD = 115200;

const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;


// ============================================================
// SPI CONFIGURATION
// ============================================================

const int SPI_SCLK_PIN = 18;
const int SPI_MISO_PIN = 19;
const int SPI_MOSI_PIN = 23;
const int SPI_CS_PIN   = 5;


// ============================================================
// READY HANDSHAKE
// ============================================================
//
// Student 3 GPIO4
//       |
//       v
// Student 2 GPIO32
//
// HIGH = Student 3 ready
// LOW  = Student 3 not ready
// ============================================================

const int SPI_READY_PIN = 32;


// ============================================================
// MESSAGE CONFIGURATION
// ============================================================

const size_t MAX_MESSAGE_LENGTH = 128;


// ============================================================
// SPI FRAME
// ============================================================
//
// BYTE 0:
//     0xA5
//
// BYTE 1:
//     0x5A
//
// BYTE 2:
//     Message length
//
// BYTE 3 - 130:
//     Message data
//
// BYTE 3 + length:
//     '\n'
//
// Total:
//     132 bytes
//
// 132 is divisible by 4.
// This is suitable for DMA on the SPI slave.
// ============================================================

const size_t FRAME_SIZE = 132;

const uint8_t FRAME_MAGIC_1 = 0xA5;
const uint8_t FRAME_MAGIC_2 = 0x5A;


// ============================================================
// SPI SETTINGS
// ============================================================

const uint32_t SPI_CLOCK = 1000000;


// ============================================================
// SPI MASTER OBJECT
// ============================================================

SPIClass spi(VSPI);


// ============================================================
// UART MESSAGE BUFFER
// ============================================================

char uartBuffer[MAX_MESSAGE_LENGTH + 1];

size_t uartLength = 0;

bool uartOverflow = false;


// ============================================================
// PENDING MESSAGE
// ============================================================
//
// Once a complete UART message is received, it is copied here.
//
// Student 2 can then wait for Student 3 READY without losing
// the message.
// ============================================================

char pendingMessage[MAX_MESSAGE_LENGTH + 1];

size_t pendingLength = 0;

bool messageWaitingForSPI = false;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void readUART();

void completeUARTMessage();

bool sendPendingMessage();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  // ----------------------------------------------------------
  // USB SERIAL MONITOR
  // ----------------------------------------------------------

  Serial.begin(115200);

  delay(500);


  // ----------------------------------------------------------
  // HARDWARE UART
  // ----------------------------------------------------------

  Serial2.begin(
    UART_BAUD,
    SERIAL_8N1,
    UART_RX_PIN,
    UART_TX_PIN
  );


  // ----------------------------------------------------------
  // READY INPUT
  // ----------------------------------------------------------

  pinMode(
    SPI_READY_PIN,
    INPUT_PULLDOWN
  );


  // ----------------------------------------------------------
  // SPI CHIP SELECT
  // ----------------------------------------------------------

  pinMode(
    SPI_CS_PIN,
    OUTPUT
  );

  // SPI idle state.
  digitalWrite(
    SPI_CS_PIN,
    HIGH
  );


  // ----------------------------------------------------------
  // START SPI MASTER
  // ----------------------------------------------------------

  spi.begin(
    SPI_SCLK_PIN,
    SPI_MISO_PIN,
    SPI_MOSI_PIN,
    SPI_CS_PIN
  );


  // ----------------------------------------------------------
  // CLEAR BUFFERS
  // ----------------------------------------------------------

  memset(
    uartBuffer,
    0,
    sizeof(uartBuffer)
  );

  memset(
    pendingMessage,
    0,
    sizeof(pendingMessage)
  );


  // ----------------------------------------------------------
  // DISPLAY CONFIGURATION
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("================================================");
  Serial.println(" STUDENT 2 - UART TO SPI BRIDGE");
  Serial.println("================================================");
  Serial.println();

  Serial.println("UART:");
  Serial.println("  RX   : GPIO16");
  Serial.println("  TX   : GPIO17");
  Serial.println("  Baud : 115200");
  Serial.println();

  Serial.println("SPI MASTER:");
  Serial.println("  SCLK : GPIO18");
  Serial.println("  MISO : GPIO19");
  Serial.println("  MOSI : GPIO23");
  Serial.println("  CS   : GPIO5");
  Serial.println("  Mode : SPI MODE1");
  Serial.println("  Clock: 1 MHz");
  Serial.println();

  Serial.println("READY:");
  Serial.println("  Input: GPIO32");
  Serial.println();

  Serial.println("================================================");
  Serial.println(" STUDENT 2 READY");
  Serial.println(" Listening for UART messages...");
  Serial.println("================================================");
  Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================
//
// IMPORTANT:
//
// UART is checked continuously.
//
// Student 2 does NOT block waiting for Student 3.
//
// If a message is waiting for SPI, the UART receiver continues
// running so incoming data is not accidentally lost.
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // ALWAYS CHECK UART
  // ----------------------------------------------------------

  readUART();


  // ----------------------------------------------------------
  // IF A COMPLETE MESSAGE IS WAITING,
//  // TRY TO SEND IT THROUGH SPI
  // ----------------------------------------------------------

  if (messageWaitingForSPI)
  {
    // --------------------------------------------------------
    // Check whether Student 3 is ready.
    //
    // No long blocking wait is used here.
    // --------------------------------------------------------

    if (digitalRead(SPI_READY_PIN) == HIGH)
    {
      bool success = sendPendingMessage();

      if (success)
      {
        messageWaitingForSPI = false;
        pendingLength = 0;

        memset(
          pendingMessage,
          0,
          sizeof(pendingMessage)
        );

        Serial.println();
        Serial.println(
          "Message successfully relayed to Student 3."
        );
        Serial.println();
        Serial.println(
          "Waiting for next UART message..."
        );
        Serial.println();
      }
    }
  }


  // ----------------------------------------------------------
  // ALLOW BACKGROUND TASKS
  // ----------------------------------------------------------

  yield();
}


// ============================================================
// READ UART
// ============================================================

void readUART()
{
  while (Serial2.available() > 0)
  {
    char c = (char)Serial2.read();


    // --------------------------------------------------------
    // END OF MESSAGE
    // --------------------------------------------------------

    if (c == '\n')
    {
      completeUARTMessage();

      continue;
    }


    // --------------------------------------------------------
    // IGNORE CR
    // --------------------------------------------------------

    if (c == '\r')
    {
      continue;
    }


    // --------------------------------------------------------
    // STORE CHARACTER
    // --------------------------------------------------------

    if (uartLength < MAX_MESSAGE_LENGTH)
    {
      uartBuffer[uartLength] = c;

      uartLength++;

      uartBuffer[uartLength] = '\0';
    }
    else
    {
      // ------------------------------------------------------
      // BUFFER OVERFLOW PROTECTION
      // ------------------------------------------------------

      uartOverflow = true;
    }
  }
}


// ============================================================
// COMPLETE UART MESSAGE
// ============================================================

void completeUARTMessage()
{
  // ----------------------------------------------------------
  // MESSAGE TOO LONG
  // ----------------------------------------------------------

  if (uartOverflow)
  {
    Serial.println();
    Serial.println("ERROR: UART message exceeded 128 characters.");
    Serial.println("Message discarded.");
    Serial.println();

    uartLength = 0;
    uartOverflow = false;

    memset(
      uartBuffer,
      0,
      sizeof(uartBuffer)
    );

    return;
  }


  // ----------------------------------------------------------
  // EMPTY MESSAGE
  // ----------------------------------------------------------

  if (uartLength == 0)
  {
    Serial.println();
    Serial.println("WARNING: Empty UART message received.");
    Serial.println();

    return;
  }


  // ----------------------------------------------------------
  // DO NOT OVERWRITE A PREVIOUS PENDING MESSAGE
  // ----------------------------------------------------------

  if (messageWaitingForSPI)
  {
    Serial.println();
    Serial.println(
      "WARNING: Previous message is still waiting for SPI."
    );

    Serial.println(
      "New UART message discarded."
    );

    Serial.println();

    uartLength = 0;

    memset(
      uartBuffer,
      0,
      sizeof(uartBuffer)
    );

    return;
  }


  // ----------------------------------------------------------
  // COPY UART MESSAGE TO PENDING BUFFER
  // ----------------------------------------------------------

  memcpy(
    pendingMessage,
    uartBuffer,
    uartLength
  );

  pendingMessage[uartLength] = '\0';

  pendingLength = uartLength;

  messageWaitingForSPI = true;


  // ----------------------------------------------------------
  // DISPLAY UART RECEIVED MESSAGE
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("--------------------------------------------");
  Serial.println("UART MESSAGE RECEIVED FROM STUDENT 1");
  Serial.println("--------------------------------------------");

  Serial.print("Message: ");
  Serial.println(pendingMessage);

  Serial.print("Length : ");
  Serial.println(pendingLength);

  Serial.println("--------------------------------------------");


  // ----------------------------------------------------------
  // RESET UART INPUT BUFFER
  // ----------------------------------------------------------

  uartLength = 0;
  uartOverflow = false;

  memset(
    uartBuffer,
    0,
    sizeof(uartBuffer)
  );


  // ----------------------------------------------------------
  // CHECK READY IMMEDIATELY
  // ----------------------------------------------------------

  if (digitalRead(SPI_READY_PIN) == HIGH)
  {
    Serial.println("Student 3 READY.");
    Serial.println("SPI transmission will begin.");
  }
  else
  {
    Serial.println(
      "Student 3 is not ready yet."
    );

    Serial.println(
      "Message safely stored. Waiting for READY..."
    );
  }

  Serial.println();
}


// ============================================================
// SEND PENDING MESSAGE THROUGH SPI
// ============================================================

bool sendPendingMessage()
{
  // ----------------------------------------------------------
  // SAFETY CHECK
  // ----------------------------------------------------------

  if (!messageWaitingForSPI)
  {
    return false;
  }


  if (pendingLength == 0)
  {
    return false;
  }


  if (pendingLength > MAX_MESSAGE_LENGTH)
  {
    return false;
  }


  // ----------------------------------------------------------
  // CREATE SPI FRAME
  // ----------------------------------------------------------

  uint8_t frame[FRAME_SIZE];


  // Clear the entire frame.
  memset(
    frame,
    0,
    FRAME_SIZE
  );


  // ----------------------------------------------------------
  // FRAME HEADER
  // ----------------------------------------------------------

  frame[0] = FRAME_MAGIC_1;
  frame[1] = FRAME_MAGIC_2;


  // ----------------------------------------------------------
  // MESSAGE LENGTH
  // ----------------------------------------------------------

  frame[2] = (uint8_t)pendingLength;


  // ----------------------------------------------------------
  // MESSAGE DATA
  // ----------------------------------------------------------

  memcpy(
    &frame[3],
    pendingMessage,
    pendingLength
  );


  // ----------------------------------------------------------
  // NEWLINE TERMINATOR
  // ----------------------------------------------------------

  frame[3 + pendingLength] = '\n';


  // ----------------------------------------------------------
  // DISPLAY SPI INFORMATION
  // ----------------------------------------------------------

  Serial.println("--------------------------------------------");
  Serial.println("SPI TRANSMISSION");
  Serial.println("--------------------------------------------");

  Serial.print("Message: ");
  Serial.println(pendingMessage);

  Serial.print("Length : ");
  Serial.println(pendingLength);

  Serial.print("Frame  : ");
  Serial.print(FRAME_SIZE);
  Serial.println(" bytes");


  // ----------------------------------------------------------
  // BEGIN SPI TRANSACTION
  // ----------------------------------------------------------

  spi.beginTransaction(
    SPISettings(
      SPI_CLOCK,
      MSBFIRST,
      SPI_MODE1
    )
  );


  // ----------------------------------------------------------
  // SELECT STUDENT 3
  // ----------------------------------------------------------

  digitalWrite(
    SPI_CS_PIN,
    LOW
  );


  // ----------------------------------------------------------
  // SEND COMPLETE FRAME
  // ----------------------------------------------------------

  for (size_t i = 0; i < FRAME_SIZE; i++)
  {
    spi.transfer(frame[i]);
  }


  // ----------------------------------------------------------
  // DESELECT STUDENT 3
  // ----------------------------------------------------------

  digitalWrite(
    SPI_CS_PIN,
    HIGH
  );


  // ----------------------------------------------------------
  // END SPI TRANSACTION
  // ----------------------------------------------------------

  spi.endTransaction();


  Serial.println("SPI frame transmitted.");
  Serial.println("--------------------------------------------");

  return true;
}
