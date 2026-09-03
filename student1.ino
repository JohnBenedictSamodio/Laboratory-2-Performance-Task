// ============================================================
// THREE-ESP32 COMMUNICATION SYSTEM
//
// STUDENT 1 - UART SENDER
//
// Function:
// Arduino IDE Serial Monitor
//          |
//          v
//     Student 1 ESP32
//          |
//       UART TX
//          |
//          v
//     Student 2 ESP32
//
// UART:
// TX = GPIO17
// RX = GPIO16 (not used for this direction)
// Baud = 115200
//
// MAXIMUM MESSAGE LENGTH = 128 characters
//
// Arduino ESP32 Core 3.3.x
// ============================================================

#include <Arduino.h>


// ============================================================
// UART SETTINGS
// ============================================================

const uint32_t UART_BAUD = 115200;

const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;


// ============================================================
// MESSAGE SETTINGS
// ============================================================

const size_t MAX_MESSAGE_LENGTH = 128;


// ============================================================
// SERIAL MONITOR INPUT BUFFER
// ============================================================

char inputBuffer[MAX_MESSAGE_LENGTH + 1];

size_t inputLength = 0;


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
  //
  // Serial2:
  //
  // RX = GPIO16
  // TX = GPIO17
  //
  // Only TX is needed by Student 1.
  // ----------------------------------------------------------

  Serial2.begin(
    UART_BAUD,
    SERIAL_8N1,
    UART_RX_PIN,
    UART_TX_PIN
  );


  // ----------------------------------------------------------
  // CLEAR BUFFER
  // ----------------------------------------------------------

  memset(
    inputBuffer,
    0,
    sizeof(inputBuffer)
  );


  // ----------------------------------------------------------
  // DISPLAY INFORMATION
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("================================================");
  Serial.println(" STUDENT 1 - UART SENDER");
  Serial.println("================================================");
  Serial.println();

  Serial.println("UART configuration:");
  Serial.println("  TX   : GPIO17");
  Serial.println("  RX   : GPIO16");
  Serial.println("  Baud : 115200");
  Serial.println();

  Serial.println("Maximum message length: 128 characters");
  Serial.println();

  Serial.println("READY.");
  Serial.println("Type a message and press Send.");
  Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // READ USB SERIAL MONITOR
  // ----------------------------------------------------------

  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();


    // --------------------------------------------------------
    // END OF MESSAGE
    // --------------------------------------------------------

    if (c == '\n')
    {
      sendCompleteMessage();

      continue;
    }


    // --------------------------------------------------------
    // IGNORE CARRIAGE RETURN
    // --------------------------------------------------------

    if (c == '\r')
    {
      continue;
    }


    // --------------------------------------------------------
    // STORE CHARACTER
    // --------------------------------------------------------

    if (inputLength < MAX_MESSAGE_LENGTH)
    {
      inputBuffer[inputLength] = c;

      inputLength++;

      inputBuffer[inputLength] = '\0';
    }
    else
    {
      // ------------------------------------------------------
      // MESSAGE TOO LONG
      // ------------------------------------------------------
      //
      // Do not allow the buffer to overflow.
      // Continue reading until ENTER/newline.
      // ------------------------------------------------------

      Serial.println();
      Serial.println("ERROR: Message exceeds 128 characters.");

      Serial.println(
        "Message will be discarded."
      );

      // Reset the buffer so we don't continue building
      // an invalid message.

      inputLength = 0;

      memset(
        inputBuffer,
        0,
        sizeof(inputBuffer)
      );

      // Consume remaining characters until newline.
      while (Serial.available() > 0)
      {
        char discard = (char)Serial.read();

        if (discard == '\n')
        {
          break;
        }
      }

      Serial.println();
      Serial.println("READY FOR NEW MESSAGE.");
      Serial.println();

      continue;
    }
  }
}


// ============================================================
// SEND COMPLETE MESSAGE
// ============================================================

void sendCompleteMessage()
{
  // ----------------------------------------------------------
  // EMPTY MESSAGE
  // ----------------------------------------------------------

  if (inputLength == 0)
  {
    Serial.println();
    Serial.println("WARNING: Empty message.");
    Serial.println();

    return;
  }


  // ----------------------------------------------------------
  // DISPLAY MESSAGE
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("--------------------------------------------");
  Serial.println("SENDING UART MESSAGE");
  Serial.println("--------------------------------------------");

  Serial.print("Message: ");
  Serial.println(inputBuffer);

  Serial.print("Length : ");
  Serial.println(inputLength);


  // ----------------------------------------------------------
  // SEND MESSAGE THROUGH UART
  // ----------------------------------------------------------
  //
  // Student 2 detects '\n' as the end-of-message marker.
  // ----------------------------------------------------------

  Serial2.write(
    (const uint8_t*)inputBuffer,
    inputLength
  );

  Serial2.write('\n');


  // ----------------------------------------------------------
  // TRANSMISSION COMPLETE
  // ----------------------------------------------------------

  Serial.println("UART transmission completed.");

  Serial.println("--------------------------------------------");
  Serial.println();


  // ----------------------------------------------------------
  // RESET BUFFER
  // ----------------------------------------------------------

  inputLength = 0;

  memset(
    inputBuffer,
    0,
    sizeof(inputBuffer)
  );
}
