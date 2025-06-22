#include <Arduino.h>
#include "ConnectionManager.h"

static ConnectionManager s_network;

void setup()
{
    Serial.begin(115200);

    s_network.begin();
    s_network.scan();

    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(900);
}