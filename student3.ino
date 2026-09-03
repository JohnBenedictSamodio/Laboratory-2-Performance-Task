// ============================================================
// THREE-ESP32 COMMUNICATION SYSTEM
//
// STUDENT 3 - SPI RECEIVER / SLAVE
//
// Function:
//
// Student 1
//      |
//      | UART
//      v
// Student 2
//      |
//      | SPI
//      v
// Student 3
//      |
//      v
// Arduino IDE Serial Monitor
//
// SPI SLAVE:
//
// SCLK = GPIO18
// MOSI = GPIO23
// MISO = GPIO19
// CS   = GPIO5
//
// READY:
//
// READY OUTPUT = GPIO4
//
// Student 2 GPIO32 reads the READY signal.
//
// MAXIMUM MESSAGE LENGTH = 128 characters
//
// SPI FRAME SIZE = 132 bytes
//
// Arduino ESP32 Core 3.3.x
// ESP-IDF SPI Slave Driver
// ============================================================

#include <Arduino.h>

#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// ============================================================
// SPI PIN CONFIGURATION
// ============================================================

const int SPI_SCLK_PIN = 18;
const int SPI_MOSI_PIN = 23;
const int SPI_MISO_PIN = 19;
const int SPI_CS_PIN   = 5;


// ============================================================
// READY PIN
// ============================================================
//
// GPIO4 is connected to Student 2 GPIO32.
//
// HIGH = ready
// LOW  = busy/not ready
// ============================================================

const int READY_PIN = 4;


// ============================================================
// MESSAGE CONFIGURATION
// ============================================================

const size_t MAX_MESSAGE_LENGTH = 128;


// ============================================================
// SPI FRAME
// ============================================================

const size_t FRAME_SIZE = 132;

const uint8_t FRAME_MAGIC_1 = 0xA5;
const uint8_t FRAME_MAGIC_2 = 0x5A;


// ============================================================
// SPI HOST
// ============================================================
//
// IMPORTANT:
//
// The name MUST NOT be "SPI_HOST".
//
// SPI_HOST is already defined inside the ESP32 headers.
//
// SPI3_HOST is the hardware SPI host used here.
// ============================================================

const spi_host_device_t SPI_SLAVE_HOST = SPI3_HOST;


// ============================================================
// DMA RECEIVE BUFFER
// ============================================================
//
// SPI DMA requires suitable memory alignment/capability.
// heap_caps_malloc() provides DMA-capable memory.
// ============================================================

uint8_t* rxBuffer = nullptr;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

bool initializeSPISlave();

bool receiveOneSPIFrame();

bool validateAndDisplayFrame();


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
  // READY PIN
  // ----------------------------------------------------------

  pinMode(
    READY_PIN,
    OUTPUT
  );

  // Never claim READY during startup.
  digitalWrite(
    READY_PIN,
    LOW
  );


  // ----------------------------------------------------------
  // ALLOCATE DMA BUFFER
  // ----------------------------------------------------------

  rxBuffer = (uint8_t*)heap_caps_malloc(
    FRAME_SIZE,
    MALLOC_CAP_DMA
  );


  // ----------------------------------------------------------
  // CHECK BUFFER
  // ----------------------------------------------------------

  if (rxBuffer == nullptr)
  {
    Serial.println();
    Serial.println("================================================");
    Serial.println("ERROR: DMA BUFFER ALLOCATION FAILED");
    Serial.println("================================================");

    digitalWrite(
      READY_PIN,
      LOW
    );

    while (true)
    {
      delay(1000);
    }
  }


  // ----------------------------------------------------------
  // CLEAR BUFFER
  // ----------------------------------------------------------

  memset(
    rxBuffer,
    0,
    FRAME_SIZE
  );


  // ----------------------------------------------------------
  // INITIALIZE SPI SLAVE
  // ----------------------------------------------------------

  if (!initializeSPISlave())
  {
    Serial.println();
    Serial.println("================================================");
    Serial.println("ERROR: SPI SLAVE INITIALIZATION FAILED");
    Serial.println("================================================");

    digitalWrite(
      READY_PIN,
      LOW
    );

    while (true)
    {
      delay(1000);
    }
  }


  // ----------------------------------------------------------
  // DISPLAY CONFIGURATION
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("================================================");
  Serial.println(" STUDENT 3 - SPI RECEIVER");
  Serial.println("================================================");
  Serial.println();

  Serial.println("SPI SLAVE:");
  Serial.println("  SCLK : GPIO18");
  Serial.println("  MOSI : GPIO23");
  Serial.println("  MISO : GPIO19");
  Serial.println("  CS   : GPIO5");
  Serial.println("  Mode : SPI MODE1");
  Serial.println();

  Serial.println("READY:");
  Serial.println("  OUTPUT: GPIO4");
  Serial.println();

  Serial.println("SPI frame size: 132 bytes");
  Serial.println("Maximum message: 128 characters");
  Serial.println();

  Serial.println("SPI slave initialized successfully.");
  Serial.println("Waiting for SPI messages...");
  Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  receiveOneSPIFrame();
}


// ============================================================
// INITIALIZE SPI SLAVE
// ============================================================

bool initializeSPISlave()
{
  // ----------------------------------------------------------
  // SPI BUS CONFIGURATION
  // ----------------------------------------------------------

  spi_bus_config_t busConfig = {};

  busConfig.mosi_io_num = SPI_MOSI_PIN;
  busConfig.miso_io_num = SPI_MISO_PIN;
  busConfig.sclk_io_num = SPI_SCLK_PIN;

  busConfig.quadwp_io_num = -1;
  busConfig.quadhd_io_num = -1;

  busConfig.max_transfer_sz = FRAME_SIZE;


  // ----------------------------------------------------------
  // SPI SLAVE INTERFACE CONFIGURATION
  // ----------------------------------------------------------

  spi_slave_interface_config_t slaveConfig = {};

  slaveConfig.spics_io_num = SPI_CS_PIN;

  // Must match Student 2.
  slaveConfig.mode = 1;

  slaveConfig.queue_size = 1;

  slaveConfig.flags = 0;

  slaveConfig.post_setup_cb = nullptr;
  slaveConfig.post_trans_cb = nullptr;


  // ----------------------------------------------------------
  // INITIALIZE SPI SLAVE
  // ----------------------------------------------------------
  //
  // SPI_DMA_CH_AUTO lets ESP-IDF select a DMA channel.
  // ----------------------------------------------------------

  esp_err_t result = spi_slave_initialize(
    SPI_SLAVE_HOST,
    &busConfig,
    &slaveConfig,
    SPI_DMA_CH_AUTO
  );


  // ----------------------------------------------------------
  // CHECK RESULT
  // ----------------------------------------------------------

  if (result != ESP_OK)
  {
    Serial.print("SPI initialization error: ");

    Serial.println(
      esp_err_to_name(result)
    );

    return false;
  }


  return true;
}


// ============================================================
// RECEIVE ONE SPI FRAME
// ============================================================

bool receiveOneSPIFrame()
{
  // ----------------------------------------------------------
  // CLEAR RECEIVE BUFFER
  // ----------------------------------------------------------

  memset(
    rxBuffer,
    0,
    FRAME_SIZE
  );


  // ----------------------------------------------------------
  // CREATE SPI TRANSACTION
  // ----------------------------------------------------------

  spi_slave_transaction_t transaction = {};


  // 132 bytes x 8 bits
  transaction.length = FRAME_SIZE * 8;


  // Student 3 only receives.
  transaction.tx_buffer = nullptr;

  transaction.rx_buffer = rxBuffer;


  // ----------------------------------------------------------
  // QUEUE TRANSACTION
  // ----------------------------------------------------------
  //
  // Once this succeeds, Student 3 is physically armed to
  // receive the next SPI transaction.
  // ----------------------------------------------------------

  esp_err_t result = spi_slave_queue_trans(
    SPI_SLAVE_HOST,
    &transaction,
    portMAX_DELAY
  );


  // ----------------------------------------------------------
  // CHECK QUEUE RESULT
  // ----------------------------------------------------------

  if (result != ESP_OK)
  {
    digitalWrite(
      READY_PIN,
      LOW
    );

    Serial.print(
      "ERROR: Could not queue SPI transaction: "
    );

    Serial.println(
      esp_err_to_name(result)
    );

    delay(100);

    return false;
  }


  // ----------------------------------------------------------
  // TELL STUDENT 2 THAT WE ARE READY
  // ----------------------------------------------------------

  digitalWrite(
    READY_PIN,
    HIGH
  );


  // ----------------------------------------------------------
  // WAIT FOR STUDENT 2
  // ----------------------------------------------------------

  spi_slave_transaction_t* completedTransaction = nullptr;


  result = spi_slave_get_trans_result(
    SPI_SLAVE_HOST,
    &completedTransaction,
    portMAX_DELAY
  );


  // ----------------------------------------------------------
  // TRANSACTION COMPLETE
  // ----------------------------------------------------------
  //
  // We are no longer ready for another transaction until the
  // next transaction has been queued.
  // ----------------------------------------------------------

  digitalWrite(
    READY_PIN,
    LOW
  );


  // ----------------------------------------------------------
  // CHECK RESULT
  // ----------------------------------------------------------

  if (result != ESP_OK)
  {
    Serial.print(
      "ERROR: SPI transaction failed: "
    );

    Serial.println(
      esp_err_to_name(result)
    );

    return false;
  }


  // ----------------------------------------------------------
  // CHECK TRANSACTION POINTER
  // ----------------------------------------------------------

  if (completedTransaction == nullptr)
  {
    Serial.println(
      "ERROR: Completed transaction is NULL."
    );

    return false;
  }


  // ----------------------------------------------------------
  // CHECK TRANSACTION LENGTH
  // ----------------------------------------------------------

  if (completedTransaction->trans_len != FRAME_SIZE * 8)
  {
    Serial.print(
      "ERROR: Incorrect SPI frame length. Received "
    );

    Serial.print(
      completedTransaction->trans_len
    );

    Serial.println(
      " bits."
    );

    return false;
  }


  // ----------------------------------------------------------
  // VALIDATE FRAME
  // ----------------------------------------------------------

  return validateAndDisplayFrame();
}


// ============================================================
// VALIDATE AND DISPLAY RECEIVED FRAME
// ============================================================

bool validateAndDisplayFrame()
{
  // ----------------------------------------------------------
  // CHECK MAGIC BYTE 1
  // ----------------------------------------------------------

  if (rxBuffer[0] != FRAME_MAGIC_1)
  {
    Serial.println();
    Serial.println(
      "ERROR: Invalid SPI frame header byte 1."
    );

    return false;
  }


  // ----------------------------------------------------------
  // CHECK MAGIC BYTE 2
  // ----------------------------------------------------------

  if (rxBuffer[1] != FRAME_MAGIC_2)
  {
    Serial.println();
    Serial.println(
      "ERROR: Invalid SPI frame header byte 2."
    );

    return false;
  }


  // ----------------------------------------------------------
  // READ MESSAGE LENGTH
  // ----------------------------------------------------------

  size_t messageLength = rxBuffer[2];


  // ----------------------------------------------------------
  // CHECK MESSAGE LENGTH
  // ----------------------------------------------------------

  if (messageLength > MAX_MESSAGE_LENGTH)
  {
    Serial.println();
    Serial.println(
      "ERROR: Received message length exceeds 128."
    );

    return false;
  }


  // ----------------------------------------------------------
  // CHECK NEWLINE TERMINATOR
  // ----------------------------------------------------------

  size_t terminatorIndex = 3 + messageLength;


  if (rxBuffer[terminatorIndex] != '\n')
  {
    Serial.println();
    Serial.println(
      "ERROR: SPI message terminator is missing."
    );

    return false;
  }


  // ----------------------------------------------------------
  // CREATE MESSAGE BUFFER
  // ----------------------------------------------------------

  char receivedMessage[MAX_MESSAGE_LENGTH + 1];


  // ----------------------------------------------------------
  // COPY MESSAGE
  // ----------------------------------------------------------

  memcpy(
    receivedMessage,
    &rxBuffer[3],
    messageLength
  );


  // Add null terminator.
  receivedMessage[messageLength] = '\0';


  // ----------------------------------------------------------
  // DISPLAY COMPLETE MESSAGE
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("============================================");
  Serial.println("       SPI MESSAGE RECEIVED");
  Serial.println("============================================");

  Serial.print("Message: ");
  Serial.println(receivedMessage);

  Serial.print("Length : ");
  Serial.println(messageLength);

  Serial.println("============================================");
  Serial.println();


  return true;
}
