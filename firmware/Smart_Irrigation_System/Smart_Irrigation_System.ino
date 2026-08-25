#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_task_wdt.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================================
// 1 & 2: Constexpr Definitions (Modern C++)
// ==========================================
constexpr uint16_t EEPROM_SIZE = 128;
constexpr uint16_t SETTINGS_ADDR = 0;

constexpr uint16_t EEPROM_MAGIC_VAL = 0xA55A;
constexpr uint8_t  EEPROM_VERSION_VAL = 1;

constexpr uint8_t SOIL_PIN   = 34;
constexpr uint8_t RELAY_PIN  = 27;
constexpr uint8_t UP_BTN     = 32;
constexpr uint8_t DOWN_BTN   = 33;
constexpr uint8_t ENTER_BTN  = 25;
constexpr uint8_t BACK_BTN   = 26;
constexpr uint8_t BUZZER_PIN = 14;

// DHT Settings
constexpr uint8_t DHT_PIN = 13;
constexpr uint8_t DHT_TYPE = DHT11;

// SIM800L Settings
constexpr char ALERT_PHONE[] = "+XXXXXXXXXX";
constexpr uint8_t AUTHORIZED_NUMBERS = 2; //it can be more

const char* authorizedNumbers[AUTHORIZED_NUMBERS] =
{
  "+xxxxxxxxxxxx",
  "+yyyyyyyyyyyy"
};

constexpr uint8_t SIM800_RX_PIN = 16;
constexpr uint8_t SIM800_TX_PIN = 17;
constexpr uint32_t SIM800_BAUD = 9600;

// Wi-Fi Settings
constexpr char WIFI_SSID[] = "YOUR WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR PASSWORD";
constexpr char WIFI_API_KEY[] = "YOUR API_KEY";


constexpr bool RELAY_ON  = LOW;
constexpr bool RELAY_OFF = HIGH;

// Menu Bounds
constexpr uint8_t MAIN_MENU_ITEMS = 3;
constexpr uint8_t SETTINGS_MENU_ITEMS = 4;

// Default Configuration Values
constexpr uint8_t DEFAULT_START = 35;
constexpr uint8_t DEFAULT_STOP  = 60;
constexpr uint16_t DEFAULT_DRY  = 2800;
constexpr uint16_t DEFAULT_WET  = 1800;

// ==========================================
// 3: System Constants
// ==========================================
constexpr uint32_t GSM_INIT_RETRY_DELAY = 3000;
constexpr uint32_t WDT_TIMEOUT_SECONDS = 30;
constexpr uint32_t EEPROM_RETRY_DELAY = 1000;
constexpr uint8_t EEPROM_MAX_RETRIES = 3;
constexpr uint32_t PUMP_FAULT_RESET_HOLD = 3000;
constexpr const char* CMD_RESET_PUMP = "RESET PUMP";
constexpr uint16_t CAL_MIN_DIFF = 300;
constexpr uint8_t DHT_FAIL_LIMIT = 3;
constexpr uint8_t DHT_RECOVERY_LIMIT = 2;
constexpr uint8_t SENSOR_BAD_SAMPLE_LIMIT = 5;
constexpr uint8_t SENSOR_GOOD_SAMPLE_LIMIT = 10;
constexpr uint8_t ALERT_QUEUE_SIZE = 8;
constexpr uint8_t ALERT_MESSAGE_SIZE = 96;

constexpr uint32_t GSM_NETWORK_CHECK_INTERVAL = 10000;
constexpr uint32_t GSM_SIGNAL_CHECK_INTERVAL  = 30000;
constexpr uint32_t GSM_RECOVERY_DELAY         = 30000;

constexpr uint8_t GSM_MAX_NETWORK_FAILURES = 3;
constexpr uint8_t GSM_MAX_MODEM_FAILURES   = 3;
constexpr uint32_t LCD_REFRESH_TIME    = 250;
constexpr uint32_t SERIAL_REFRESH_TIME = 1000;
constexpr uint32_t BUTTON_DEBOUNCE     = 50; 
constexpr uint32_t EEPROM_COMMIT_DELAY = 5000;
constexpr uint32_t MAX_PUMP_RUNTIME    = 900000; // 15 Minutes
constexpr uint32_t DHT_INTERVAL        = 2000; 

// ==========================================
// Hardware Objects
// ==========================================
DHT dht(DHT_PIN, DHT_TYPE);
HardwareSerial sim800(2);

// ==========================================
// 4: Enum Classes
// ==========================================
enum class Mode : uint8_t
{
  AUTO,
  MANUAL
};

enum class Screen : uint8_t
{
  DASHBOARD,
  MAIN_MENU,
  SETTINGS_MENU,
  MANUAL_MENU,
  EDIT_START,
  EDIT_STOP,
  EDIT_DRY,
  EDIT_WET
};

// ==========================================
// 5 & 6: Struct & Memory Layout Validation
// ==========================================
struct DeviceSettings
{
  uint16_t magic;
  uint8_t  version;
  Mode     mode;
  uint8_t  startMoisture;
  uint8_t  stopMoisture;
  uint16_t dryValue;
  uint16_t wetValue;
};

static_assert(sizeof(DeviceSettings) <= 16, "DeviceSettings grew unexpectedly.");

DeviceSettings settings;

// Forward Declarations
void changeMode(Mode mode);
int getMoisture();

bool isCalibrationValid(
  uint16_t dryValue,
  uint16_t wetValue
);

// ==========================================
// 7: Hardware Abstraction (Input & Sensors)
// ==========================================
struct Button {
  uint8_t pin;
  bool state = HIGH;
  bool lastFlicker = HIGH;
  bool lastState = HIGH;
  unsigned long lastDebounceTime = 0;

  void init(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
  }

  void update() {
    lastState = state; 
    bool reading = digitalRead(pin);

    if (reading != lastFlicker) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE) {
      if (reading != state) {
        state = reading;
      }
    }
    lastFlicker = reading;
  }

  bool isPressed() const {
    return (lastState == HIGH && state == LOW);
  }
};

Button btnUp, btnDown, btnEnter, btnBack;

struct DHTData
{
  float temperature = 0.0f;
  float humidity = 0.0f;

  bool valid = false;

  uint8_t failCount = 0;
  uint8_t successCount = 0;

  unsigned long lastRead = 0;
};

DHTData dhtData;

// ==========================================
// 8: Error Manager
// ==========================================
struct ErrorManager
{
  bool sensorFault = false;
  bool pumpTimeout = false;
  bool calibrationError = false;
  bool eepromError = false;

  uint8_t sensorBadSamples = 0;
  uint8_t sensorGoodSamples = 0;
};
ErrorManager systemError;

// ==========================================
// Communication Subsystems (GSM & Wi-Fi)
// ==========================================
struct GSMManager
{
  bool initialized = false;
  bool networkRegistered = false;
  bool smsReady = false;

  int signalStrength = -1;

  uint8_t networkFailCount = 0;
  uint8_t modemFailCount = 0;

  unsigned long lastNetworkCheck = 0;
  unsigned long lastSignalCheck = 0;
  unsigned long lastRecoveryAttempt = 0;

  bool pumpTimeoutAlertSent = false;
  bool sensorFaultAlertSent = false;
};

GSMManager gsm;

enum class GSMState : uint8_t
{
  OFFLINE,
  INITIALIZING,
  ONLINE,
  DEGRADED,
  RECOVERING
};

enum class GSMInitStep : uint8_t
{
  NONE,
  AT,
  ECHO_OFF,
  SIM_CHECK,
  SMS_MODE,
  CHARSET,
  SMS_NOTIFICATION,
  COMPLETE,
  FAILED
};

enum class CommandType : uint8_t
{
  NONE,
  PUMP_ON,
  PUMP_OFF,
  MODE_AUTO,
  MODE_MANUAL,
  RESET_PUMP_FAULT,
  GET_STATUS
};
enum class CommandResult : uint8_t
{
  SUCCESS,
  REJECTED,
  INVALID,
  SAFETY_LOCKED
};

CommandResult executeCommand(CommandType command);
GSMInitStep gsmInitStep = GSMInitStep::NONE;



enum class GSMOperation : uint8_t
{
  NONE,
  NETWORK_CHECK,
  SIGNAL_CHECK
};

GSMState gsmState = GSMState::OFFLINE;
GSMOperation gsmOperation = GSMOperation::NONE;

struct GSMTransaction
{
  bool active = false;

  String command;
  String expected;

  String response;

  unsigned long startTime = 0;
  uint32_t timeout = 0;
};

GSMTransaction gsmTransaction;

bool startGSMCommand(
  const char* command,
  const char* expected,
  uint32_t timeout
)
{
  if (gsmTransaction.active)
    return false;

  gsmTransaction.command = command;
  gsmTransaction.expected = expected;
  gsmTransaction.response = "";
  gsmTransaction.timeout = timeout;
  gsmTransaction.startTime = millis();
  gsmTransaction.active = true;

  sim800.println(command);

  return true;
}


struct SMSTransaction
{
  bool pending = false;
  bool waitingForPrompt = false;
  bool waitingForResult = false;
  bool isAlert = false;
  String phoneNumber;
  String message;
  String response;

  unsigned long startTime = 0;
};

SMSTransaction smsTransaction;

bool startSMS(
  const char* phoneNumber,
  const char* message,
  bool alertMessage = false
);

struct AlertQueue
{
  char messages[ALERT_QUEUE_SIZE][ALERT_MESSAGE_SIZE];

  uint8_t head = 0;
  uint8_t tail = 0;
  uint8_t count = 0;
};

// ==========================================
// Single Source of Truth Definitions
// ==========================================
struct SystemState
{
  bool pumpState = false;
  bool previousPumpState = false;
  bool manualPumpState = false;

  unsigned long pumpStartTime = 0;

  float filteredADC = 0;
};

struct UIState
{
  Screen currentScreen = Screen::DASHBOARD;
  Screen previousScreen = Screen::DASHBOARD;
  bool lcdNeedsRefresh = true;

  int menuIndex = 0;
  int settingsIndex = 0;
  int editValue = 0;
};

SystemState sys;
UIState ui;


AlertQueue alertQueue;

// GSM SMS Receiver Variables
String gsmRxBuffer = "";
String incomingSMSNumber = "";
String incomingSMSMessage = "";
bool smsReceiving = false;

void processGSM();
void processGSMRx();
void processGSMTransaction();
void processSMSTransaction();
void processGSMInitialization();


bool startSMS(
  const char* phoneNumber,
  const char* message,
  bool alertMessage
)
{
  if (smsTransaction.pending)
    return false;

  if (!gsm.initialized ||
      !gsm.networkRegistered)
    return false;

  smsTransaction.phoneNumber = phoneNumber;
  smsTransaction.message = message;
  smsTransaction.response = "";

  smsTransaction.pending = true;
  smsTransaction.waitingForPrompt = false;
  smsTransaction.waitingForResult = false;
  smsTransaction.isAlert = alertMessage;
  smsTransaction.startTime = millis();

  return true;
}

bool isAuthorizedNumber(const String& sender);
void processSMS(const String& sender, const String& message);
void sendStatusSMS(const char* phoneNumber);

void sendGSMAlert(const char* message);
void processGSMAlerts();
void processAlertQueue();

struct WiFiManager
{
  bool connected = false;
  bool apiStarted = false;
  unsigned long lastAttempt = 0;
};

WiFiManager wifi;
WebServer server(80);

bool isAuthorizedAPI()
{
  if (!server.hasArg("key"))
    return false;

  return server.arg("key") == WIFI_API_KEY;
}

void handleStatus()
{
  if (!isAuthorizedAPI())
  {
    server.send(
      401,
      "application/json",
      "{\"error\":\"unauthorized\"}"
    );
    return;
  }

  String json = "{";

  json += "\"moisture\":";
  json += getMoisture();

  json += ",\"temperature\":";
  json += dhtData.valid
    ? String(dhtData.temperature, 1)
    : "null";

  json += ",\"humidity\":";
  json += dhtData.valid
    ? String(dhtData.humidity, 1)
    : "null";

  json += ",\"pump\":";
  json += sys.pumpState ? "true" : "false";

  json += ",\"mode\":\"";
  json += settings.mode == Mode::AUTO
    ? "AUTO"
    : "MANUAL";
  json += "\"";

  json += ",\"sensorFault\":";
  json += systemError.sensorFault ? "true" : "false";

  json += ",\"pumpTimeout\":";
  json += systemError.pumpTimeout ? "true" : "false";

  json += ",\"gsm\":";
  json += gsm.networkRegistered ? "true" : "false";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

void handleControl()
{
  if (!isAuthorizedAPI())
  {
    server.send(
      401,
      "application/json",
      "{\"error\":\"unauthorized\"}"
    );
    return;
  }

  if (!server.hasArg("cmd"))
  {
    server.send(
      400,
      "application/json",
      "{\"error\":\"missing cmd\"}"
    );
    return;
  }

  String command = server.arg("cmd");
  command.toUpperCase();

  CommandType type = CommandType::NONE;

  if (command == "PUMP_ON")
    type = CommandType::PUMP_ON;
  else if (command == "PUMP_OFF")
    type = CommandType::PUMP_OFF;
  else if (command == "AUTO")
    type = CommandType::MODE_AUTO;
  else if (command == "MANUAL")
    type = CommandType::MODE_MANUAL;
  else if (command == "RESET_PUMP")
    type = CommandType::RESET_PUMP_FAULT;
  else
  {
    server.send(
      400,
      "application/json",
      "{\"error\":\"unknown command\"}"
    );
    return;
  }

  CommandResult result = executeCommand(type);

  switch (result)
  {
    case CommandResult::SUCCESS:
      server.send(
        200,
        "application/json",
        "{\"result\":\"accepted\"}"
      );
      break;

    case CommandResult::REJECTED:
      server.send(
        409,
        "application/json",
        "{\"result\":\"rejected\"}"
      );
      break;

    case CommandResult::SAFETY_LOCKED:
      server.send(
        423,
        "application/json",
        "{\"result\":\"safety_locked\"}"
      );
      break;

    case CommandResult::INVALID:
    default:
      server.send(
        400,
        "application/json",
        "{\"result\":\"invalid\"}"
      );
      break;
  }
}
void initializeWiFiAPI()
{
  server.on(
    "/status",
    HTTP_GET,
    handleStatus
  );

  server.on(
    "/control",
    HTTP_GET,
    handleControl
  );

  server.begin();

  Serial.println(
    F("[WIFI] HTTP API started.")
  );
}


// ==========================================
// GSM Subsystem Functions
// ==========================================


void initializeGSM()
{
  if (gsmState == GSMState::INITIALIZING)
    return;

  Serial.println(F("[GSM] Starting initialization state machine."));

  gsmState = GSMState::INITIALIZING;
  gsmInitStep = GSMInitStep::AT;

  gsm.initialized = false;
  gsm.networkRegistered = false;
  gsm.smsReady = false;
  gsm.modemFailCount = 0;

  gsmTransaction.active = false;
  gsmTransaction.response = "";
  gsmOperation = GSMOperation::NONE;

  gsmInitStep = GSMInitStep::AT;

  startGSMCommand(
    "AT",
    "OK",
    3000
  );
}




void updateGSM()
{
  if (gsmState == GSMState::INITIALIZING)
    return;

  if (gsmState == GSMState::OFFLINE)
  {
    static unsigned long lastInitAttempt = 0;

    if (millis() - lastInitAttempt >= GSM_INIT_RETRY_DELAY)
    {
      lastInitAttempt = millis();

      Serial.println(
        F("[GSM] Retrying initialization...")
      );

      initializeGSM();
    }

    return;
  }

  if (gsmState != GSMState::ONLINE &&
      gsmState != GSMState::DEGRADED)
  {
    return;
  }

  if (gsmTransaction.active ||
      smsTransaction.pending)
  {
    return;
  }

  // Network check
  if (
    millis() - gsm.lastNetworkCheck >=
    GSM_NETWORK_CHECK_INTERVAL
  )
  {
    gsm.lastNetworkCheck = millis();

    gsmOperation =
      GSMOperation::NETWORK_CHECK;

    startGSMCommand(
      "AT+CREG?",
      "OK",
      3000
    );

    return;
  }

  // Signal check
  if (
    millis() - gsm.lastSignalCheck >=
    GSM_SIGNAL_CHECK_INTERVAL
  )
  {
    gsm.lastSignalCheck = millis();

    gsmOperation =
      GSMOperation::SIGNAL_CHECK;

    startGSMCommand(
      "AT+CSQ",
      "OK",
      2000
    );
  }
}



// ==========================================
// GSM SMS Processing Logic
// ==========================================
void processGSMRx()
{
  while (sim800.available())
  {
    char c = sim800.read();

    // =====================================================
    // 1. SMS SEND: WAITING FOR '>' PROMPT
    // =====================================================
    if (smsTransaction.waitingForPrompt)
    {
      if (c == '>')
      {
        sim800.print(smsTransaction.message);
        sim800.write(26); // CTRL+Z

        smsTransaction.waitingForPrompt = false;
        smsTransaction.waitingForResult = true;
        smsTransaction.startTime = millis();
        smsTransaction.response = "";
      }

      continue;
    }

    // =====================================================
    // 2. SMS SEND: WAITING FOR FINAL RESULT
    // =====================================================
    if (smsTransaction.waitingForResult)
    {
      smsTransaction.response += c;

      if (smsTransaction.response.indexOf("OK") >= 0)
      {
        Serial.println(F("[GSM] SMS sent."));

        if (smsTransaction.isAlert)
        {
          if (alertQueue.count > 0)
          {
            alertQueue.head =
              (alertQueue.head + 1) % ALERT_QUEUE_SIZE;

            alertQueue.count--;

            Serial.println(
              F("[ALERT] Queue item delivered.")
            );
          }
        }

        smsTransaction.pending = false;
        smsTransaction.waitingForPrompt = false;
        smsTransaction.waitingForResult = false;
        smsTransaction.isAlert = false;
        smsTransaction.response = "";
      }
      else if (
        smsTransaction.response.indexOf("ERROR") >= 0
      )
      {
        Serial.println(F("[GSM] SMS failed."));

        // Keep alert in queue on failure.
        smsTransaction.pending = false;
        smsTransaction.waitingForPrompt = false;
        smsTransaction.waitingForResult = false;
        smsTransaction.isAlert = false;
        smsTransaction.response = "";
      }

      continue;
    }

    // =====================================================
    // 3. NORMAL GSM / AT RESPONSE
    // =====================================================
    if (gsmTransaction.active)
    {
      gsmTransaction.response += c;
      continue;
    }

    // =====================================================
    // 4. UNSOLICITED GSM DATA / INCOMING SMS
    // =====================================================
    gsmRxBuffer += c;

    if (gsmRxBuffer.length() > 512)
    {
      gsmRxBuffer = "";
      smsReceiving = false;
    }

    // -----------------------------------------
    // Find incoming SMS header
    // -----------------------------------------
    if (!smsReceiving)
    {
      int headerIndex =
        gsmRxBuffer.indexOf("+CMT:");

      if (headerIndex >= 0)
      {
        int lineEnd =
          gsmRxBuffer.indexOf('\n', headerIndex);

        if (lineEnd >= 0)
        {
          String header =
            gsmRxBuffer.substring(
              headerIndex,
              lineEnd
            );

          int q1 = header.indexOf('"');
          int q2 = header.indexOf('"', q1 + 1);

          if (q1 >= 0 && q2 > q1)
          {
            incomingSMSNumber =
              header.substring(q1 + 1, q2);

            smsReceiving = true;

            gsmRxBuffer =
              gsmRxBuffer.substring(lineEnd + 1);
          }
        }
      }
    }

    // -----------------------------------------
    // Read SMS body
    // -----------------------------------------
    if (smsReceiving)
    {
      int lineEnd =
        gsmRxBuffer.indexOf('\n');

      if (lineEnd >= 0)
      {
        incomingSMSMessage =
          gsmRxBuffer.substring(0, lineEnd);

        incomingSMSMessage.trim();

        processSMS(
          incomingSMSNumber,
          incomingSMSMessage
        );

        incomingSMSNumber = "";
        incomingSMSMessage = "";

        gsmRxBuffer =
          gsmRxBuffer.substring(lineEnd + 1);

        smsReceiving = false;
      }
    }
  }
}

void processGSMTransaction()
{
  if (!gsmTransaction.active)
    return;

  // =====================================================
  // Expected response received
  // =====================================================
  if (
    gsmTransaction.response.indexOf(
      gsmTransaction.expected
    ) >= 0
  )
  {
    String response =
      gsmTransaction.response;

    gsmTransaction.active = false;
    gsmTransaction.response = "";
    gsm.modemFailCount = 0;

    // ===================================================
    // Network check
    // ===================================================
    if (gsmOperation == GSMOperation::NETWORK_CHECK)
    {
      if (
        response.indexOf("+CREG: 0,1") >= 0 ||
        response.indexOf("+CREG: 0,5") >= 0
      )
      {
        gsm.networkRegistered = true;
        gsm.networkFailCount = 0;

        if (gsmState == GSMState::DEGRADED)
        {
          gsmState = GSMState::ONLINE;

          Serial.println(
            F("[GSM] Network recovered.")
          );
        }
      }
      else
      {
        gsm.networkRegistered = false;

        if (gsm.networkFailCount < 255)
          gsm.networkFailCount++;

        if (
          gsm.networkFailCount >=
          GSM_MAX_NETWORK_FAILURES
        )
        {
          gsmState = GSMState::DEGRADED;

          Serial.println(
            F("[GSM] Network DEGRADED.")
          );
        }
      }
    }

    // ===================================================
    // Signal strength
    // ===================================================
    if (gsmOperation == GSMOperation::SIGNAL_CHECK)
    {
      int index =
        response.indexOf("+CSQ:");

      if (index >= 0)
      {
        int comma =
          response.indexOf(',', index);

        if (comma > index)
        {
          String value =
            response.substring(
              index + 5,
              comma
            );

          gsm.signalStrength =
            value.toInt();
        }
      }
    }

    gsmOperation = GSMOperation::NONE;

    return;
  }

  // =====================================================
  // ERROR received
  // =====================================================
  if (
    gsmTransaction.response.indexOf("ERROR") >= 0
  )
  {
    gsmTransaction.active = false;
    gsmTransaction.response = "";

    gsm.modemFailCount++;

    Serial.println(
      F("[GSM] AT command ERROR.")
    );

    if (gsmState == GSMState::INITIALIZING)
    {
      gsmInitStep = GSMInitStep::FAILED;

      gsm.initialized = false;
      gsm.networkRegistered = false;
      gsm.smsReady = false;

      gsmState = GSMState::OFFLINE;

      Serial.println(
        F("[GSM] Initialization FAILED.")
      );
    }
    else if (
      gsm.modemFailCount >=
      GSM_MAX_MODEM_FAILURES
    )
    {
      gsm.initialized = false;
      gsm.networkRegistered = false;
      gsm.smsReady = false;

      gsmState = GSMState::OFFLINE;

      Serial.println(
        F("[GSM] Modem failure threshold reached. OFFLINE.")
      );
    }

    gsmOperation = GSMOperation::NONE;

    return;
  }

  // =====================================================
  // TIMEOUT
  // =====================================================
  if (
    millis() - gsmTransaction.startTime >
    gsmTransaction.timeout
  )
  {
    gsmTransaction.active = false;
    gsmTransaction.response = "";

    gsm.modemFailCount++;

    Serial.println(
      F("[GSM] AT command TIMEOUT.")
    );

    if (gsmState == GSMState::INITIALIZING)
    {
      gsmInitStep = GSMInitStep::FAILED;

      gsm.initialized = false;
      gsm.networkRegistered = false;
      gsm.smsReady = false;

      gsmState = GSMState::OFFLINE;

      Serial.println(
        F("[GSM] Initialization TIMEOUT.")
      );
    }
    else if (
      gsm.modemFailCount >=
      GSM_MAX_MODEM_FAILURES
    )
    {
      gsm.initialized = false;
      gsm.networkRegistered = false;
      gsm.smsReady = false;

      gsmState = GSMState::OFFLINE;

      Serial.println(
        F("[GSM] Modem timeout threshold reached. OFFLINE.")
      );
    }

    gsmOperation = GSMOperation::NONE;
  }
}

void processGSMInitialization()
{
  if (gsmState != GSMState::INITIALIZING)
    return;

  if (gsmTransaction.active)
    return;

  if (gsmInitStep == GSMInitStep::AT)
  {
    gsmInitStep = GSMInitStep::ECHO_OFF;

    startGSMCommand(
      "ATE0",
      "OK",
      2000
    );

    return;
  }

  if (gsmInitStep == GSMInitStep::ECHO_OFF)
  {
    gsmInitStep = GSMInitStep::SIM_CHECK;

    startGSMCommand(
      "AT+CPIN?",
      "READY",
      3000
    );

    return;
  }

  if (gsmInitStep == GSMInitStep::SIM_CHECK)
  {
    gsmInitStep = GSMInitStep::SMS_MODE;

    startGSMCommand(
      "AT+CMGF=1",
      "OK",
      2000
    );

    return;
  }

  if (gsmInitStep == GSMInitStep::SMS_MODE)
  {
    gsmInitStep = GSMInitStep::CHARSET;

    startGSMCommand(
      "AT+CSCS=\"GSM\"",
      "OK",
      2000
    );

    return;
  }

  if (gsmInitStep == GSMInitStep::CHARSET)
  {
    gsmInitStep = GSMInitStep::SMS_NOTIFICATION;

    startGSMCommand(
      "AT+CNMI=2,2,0,0,0",
      "OK",
      2000
    );

    return;
  }

  if (gsmInitStep == GSMInitStep::SMS_NOTIFICATION)
  {
    gsmInitStep = GSMInitStep::COMPLETE;

    gsm.initialized = true;
    gsm.smsReady = true;
    gsm.networkFailCount = 0;
    gsm.modemFailCount = 0;

    gsmState = GSMState::ONLINE;

    Serial.println(
      F("[GSM] Initialization complete.")
    );

    return;
  }
}

void processSMSTransaction()
{
  if (!smsTransaction.pending)
    return;

  // ==========================================
  // Start CMGS command
  // ==========================================
  if (
    !smsTransaction.waitingForPrompt &&
    !smsTransaction.waitingForResult
  )
  {
    sim800.print("AT+CMGS=\"");
    sim800.print(smsTransaction.phoneNumber);
    sim800.println("\"");

    smsTransaction.response = "";
    smsTransaction.waitingForPrompt = true;
    smsTransaction.startTime = millis();

    return;
  }

  // ==========================================
  // Prompt timeout
  // ==========================================
  if (
    smsTransaction.waitingForPrompt &&
    millis() - smsTransaction.startTime > 5000
  )
  {
    Serial.println(
      F("[GSM] SMS prompt timeout.")
    );

    smsTransaction.pending = false;
    smsTransaction.waitingForPrompt = false;
    smsTransaction.waitingForResult = false;
    smsTransaction.isAlert = false;
    smsTransaction.response = "";

    return;
  }

  // ==========================================
  // Result timeout
  // ==========================================
  if (
    smsTransaction.waitingForResult &&
    millis() - smsTransaction.startTime > 10000
  )
  {
    Serial.println(
      F("[GSM] SMS result timeout.")
    );

    // IMPORTANT:
    // Alert is NOT removed from queue.
    smsTransaction.pending = false;
    smsTransaction.waitingForPrompt = false;
    smsTransaction.waitingForResult = false;
    smsTransaction.isAlert = false;
    smsTransaction.response = "";

    return;
  }
}
void processGSM()
{
  processGSMRx();
  processGSMTransaction();
  processSMSTransaction();
  processGSMInitialization();
}

bool isAuthorizedNumber(const String& number)
{
  for (uint8_t i = 0; i < AUTHORIZED_NUMBERS; i++)
  {
    if (number == authorizedNumbers[i])
      return true;
  }

  return false;
}

void processSMS(
  const String& sender,
  const String& message
)
{
  Serial.print(F("[GSM] SMS from: "));
  Serial.println(sender);

  if (!isAuthorizedNumber(sender))
  {
    Serial.println(
      F("[GSM] Unauthorized sender.")
    );

    return;
  }

  String command = message;
  command.trim();
  command.toUpperCase();

  CommandType type = CommandType::NONE;

  if (command == "STATUS")
  {
    type = CommandType::GET_STATUS;
  }
  else if (command == "PUMP ON")
  {
    type = CommandType::PUMP_ON;
  }
  else if (command == "PUMP OFF")
  {
    type = CommandType::PUMP_OFF;
  }
  else if (command == "AUTO")
  {
    type = CommandType::MODE_AUTO;
  }
  else if (command == "MANUAL")
  {
    type = CommandType::MODE_MANUAL;
  }
  else if (command == "RESET PUMP")
  {
    type = CommandType::RESET_PUMP_FAULT;
  }
  else
  {
    startSMS(
      sender.c_str(),
      "Unknown command."
    );

    return;
  }

  if (type == CommandType::GET_STATUS)
  {
    sendStatusSMS(sender.c_str());
    return;
  }

  CommandResult result =
    executeCommand(type);

  switch (result)
  {
    case CommandResult::SUCCESS:
      startSMS(
        sender.c_str(),
        "Command accepted."
      );
      break;

    case CommandResult::REJECTED:
      startSMS(
        sender.c_str(),
        "Command rejected."
      );
      break;

    case CommandResult::SAFETY_LOCKED:
      startSMS(
        sender.c_str(),
        "Command blocked by safety fault."
      );
      break;

    case CommandResult::INVALID:
    default:
      startSMS(
        sender.c_str(),
        "Invalid command."
      );
      break;
  }
}

void sendStatusSMS(const char* phoneNumber)
{
  char message[160];
  int moisture = getMoisture();

  if (dhtData.valid)
  {
    snprintf(
      message,
      sizeof(message),
      "Moist:%d%% Temp:%.1fC Hum:%.1f%% Pump:%s Mode:%s",
      moisture,
      dhtData.temperature,
      dhtData.humidity,
      sys.pumpState ? "ON" : "OFF",
      settings.mode == Mode::AUTO ? "AUTO" : "MANUAL"
    );
  }
  else
  {
    snprintf(
      message,
      sizeof(message),
      "Moist:%d%% DHT:ERROR Pump:%s Mode:%s",
      moisture,
      sys.pumpState ? "ON" : "OFF",
      settings.mode == Mode::AUTO ? "AUTO" : "MANUAL"
    );
  }

  startSMS(phoneNumber, message);
}

bool enqueueAlert(const char* message)
{
  if (alertQueue.count >= ALERT_QUEUE_SIZE)
  {
    Serial.println(F("[ALERT] Queue full. Event dropped."));
    return false;
  }

  strncpy(
    alertQueue.messages[alertQueue.tail],
    message,
    ALERT_MESSAGE_SIZE - 1
  );

  alertQueue.messages[alertQueue.tail][ALERT_MESSAGE_SIZE - 1] = '\0';

  alertQueue.tail =
    (alertQueue.tail + 1) % ALERT_QUEUE_SIZE;

  alertQueue.count++;

  Serial.print(F("[ALERT] Queued: "));
  Serial.println(message);

  return true;
}

void sendGSMAlert(const char* message)
{
  enqueueAlert(message);
}
void processAlertQueue()
{
  if (alertQueue.count == 0)
    return;

  if (!gsm.initialized ||
      !gsm.networkRegistered)
    return;

  if (smsTransaction.pending)
    return;

  const char* message =
    alertQueue.messages[alertQueue.head];

  if (!startSMS(
        ALERT_PHONE,
        message,
        true
      ))
  {
    Serial.println(
      F("[ALERT] Unable to start alert SMS.")
    );

    return;
  }

  Serial.println(
    F("[ALERT] SMS transaction started.")
  );
}

void processPumpEvents()
{
  // OFF -> ON
  if (sys.pumpState && !sys.previousPumpState)
  {
    Serial.println(F("[EVENT] PUMP_STARTED"));

    // Queue pump transition alert
    sendGSMAlert("ALERT: Pump STARTED.");
  }

  // ON -> OFF
  if (!sys.pumpState && sys.previousPumpState)
  {
    Serial.println(F("[EVENT] PUMP_STOPPED"));

    // Queue pump transition alert
    sendGSMAlert("ALERT: Pump STOPPED.");
  }

  // Save current state for the next loop
  sys.previousPumpState = sys.pumpState;
}

void processGSMAlerts()
{
  // Pump Timeout
  if (systemError.pumpTimeout &&
      !gsm.pumpTimeoutAlertSent)
  {
    sendGSMAlert(
      "ALERT: Pump TIMEOUT. Pump stopped."
    );

    gsm.pumpTimeoutAlertSent = true;
  }



  // Reset alert flags after faults disappear
  if (!systemError.pumpTimeout)
  {
    gsm.pumpTimeoutAlertSent = false;
  }


}

// Wi-Fi Functions
void initializeWiFi()
{
  Serial.println(F("[WIFI] Starting..."));

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  wifi.lastAttempt = millis();
}

void updateWiFi()
{
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED)
  {
    if (!wifi.connected)
    {
      wifi.connected = true;

      Serial.println(F("[WIFI] Connected."));
      Serial.print(F("[WIFI] IP: "));
      Serial.println(WiFi.localIP());

      if (!wifi.apiStarted)
      {
        initializeWiFiAPI();
        wifi.apiStarted = true;
      }
    }

    return;
  }

  if (wifi.connected)
  {
    wifi.connected = false;

    Serial.println(
      F("[WIFI] Connection lost.")
    );
  }

  if (millis() - wifi.lastAttempt >= 10000)
  {
    wifi.lastAttempt = millis();

    Serial.println(
      F("[WIFI] Reconnecting...")
    );

    WiFi.disconnect();
    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }
}

// ==========================================
// System States, Flags & Timers
// ==========================================
bool settingsDirty = false;
unsigned long lastSettingChange = 0;

bool buzzerActive = false;
unsigned long buzzerStart = 0;
uint16_t buzzerDuration = 0;

// ==========================================
// Task Prototypes (State Machine)
// ==========================================
void dashboardTask();
void mainMenuTask();
void manualMenuTask();
void settingsMenuTask();
void editStartTask();
void editStopTask();
void editDryTask();
void editWetTask();

// ==========================================
// Helper Functions
// ==========================================

CommandResult executeCommand(CommandType command)
{
  switch (command)
  {
    case CommandType::PUMP_ON:

      if (systemError.pumpTimeout)
        return CommandResult::SAFETY_LOCKED;

      if (systemError.sensorFault)
        return CommandResult::SAFETY_LOCKED;

      if (settings.mode != Mode::MANUAL)
        return CommandResult::REJECTED;

      sys.manualPumpState = true;

      return CommandResult::SUCCESS;


    case CommandType::PUMP_OFF:

      sys.manualPumpState = false;

      return CommandResult::SUCCESS;


        case CommandType::MODE_AUTO:
      changeMode(Mode::AUTO); 
      sys.manualPumpState = false;
      return CommandResult::SUCCESS;


    case CommandType::MODE_MANUAL:

      changeMode(Mode::MANUAL);

      sys.manualPumpState = false;

      return CommandResult::SUCCESS;


    case CommandType::RESET_PUMP_FAULT:

  if (!systemError.pumpTimeout)
    return CommandResult::INVALID;

  resetPumpFault();

  return CommandResult::SUCCESS;


    case CommandType::GET_STATUS:

      return CommandResult::SUCCESS;


    case CommandType::NONE:
    default:

      return CommandResult::INVALID;
  }
}

void setPump(bool state)
{
  if (sys.pumpState == state)
    return;

  sys.pumpState = state;

  if (state)
  {
    sys.pumpStartTime = millis();

    Serial.println(F("[PUMP] STARTED"));
  }
  else
  {
    sys.pumpStartTime = 0;

    Serial.println(F("[PUMP] STOPPED"));
  }

  digitalWrite(
    RELAY_PIN,
    state ? RELAY_OFF : RELAY_ON
  );
}

void emergencyStop()
{
  setPump(false);
  sys.manualPumpState = false;
}
void resetPumpFault()
{
  if (!systemError.pumpTimeout)
  {
    Serial.println(
      F("[SAFETY] No pump timeout fault to reset.")
    );

    return;
  }

  systemError.pumpTimeout = false;

  // Allow a fresh timeout cycle after acknowledgement.
  sys.pumpStartTime = 0;

  Serial.println(
    F("[SAFETY] Pump timeout fault RESET.")
  );
}

void startBeep(uint16_t duration = 30)
{
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerStart = millis();
  buzzerDuration = duration;
}

void updateBuzzer()
{
  if (!buzzerActive)
    return;

  if (millis() - buzzerStart >= buzzerDuration)  
  {  
    digitalWrite(BUZZER_PIN, LOW);  
    buzzerActive = false;  
  }
}

void saveSettings()
{
  settingsDirty = true;
  lastSettingChange = millis();
}

bool verifyEEPROM()
{
  DeviceSettings storedSettings;

  EEPROM.get(
    SETTINGS_ADDR,
    storedSettings
  );

  if (storedSettings.magic != settings.magic)
    return false;

  if (storedSettings.version != settings.version)
    return false;

  if (storedSettings.mode != settings.mode)
    return false;

  if (storedSettings.startMoisture != settings.startMoisture)
    return false;

  if (storedSettings.stopMoisture != settings.stopMoisture)
    return false;

  if (storedSettings.dryValue != settings.dryValue)
    return false;

  if (storedSettings.wetValue != settings.wetValue)
    return false;

  return true;
}

bool isCalibrationValid(uint16_t dryValue, uint16_t wetValue)
{
  if (dryValue > 4095)
    return false;

  if (wetValue > 4095)
    return false;

  if (dryValue <= wetValue)
    return false;

  if ((dryValue - wetValue) < CAL_MIN_DIFF)
    return false;

  return true;
}

bool saveImmediately()
{
  EEPROM.put(SETTINGS_ADDR, settings);

  for (uint8_t attempt = 1;
       attempt <= EEPROM_MAX_RETRIES;
       attempt++)
  {
    if (EEPROM.commit())
{
  if (!verifyEEPROM())
  {
    systemError.eepromError = true;
    Serial.println(
      F("[EEPROM] Verification FAILED.")
    );

    return false;
  }

  systemError.eepromError = false;
  settingsDirty = false;

  Serial.println(
    F("[EEPROM] Commit + verification successful.")
  );

  return true;
}

    Serial.print(F("[EEPROM] Commit failed. Attempt "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.println(EEPROM_MAX_RETRIES);

    systemError.eepromError = true;

    if (attempt < EEPROM_MAX_RETRIES)
    {
      delay(EEPROM_RETRY_DELAY);
    }
  }

  Serial.println(F("[EEPROM] CRITICAL: Commit failed."));
  return false;
}

void resetSettings()
{
  settings.magic = EEPROM_MAGIC_VAL;
  settings.version = EEPROM_VERSION_VAL;
  settings.mode = Mode::AUTO;
  settings.startMoisture = DEFAULT_START;
  settings.stopMoisture = DEFAULT_STOP;
  settings.dryValue = DEFAULT_DRY;
  settings.wetValue = DEFAULT_WET;
}

bool isSettingsValid()
{
  if (settings.magic != EEPROM_MAGIC_VAL)
    return false;

  if (settings.version != EEPROM_VERSION_VAL)
    return false;

  if (settings.startMoisture > 100)
    return false;

  if (settings.stopMoisture > 100)  
    return false;  

  if (settings.startMoisture >= settings.stopMoisture)  
    return false;  

 if (!isCalibrationValid(
      settings.dryValue,
      settings.wetValue))
{
  return false;
}

  if (settings.mode != Mode::AUTO && settings.mode != Mode::MANUAL)  
    return false;  

  return true;
}

void loadSettings()
{
  EEPROM.get(SETTINGS_ADDR, settings);

  if (!isSettingsValid())  
  {  
    Serial.println(F("EEPROM Invalid or Version Mismatch. Resetting..."));
    resetSettings();  
    if (!saveImmediately())
    {
      Serial.println(
        F("[EEPROM] WARNING: Default settings could not be stored.")
      );
    }
  }
} 

void handleEEPROM()
{
  if (!settingsDirty)
    return;

  if (millis() - lastSettingChange < EEPROM_COMMIT_DELAY)
    return;

  if (saveImmediately())
  {
    Serial.println(F("[EEPROM] Settings saved."));
  }
  else
  {
    Serial.println(
      F("[EEPROM] Settings NOT saved.")
    );

    // Keep the dirty flag active so the next cycle can retry.
    settingsDirty = true;
    lastSettingChange = millis();
  }
}



void enterScreen(Screen screen)
{
  ui.currentScreen = screen;
  ui.lcdNeedsRefresh = true;
}

void changeMode(Mode mode)
{
  settings.mode = mode;
  saveSettings();
}

// ==========================================
// Sensor & Control Tasks
// ==========================================
void readButtons()
{
  btnUp.update();
  btnDown.update();
  btnEnter.update();
  btnBack.update();
}

void readSensors()
{
  const int raw = analogRead(SOIL_PIN);

  sys.filteredADC =
    sys.filteredADC * 0.90f +
    raw * 0.10f;

  // Calibration itself is invalid
  if (settings.dryValue <= settings.wetValue)
  {
    systemError.sensorFault = true;
    systemError.sensorBadSamples = SENSOR_BAD_SAMPLE_LIMIT;
    systemError.sensorGoodSamples = 0;
    return;
  }

  const bool invalidReading =
    (raw < 50 || raw > 4050);

  if (invalidReading)
  {
    systemError.sensorGoodSamples = 0;

    if (systemError.sensorBadSamples < SENSOR_BAD_SAMPLE_LIMIT)
      systemError.sensorBadSamples++;

    if (systemError.sensorBadSamples >= SENSOR_BAD_SAMPLE_LIMIT &&
        !systemError.sensorFault)
    {
      systemError.sensorFault = true;

      Serial.println(
        F("[SENSOR] Soil sensor fault detected.")
      );

      enqueueAlert(
        "ALERT: Soil sensor FAULT."
      );

      emergencyStop();
    }

    return;
  }

  // Valid reading
  systemError.sensorBadSamples = 0;

  if (systemError.sensorFault)
  {
    if (systemError.sensorGoodSamples < SENSOR_GOOD_SAMPLE_LIMIT)
      systemError.sensorGoodSamples++;

    if (systemError.sensorGoodSamples >= SENSOR_GOOD_SAMPLE_LIMIT)
    {
      systemError.sensorFault = false;
      systemError.sensorGoodSamples = 0;

      Serial.println(
        F("[SENSOR] Soil sensor recovered.")
      );

      enqueueAlert(
        "INFO: Soil sensor RECOVERED."
      );
    }
  }
  else
  {
    systemError.sensorGoodSamples = 0;
  }
}
void readDHT()
{
  if (millis() - dhtData.lastRead < DHT_INTERVAL)
    return;

  dhtData.lastRead = millis();

  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  // ------------------------------------------
  // Failed reading
  // ------------------------------------------
  if (isnan(humidity) || isnan(temperature))
  {
    dhtData.successCount = 0;

    if (dhtData.failCount < DHT_FAIL_LIMIT)
      dhtData.failCount++;

    if (dhtData.failCount >= DHT_FAIL_LIMIT)
    {
      if (dhtData.valid)
      {
        dhtData.valid = false;

        Serial.println(
          F("[DHT] Sensor fault detected.")
        );

        enqueueAlert(
          "ALERT: DHT sensor FAULT."
        );
      }
    }

    return;
  }

  // ------------------------------------------
  // Successful reading
  // ------------------------------------------
  dhtData.failCount = 0;

  if (!dhtData.valid)
  {
    if (dhtData.successCount < DHT_RECOVERY_LIMIT)
      dhtData.successCount++;

    if (dhtData.successCount >= DHT_RECOVERY_LIMIT)
    {
      dhtData.valid = true;
      dhtData.successCount = 0;

      Serial.println(
        F("[DHT] Sensor recovered.")
      );

      enqueueAlert(
        "INFO: DHT sensor RECOVERED."
      );
    }
  }
  else
  {
    dhtData.successCount = 0;
  }

  dhtData.temperature = temperature;
  dhtData.humidity = humidity;
}
int getMoisture()
{
  int moisture = map((int)sys.filteredADC, settings.dryValue, settings.wetValue, 0, 100);
  return constrain(moisture, 0, 100);
}

void controlPump()
{
  if (systemError.sensorFault)
  {
    emergencyStop();
    return;
  }

  if (systemError.pumpTimeout)  
  {  
    emergencyStop();  
    return;  
  }  

  int moisture = getMoisture();  

  if (settings.mode == Mode::AUTO)  
  {  
    if (moisture < settings.startMoisture)  
      setPump(true);  

    if (moisture > settings.stopMoisture)  
      setPump(false);  
  }  
  else  
  {  
    setPump(sys.manualPumpState);  
  }  

  if (sys.pumpState)
{
  if (millis() - sys.pumpStartTime > MAX_PUMP_RUNTIME)
  {
    systemError.pumpTimeout = true;

    Serial.println(
      F("[SAFETY] Pump runtime exceeded limit.")
    );

    emergencyStop();
    return;
  }
}
}

// ==========================================
// UI Navigation & Actions (State Machine)
// ==========================================
void runCurrentScreen()
{
  switch (ui.currentScreen)
  {
    case Screen::DASHBOARD:      dashboardTask();    break;
    case Screen::MANUAL_MENU:    manualMenuTask();   break;
    case Screen::MAIN_MENU:      mainMenuTask();     break;
    case Screen::SETTINGS_MENU:  settingsMenuTask(); break;
    case Screen::EDIT_START:     editStartTask();    break;
    case Screen::EDIT_STOP:      editStopTask();     break;
    case Screen::EDIT_DRY:       editDryTask();      break;
    case Screen::EDIT_WET:       editWetTask();      break;
  }
}

void dashboardTask()
{
  if (btnEnter.isPressed())
  {
    startBeep();
    enterScreen(Screen::MAIN_MENU);
  }
}

void mainMenuTask()
{
  if (btnUp.isPressed())
  {
    startBeep();
    ui.menuIndex--;
    if (ui.menuIndex < 0)
      ui.menuIndex = MAIN_MENU_ITEMS - 1;
    ui.lcdNeedsRefresh = true;
  }
  if (btnDown.isPressed())
  {
    startBeep();
    ui.menuIndex++;
    if (ui.menuIndex >= MAIN_MENU_ITEMS)
      ui.menuIndex = 0;
    ui.lcdNeedsRefresh = true;
  }
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::DASHBOARD);
  }
  if (btnEnter.isPressed())
  {
    startBeep();
    switch (ui.menuIndex)
    {
      case 0:
  executeCommand(CommandType::MODE_AUTO);
  break;

case 1:
  if (
    executeCommand(CommandType::MODE_MANUAL)
    == CommandResult::SUCCESS
  )
  {
    enterScreen(Screen::MANUAL_MENU);
  }
  break;
      case 2:
        enterScreen(Screen::SETTINGS_MENU);
        break;
    }
  }
}

void manualMenuTask()
{
  // ==========================================
  // UP = PUMP ON
  // ==========================================
  if (btnUp.isPressed())
  {
    startBeep();

    CommandResult result =
      executeCommand(CommandType::PUMP_ON);

    if (result == CommandResult::SUCCESS)
    {
      Serial.println(F("[UI] Manual PUMP ON."));
    }
    else if (result == CommandResult::SAFETY_LOCKED)
    {
      Serial.println(
        F("[UI] PUMP ON blocked by safety fault.")
      );

      startBeep(150);
    }

    ui.lcdNeedsRefresh = true;
  }


  // ==========================================
  // DOWN = PUMP OFF
  // ==========================================
  if (btnDown.isPressed())
  {
    startBeep();

    CommandResult result =
      executeCommand(CommandType::PUMP_OFF);

    if (result == CommandResult::SUCCESS)
    {
      Serial.println(F("[UI] Manual PUMP OFF."));
    }

    ui.lcdNeedsRefresh = true;
  }


  // ==========================================
  // BACK = EXIT ONLY
  // ==========================================
  if (btnBack.isPressed())
  {
    startBeep();

    enterScreen(Screen::MAIN_MENU);
  }
}

void settingsMenuTask()
{
  if (btnUp.isPressed())
  {
    startBeep();
    ui.settingsIndex--;
    if (ui.settingsIndex < 0)
      ui.settingsIndex = SETTINGS_MENU_ITEMS - 1;
    ui.lcdNeedsRefresh = true;
  }
  if (btnDown.isPressed())
  {
    startBeep();
    ui.settingsIndex++;
    if (ui.settingsIndex >= SETTINGS_MENU_ITEMS)
      ui.settingsIndex = 0;
    ui.lcdNeedsRefresh = true;
  }
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::MAIN_MENU);
  }
  if (btnEnter.isPressed())
  {
    startBeep();
    switch (ui.settingsIndex)
    {
      case 0:
        ui.editValue = settings.startMoisture;
        enterScreen(Screen::EDIT_START);
        break;
      case 1:
        ui.editValue = settings.stopMoisture;
        enterScreen(Screen::EDIT_STOP);
        break;
      case 2:
        enterScreen(Screen::EDIT_DRY);
        break;
      case 3:
        enterScreen(Screen::EDIT_WET);
        break;
    }
  }
}

void editStartTask()
{
  if (btnUp.isPressed())
  {
    startBeep();
    if (ui.editValue < 100)
      ui.editValue++;
    ui.lcdNeedsRefresh = true;
  }
  if (btnDown.isPressed())
  {
    startBeep();
    if (ui.editValue > 0)
      ui.editValue--;
    ui.lcdNeedsRefresh = true;
  }
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::SETTINGS_MENU);
  }
  if (btnEnter.isPressed())
  {
    startBeep(100);
    if (ui.editValue < settings.stopMoisture)
    {
      settings.startMoisture = ui.editValue;
      saveSettings();
      enterScreen(Screen::SETTINGS_MENU);
    }
  }
}

void editStopTask()
{
  if (btnUp.isPressed())
  {
    startBeep();
    if (ui.editValue < 100)
      ui.editValue++;
    ui.lcdNeedsRefresh = true;
  }
  if (btnDown.isPressed())
  {
    startBeep();
    if (ui.editValue > 0)
      ui.editValue--;
    ui.lcdNeedsRefresh = true;
  }
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::SETTINGS_MENU);
  }
  if (btnEnter.isPressed())
  {
    startBeep(100);
    if (ui.editValue > settings.startMoisture)
    {
      settings.stopMoisture = ui.editValue;
      saveSettings();
      enterScreen(Screen::SETTINGS_MENU);
    }
  }
}


void editDryTask()
{
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::SETTINGS_MENU);
    return;
  }

  if (btnEnter.isPressed())
  {
    startBeep(100);

    uint16_t candidate =
      static_cast<uint16_t>(sys.filteredADC);

    if (isCalibrationValid(
          candidate,
          settings.wetValue))
    {
      settings.dryValue = candidate;
      systemError.calibrationError = false;

      saveSettings();

      Serial.print(F("[CAL] Dry value saved: "));
      Serial.println(candidate);

      enterScreen(Screen::SETTINGS_MENU);
    }
    else
    {
      systemError.calibrationError = true;

      Serial.print(F("[CAL] Invalid dry value: "));
      Serial.println(candidate);

      // Stay on the calibration screen.
      ui.lcdNeedsRefresh = true;
    }
  }
}


void editWetTask()
{
  if (btnBack.isPressed())
  {
    startBeep();
    enterScreen(Screen::SETTINGS_MENU);
    return;
  }

  if (btnEnter.isPressed())
  {
    startBeep(100);

    uint16_t candidate =
      static_cast<uint16_t>(sys.filteredADC);

    if (isCalibrationValid(
          settings.dryValue,
          candidate))
    {
      settings.wetValue = candidate;
      systemError.calibrationError = false;

      saveSettings();

      Serial.print(F("[CAL] Wet value saved: "));
      Serial.println(candidate);

      enterScreen(Screen::SETTINGS_MENU);
    }
    else
    {
      systemError.calibrationError = true;

      Serial.print(F("[CAL] Invalid wet value: "));
      Serial.println(candidate);

      ui.lcdNeedsRefresh = true;
    }
  }
}

void updateDisplay()
{

  if (ui.currentScreen == Screen::DASHBOARD)
{
    ui.lcdNeedsRefresh = true;
}
  static unsigned long lastRefresh = 0;

  if (millis() - lastRefresh < LCD_REFRESH_TIME)  
    return;  

  lastRefresh = millis();  

  if (ui.currentScreen != ui.previousScreen)  
  {  
    lcd.clear();  
    ui.previousScreen = ui.currentScreen;  
    ui.lcdNeedsRefresh = true;  
  }  

  if (!ui.lcdNeedsRefresh)  
    return;  

  ui.lcdNeedsRefresh = false;  

  switch (ui.currentScreen)  
  {  
    case Screen::DASHBOARD:  
      lcd.setCursor(0,0);  
      lcd.print("Moist:");  
      lcd.print(getMoisture());  
      lcd.print("%   ");  

      lcd.setCursor(0,1);  
      lcd.print(settings.mode == Mode::AUTO ? "AUTO " : "MAN ");  
      lcd.print(sys.pumpState ? "ON " : "OFF");  
      break;  

    case Screen::MAIN_MENU:  
      lcd.setCursor(0,0);  
      lcd.print("Main Menu");  

      lcd.setCursor(0,1);  
      if (ui.menuIndex == 0)  
        lcd.print(">Auto       ");  
      else if (ui.menuIndex == 1)  
        lcd.print(">Manual     ");  
      else  
        lcd.print(">Settings   ");  
      break;  

    case Screen::MANUAL_MENU:

  lcd.setCursor(0, 0);
  lcd.print("Manual Pump     ");

  lcd.setCursor(0, 1);

  lcd.print("Pump: ");

  if (sys.pumpState)
    lcd.print("ON ");
  else
    lcd.print("OFF");

  lcd.print("       ");

  break;


    case Screen::SETTINGS_MENU:  
      lcd.setCursor(0,0);  
      lcd.print("Settings      ");  

      lcd.setCursor(0,1);  
      switch (ui.settingsIndex)  
      {  
        case 0: lcd.print(">Start       "); break;  
        case 1: lcd.print(">Stop        "); break;  
        case 2: lcd.print(">Dry         "); break;  
        case 3: lcd.print(">Wet         "); break;  
      }  
      break;  

    case Screen::EDIT_START:  
      lcd.setCursor(0,0);  
      lcd.print("Start Moist       ");  

      lcd.setCursor(0,1);  
      lcd.print(ui.editValue);  
      lcd.print("%");  
      break;  

    case Screen::EDIT_STOP:  
      lcd.setCursor(0,0);  
      lcd.print("Stop Moist       ");  

      lcd.setCursor(0,1);  
      lcd.print(ui.editValue);  
      lcd.print("%");  
      break;  

  case Screen::EDIT_DRY:

  lcd.setCursor(0, 0);
  lcd.print("Dry Cal         ");

  lcd.setCursor(0, 1);

  if (systemError.calibrationError)
    lcd.print("INVALID CAL     ");
  else
    lcd.print("Press Enter     ");

  break;


case Screen::EDIT_WET:

  lcd.setCursor(0, 0);
  lcd.print("Wet Cal         ");

  lcd.setCursor(0, 1);

  if (systemError.calibrationError)
    lcd.print("INVALID CAL     ");
  else
    lcd.print("Press Enter     ");

  break;
  }
}

void printSerial()
{
  static unsigned long lastPrint = 0;

  if (millis() - lastPrint < SERIAL_REFRESH_TIME)  
    return;  

  lastPrint = millis();  

  Serial.print(F("ADC="));  
  Serial.print((int)sys.filteredADC);  

  Serial.print(F(" Moist="));  
  Serial.print(getMoisture());  

  Serial.print(F("% Pump="));  
  Serial.print(sys.pumpState ? "ON" : "OFF");  

  Serial.print(F(" Mode="));  
  Serial.print(settings.mode == Mode::AUTO ? "AUTO" : "MANUAL");

  if(dhtData.valid)
  {
    Serial.print(F(" Temp="));
    Serial.print(dhtData.temperature);

    Serial.print(F("C Hum="));
    Serial.print(dhtData.humidity);
    Serial.print(F("%"));
  }
  else
  {
    Serial.print(F(" DHT Error"));
  }

  if (wifi.connected)
  {
    Serial.print(F(" IP="));
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(F(" WiFi Offline"));
  }
}

// ==========================================
// Setup & Loop
// ==========================================
void setup()
{
  // Startup Relay Safety
  pinMode(RELAY_PIN, OUTPUT);  
  digitalWrite(RELAY_PIN, RELAY_OFF);  

  Serial.begin(115200);

esp_task_wdt_config_t wdtConfig =
{
  .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
  .idle_core_mask = 0,
  .trigger_panic = true
};

esp_task_wdt_init(&wdtConfig);
esp_task_wdt_add(NULL);


  initializeWiFi(); 

  sim800.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX_PIN, SIM800_TX_PIN);
  initializeGSM();

  EEPROM.begin(EEPROM_SIZE);  
  dht.begin(); 
  lcd.init();  
  lcd.backlight();  

  pinMode(BUZZER_PIN, OUTPUT);  

  btnUp.init(UP_BTN);
  btnDown.init(DOWN_BTN);
  btnEnter.init(ENTER_BTN);
  btnBack.init(BACK_BTN);

  loadSettings();  
  sys.filteredADC = analogRead(SOIL_PIN);  
  emergencyStop();  

  lcd.clear();  
  ui.lcdNeedsRefresh = true;  

  Serial.println();  
  Serial.println(F("========== SYSTEM STARTED =========="));


}

void loop()
{
  esp_task_wdt_reset();

  readButtons();
  runCurrentScreen();

  readSensors();
  readDHT();

  processGSM();
  updateGSM();

  updateWiFi();
  server.handleClient();

  controlPump();

  processPumpEvents();
  processGSMAlerts();
  processAlertQueue();

  updateDisplay();
  handleEEPROM();
  updateBuzzer();
  printSerial();
}