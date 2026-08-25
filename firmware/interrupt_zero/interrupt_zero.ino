/* =====================================================================
   MYOSA 6.0 — Edge-Fused In-Cabin Telematics Hub
   Team: Interrupt Zero

   BLE:
   - Custom BLE GATT removed completely.
   - Uses ONLY MYOSA official BLE.
   - Data transmission is done through:
       myosa.sendBleData();

   HMI:
   - Screen 0: Telemetry
   - Screen 1: Artificial Horizon
   - LEFT / RIGHT: switch screens
   - UP / DOWN twice: acknowledge crash

   Crash:
   - High-G detection
   - Pressure dP/dt detection
   - Coincidence-based crash detection
   - 5-second pre-impact circular buffer
   - CSV dump to Serial when crash is detected

   Sensors:
   - MPU6050: Accelerometer + Gyroscope
   - BMP180: Pressure + Temperature
   - APDS9960: RGB + Gesture
   ===================================================================== */

#include <Wire.h>
#include <myosa.h>

MYOSA myosa;


// =====================================================================
// DEBUG / CALIBRATION
// =====================================================================

#define CALIBRATION_LOGGING 1

#define BUZZER_PIN 12

static const uint32_t BUZZER_BEEP_MS = 200;

static const float G_TO_CM_S2      = 980.665f;
static const float G_THRESHOLD     = 4.5f;
static const float DP_DT_THRESHOLD = 150.0f;   // Pa/s

static const float ALPHA = 0.98f;

static const TickType_t FUSION_PERIOD_TICKS =
    pdMS_TO_TICKS(10);

static const uint32_t PRESSURE_SAMPLE_PERIOD_MS = 40;

static const uint32_t HIGH_G_HOLD_MS = 150;

static const uint8_t REQUIRED_COINCIDENCES = 2;


// =====================================================================
// CRASH ACK
// =====================================================================

static const uint32_t ACK_CONFIRM_WINDOW_MS = 1500;


// =====================================================================
// HMI TIMING
// =====================================================================

static const uint32_t HMI_LOOP_PERIOD_MS = 15;

static const uint32_t RENDER_PERIOD_MS = 150;


// =====================================================================
// PRE-IMPACT CIRCULAR BUFFER
// =====================================================================

#define PREBUFFER_LEN 500

typedef struct {

  uint32_t tMs;

  float accelMagG;

  int32_t pressurePa;

  float dPdt_Pa_s;

} PreSample_t;


PreSample_t g_preBuffer[PREBUFFER_LEN];

volatile uint16_t g_preBufIdx = 0;

volatile bool g_preBufFull = false;

SemaphoreHandle_t g_preBufMutex;


// =====================================================================
// FUSION STATE
// =====================================================================

typedef struct {

  float accelMagG;

  float pitchDeg;

  float gyroXDps;

  float tempC;

  float dPdt_Pa_s;

  int32_t pressurePa;

  bool crashDetected;

  float peakAccelG;

  int32_t peakPressurePa;

} FusionState_t;


FusionState_t g_state = {0};


// =====================================================================
// MUTEXES
// =====================================================================

SemaphoreHandle_t g_stateMutex;

SemaphoreHandle_t g_i2cMutex;


// =====================================================================
// TASK HANDLES
// =====================================================================

TaskHandle_t fusionTaskHandle = nullptr;

TaskHandle_t hmiTaskHandle = nullptr;


// =====================================================================
// HMI STATE
// =====================================================================

volatile uint8_t g_currentScreen = 0;

// 0 = Telemetry
// 1 = Artificial Horizon


// =====================================================================
// CRASH ACK STATE
// =====================================================================

static char s_lastAckGesture[8] = {0};

static uint32_t s_lastAckTimeMs = 0;


// =====================================================================
// PRE-IMPACT BUFFER
// =====================================================================

void pushPreSample(
    uint32_t tMs,
    float accelMagG,
    int32_t pressurePa,
    float dPdt
) {

  if (xSemaphoreTake(
        g_preBufMutex,
        pdMS_TO_TICKS(2)
      ) == pdTRUE) {

    g_preBuffer[g_preBufIdx].tMs =
        tMs;

    g_preBuffer[g_preBufIdx].accelMagG =
        accelMagG;

    g_preBuffer[g_preBufIdx].pressurePa =
        pressurePa;

    g_preBuffer[g_preBufIdx].dPdt_Pa_s =
        dPdt;

    g_preBufIdx =
        (g_preBufIdx + 1) % PREBUFFER_LEN;

    if (g_preBufIdx == 0) {

      g_preBufFull = true;
    }

    xSemaphoreGive(g_preBufMutex);
  }
}


// =====================================================================
// DUMP PRE-IMPACT BUFFER TO SERIAL
// =====================================================================

void dumpPreBufferCSV() {

  if (xSemaphoreTake(
        g_preBufMutex,
        pdMS_TO_TICKS(20)
      ) == pdTRUE) {

    Serial.println(
        "t_ms,accelMagG,pressurePa,dPdt_Pa_s"
    );

    uint16_t count =
        g_preBufFull
        ? PREBUFFER_LEN
        : g_preBufIdx;

    uint16_t startIdx =
        g_preBufFull
        ? g_preBufIdx
        : 0;

    for (uint16_t i = 0; i < count; i++) {

      uint16_t idx =
          (startIdx + i) % PREBUFFER_LEN;

      Serial.print(
          g_preBuffer[idx].tMs
      );

      Serial.print(",");

      Serial.print(
          g_preBuffer[idx].accelMagG,
          3
      );

      Serial.print(",");

      Serial.print(
          g_preBuffer[idx].pressurePa
      );

      Serial.print(",");

      Serial.println(
          g_preBuffer[idx].dPdt_Pa_s,
          1
      );
    }

    xSemaphoreGive(g_preBufMutex);
  }
}


// =====================================================================
// RENDER TELEMETRY
// =====================================================================

void renderLiveTelemetry(
    const FusionState_t &s,
    uint16_t r,
    uint16_t g,
    uint16_t b
) {

  myosa.display.clearDisplay();

  myosa.display.setTextSize(1);


  myosa.display.setCursor(0, 0);

  myosa.display.print("TELEMETRY");


  myosa.display.setCursor(0, 14);

  myosa.display.print("G:");

  myosa.display.print(
      s.accelMagG,
      2
  );

  myosa.display.print("g  T:");

  myosa.display.print(
      s.tempC,
      1
  );

  myosa.display.print("C");


  myosa.display.setCursor(0, 26);

  myosa.display.print("P:");

  myosa.display.print(
      s.pressurePa
  );

  myosa.display.print("Pa");


  myosa.display.setCursor(0, 38);

  myosa.display.print("Gyro-X:");

  myosa.display.print(
      s.gyroXDps,
      1
  );

  myosa.display.print("dps");


  myosa.display.setCursor(0, 50);

  myosa.display.print("RGB:");

  myosa.display.print(r);

  myosa.display.print(",");

  myosa.display.print(g);

  myosa.display.print(",");

  myosa.display.print(b);


  myosa.display.display();
}


// =====================================================================
// ARTIFICIAL HORIZON
// =====================================================================

void renderArtificialHorizon(
    const FusionState_t &s
) {

  myosa.display.clearDisplay();

  myosa.display.setTextSize(1);


  myosa.display.setCursor(0, 0);

  myosa.display.print("HORIZON");


  const int16_t cx = 64;

  const int16_t cy = 36;

  const int16_t halfLen = 50;


  float pitchClamped =
      s.pitchDeg;


  if (pitchClamped > 45.0f) {

    pitchClamped = 45.0f;
  }


  if (pitchClamped < -45.0f) {

    pitchClamped = -45.0f;
  }


  int16_t vOffset =
      (int16_t)(
          pitchClamped * 0.6f
      );


  int16_t y =
      cy + vOffset;


  myosa.display.drawLine(
      cx - halfLen,
      y,
      cx + halfLen,
      y,
      SSD1306_WHITE
  );


  myosa.display.drawLine(
      cx,
      y - 6,
      cx,
      y + 6,
      SSD1306_WHITE
  );


  myosa.display.setCursor(0, 56);

  myosa.display.print("Pitch:");

  myosa.display.print(
      s.pitchDeg,
      1
  );


  myosa.display.display();
}


// =====================================================================
// EMERGENCY SCREEN
// =====================================================================

void renderEmergencyScreen(
    const FusionState_t &s
) {

  myosa.display.clearDisplay();

  myosa.display.setTextSize(2);

  myosa.display.setCursor(0, 0);

  myosa.display.print("CRASH");


  myosa.display.setTextSize(1);


  myosa.display.setCursor(0, 20);

  myosa.display.print("Peak G: ");

  myosa.display.print(
      s.peakAccelG,
      2
  );


  myosa.display.setCursor(0, 32);

  myosa.display.print("Peak P: ");

  myosa.display.print(
      s.peakPressurePa
  );

  myosa.display.print(" Pa");


  myosa.display.setCursor(0, 44);

  myosa.display.print("dP/dt : ");

  myosa.display.print(
      s.dPdt_Pa_s,
      1
  );


  myosa.display.setCursor(0, 56);

  myosa.display.print(
      "Hold UP/DOWN x2=ack"
  );


  myosa.display.display();
}


// =====================================================================
// BUZZER
// =====================================================================

void updateBuzzer(
    bool crashActive
) {

  static bool buzzerOn = false;

  static uint32_t lastToggle = 0;


  if (!crashActive) {

    if (buzzerOn) {

      buzzerOn = false;

      digitalWrite(
          BUZZER_PIN,
          LOW
      );
    }

    return;
  }


  uint32_t now =
      millis();


  if (
      now - lastToggle >=
      BUZZER_BEEP_MS
  ) {

    lastToggle = now;

    buzzerOn = !buzzerOn;

    digitalWrite(
        BUZZER_PIN,
        buzzerOn
        ? HIGH
        : LOW
    );
  }
}


// =====================================================================
// CORE 0 — SENSOR FUSION
// =====================================================================

void fusionTask(
    void *param
) {

  float pitchEst = 0.0f;

  float rollEst = 0.0f;


  static float axG = 0;

  static float ayG = 0;

  static float azG = 0;

  static float gxDps = 0;

  static float gyDps = 0;

  static float tempC = 0;


  int32_t lastPressurePa = 0;

  bool havePrevPressure = false;


  uint32_t lastPressureSampleTime =
      millis();


  uint32_t highGUntilMs = 0;


  uint8_t coincidenceCount = 0;


  TickType_t lastWake =
      xTaskGetTickCount();


  bool crashJustLatched = false;


  for (;;) {


    // ---------------------------------------------------------------
    // MPU6050
    // ---------------------------------------------------------------

    if (
        xSemaphoreTake(
            g_i2cMutex,
            pdMS_TO_TICKS(8)
        ) == pdTRUE
    ) {

      axG =
          myosa.Ag.getAccelX(false)
          / G_TO_CM_S2;

      ayG =
          myosa.Ag.getAccelY(false)
          / G_TO_CM_S2;

      azG =
          myosa.Ag.getAccelZ(false)
          / G_TO_CM_S2;


      gxDps =
          myosa.Ag.getGyroX(false);

      gyDps =
          myosa.Ag.getGyroY(false);


      xSemaphoreGive(
          g_i2cMutex
      );
    }


    // ---------------------------------------------------------------
    // ACCELERATION MAGNITUDE
    // ---------------------------------------------------------------

    float accelMagG =
        sqrtf(
            axG * axG +
            ayG * ayG +
            azG * azG
        );


    // ---------------------------------------------------------------
    // ACCELEROMETER ANGLES
    // ---------------------------------------------------------------

    float accPitchDeg =
        atan2f(
            ayG,
            azG
        ) *
        180.0f /
        PI;


    float accRollDeg =
        atan2f(
            -axG,
            azG
        ) *
        180.0f /
        PI;


    // ---------------------------------------------------------------
    // COMPLEMENTARY FILTER
    // ---------------------------------------------------------------

    static uint32_t lastFilterMs =
        millis();


    uint32_t nowMs =
        millis();


    float realDt =
        (nowMs - lastFilterMs)
        / 1000.0f;


    if (
        realDt <= 0.0f ||
        realDt > 0.5f
    ) {

      realDt = 0.01f;
    }


    lastFilterMs =
        nowMs;


    pitchEst =
        ALPHA *
        (
          pitchEst +
          gxDps * realDt
        )
        +
        (1.0f - ALPHA) *
        accPitchDeg;


    rollEst =
        ALPHA *
        (
          rollEst +
          gyDps * realDt
        )
        +
        (1.0f - ALPHA) *
        accRollDeg;


    // ---------------------------------------------------------------
    // HIGH-G DETECTION
    // ---------------------------------------------------------------

    if (
        accelMagG >
        G_THRESHOLD
    ) {

      highGUntilMs =
          nowMs +
          HIGH_G_HOLD_MS;
    }


    bool highGActive =
        nowMs <
        highGUntilMs;


    // ---------------------------------------------------------------
    // PRESSURE
    // ---------------------------------------------------------------

    int32_t pressurePa =
        lastPressurePa;


    float dPdt = 0.0f;


    bool pressureSampledThisTick =
        false;


    if (
        nowMs -
        lastPressureSampleTime >=
        PRESSURE_SAMPLE_PERIOD_MS
    ) {

      if (
          xSemaphoreTake(
              g_i2cMutex,
              pdMS_TO_TICKS(50)
          ) == pdTRUE
      ) {

        pressurePa =
            myosa.Pr.getPressure();

        tempC =
            myosa.Pr.getTempC(false);


        xSemaphoreGive(
            g_i2cMutex
        );


        pressureSampledThisTick =
            true;
      }


      uint32_t realIntervalMs =
          nowMs -
          lastPressureSampleTime;


      lastPressureSampleTime =
          nowMs;


      if (
          pressureSampledThisTick
      ) {

        if (
            havePrevPressure &&
            realIntervalMs > 0
        ) {

          dPdt =
              fabsf(
                  (float)(
                      pressurePa -
                      lastPressurePa
                  )
              )
              /
              (
                realIntervalMs /
                1000.0f
              );

        } else {

          havePrevPressure =
              true;
        }


        lastPressurePa =
            pressurePa;


#if CALIBRATION_LOGGING

        Serial.print("CAL,G:");

        Serial.print(
            accelMagG,
            2
        );

        Serial.print(",dPdt:");

        Serial.println(
            dPdt,
            1
        );

#endif


        // -----------------------------------------------------------
        // CRASH DECISION
        // -----------------------------------------------------------

        bool pressureCandidate =
            dPdt >
            DP_DT_THRESHOLD;


        if (
            highGActive &&
            pressureCandidate
        ) {

          coincidenceCount++;

        } else {

          coincidenceCount = 0;
        }


        if (
            coincidenceCount >=
            REQUIRED_COINCIDENCES
        ) {

          if (
              xSemaphoreTake(
                  g_stateMutex,
                  pdMS_TO_TICKS(5)
              ) == pdTRUE
          ) {

            if (
                !g_state.crashDetected
            ) {

              g_state.peakAccelG =
                  accelMagG;

              g_state.peakPressurePa =
                  pressurePa;

              g_state.crashDetected =
                  true;

              crashJustLatched =
                  true;
            }


            xSemaphoreGive(
                g_stateMutex
            );
          }
        }
      }
    }


    // ---------------------------------------------------------------
    // PRE-IMPACT BUFFER
    // ---------------------------------------------------------------

    pushPreSample(
        nowMs,
        accelMagG,
        pressurePa,
        dPdt
    );


    // ---------------------------------------------------------------
    // CRASH EVENT
    // ---------------------------------------------------------------

    if (
        crashJustLatched
    ) {

      Serial.println(
          "===== CRASH DETECTED ====="
      );


      dumpPreBufferCSV();


      Serial.println(
          "===== CRASH DATA END ====="
      );


      crashJustLatched =
          false;
    }


    // ---------------------------------------------------------------
    // UPDATE SHARED STATE
    // ---------------------------------------------------------------

    if (
        xSemaphoreTake(
            g_stateMutex,
            pdMS_TO_TICKS(5)
        ) == pdTRUE
    ) {

      g_state.accelMagG =
          accelMagG;

      g_state.pitchDeg =
          pitchEst;

      g_state.gyroXDps =
          gxDps;

      g_state.tempC =
          tempC;

      g_state.dPdt_Pa_s =
          dPdt;

      g_state.pressurePa =
          pressurePa;


      xSemaphoreGive(
          g_stateMutex
      );
    }


    vTaskDelayUntil(
        &lastWake,
        FUSION_PERIOD_TICKS
    );
  }
}


// =====================================================================
// CORE 1 — HMI + OFFICIAL MYOSA BLE
// =====================================================================

void hmiTask(
    void *param
) {

  uint32_t lastBleSend = 0;


  // Official MYOSA BLE transmission period

  const uint32_t BLE_SEND_PERIOD_MS =
      300;


  uint32_t lastRenderMs = 0;


  for (;;) {


    // ---------------------------------------------------------------
    // GET CURRENT FUSION STATE
    // ---------------------------------------------------------------

    FusionState_t localState;


    if (
        xSemaphoreTake(
            g_stateMutex,
            pdMS_TO_TICKS(5)
        ) == pdTRUE
    ) {

      localState =
          g_state;

      xSemaphoreGive(
          g_stateMutex
      );
    }


    // ---------------------------------------------------------------
    // RGB
    // ---------------------------------------------------------------

    static uint16_t redVal = 0;

    static uint16_t greenVal = 0;

    static uint16_t blueVal = 0;


    uint32_t nowMs =
        millis();


    bool doRender =
        (
          nowMs -
          lastRenderMs
        )
        >=
        RENDER_PERIOD_MS;


    // ---------------------------------------------------------------
    // FAST SECTION
    // Gesture + RGB
    // ---------------------------------------------------------------

    if (
        xSemaphoreTake(
            g_i2cMutex,
            pdMS_TO_TICKS(15)
        ) == pdTRUE
    ) {


      // -------------------------------------------------------------
      // GESTURE
      // -------------------------------------------------------------

      if (
          myosa.Lpg.ping()
      ) {

        char *gst =
            myosa.Lpg.getGesture(false);


        if (
            strcmp(
                gst,
                "LEFT"
            ) == 0
        ) {

          g_currentScreen = 0;

        }

        else if (
            strcmp(
                gst,
                "RIGHT"
            ) == 0
        ) {

          g_currentScreen = 1;

        }

        else if (
            strcmp(gst, "UP") == 0 ||
            strcmp(gst, "DOWN") == 0
        ) {

          bool sameAsLast =
              (
                strcmp(
                    gst,
                    s_lastAckGesture
                ) == 0
              );


          bool withinWindow =
              (
                nowMs -
                s_lastAckTimeMs
              )
              <=
              ACK_CONFIRM_WINDOW_MS;


          if (
              sameAsLast &&
              withinWindow
          ) {

            if (
                xSemaphoreTake(
                    g_stateMutex,
                    pdMS_TO_TICKS(5)
                ) == pdTRUE
            ) {

              g_state.crashDetected =
                  false;

              xSemaphoreGive(
                  g_stateMutex
              );
            }


            localState.crashDetected =
                false;


            s_lastAckGesture[0] =
                '\0';

            s_lastAckTimeMs =
                0;

          }

          else {

            strncpy(
                s_lastAckGesture,
                gst,
                sizeof(
                    s_lastAckGesture
                ) - 1
            );


            s_lastAckGesture[
                sizeof(
                    s_lastAckGesture
                ) - 1
            ] = '\0';


            s_lastAckTimeMs =
                nowMs;
          }
        }
      }


      // -------------------------------------------------------------
      // RGB
      // -------------------------------------------------------------

      redVal =
          myosa.Lpg.getRedProportion();

      greenVal =
          myosa.Lpg.getGreenProportion();

      blueVal =
          myosa.Lpg.getBlueProportion();


      xSemaphoreGive(
          g_i2cMutex
      );
    }


    // ---------------------------------------------------------------
    // DISPLAY RENDER
    // ---------------------------------------------------------------

    if (doRender) {

      lastRenderMs =
          nowMs;


      if (
          xSemaphoreTake(
              g_i2cMutex,
              pdMS_TO_TICKS(30)
          ) == pdTRUE
      ) {


        if (
            localState.crashDetected
        ) {

          renderEmergencyScreen(
              localState
          );

        }

        else if (
            g_currentScreen == 1
        ) {

          renderArtificialHorizon(
              localState
          );

        }

        else {

          renderLiveTelemetry(
              localState,
              redVal,
              greenVal,
              blueVal
          );
        }


        // -----------------------------------------------------------
        // OFFICIAL MYOSA BLE
        // -----------------------------------------------------------

        if (
            nowMs -
            lastBleSend >=
            BLE_SEND_PERIOD_MS
        ) {

          lastBleSend =
              nowMs;


          myosa.sendBleData();
        }


        xSemaphoreGive(
            g_i2cMutex
        );
      }
    }


    // ---------------------------------------------------------------
    // BUZZER
    // ---------------------------------------------------------------

    updateBuzzer(
        localState.crashDetected
    );


    // ---------------------------------------------------------------
    // LOOP
    // ---------------------------------------------------------------

    vTaskDelay(
        pdMS_TO_TICKS(
            HMI_LOOP_PERIOD_MS
        )
    );
  }
}


// =====================================================================
// SETUP
// =====================================================================

void setup() {

  Serial.begin(115200);


  Wire.begin();

  Wire.setClock(400000);


  // ---------------------------------------------------------------
  // MYOSA INITIALIZATION
  // ---------------------------------------------------------------

  Serial.println(
      myosa.begin()
  );


  // ---------------------------------------------------------------
  // MPU6050 CONFIGURATION
  // ---------------------------------------------------------------

  myosa.Ag.setFullScaleAccelRange(
      MPU_ACCEL_CONFIG_FS_SEL_8g
  );


  myosa.Ag.setFullScaleGyroRange(
      MPU_GYRO_CONFIG_FS_SEL_2000
  );


  // ---------------------------------------------------------------
  // APDS9960 GESTURE FIX
  // ---------------------------------------------------------------

  // First ping absorbs the library's internal reset.

  myosa.Lpg.ping();


  // Enable gesture mode AFTER the first ping.

  myosa.Lpg.enableGestureSensor(
      ENABLE
  );


  // ---------------------------------------------------------------
  // BUZZER
  // ---------------------------------------------------------------

  pinMode(
      BUZZER_PIN,
      OUTPUT
  );


  digitalWrite(
      BUZZER_PIN,
      LOW
  );


  // ---------------------------------------------------------------
  // MUTEXES
  // ---------------------------------------------------------------

  g_stateMutex =
      xSemaphoreCreateMutex();


  g_i2cMutex =
      xSemaphoreCreateMutex();


  g_preBufMutex =
      xSemaphoreCreateMutex();


  // ---------------------------------------------------------------
  // TASKS
  // ---------------------------------------------------------------

  xTaskCreatePinnedToCore(
      fusionTask,
      "FusionTask",
      4096,
      nullptr,
      3,
      &fusionTaskHandle,
      0
  );


  xTaskCreatePinnedToCore(
      hmiTask,
      "HmiTask",
      8192,
      nullptr,
      2,
      &hmiTaskHandle,
      1
  );


  Serial.println(
      "MYOSA SYSTEM READY"
  );

  Serial.println(
      "Official MYOSA BLE ENABLED"
  );

  Serial.println(
      "Custom BLE DISABLED"
  );
}


// =====================================================================
// LOOP
// =====================================================================

void loop() {

  vTaskDelay(
      pdMS_TO_TICKS(1000)
  );
}
