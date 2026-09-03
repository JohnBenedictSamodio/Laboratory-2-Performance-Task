// ============================================================
// ESP32 UART → SPI COMMUNICATION SYSTEM
//
// STUDENT 1 — UART TRANSMITTER
//
// FUNCTION:
// Serial Monitor → UART → Student 2
//
// UART:
// TX = GPIO17
// RX = GPIO16 (not used by Student 1)
// Baud Rate = 115200
// ============================================================

#include <Arduino.h>

// ============================================================
// UART CONFIGURATION
// ============================================================

const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;

const unsigned long UART_BAUD_RATE = 115200;

// ============================================================
// MESSAGE CONFIGURATION
// ============================================================

const int MAX_MESSAGE_LENGTH = 128;

char inputBuffer[MAX_MESSAGE_LENGTH + 1];
int inputLength = 0;

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial2.begin(
    UART_BAUD_RATE,
    SERIAL_8N1,
    UART_RX_PIN,
    UART_TX_PIN
  );

  Serial.println();
  Serial.println("==============================================");
  Serial.println(" STUDENT 1 - UART TRANSMITTER");
  Serial.println("==============================================");
  Serial.println("Enter a message:");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  while (Serial.available() > 0)
  {
    char receivedChar = Serial.read();

    // Ignore carriage return
    if (receivedChar == '\r')
    {
      continue;
    }

    // End of message
    if (receivedChar == '\n')
    {
      if (inputLength > 0)
      {
        inputBuffer[inputLength] = '\0';

        // ----------------------------------------------------
        // SEND MESSAGE TO STUDENT 2 THROUGH UART
        // ----------------------------------------------------

        Serial2.write(
          (const uint8_t *)inputBuffer,
          inputLength
        );

        Serial2.write('\n');

        // ----------------------------------------------------
        // DISPLAY SENT MESSAGE
        // ----------------------------------------------------

        Serial.println("----------------------------------------------");
        Serial.print("Message Sent: ");
        Serial.println(inputBuffer);

        Serial.print("Length: ");
        Serial.println(inputLength);

        Serial.println("----------------------------------------------");
        Serial.println();

        inputLength = 0;
      }

      continue;
    }

    // --------------------------------------------------------
    // STORE CHARACTER
    // --------------------------------------------------------

    if (inputLength < MAX_MESSAGE_LENGTH)
    {
      inputBuffer[inputLength] = receivedChar;
      inputLength++;
    }
    else
    {
      // Prevent buffer overflow
      inputLength = 0;

      Serial.println();
      Serial.println("ERROR: Message too long.");
      Serial.println("Maximum length is 128 characters.");
      Serial.println();
    }
  }

  // ==========================================================
  // RECEIVE ACKNOWLEDGMENT FROM STUDENT 2
  // ==========================================================

  while (Serial2.available() > 0)
  {
    char ackChar = Serial2.read();

    if (ackChar == '\n')
    {
      Serial.println("Acknowledgment from Student 3:");
      Serial.println("MESSAGE RECEIVED");
      Serial.println();
    }
  }
}