// ============================================================
// ESP32 UART → SPI COMMUNICATION SYSTEM
//
// STUDENT 3 — SPI SLAVE / RECEIVER
//
// FUNCTION:
//
// Receive message from Student 2 through SPI.
// Reconstruct the complete message.
// Send acknowledgment back to Student 2 through SPI.
//
// SPI:
// SCLK = GPIO18
// MISO = GPIO19
// MOSI = GPIO23
// CS   = GPIO5
//
// NO EXTRA READY GPIO IS USED.
// ============================================================

#include <Arduino.h>

#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_err.h"

// ============================================================
// SPI PINS
// ============================================================

const int SPI_SCLK_PIN = 18;
const int SPI_MISO_PIN = 19;
const int SPI_MOSI_PIN = 23;
const int SPI_CS_PIN   = 5;

// ============================================================
// SPI HOST
//
// IMPORTANT:
// Do NOT name this variable SPI_HOST.
// SPI_HOST is already defined by ESP-IDF.
// ============================================================

const spi_host_device_t SPI_SLAVE_HOST = SPI3_HOST;

// ============================================================
// SPI CONFIGURATION
// ============================================================

const int SPI_FRAME_SIZE = 132;

const int MAX_MESSAGE_LENGTH = 128;

// ============================================================
// FRAME HEADER
// ============================================================

const uint8_t FRAME_START_1 = 0xA5;
const uint8_t FRAME_START_2 = 0x5A;

// ============================================================
// ACKNOWLEDGMENT
// ============================================================

const char ACK_MESSAGE[] = "MESSAGE RECEIVED";

// ============================================================
// DMA RECEIVE BUFFER
// ============================================================

uint8_t *rxBuffer = nullptr;

// ============================================================
// DMA TRANSMIT BUFFER
// ============================================================

uint8_t *txBuffer = nullptr;

// ============================================================
// SPI TRANSACTION
// ============================================================

spi_slave_transaction_t transaction;

// ============================================================
// PREPARE ACKNOWLEDGMENT
// ============================================================

void prepareAcknowledgment()
{
  // ----------------------------------------------------------
  // CLEAR TX BUFFER
  // ----------------------------------------------------------

  memset(
    txBuffer,
    0,
    SPI_FRAME_SIZE
  );

  // ----------------------------------------------------------
  // ACKNOWLEDGMENT LENGTH
  // ----------------------------------------------------------

  int ackLength =
    strlen(ACK_MESSAGE);

  // ----------------------------------------------------------
  // FRAME HEADER
  // ----------------------------------------------------------

  txBuffer[0] =
    FRAME_START_1;

  txBuffer[1] =
    FRAME_START_2;

  txBuffer[2] =
    (uint8_t)ackLength;

  // ----------------------------------------------------------
  // ACKNOWLEDGMENT DATA
  // ----------------------------------------------------------

  memcpy(
    &txBuffer[3],
    ACK_MESSAGE,
    ackLength
  );

  // ----------------------------------------------------------
  // ACKNOWLEDGMENT TERMINATOR
  // ----------------------------------------------------------

  txBuffer[3 + ackLength] =
    '\n';
}

// ============================================================
// VALIDATE RECEIVED MESSAGE
// ============================================================

bool validateReceivedFrame(
  int &messageLength
)
{
  // ----------------------------------------------------------
  // CHECK FRAME HEADER
  // ----------------------------------------------------------

  if (rxBuffer[0] != FRAME_START_1)
  {
    return false;
  }

  if (rxBuffer[1] != FRAME_START_2)
  {
    return false;
  }

  // ----------------------------------------------------------
  // READ MESSAGE LENGTH
  // ----------------------------------------------------------

  messageLength =
    rxBuffer[2];

  // ----------------------------------------------------------
  // CHECK MESSAGE LENGTH
  // ----------------------------------------------------------

  if (messageLength > MAX_MESSAGE_LENGTH)
  {
    return false;
  }

  // ----------------------------------------------------------
  // CHECK MESSAGE TERMINATOR
  // ----------------------------------------------------------

  if (rxBuffer[3 + messageLength] != '\n')
  {
    return false;
  }

  return true;
}

// ============================================================
// DISPLAY RECEIVED MESSAGE
// ============================================================

void displayReceivedMessage(
  int messageLength
)
{
  char receivedMessage[
    MAX_MESSAGE_LENGTH + 1
  ];

  memset(
    receivedMessage,
    0,
    sizeof(receivedMessage)
  );

  memcpy(
    receivedMessage,
    &rxBuffer[3],
    messageLength
  );

  receivedMessage[messageLength] =
    '\0';

  Serial.println("----------------------------------------------");
  Serial.println("SPI MESSAGE RECEIVED");
  Serial.println("----------------------------------------------");

  Serial.print("Message: ");
  Serial.println(receivedMessage);

  Serial.print("Length: ");
  Serial.println(messageLength);

  Serial.println("----------------------------------------------");

  Serial.println(
    "Acknowledgment sent: MESSAGE RECEIVED"
  );

  Serial.println();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================================");
  Serial.println(" STUDENT 3 - SPI SLAVE / RECEIVER");
  Serial.println("==============================================");
  Serial.println("SPI SCLK: GPIO18");
  Serial.println("SPI MISO: GPIO19");
  Serial.println("SPI MOSI: GPIO23");
  Serial.println("SPI CS  : GPIO5");
  Serial.println();

  // ----------------------------------------------------------
  // ALLOCATE DMA-CAPABLE RECEIVE BUFFER
  // ----------------------------------------------------------

  rxBuffer = (uint8_t *)heap_caps_malloc(
    SPI_FRAME_SIZE,
    MALLOC_CAP_DMA
  );

  // ----------------------------------------------------------
  // ALLOCATE DMA-CAPABLE TRANSMIT BUFFER
  // ----------------------------------------------------------

  txBuffer = (uint8_t *)heap_caps_malloc(
    SPI_FRAME_SIZE,
    MALLOC_CAP_DMA
  );

  // ----------------------------------------------------------
  // CHECK MEMORY ALLOCATION
  // ----------------------------------------------------------

  if (rxBuffer == nullptr)
  {
    Serial.println(
      "ERROR: Failed to allocate RX buffer."
    );

    while (true)
    {
      delay(1000);
    }
  }

  if (txBuffer == nullptr)
  {
    Serial.println(
      "ERROR: Failed to allocate TX buffer."
    );

    while (true)
    {
      delay(1000);
    }
  }

  // ----------------------------------------------------------
  // CLEAR BUFFERS
  // ----------------------------------------------------------

  memset(
    rxBuffer,
    0,
    SPI_FRAME_SIZE
  );

  memset(
    txBuffer,
    0,
    SPI_FRAME_SIZE
  );

  // ----------------------------------------------------------
  // PREPARE FIRST ACKNOWLEDGMENT
  // ----------------------------------------------------------

  prepareAcknowledgment();

  // ----------------------------------------------------------
  // SPI BUS CONFIGURATION
  // ----------------------------------------------------------

  spi_bus_config_t busConfig;

  memset(
    &busConfig,
    0,
    sizeof(busConfig)
  );

  busConfig.mosi_io_num =
    SPI_MOSI_PIN;

  busConfig.miso_io_num =
    SPI_MISO_PIN;

  busConfig.sclk_io_num =
    SPI_SCLK_PIN;

  busConfig.quadwp_io_num =
    -1;

  busConfig.quadhd_io_num =
    -1;

  busConfig.max_transfer_sz =
    SPI_FRAME_SIZE;

  // ----------------------------------------------------------
  // SPI SLAVE CONFIGURATION
  // ----------------------------------------------------------

  spi_slave_interface_config_t slaveConfig;

  memset(
    &slaveConfig,
    0,
    sizeof(slaveConfig)
  );

  slaveConfig.spics_io_num =
    SPI_CS_PIN;

  // Must match Student 2
  slaveConfig.mode =
    1;

  slaveConfig.queue_size =
    1;

  slaveConfig.flags =
    0;

  slaveConfig.post_setup_cb =
    nullptr;

  slaveConfig.post_trans_cb =
    nullptr;

  // ----------------------------------------------------------
  // INITIALIZE SPI SLAVE
  // ----------------------------------------------------------

  esp_err_t result =
    spi_slave_initialize(
      SPI_SLAVE_HOST,
      &busConfig,
      &slaveConfig,
      SPI_DMA_CH_AUTO
    );

  if (result != ESP_OK)
  {
    Serial.print(
      "ERROR: SPI slave initialization failed. Code: "
    );

    Serial.println(
      (int)result
    );

    while (true)
    {
      delay(1000);
    }
  }

  // ----------------------------------------------------------
  // PREPARE SPI TRANSACTION
  // ----------------------------------------------------------

  memset(
    &transaction,
    0,
    sizeof(transaction)
  );

  // Total transaction length in bits
  transaction.length =
    SPI_FRAME_SIZE * 8;

  // Receive buffer
  transaction.rx_buffer =
    rxBuffer;

  // Transmit buffer
  transaction.tx_buffer =
    txBuffer;

  // ----------------------------------------------------------
  // INITIALIZATION COMPLETE
  // ----------------------------------------------------------

  Serial.println(
    "SPI slave initialized successfully."
  );

  Serial.println(
    "Waiting for Student 2..."
  );

  Serial.println();
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // CLEAR RECEIVE BUFFER
  // ----------------------------------------------------------

  memset(
    rxBuffer,
    0,
    SPI_FRAME_SIZE
  );

  // ----------------------------------------------------------
  // PREPARE ACKNOWLEDGMENT
  //
  // Student 2 receives this through MISO while Student 2
  // sends the message through MOSI.
  // ----------------------------------------------------------

  prepareAcknowledgment();

  // ----------------------------------------------------------
  // RESET TRANSACTION STRUCTURE
  // ----------------------------------------------------------

  memset(
    &transaction,
    0,
    sizeof(transaction)
  );

  // Total transaction length in bits
  transaction.length =
    SPI_FRAME_SIZE * 8;

  // Receive buffer
  transaction.rx_buffer =
    rxBuffer;

  // Transmit buffer
  transaction.tx_buffer =
    txBuffer;

  // ----------------------------------------------------------
  // WAIT FOR SPI TRANSACTION
  // ----------------------------------------------------------

  esp_err_t result =
    spi_slave_transmit(
      SPI_SLAVE_HOST,
      &transaction,
      portMAX_DELAY
    );

  // ----------------------------------------------------------
  // CHECK SPI RESULT
  // ----------------------------------------------------------

  if (result != ESP_OK)
  {
    Serial.print(
      "ERROR: SPI transaction failed. Code: "
    );

    Serial.println(
      (int)result
    );

    delay(10);

    return;
  }

  // ----------------------------------------------------------
  // CHECK RECEIVED LENGTH
  // ----------------------------------------------------------

  if (transaction.trans_len !=
      SPI_FRAME_SIZE * 8)
  {
    Serial.println(
      "ERROR: Incomplete SPI frame received."
    );

    delay(10);

    return;
  }

  // ----------------------------------------------------------
  // VALIDATE MESSAGE
  // ----------------------------------------------------------

  int messageLength = 0;

  if (validateReceivedFrame(
        messageLength
      ))
  {
    displayReceivedMessage(
      messageLength
    );
  }
  else
  {
    Serial.println(
      "ERROR: Invalid SPI message frame."
    );

    Serial.println();
  }
}