#include "linear.h"

// Linear

Linear::Linear(float k, float c, String config_path)
    : NumericTransform(config_path), k{k}, c{c} {
  className = "Linear";
  load_configuration();
}

void Linear::set_input(float input, uint8_t inputChannel) {
  output = k * input + c;
  notify();
}

void Linear::get_configuration(JsonObject& root) {
  root["k"] = k;
  root["c"] = c;
  root["value"] = output;
}

static const char SCHEMA[] PROGMEM = R"({
    "type": "object",
    "properties": {
        "k": { "title": "Multiplier", "type": "number" },
        "c": { "title": "Constant offset", "type": "number" },
        "value": { "title": "Last value", "type" : "number", "readOnly": true }
    }
  })";

String Linear::get_config_schema() { return FPSTR(SCHEMA); }

bool Linear::set_configuration(const JsonObject& config) {
  String expected[] = {"k", "c"};
  for (auto str : expected) {
    if (!config.containsKey(str)) {
      return false;
    }
  }
  k = config["k"];
  c = config["c"];
  return true;
}
