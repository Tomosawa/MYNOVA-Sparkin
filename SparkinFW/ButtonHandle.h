/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

enum ButtonNotifyType {
    BUTTON_NOTIFY_3S = 1,
    BUTTON_NOTIFY_10S = 2,
    BUTTON_RELEASE_3S = 4,
    BUTTON_RELEASE_10S = 8
};

class ButtonHandler {
public:
    ButtonHandler(uint8_t pin);
    void begin();
    void end();
private:
    static void buttonTask(void* pvParameters);
    uint8_t _pin;
    TaskHandle_t _taskHandle;
};

#endif