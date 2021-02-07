#include "ina219.h"

#include "sensesp.h"
//#include "i2c_tools.h"
#include <RemoteDebug.h>

// INA219 represents an ADAfruit (or compatible) INA219 High Side DC Current
// Sensor.
INA219::INA219(uint8_t addr, INA219_BusVoltage range, INA219_ShuntGain gain,
    uint max_current_ma, uint shunt_uohms, String config_path)
    : Sensor(config_path),
    range_{range},
    gain_{gain},
    max_current_ma_{max_current_ma},
    shunt_uohms_{shunt_uohms} {
  load_configuration();
  ada_ina219_ = new Adafruit_INA219(addr);
  ada_ina219_->begin();
  // Default calibration in the Adafruit_INA219 constructor is 32V and 2A, so
  // that's what it will be unless it's set to something different in the call
  // to this constructor:
  ada_ina219_->setCalibration(range_, gain_, max_current_ma_ / 1000.0, shunt_uohms_ / 1000000.0);
}

void INA219::get_configuration(JsonObject& root) {
  switch (range_) {
    case INA219_CONFIG_BVOLTAGERANGE_16V:
      root["range"] = 16;
      break;
    
    case INA219_CONFIG_BVOLTAGERANGE_32V:
      root["range"] = 32;
      break;

    default:
      root["range"] = 16;
  }
  
  switch (gain_) {
    case INA219_CONFIG_GAIN_1_40MV:
      root["gain"] = 1;
      break;

    case INA219_CONFIG_GAIN_2_80MV:
      root["gain"] = 2;
      break;

    case INA219_CONFIG_GAIN_4_160MV:
      root["gain"] = 4;
      break;

    case INA219_CONFIG_GAIN_8_320MV:
      root["gain"] = 8;
      break;

    default:
      root["gain"] = 1;
  }

  root["max_current_ma"] = max_current_ma_;
  root["shunt_uohms"] = shunt_uohms_;
}

static const char SCHEMA_SENSOR[] PROGMEM = R"###({
    "type": "object",
    "properties": {
        "range": { "title": "Bus Voltage Range", "type": "number", "description": "Must be 16 or 32, the max voltage expected on the positive side of the shunt" },
        "gain": { "title": "Shunt Gain", "type": "number", "description": "Must be 1 (40mv), 2 (80mv), 4 (160mv) or 8 (320mv), the gain that matches the max voltage expected across the shunt" },
        "max_current_ma": { "title": "Shunt Max Current in mA", "type": "number", "description": "Max current the shunt can handle, in milliAmps" },
        "shunt_uohms": { "title": "Resistance of the shunt in uOhms", "type": "number", "description": "Resistance of the shunt, in microOhms" }
    }
  })###";

String INA219::get_config_schema() { return FPSTR(SCHEMA_SENSOR); }

bool INA219::set_configuration(const JsonObject& config) {
  String expected[] = {"range", "gain", "max_current_ma", "shunt_uohms"};
  for (auto str : expected) {
    if (!config.containsKey(str)) {
      return false;
    }
  }
  int rangeInt = config["range"];
  switch (rangeInt) {
    case 16:
      range_ = INA219_CONFIG_BVOLTAGERANGE_16V;
      break;
    
    case 32:
      range_ = INA219_CONFIG_BVOLTAGERANGE_32V;
      break;

    default:
      range_ = INA219_CONFIG_BVOLTAGERANGE_16V;
  }

  int gainInt = config["gain"];
  switch (gainInt) {
    case 1:
      gain_ = INA219_CONFIG_GAIN_1_40MV;
      break;

    case 2:
      gain_ = INA219_CONFIG_GAIN_2_80MV;
      break;

    case 4:
      gain_ = INA219_CONFIG_GAIN_4_160MV;
      break;

    case 8:
      gain_ = INA219_CONFIG_GAIN_8_320MV;
      break;

    default:
      gain_ = INA219_CONFIG_GAIN_1_40MV;
  }
  max_current_ma_ = config["max_current_ma"];
  shunt_uohms_ = config["shunt_uohms"];
  return true;
}

// INA219Value reads and outputs the specified type of value of a INA219 sensor
INA219Value::INA219Value(INA219* ina219, INA219ValType val_type,
                         uint read_delay, String config_path)
    : NumericSensor(config_path),
      ina219_{ina219},
      val_type_{val_type},
      read_delay_{read_delay} {
  load_configuration();
}

void INA219Value::enable() {
  app.onRepeat(read_delay_, [this]() {
    switch (val_type_) {
      case bus_voltage:
        output = ina219_->ada_ina219_->getBusVoltage_V();
        break;
      case shunt_voltage:
        output = (ina219_->ada_ina219_->getShuntVoltage_mV() /
                  1000);  // SK wants volts, not mV
        break;
      case current:
        output =
            (ina219_->ada_ina219_->getCurrent_mA() / 1000);  // SK wants amps, not mA
        break;
      case power:
        output =
            (ina219_->ada_ina219_->getPower_mW() / 1000);  // SK want watts, not mW
        break;
      case load_voltage:
        output = (ina219_->ada_ina219_->getBusVoltage_V() +
                  (ina219_->ada_ina219_->getShuntVoltage_mV() / 1000));
        break;
    }
    notify();
  });
}

void INA219Value::get_configuration(JsonObject& root) {
  root["read_delay"] = read_delay_;
}

static const char SCHEMA[] PROGMEM = R"###({
    "type": "object",
    "properties": {
        "read_delay": { "title": "Read delay", "type": "number", "description": "The time, in milliseconds, between each read of the input" }
    }
  })###";

String INA219Value::get_config_schema() { return FPSTR(SCHEMA); }

bool INA219Value::set_configuration(const JsonObject& config) {
  String expected[] = {"read_delay"};
  for (auto str : expected) {
    if (!config.containsKey(str)) {
      return false;
    }
  }
  read_delay_ = config["read_delay"];
  return true;
}
