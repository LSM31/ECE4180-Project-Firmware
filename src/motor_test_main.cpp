#include <Arduino.h>

#define MOTOR_SIG 5

constexpr uint32_t PWM_FREQ_HZ  = 50;                       // 20 ms servo period
constexpr uint8_t  PWM_RES_BITS = 14;
constexpr uint32_t PERIOD_US    = 1000000 / PWM_FREQ_HZ;
constexpr uint32_t MIN_SAFE_US  = 0;                      // reject anything outside this window
constexpr uint32_t MAX_SAFE_US  = 10000;

static String inputLine;

static void writePulseUs(uint32_t us)
{
    uint32_t duty = us * ((1u << PWM_RES_BITS) - 1) / PERIOD_US;
    ledcWrite(MOTOR_SIG, duty);
}

static void prompt()
{
    Serial.print("Enter a pulse width: ");
}

static bool isNumber(const String &s)
{
    if (s.length() == 0) return false;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (!isDigit(s[i])) return false;
    }
    return true;
}

static void handleLine(String line)
{
    line.trim();
    if (!isNumber(line))
    {
        Serial.printf("'%s' is not a number.\n", line.c_str());
        return;
    }

    uint32_t us = line.toInt();
    if (us < MIN_SAFE_US || us > MAX_SAFE_US)
    {
        Serial.printf("%lu us is outside %lu-%lu us, ignoring.\n", us, MIN_SAFE_US, MAX_SAFE_US);
        return;
    }

    writePulseUs(us);
    Serial.printf("Driving servo at %lu us.\n", us);
}

void setup()
{
    Serial.begin(115200);
    ledcAttach(MOTOR_SIG, PWM_FREQ_HZ, PWM_RES_BITS);
    writePulseUs(1500);  // start centered

    // Native USB: wait until the serial monitor connects so the prompt isn't lost
    while (!Serial) delay(10);
    prompt();
}

void loop()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\r' || c == '\n')
        {
            if (inputLine.length() == 0) continue;  // skip the \n of a \r\n pair
            Serial.println();
            handleLine(inputLine);
            inputLine = "";
            prompt();
        }
        else
        {
            Serial.print(c);  // echo, since the monitor doesn't by default
            inputLine += c;
        }
    }
}
