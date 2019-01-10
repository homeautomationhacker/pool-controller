
/*
PoolDevice.h - Library for PoolDevice
Copyright (C) 2018 Frank Clemnents

This program is free software: you can redistribute it and/or modify
it under the terms of the version 3 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "PoolDevice.h"

PoolDevice::PoolDevice(int panelPin, String deviceId) {
  this->_panelPin = panelPin;
  this->_deviceId = deviceId;
  this->_currentStatus = PoolDeviceStatus::OFF;

  pinMode(this->_panelPin, OUTPUT);
}

// Flip the status of the _currentStatus
PoolDeviceStatus PoolDevice::_changeStatus() {
  if(this->_currentStatus == OFF) {
    this->_currentStatus = ON;
  }
  else {
    this->_currentStatus = OFF;
  }
  
  return this->_currentStatus;
}

// Simulate a button press for 0.5 seconds and then set the current stat opposite to current state.
PoolDeviceStatus PoolDevice::_firePin() {
  digitalWrite(this->_panelPin, HIGH);
  delay(500);
  digitalWrite(this->_panelPin, LOW);

  this->_changeStatus();
  return this->_currentStatus;
}

// Return the object's device ID
String PoolDevice::getDeviceId() {
  return this->_deviceId;
}

// Turn on the device
void PoolDevice::turnOn() {
  if(this->_currentStatus == OFF) {
    this->_firePin();
    this->_changeStatus();
  }

  return;
}

// Turn off the device
void PoolDevice::turnOff() {
  if(this->_currentStatus == ON) {
    this->_firePin();
    this->_changeStatus();
  }

  return;
}
