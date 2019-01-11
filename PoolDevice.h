/**
 * PoolDevice.h - Library for controlling pool andn spa functions.
 *
 * Created by Frank W Clements, January 3, 2019.
 * Released into the public domain.
 *
 */
#ifndef PoolDevice_h
#define PoolDevice_h

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include "pins_arduino.h"
  #include "WConstants.h"
#endif

enum PoolDeviceMode
{
  UNKNOWN,
  ON,
  OFF
};

class PoolMode
{
  public:
    PoolMode(int panelStatusPin);
    PoolDeviceMode getCurrentMode();
  private:
    PoolDeviceMode _currentMode;
};

class PoolDevice
{
  public:
    PoolDevice(int panelPin, String deviceId);
    void turnOn();
    void turnOff();
    String getDeviceId();

  private:
    int _panelPin;
    String _deviceId;
    PoolDeviceMode _currentStatus;
    PoolDeviceMode _firePin();
    PoolDeviceMode _changeStatus();
};

#endif
