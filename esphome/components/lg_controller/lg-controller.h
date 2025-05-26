#pragma once

#include "esphome.h"
#include "esphome/components/uart/uart.h"

static const char* const TAG = "lg-controller";

namespace esphome::lg_controller {

static constexpr size_t MIN_TEMP_SETPOINT = 16;
static constexpr size_t MAX_TEMP_SETPOINT = 30;

class LgSwitch final : public switch_::Switch {
    void write_state(bool value) override {
        publish_state(value);
    }

public:
    void restore_and_set_mode(switch_::SwitchRestoreMode mode) {
        set_restore_mode(mode);
        if (auto state = get_initial_state_with_restore_mode()) {
            write_state(*state);
        }
    }
};

class LgSelect final : public select::Select {
    void control(const std::string& value) override {
        if (this->state != value) {
            this->publish_state(value); 
        }
    }
};

class LgNumber final : public number::Number {
    void control(float value) override {
        if (this->state != value) {
            this->publish_state(value); 
        }
    }
};

// The LG protocol always uses Celsius. The HA/ESPHome climate component internally
// converts between Fahrenheit and Celsius. Values from the Home Assistant room temperature sensor
// are not converted automatically so can be Celsius or Fahrenheit.
//
// Unfortunately LG uses their own Fahrenheit/Celsius mapping that's different from what you'd
// expect. For example, 78F is ~25.5C, but LG controllers will send 26C for 78F. A value of 25.5C
// would be interpreted by the AC as 77F.
//
// This class has some functions to convert between Fahrenheit, Celsius and "LG-Celsius" (values
// we send to or receive from the unit). This ensures Home Assistant and the LG unit always agree
// on the setpoint in Fahrenheit.
//
// These conversions are only used in Fahrenheit mode.
class TempConversion {
private:
    static constexpr int8_t FahToLGCel[] = {
        0  /* 32  */, 1  /* 33  */, 2  /* 34  */, 3  /* 35  */, 4  /* 36  */,
        5  /* 37  */, 6  /* 38  */, 7  /* 39  */, 8  /* 40  */, 10 /* 41  */,
        12 /* 42  */, 13 /* 43  */, 14 /* 44  */, 15 /* 45  */, 16 /* 46  */,
        17 /* 47  */, 18 /* 48  */, 19 /* 49  */, 20 /* 50  */, 21 /* 51  */,
        22 /* 52  */, 23 /* 53  */, 24 /* 54  */, 25 /* 55  */, 26 /* 56  */,
        27 /* 57  */, 28 /* 58  */, 30 /* 59  */, 32 /* 60  */, 33 /* 61  */,
        34 /* 62  */, 35 /* 63  */, 36 /* 64  */, 37 /* 65  */, 38 /* 66  */,
        39 /* 67  */, 40 /* 68  */, 41 /* 69  */, 42 /* 70  */, 43 /* 71  */,
        44 /* 72  */, 45 /* 73  */, 46 /* 74  */, 47 /* 75  */, 48 /* 76  */,
        50 /* 77  */, 52 /* 78  */, 53 /* 79  */, 54 /* 80  */, 55 /* 81  */,
        56 /* 82  */, 57 /* 83  */, 58 /* 84  */, 59 /* 85  */, 60 /* 86  */,
        61 /* 87  */, 62 /* 88  */, 63 /* 89  */, 64 /* 90  */, 65 /* 91  */,
        66 /* 92  */, 67 /* 93  */, 68 /* 94  */, 70 /* 95  */, 72 /* 96  */,
        73 /* 97  */, 74 /* 98  */, 75 /* 99  */, 76 /* 100 */, 77 /* 101 */,
        78 /* 102 */, 79 /* 103 */, 80 /* 104 */
    };
    static constexpr int8_t LGCelToCelAdjustment[] = {
         0 /* 0    */,  0 /* 0.5  */,  0 /* 1.0  */,  0 /* 1.5  */,  0 /* 2.0  */,
         1 /* 2.5  */,  1 /* 3.0  */,  1 /* 3.5  */,  1 /* 4.0  */,  0 /* 4.5  */,
         0 /* 5.0  */, -1 /* 5.5  */, -1 /* 6.0  */, -1 /* 6.5  */, -1 /* 7.0  */,
        -1 /* 7.5  */,  0 /* 8.0  */,  0 /* 8.5  */,  0 /* 9.0  */,  0 /* 9.5  */,
         0 /* 10.0 */,  0 /* 10.5 */,  0 /* 11.0 */,  0 /* 11.5 */,  0 /* 12.0 */,
         1 /* 12.5 */,  1 /* 13.0 */,  1 /* 13.5 */,  1 /* 14.0 */,  0 /* 14.5 */,
         0 /* 15.0 */, -1 /* 15.5 */, -1 /* 16.0 */, -1 /* 16.5 */, -1 /* 17.0 */,
        -1 /* 17.5 */,  0 /* 18.0 */,  0 /* 18.5 */,  0 /* 19.0 */,  0 /* 19.5 */,
         0 /* 20.0 */,  0 /* 20.5 */,  0 /* 21.0 */,  0 /* 21.5 */,  0 /* 22.0 */,
         1 /* 22.5 */,  1 /* 23.0 */,  1 /* 23.5 */,  1 /* 24.0 */,  0 /* 24.5 */,
         0 /* 25.0 */, -1 /* 25.5 */, -1 /* 26.0 */, -1 /* 26.5 */, -1 /* 27.0 */,
        -1 /* 27.5 */,  0 /* 28.0 */,  0 /* 28.5 */,  0 /* 29.0 */,  0 /* 29.5 */,
         0 /* 30.0 */,  0 /* 30.5 */,  0 /* 31.0 */,  0 /* 31.5 */,  0 /* 32.0 */,
         1 /* 32.5 */,  1 /* 33.0 */,  1 /* 33.5 */,  1 /* 34.0 */,  0 /* 34.5 */,
         0 /* 35.0 */, -1 /* 35.5 */, -1 /* 36.0 */, -1 /* 36.5 */, -1 /* 37.0 */,
        -1 /* 37.5 */,  0 /* 38.0 */,  0 /* 38.5 */,  0 /* 39.0 */,  0 /* 39.5 */,
         0 /* 40.0 */
    };

public:
    // Convert from Fahrenheit to LG-Celsius (using the LG-compatible conversion).
    static float fahrenheit_to_lgcelsius(float temp) {
        int temp_int = int(round(temp));
        if (temp_int < 32 || temp_int > 104) {
            return esphome::fahrenheit_to_celsius(temp);
        }
        int8_t val = FahToLGCel[temp_int - 32];
        return float(val / 2) + ((val & 1) ? 0.5f : 0.0f);
    }
    // Convert an LG-Celsius value to Celsius. This is done to ensure the LG unit and HA agree
    // on the value in Fahrenheit. For example, the unit sends 78F as 26C (LG Celsius), but HA
    // would convert this to 78.8F => 79F. To work around this, we adjust 26C to 25.5C because
    // this maps to 78F in HA.
    static float lgcelsius_to_celsius(float temp) {
        int index = int(temp * 2);
        if (index < 0 || index >= sizeof(LGCelToCelAdjustment)) {
            return temp;
        }
        int8_t adjustment = LGCelToCelAdjustment[index];
        if (adjustment == -1) {
            return temp - 0.5;
        }
        if (adjustment == 1) {
            return temp + 0.5;
        }
        return temp;
    }
    static float celsius_to_lgcelsius(float temp) {
        float fahrenheit = esphome::celsius_to_fahrenheit(temp);
        return fahrenheit_to_lgcelsius(fahrenheit);
    }
};
constexpr int8_t TempConversion::FahToLGCel[];
constexpr int8_t TempConversion::LGCelToCelAdjustment[];

class LgController final : public climate::Climate, public uart::UARTDevice, public Component {
    static constexpr size_t MsgLen = 13;

    climate::ClimateTraits supported_traits_{};

    InternalGPIOPin& rx_pin_;
    esphome::sensor::Sensor* temperature_sensor_;

    LgSelect& vane_select_1_;
    LgSelect& vane_select_2_;
    LgSelect& vane_select_3_;
    LgSelect& vane_select_4_;
    LgSelect& overheating_select_;

    LgNumber& fan_speed_slow_;
    LgNumber& fan_speed_low_;
    LgNumber& fan_speed_medium_;
    LgNumber& fan_speed_high_;

    LgNumber& sleep_timer_;

    esphome::sensor::Sensor& error_code_;
    esphome::sensor::Sensor& pipe_temp_in_;
    esphome::sensor::Sensor& pipe_temp_mid_;
    esphome::sensor::Sensor& pipe_temp_out_;
    esphome::binary_sensor::BinarySensor& defrost_;
    esphome::binary_sensor::BinarySensor& preheat_;
    esphome::binary_sensor::BinarySensor& outdoor_;
    esphome::binary_sensor::BinarySensor& auto_dry_active_;
    uint32_t last_outdoor_change_millis_ = 0;

    LgSwitch& purifier_;
    LgSwitch& internal_thermistor_;
    LgSwitch& auto_dry_;

    uint8_t recv_buf_[MsgLen] = {};
    uint32_t recv_buf_len_ = 0;
    uint32_t last_recv_millis_ = 0;

    // Last received 0xC8 message.
    uint8_t last_recv_status_[MsgLen] = {};

    // Last received 0xCA message.
    uint8_t last_recv_type_a_settings_[MsgLen] = {};

    // Last received 0xCB message.
    uint8_t last_recv_type_b_settings_[MsgLen] = {};

    uint8_t send_buf_[MsgLen] = {};
    uint32_t last_sent_status_millis_ = 0;
    uint32_t last_sent_recv_type_b_millis_ = 0;

    enum class PendingSendKind : uint8_t { None, Status, TypeA, TypeB };
    PendingSendKind pending_send_ = PendingSendKind::None;

    bool pending_status_change_ = false;
    bool pending_type_a_settings_change_ = false;
    bool pending_type_b_settings_change_ = false;

    bool is_initializing_ = true;

    uint8_t vane_position_[4] = {0,0,0,0};
    uint8_t fan_speed_[4] = {0,0,0,0};
    uint8_t overheating_ = 0;

    optional<uint32_t> sleep_timer_target_millis_{};
    bool active_reservation_ = false;
    bool ignore_sleep_timer_callback_ = false;

    uint32_t NVS_STORAGE_VERSION = 2843654U; // Change version if the NVSStorage struct changes
    struct NVSStorage {
        uint8_t capabilities_message[13] = {};
    };
    NVSStorage nvs_storage_;

    const bool fahrenheit_;

    // Whether a message came from the HVAC unit, a master controller, or a slave controller.
    enum class MessageSender : uint8_t { Unit, Master, Slave };
    // Set if this controller is configured as slave controller.
    const bool slave_;

    enum class LgCapability {
        PURIFIER,
        FAN_AUTO,
        FAN_SLOW,
        FAN_LOW,
        FAN_LOW_MEDIUM,
        FAN_MEDIUM,
        FAN_MEDIUM_HIGH,
        FAN_HIGH,
        MODE_HEATING,
        MODE_FAN,
        MODE_AUTO,
        MODE_DEHUMIDIFY,
        HAS_ONE_VANE,
        HAS_TWO_VANES,
        HAS_FOUR_VANES,
        VERTICAL_SWING,
        HORIZONTAL_SWING,
        HAS_ESP_VALUE_SETTING,
        OVERHEATING_SETTING,
        AUTO_DRY,
    };

    bool parse_capability(LgCapability capability) {
        switch (capability) {
            case LgCapability::PURIFIER:
                return (nvs_storage_.capabilities_message[2] & 0x02) != 0;
            case LgCapability::FAN_AUTO:
                return (nvs_storage_.capabilities_message[3] & 0x01) != 0;
            case LgCapability::FAN_SLOW:
                return (nvs_storage_.capabilities_message[3] & 0x20) != 0;
            case LgCapability::FAN_LOW:
                return (nvs_storage_.capabilities_message[3] & 0x10) != 0;
            case LgCapability::FAN_LOW_MEDIUM:
                return (nvs_storage_.capabilities_message[6] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM:
                return (nvs_storage_.capabilities_message[3] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM_HIGH:
                return (nvs_storage_.capabilities_message[6] & 0x10) != 0;
            case LgCapability::FAN_HIGH:
                return true;
            case LgCapability::MODE_HEATING:
                return (nvs_storage_.capabilities_message[2] & 0x40) != 0;
            case LgCapability::MODE_FAN:
                return (nvs_storage_.capabilities_message[2] & 0x80) != 0;
            case LgCapability::MODE_AUTO:
                return (nvs_storage_.capabilities_message[2] & 0x08) != 0;
            case LgCapability::MODE_DEHUMIDIFY:
                return (nvs_storage_.capabilities_message[2] & 0x80) != 0;
            case LgCapability::HAS_ONE_VANE:
                return (nvs_storage_.capabilities_message[5] & 0x40) != 0;
            case LgCapability::HAS_TWO_VANES:
                return (nvs_storage_.capabilities_message[5] & 0x80) != 0;
            case LgCapability::HAS_FOUR_VANES:
                // Actual flag is unknown, assume 4 vanes if neither 1 nor 2 vanes are supported
                // and the vane control bit is set.
                return (nvs_storage_.capabilities_message[5] & 0x40) == 0 &&
                       (nvs_storage_.capabilities_message[5] & 0x80) == 0 &&
                       (nvs_storage_.capabilities_message[4] & 0x01) != 0;
            case LgCapability::VERTICAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x80) != 0;
            case LgCapability::HORIZONTAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x40) != 0;
            case LgCapability::HAS_ESP_VALUE_SETTING:
                return (nvs_storage_.capabilities_message[4] & 0x02) != 0;
            case LgCapability::OVERHEATING_SETTING:
                return (nvs_storage_.capabilities_message[7] & 0x80) != 0;
            case LgCapability::AUTO_DRY:
                return (nvs_storage_.capabilities_message[4] & 0x80) != 0;
        }
        return false;
    }

    void configure_capabilities() {
        // Default traits
        supported_traits_.set_supported_modes({
            climate::CLIMATE_MODE_OFF,
            climate::CLIMATE_MODE_COOL,
            climate::CLIMATE_MODE_HEAT,
            climate::CLIMATE_MODE_DRY,
            climate::CLIMATE_MODE_FAN_ONLY,
            climate::CLIMATE_MODE_HEAT_COOL,
        });
        supported_traits_.set_supported_fan_modes({
            climate::CLIMATE_FAN_LOW,
            climate::CLIMATE_FAN_MEDIUM,
            climate::CLIMATE_FAN_HIGH,
            climate::CLIMATE_FAN_AUTO,
        });
        supported_traits_.set_supported_swing_modes({
            climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_SWING_BOTH,
            climate::CLIMATE_SWING_VERTICAL,
            climate::CLIMATE_SWING_HORIZONTAL,
        });
        supported_traits_.set_supports_current_temperature(true);
        supported_traits_.set_supports_two_point_target_temperature(false);
        supported_traits_.set_supports_action(false);
        supported_traits_.set_visual_min_temperature(MIN_TEMP_SETPOINT);
        supported_traits_.set_visual_max_temperature(MAX_TEMP_SETPOINT);
        supported_traits_.set_visual_current_temperature_step(fahrenheit_ ? 1 : 0.5);
        supported_traits_.set_visual_target_temperature_step(fahrenheit_ ? 1 : 0.5);

        // Only override defaults if the capabilities are known
        if (nvs_storage_.capabilities_message[0] != 0) {
            // Configure the climate traits
            std::set<climate::ClimateMode> device_modes;
            device_modes.insert(climate::CLIMATE_MODE_OFF);
            device_modes.insert(climate::CLIMATE_MODE_COOL);
            if (parse_capability(LgCapability::MODE_HEATING))
                device_modes.insert(climate::CLIMATE_MODE_HEAT);
            if (parse_capability(LgCapability::MODE_FAN))
                device_modes.insert(climate::CLIMATE_MODE_FAN_ONLY);
            if (parse_capability(LgCapability::MODE_AUTO))
                device_modes.insert(climate::CLIMATE_MODE_HEAT_COOL);
            if (parse_capability(LgCapability::MODE_DEHUMIDIFY))
                device_modes.insert(climate::CLIMATE_MODE_DRY);
            supported_traits_.set_supported_modes(device_modes);

            std::set<climate::ClimateFanMode> fan_modes;
            if (parse_capability(LgCapability::FAN_AUTO))
                fan_modes.insert(climate::CLIMATE_FAN_AUTO);
            if (parse_capability(LgCapability::FAN_SLOW))
                fan_modes.insert(climate::CLIMATE_FAN_QUIET);
            if (parse_capability(LgCapability::FAN_LOW))
                fan_modes.insert(climate::CLIMATE_FAN_LOW);
            if (parse_capability(LgCapability::FAN_MEDIUM))
                fan_modes.insert(climate::CLIMATE_FAN_MEDIUM);
            if (parse_capability(LgCapability::FAN_HIGH))
                fan_modes.insert(climate::CLIMATE_FAN_HIGH);
            supported_traits_.set_supported_fan_modes(fan_modes);

            std::set<climate::ClimateSwingMode> swing_modes;
            swing_modes.insert(climate::CLIMATE_SWING_OFF);
            if (parse_capability(LgCapability::VERTICAL_SWING) && parse_capability(LgCapability::HORIZONTAL_SWING))
                swing_modes.insert(climate::CLIMATE_SWING_BOTH);
            if (parse_capability(LgCapability::VERTICAL_SWING))
                swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
            if (parse_capability(LgCapability::HORIZONTAL_SWING))
                swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
            supported_traits_.set_supported_swing_modes(swing_modes);

            // Disable unsupported entities
            vane_select_1_.set_internal(true);
            vane_select_2_.set_internal(true);
            vane_select_3_.set_internal(true);
            vane_select_4_.set_internal(true);

            if (parse_capability(LgCapability::HAS_ONE_VANE)) {
                vane_select_1_.set_internal(false);
            } else if (parse_capability(LgCapability::HAS_TWO_VANES)) {
                vane_select_1_.set_internal(false);
                vane_select_2_.set_internal(false);
            } else if (parse_capability(LgCapability::HAS_FOUR_VANES)) {
                vane_select_1_.set_internal(false);
                vane_select_2_.set_internal(false);
                vane_select_3_.set_internal(false);
                vane_select_4_.set_internal(false);
            }

            fan_speed_slow_.set_internal(true);
            fan_speed_low_.set_internal(true);
            fan_speed_medium_.set_internal(true);
            fan_speed_high_.set_internal(true);
            overheating_select_.set_internal(true);

            if (!slave_) {
                if (parse_capability(LgCapability::HAS_ESP_VALUE_SETTING)) {
                    if (parse_capability(LgCapability::FAN_SLOW)) {
                        fan_speed_slow_.set_internal(false);
                    }
                    if (parse_capability(LgCapability::FAN_LOW)) {
                        fan_speed_low_.set_internal(false);
                    }
                    if (parse_capability(LgCapability::FAN_MEDIUM)) {
                        fan_speed_medium_.set_internal(false);
                    }
                    if (parse_capability(LgCapability::FAN_HIGH)) {
                        fan_speed_high_.set_internal(false);
                    }
                }
                if (parse_capability(LgCapability::OVERHEATING_SETTING)) {
                    overheating_select_.set_internal(false);
                }
            }
            purifier_.set_internal(!parse_capability(LgCapability::PURIFIER));
            auto_dry_.set_internal(!parse_capability(LgCapability::AUTO_DRY));
            auto_dry_active_.set_internal(!parse_capability(LgCapability::AUTO_DRY));
        }

        internal_thermistor_.set_internal(slave_);
        sleep_timer_.set_internal(slave_);
    }

public:
    LgController(InternalGPIOPin* rx_pin,
                 sensor::Sensor* temperature_sensor,
                 LgSelect* vane_select_1,
                 LgSelect* vane_select_2,
                 LgSelect* vane_select_3,
                 LgSelect* vane_select_4,
                 LgSelect* overheating_select,
                 LgNumber* fan_speed_slow,
                 LgNumber* fan_speed_low,
                 LgNumber* fan_speed_medium,
                 LgNumber* fan_speed_high,
                 LgNumber* sleep_timer,
                 sensor::Sensor* error_code,
                 sensor::Sensor* pipe_temp_in,
                 sensor::Sensor* pipe_temp_mid,
                 sensor::Sensor* pipe_temp_out,
                 binary_sensor::BinarySensor* defrost,
                 binary_sensor::BinarySensor* preheat,
                 binary_sensor::BinarySensor* outdoor,
                 binary_sensor::BinarySensor* auto_dry_active,
                 LgSwitch* purifier,
                 LgSwitch* internal_thermistor,
                 LgSwitch* auto_dry,
                 bool fahrenheit, bool is_slave_controller)
      : rx_pin_(*rx_pin),
        temperature_sensor_(temperature_sensor),
        vane_select_1_(*vane_select_1),
        vane_select_2_(*vane_select_2),
        vane_select_3_(*vane_select_3),
        vane_select_4_(*vane_select_4),
        overheating_select_(*overheating_select),
        fan_speed_slow_(*fan_speed_slow),
        fan_speed_low_(*fan_speed_low),
        fan_speed_medium_(*fan_speed_medium),
        fan_speed_high_(*fan_speed_high),
        sleep_timer_(*sleep_timer),
        error_code_(*error_code),
        pipe_temp_in_(*pipe_temp_in),
        pipe_temp_mid_(*pipe_temp_mid),
        pipe_temp_out_(*pipe_temp_out),
        defrost_(*defrost),
        preheat_(*preheat),
        outdoor_(*outdoor),
        auto_dry_active_(*auto_dry_active),
        purifier_(*purifier),
        internal_thermistor_(*internal_thermistor),
        auto_dry_(*auto_dry),
        fahrenheit_(fahrenheit),
        slave_(is_slave_controller)
    {
        vane_select_1_.add_on_state_callback([this](std::string v, size_t index) {
            set_vane_position(1, index);
        });
        vane_select_2_.add_on_state_callback([this](std::string v, size_t index) {
            set_vane_position(2, index);
        });
        vane_select_3_.add_on_state_callback([this](std::string v, size_t index) {
            set_vane_position(3, index);
        });
        vane_select_4_.add_on_state_callback([this](std::string v, size_t index) {
            set_vane_position(4, index);
        });
        overheating_select_.add_on_state_callback([this](std::string v, size_t index) {
            set_overheating(index);
        });

        fan_speed_slow_.add_on_state_callback([this](float v) {
            set_fan_speed(0, v);
        });
        fan_speed_low_.add_on_state_callback([this](float v) {
            set_fan_speed(1, v);
        });
        fan_speed_medium_.add_on_state_callback([this](float v) {
            set_fan_speed(2, v);
        });
        fan_speed_high_.add_on_state_callback([this](float v) {
            set_fan_speed(3, v);
        });
        sleep_timer_.add_on_state_callback([this](float v) {
            set_sleep_timer(v);
        });

        purifier_.add_on_state_callback([this](bool) {
            pending_status_change_ = true;
        });
        internal_thermistor_.add_on_state_callback([this](bool) {
            pending_status_change_ = true;
        });
        auto_dry_.add_on_state_callback([this](bool) {
            pending_type_a_settings_change_ = true;
        });
    }

    float get_setup_priority() const override {
        return esphome::setup_priority::BUS;
    }

    void setup() override {
        // Load our custom NVS storage to get the capabilities message
        ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
        pref.load(&nvs_storage_);

        auto restore = this->restore_state_();
        if (restore.has_value()) {
            restore->apply(this);
        } else {
            this->mode = climate::CLIMATE_MODE_OFF;
            this->target_temperature = 20;
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
            this->swing_mode = climate::CLIMATE_SWING_OFF;
            this->publish_state();
        }

        internal_thermistor_.restore_and_set_mode(esphome::switch_::SWITCH_RESTORE_DEFAULT_OFF);

        sleep_timer_.publish_state(0);

        // Configure climate traits and entities based on the capabilities message (if available)
        configure_capabilities();

        while (UARTDevice::available() > 0) {
            uint8_t b;
            UARTDevice::read_byte(&b);
        }
        pending_status_change_ = true;

        // Call `update` every 6 seconds, but first wait 10 seconds.
        set_timeout("initial_send", 10000, [this]() {
            set_interval("update", 6000, [this]() { update(); });
        });
    }

    // Process changes from HA.
    void control(const climate::ClimateCall &call) override {
        if (call.get_mode().has_value()) {
            this->mode = *call.get_mode();
        }
        if (call.get_target_temperature().has_value()) {
            this->target_temperature = *call.get_target_temperature();
        }
        if (call.get_fan_mode().has_value()) {
            this->fan_mode = *call.get_fan_mode();
        }
        if (call.get_swing_mode().has_value()) {
            set_swing_mode(*call.get_swing_mode());
        }
        this->pending_status_change_ = true;
        this->publish_state();
    }

    climate::ClimateTraits traits() override {
        return supported_traits_;
    }

private:
    // Sets position of vane index (1-4) to position (0-6).
    void set_vane_position(int index, int position) {
        if (index < 1 || index > 4) {
            ESP_LOGE(TAG, "Unexpected vane index: %d", index);
            return;
        }
        if (position < 0 || position > 6) {
            ESP_LOGE(TAG, "Unexpected vane position: %d", position);
            return;
        }
        if (vane_position_[index-1] == position) {
            return;
        }
        ESP_LOGD(TAG, "Setting vane %d position: %d", index, position);
        vane_position_[index-1] = position;
        if (!is_initializing_) {
            pending_type_a_settings_change_ = true;
        }
    }

    // Sets installer setting fan speed index (0-3 for slow-high) to value (0-255), with "0" being the factory default
    void set_fan_speed(int index, int value) {
        if (index < 0 || index > 3) {
            ESP_LOGE(TAG, "Unexpected fan speed index: %d", index);
            return;
        }
        if (value<0 || value>255) {
            ESP_LOGE(TAG, "Unexpected fan speed: %d", value);
            return;
        }

        if (fan_speed_[index] == value) {
            return;
        }

        fan_speed_[index] = value;
        if (!is_initializing_) {
            pending_type_a_settings_change_ = true;
        }
    }

    // Set overheating installer setting 15 to 0-4.
    void set_overheating(int value) {
        if (value < 0 || value > 4) {
            ESP_LOGE(TAG, "Unexpected overheating value: %d", value);
            return;
        }

        if (overheating_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting overheating installer setting: %d", value);

        overheating_ = value;
        if (!is_initializing_) {
            pending_type_b_settings_change_ = true;
        }
    }

    void set_sleep_timer(int minutes) {
        if (ignore_sleep_timer_callback_) {
            return;
        }
        // 0 clears the timer. Accept max 7 hours.
        if (minutes < 0 || minutes > 7 * 60) {
            ESP_LOGE(TAG, "Ignoring invalid sleep timer value: %d minutes", minutes);
            return;
        }
        if (slave_) {
            ESP_LOGE(TAG, "Ignoring sleep timer for slave controller");
            return;
        }
        ESP_LOGD(TAG, "Setting sleep timer: %d minutes", minutes);
        if (minutes > 0) {
            sleep_timer_target_millis_ = millis() + unsigned(minutes) * 60 * 1000;
            active_reservation_ = true;
        } else {
            sleep_timer_target_millis_.reset();
            active_reservation_ = false;
        }
        pending_status_change_ = true;
    }

    optional<float> get_room_temp() const {
        if (temperature_sensor_ == nullptr) {
            return {};
        }

        float temp = temperature_sensor_->get_state();
        if (std::isnan(temp) || temp == 0) {
            return {};
        }
        if (fahrenheit_) {
            temp = TempConversion::fahrenheit_to_lgcelsius(temp);
        }
        if (temp < 11) {
            return 11;
        }
        if (temp > 35) {
            return 35;
        }
        // Round to nearest 0.5 degrees.
        return round(temp * 2) / 2;
    }

    optional<uint32_t> get_sleep_timer_minutes() const {
        if (!sleep_timer_target_millis_.has_value()) {
            return {};
        }
        int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis());
        if (diff <= 0) {
            return {};
        }
        uint32_t minutes = uint32_t(diff) / 1000 / 60 + 1;
        return minutes;
    }

    static uint8_t calc_checksum(const uint8_t* buffer) {
        size_t result = 0;
        for (size_t i = 0; i < 12; i++) {
            result += buffer[i];
        }
        return (result & 0xff) ^ 0x55;
    }

    void set_swing_mode(climate::ClimateSwingMode mode) {
        if (this->swing_mode != mode) {
            // If vertical swing is off, send a 0xAA message to restore the vane position.
            if (mode == climate::CLIMATE_SWING_OFF || mode == climate::CLIMATE_SWING_HORIZONTAL) {
                pending_type_a_settings_change_ = true;
            }
        }
        this->swing_mode = mode;
    }

    void send_status_message() {
        // Byte 0: message type.
        send_buf_[0] = slave_ ? 0x28 : 0xA8;

        // Byte 1: changed flag (0x1), power on (0x2), mode (0x1C), fan speed (0x70).
        uint8_t b = 0;
        if (pending_status_change_) {
            b |= 0x1;
        }
        switch (this->mode) {
            case climate::CLIMATE_MODE_COOL:
                b |= (0 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_DRY:
                b |= (1 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_FAN_ONLY:
                b |= (2 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT_COOL:
                b |= (3 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT:
                b |= (4 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_OFF:
                // Don't set power-on flag, but preserve previous operation mode.
                b |= (last_recv_status_[1] & 0x1C);
                break;
            default:
                ESP_LOGE(TAG, "unknown operation mode, turning off");
                b |= (2 << 2);
                break;
        }
        switch (*this->fan_mode) {
            case climate::CLIMATE_FAN_LOW:
                b |= 0 << 5;
                break;
            case climate::CLIMATE_FAN_MEDIUM:
                b |= 1 << 5;
                break;
            case climate::CLIMATE_FAN_HIGH:
                b |= 2 << 5;
                break;
            case climate::CLIMATE_FAN_AUTO:
                b |= 3 << 5;
                break;
            case climate::CLIMATE_FAN_QUIET:
                b |= 4 << 5;
                break;
            default:
                ESP_LOGE(TAG, "unknown fan mode, using Medium");
                b |= 1 << 5;
                break;
        }
        send_buf_[1] = b;

        // Byte 2: swing mode and purifier/plasma setting. Preserve the other bits.
        b = last_recv_status_[2] & ~(0x4|0x40|0x80);
        if (purifier_.state) {
            b |= 0x4;
        }
        switch (this->swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                b |= 0x40;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                b |= 0x80;
                break;
            case climate::CLIMATE_SWING_BOTH:
                b |= 0x40 | 0x80;
                break;
            default:
                ESP_LOGE(TAG, "unknown swing mode");
                break;
        }
        send_buf_[2] = b;

        // Byte 3.
        send_buf_[3] = last_recv_status_[3];
        if (active_reservation_) {
            send_buf_[3] |= 0x10;
        } else {
            send_buf_[3] &= ~0x10;
        }

        // Byte 4.
        send_buf_[4] = last_recv_status_[4];

        float target = this->target_temperature;
        if (fahrenheit_) {
            target = TempConversion::celsius_to_lgcelsius(target);
        }
        if (target < MIN_TEMP_SETPOINT) {
            target = MIN_TEMP_SETPOINT;
        } else if (target > MAX_TEMP_SETPOINT) {
            target = MAX_TEMP_SETPOINT;
        }

        // Byte 5. Unchanged except for the low bit which indicates the target temperature has a
        // 0.5 fractional part.
        send_buf_[5] = last_recv_status_[5] & ~0x1;
        if (target - uint8_t(target) == 0.5) {
            send_buf_[5] |= 0x1;
        }

        // Byte 6: thermistor setting and target temperature (fractional part in byte 5).
        // Byte 7: room temperature. Preserve the (unknown) upper two bits.
        enum ThermistorSetting { Unit = 0, Controller = 1, TwoTH = 2 };
        ThermistorSetting thermistor =
            internal_thermistor_.state ? ThermistorSetting::Unit : ThermistorSetting::Controller;
        float temp;
        if (auto maybe_temp = get_room_temp()) {
            temp = *maybe_temp;
        } else {
            // Room temperature isn't available. Use the unit's thermistor and send something
            // reasonable.
            thermistor = ThermistorSetting::Unit;
            temp = 20;
        }
        send_buf_[6] = (thermistor << 4) | ((uint8_t(target) - 15) & 0xf);
        send_buf_[7] = (last_recv_status_[7] & 0xC0) | uint8_t((temp - 10) * 2);

        // Bytes 8-10. Initialize bytes 8-9 to 0 to not echo back timer settings set by the AC.
        send_buf_[8] = 0;
        send_buf_[9] = 0;
        send_buf_[10] = last_recv_status_[10];

        if (is_initializing_) {
            // Request settings when controller turns on.
            send_buf_[8] |= 0x40;
            // Set bit 0x80 of byte 10 to use byte 9 for the Fahrenheit setting flag (0x40).
            if (fahrenheit_) {
                send_buf_[9] |= 0x40;
            }
            send_buf_[10] = 0x80;
        } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
            // Set sleep timer.
            // Byte 8 stores the kind (0x38) and high bits of number of minutes (0x7).
            // Byte 9 stores the low bits of the number of minutes.
            constexpr uint8_t timer_kind_sleep = 3;
            send_buf_[8] = timer_kind_sleep << 3;
            send_buf_[8] |= (*minutes >> 8) & 0b111;
            send_buf_[9] = *minutes & 0xff;
        }

        // Byte 11.
        send_buf_[11] = last_recv_status_[11];

        // Byte 12.
        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        pending_status_change_ = false;
        pending_send_ = PendingSendKind::Status;
        last_sent_status_millis_ = millis();

        // If we sent an updated temperature to the AC, update temperature in HA too.
        // Slave controller temperature sensor is ignored.
        if (!slave_ && thermistor == ThermistorSetting::Controller) {
            float ha_temp = temp;
            if (fahrenheit_) {
                ha_temp = TempConversion::lgcelsius_to_celsius(ha_temp);
            }
            if (this->current_temperature != ha_temp) {
                this->current_temperature = ha_temp;
                publish_state();
            }
        }
    }

    void send_type_a_settings_message() {
        if (last_recv_type_a_settings_[0] != 0xCA && last_recv_type_a_settings_[0] != 0xAA) {
            ESP_LOGE(TAG, "Unexpected missing previous CA/AA message");
            pending_type_a_settings_change_ = false;
            return;
        }

        // Copy settings from the CA/AA message we received.
        memcpy(send_buf_, last_recv_type_a_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2A : 0xAA;

        // Bytes 2-6 store the installer fan speeds
        send_buf_[2] = fan_speed_[0];
        send_buf_[3] = fan_speed_[1];
        send_buf_[4] = fan_speed_[2];
        send_buf_[5] = fan_speed_[3];

        // Bytes 7-8 store vane positions.
        send_buf_[7] = (send_buf_[7] & 0xf0) | (vane_position_[0] & 0x0f); // Set vane 1
        send_buf_[7] = (send_buf_[7] & 0x0f) | ((vane_position_[1] & 0x0f) << 4); // Set vane 2
        send_buf_[8] = (send_buf_[8] & 0xf0) | (vane_position_[2] & 0x0f); // Set vane 3
        send_buf_[8] = (send_buf_[8] & 0x0f) | ((vane_position_[3] & 0x0f) << 4); // Set vane 4

        // Set auto dry setting.
        uint8_t b = send_buf_[11] & ~0x8;
        if (auto_dry_.state) {
            b |= 0x8;
        }
        send_buf_[11] = b;

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        pending_type_a_settings_change_ = false;
        pending_send_ = PendingSendKind::TypeA;
    }

    void send_type_b_settings_message(bool timed) {
        if (timed) {
            ESP_LOGD(TAG, "sending timed AB message");
        }
        if (last_recv_type_b_settings_[0] != 0xCB && last_recv_type_b_settings_[0] != 0xAB) {
            ESP_LOGE(TAG, "Unexpected missing previous CB/AB message");
            pending_type_b_settings_change_ = false;
            // Don't try to send another message immediately after.
            last_sent_recv_type_b_millis_ = millis();
            return;
        }

        // Copy settings from the CB/AB message we received.
        memcpy(send_buf_, last_recv_type_b_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2B : 0xAB;

        // Set the high bit of the second byte to request a CB message from the unit.
        if (timed) {
            send_buf_[1] |= 0x80;
        } else {
            send_buf_[1] &= ~0x80;
        }

        // Byte 2 stores installer setting 15.
        send_buf_[2] = (send_buf_[2] & 0xC7) | (overheating_ << 3);

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        pending_type_b_settings_change_ = false;
        pending_send_ = PendingSendKind::TypeB;
        last_sent_recv_type_b_millis_ = millis();
    }

    void process_message(const uint8_t* buffer, bool* had_error) {
        ESP_LOGD(TAG, "received %s", format_hex_pretty(buffer, MsgLen).c_str());

        if (calc_checksum(buffer) != buffer[12]) {
            // When initializing, the unit sends an all-zeroes message as padding between
            // messages. Ignore those false checksum failures.
            auto is_zero = [](uint8_t b) { return b == 0; };
            if (std::all_of(buffer, buffer + MsgLen, is_zero)) {
                ESP_LOGD(TAG, "Ignoring padding message sent by unit");
                return;
            }
            ESP_LOGE(TAG, "invalid checksum %s", format_hex_pretty(buffer, MsgLen).c_str());
            *had_error = true;
            return;
        }

        if (pending_send_ != PendingSendKind::None && memcmp(send_buf_, buffer, MsgLen) == 0) {
            ESP_LOGD(TAG, "verified send");
            pending_send_ = PendingSendKind::None;
            return;
        }

        // Determine message type.
        optional<MessageSender> sender;
        switch (buffer[0] & 0xf8) {
            case 0xC8:
                sender = MessageSender::Unit;
                break;
            case 0xA8:
                if (!slave_) {
                    // Ignore (our own?) master controller messages.
                    return;
                }
                sender = MessageSender::Master;
                break;
            case 0x28:
                if (slave_) {
                    // Ignore (our own?) slave controller messages.
                    return;
                }
                sender = MessageSender::Slave;
                break;
            default:
                return; // Unknown message sender. Ignore.
        }

        switch (buffer[0] & 0b111) {
            case 0: // 0xC8/A8/28
                process_status_message(*sender, buffer, had_error);
                break;
            case 1: // 0xC9
                process_capabilities_message(*sender, buffer);
                break;
            case 2: // 0xCA/AA/2A
                process_type_a_settings_message(*sender, buffer);
                break;
            case 3: // 0xCB/AB/2B
                process_type_b_settings_message(*sender, buffer);
                break;
            default:
                return;
        }
    }

    void process_status_message(MessageSender sender, const uint8_t* buffer, bool* had_error) {
        // If we just had a failure, ignore this messsage because it might be invalid too.
        if (*had_error) {
            ESP_LOGE(TAG, "ignoring due to previous error %s",
                     format_hex_pretty(buffer, MsgLen).c_str());
            return;
        }

        // Consider slave controller initialized if we received a status message from the other
        // controller or the unit.
        if (slave_) {
            is_initializing_ = false;
        }

        // Handle simple input sensors first. These are safe to update even if we have a pending
        // change.

        defrost_.publish_state(buffer[3] & 0x4);
        preheat_.publish_state(buffer[3] & 0x8);

        if (sender == MessageSender::Unit) {
            error_code_.publish_state(buffer[11]);
        }

        // When turning on the outdoor unit, the AC sometimes reports ON => OFF => ON within a
        // few seconds. No big deal but it causes noisy state changes in HA. Only report OFF if
        // the last state change was at least 8 seconds ago.
        bool outdoor_on = buffer[5] & 0x4;
        bool outdoor_changed = outdoor_.state != outdoor_on;
        if (outdoor_on) {
            outdoor_.publish_state(true);
        } else if (millis() - last_outdoor_change_millis_ > 8000) {
            outdoor_.publish_state(false);
        }
        if (outdoor_changed) {
            last_outdoor_change_millis_ = millis();
        }

        if (sender == MessageSender::Unit && !auto_dry_.is_internal()) {
            bool unit_off = (buffer[1] & 0x2) == 0;
            bool drying = (buffer[10] & 0x10) && unit_off;
            auto_dry_active_.publish_state(drying);
        }

        bool read_temp = false;
        if (slave_) {
            // Let the slave controller report the temperature from the master.
            read_temp = (sender == MessageSender::Master);
        } else {
            // Report the unit's room temperature only if we're using the internal thermistor.
            // With an external temperature sensor, some units report the temperature we sent and
            // others always send the internal temperature.
            read_temp = (sender == MessageSender::Unit && internal_thermistor_.state);
        }
        if (read_temp) {
            float room_temp = float(buffer[7] & 0x3F) / 2 + 10;
            if (fahrenheit_) {
                room_temp = TempConversion::lgcelsius_to_celsius(room_temp);
            }
            if (this->current_temperature != room_temp) {
                this->current_temperature = room_temp;
                publish_state();
            }
        }

        // Don't update our settings if we have a pending change/send, because else we overwrite
        // changes we still have to send (or are sending) to the AC.
        if (pending_status_change_) {
            ESP_LOGD(TAG, "ignoring because pending change");
            return;
        }
        if (pending_send_ == PendingSendKind::Status) {
            ESP_LOGD(TAG, "ignoring because pending send");
            return;
        }

        if (sender != MessageSender::Slave) {
            memcpy(last_recv_status_, buffer, MsgLen);
        }

        uint8_t b = buffer[1];
        if ((b & 0x2) == 0) {
            this->mode = climate::CLIMATE_MODE_OFF;
        } else {
            uint8_t mode_val = (b >> 2) & 0b111;
            switch (mode_val) {
                case 0:
                    this->mode = climate::CLIMATE_MODE_COOL;
                    break;
                case 1:
                    this->mode = climate::CLIMATE_MODE_DRY;
                    break;
                case 2:
                    this->mode = climate::CLIMATE_MODE_FAN_ONLY;
                    break;
                case 3:
                    this->mode = climate::CLIMATE_MODE_HEAT_COOL;
                    break;
                case 4:
                    this->mode = climate::CLIMATE_MODE_HEAT;
                    break;
                default:
                    ESP_LOGE(TAG, "received invalid operation mode from AC (%u)", mode_val);
                    *had_error = true;
                    return;
            }
        }

        uint8_t fan_val = b >> 5;
        switch (fan_val) {
            case 0:
                this->fan_mode = climate::CLIMATE_FAN_LOW;
                break;
            case 1:
                this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;
            case 2:
                this->fan_mode = climate::CLIMATE_FAN_HIGH;
                break;
            case 3:
                this->fan_mode = climate::CLIMATE_FAN_AUTO;
                break;
            case 4:
                this->fan_mode = climate::CLIMATE_FAN_QUIET;
                break;
            default:
                ESP_LOGE(TAG, "received unexpected fan mode from AC (%u)", fan_val);
                *had_error = true;
                return;
        }

        purifier_.publish_state(buffer[2] & 0x4);

        bool horiz_swing = buffer[2] & 0x40;
        bool vert_swing = buffer[2] & 0x80;
        if (horiz_swing && vert_swing) {
            set_swing_mode(climate::CLIMATE_SWING_BOTH);
        } else if (horiz_swing) {
            set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
        } else if (vert_swing) {
            set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
        } else {
            set_swing_mode(climate::CLIMATE_SWING_OFF);
        }

        float target = float((buffer[6] & 0xf) + 15);
        if (buffer[5] & 0x1) {
            target += 0.5;
        }
        if (fahrenheit_) {
            target = TempConversion::lgcelsius_to_celsius(target);
        }
        this->target_temperature = target;

        active_reservation_ = buffer[3] & 0x10;

        // Set or clear sleep timer.
        if (!slave_) {
            if (sleep_timer_target_millis_.has_value() && !active_reservation_) {
                sleep_timer_.publish_state(0);
            } else if (((buffer[8] >> 3) & 0x7) == 3) {
                uint32_t minutes = (uint32_t(buffer[8] & 0x7) << 8) | buffer[9];
                sleep_timer_.publish_state(minutes);
            }
        }

        publish_state();
    }

    void process_capabilities_message(MessageSender sender, const uint8_t* buffer) {
        // Capabilities message. The unit sends this with the other settings so we're now
        // initialized.

        if (sender != MessageSender::Unit) {
            ESP_LOGE(TAG, "ignoring capabilities message not from unit");
            return;
        }

        // Check if we need to update the capabilities message.
        if (nvs_storage_.capabilities_message[0] == 0 ||
            std::memcmp(nvs_storage_.capabilities_message, buffer, MsgLen - 1) != 0) {

            bool needsRestart = (nvs_storage_.capabilities_message[0] == 0);

            ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
            memcpy(nvs_storage_.capabilities_message, buffer, MsgLen);

            // If no capabilities were stored in NVS before, restart to make sure we get the correct traits for climate
            // No restart just on changes (which is unlikely anyway) to make sure we don't end up in a bootloop for faulty devices / communication
            if (needsRestart) {
                pref.save(&nvs_storage_);
                global_preferences->sync();
                ESP_LOGD(TAG, "restarting to apply initial capabilities");
                App.safe_reboot();
            }
            else {
                ESP_LOGD(TAG, "updated device capabilities, manual restart required to take effect");
            }
        }

        is_initializing_ = false;
    }

    void process_type_a_settings_message(MessageSender sender, const uint8_t* buffer) {
        // Send settings the first time we receive a 0xCA message.
        if (sender != MessageSender::Slave) {
            bool first_time = last_recv_type_a_settings_[0] == 0;
            memcpy(last_recv_type_a_settings_, buffer, MsgLen);
            if (first_time) {
                pending_type_a_settings_change_ = true;
            }
        }

        // Handle vane 1 position change
        uint8_t vane1 = buffer[7] & 0x0F;
        if (vane1 <= 6) {
            vane_position_[0] = vane1;
            vane_select_1_.publish_state(*vane_select_1_.at(vane1));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 1 position: %u", vane1);
        }

        // Handle vane 2 position change
        uint8_t vane2 = (buffer[7] >> 4) & 0x0F;
        if (vane2 <= 6) {
            vane_position_[1] = vane2;
            vane_select_2_.publish_state(*vane_select_2_.at(vane2));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 2 position: %u", vane2);
        }

        // Handle vane 3 position change
        uint8_t vane3 = buffer[8] & 0x0F;
        if (vane3 <= 6) {
            vane_position_[2] = vane3;
            vane_select_3_.publish_state(*vane_select_3_.at(vane3));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 3 position: %u", vane3);
        }

        // Handle vane 4 position change
        uint8_t vane4 = (buffer[8] >> 4) & 0x0F;
        if (vane4 <= 6) {
            vane_position_[3] = vane4;
            vane_select_4_.publish_state(*vane_select_4_.at(vane4));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 4 position: %u", vane4);
        }

        auto_dry_.publish_state(buffer[11] & 0x8);

        if (sender != MessageSender::Slave) {
            // Handle fan speed 0 (slow) change
            fan_speed_[0] = buffer[2];
            fan_speed_slow_.publish_state(fan_speed_[0]);

            // Handle fan speed 1 (low) change
            fan_speed_[1] = buffer[3];
            fan_speed_low_.publish_state(fan_speed_[1]);

            // Handle fan speed 2 (medium) change
            fan_speed_[2] = buffer[4];
            fan_speed_medium_.publish_state(fan_speed_[2]);

            // Handle fan speed 3 (high) change
            fan_speed_[3] = buffer[5];
            fan_speed_high_.publish_state(fan_speed_[3]);
        }
    }

    void process_type_b_settings_message(MessageSender sender, const uint8_t* buffer) {
        // Ignore this message from other controllers.
        if (sender != MessageSender::Unit) {
            return;
        }

        // Send installer settings the first time we receive a 0xCB message.
        bool first_time = last_recv_type_b_settings_[0] == 0;
        memcpy(last_recv_type_b_settings_, buffer, MsgLen);
        if (first_time) {
            pending_type_b_settings_change_ = true;
        }

        last_sent_recv_type_b_millis_ = millis();

        uint8_t overheating = (buffer[2] >> 3) & 0b111;
        if (overheating <= 4) {
            overheating_ = overheating;
            overheating_select_.publish_state(*overheating_select_.at(overheating));
        } else {
            ESP_LOGE(TAG, "Unexpected overheating value: %u", overheating);
        }

        // Table mapping a byte value to degrees Celsius based on values displayed by PREMTB100.
        // INT8_MIN indicates an invalid value.
        static constexpr int8_t PipeTempTable[] = {
            /* 0x00 */ INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, 108, 104, 101, 100, 98, 95,
            /* 0x10 */ 93, 91, 89, 87, 85, 84, 82, 81, 79, 78, 76, 75, 74, 73, 72, 71,
            /* 0x20 */ 70, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 60, 59, 58, 57, 57,
            /* 0x30 */ 56, 55, 55, 54, 53, 53, 52, 52, 51, 50, 50, 49, 49, 48, 47, 47,
            /* 0x40 */ 46, 46, 45, 45, 44, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
            /* 0x50 */ 39, 38, 38, 37, 37, 36, 36, 36, 35, 35, 34, 34, 33, 33, 33, 32,
            /* 0x60 */ 32, 31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 27, 27, 27, 26,
            /* 0x70 */ 26, 26, 25, 25, 24, 24, 24, 23, 23, 23, 22, 22, 22, 21, 21, 21,
            /* 0x80 */ 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15,
            /* 0x90 */ 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 10, 10,
            /* 0xa0 */ 10, 9, 9, 9, 8, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4,
            /* 0xb0 */ 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0, -1,
            /* 0xc0 */ -1, -2, -2, -2, -3, -3, -4, -4, -5, -5, -5, -6, -6, -7, -7, -8,
            /* 0xd0 */ -8, -9, -9, -9, -10, -10, -11, -11, -12, -12, -13, -14, -14, -15, -15, -16,
            /* 0xe0 */ -16, -17, -18, -18, -19, -20, -20, -21, -22, -22, -23, -24, -25, -26, -27,
                       -28,
            /* 0xf0 */ -29, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN
        };
        static_assert(sizeof(PipeTempTable) == 256);
        static_assert(PipeTempTable[UINT8_MAX] == INT8_MIN);

        int8_t pipe_temp_in = PipeTempTable[buffer[3]];
        if (pipe_temp_in == INT8_MIN) {
            pipe_temp_in_.set_internal(true);
        } else {
            pipe_temp_in_.set_internal(false);
            pipe_temp_in_.publish_state(pipe_temp_in);
        }

        int8_t pipe_temp_out = PipeTempTable[buffer[4]];
        if (pipe_temp_out == INT8_MIN) {
            pipe_temp_out_.set_internal(true);
        } else {
            pipe_temp_out_.set_internal(false);
            pipe_temp_out_.publish_state(pipe_temp_out);
        }

        int8_t pipe_temp_mid = PipeTempTable[buffer[5]];
        if (pipe_temp_mid == INT8_MIN) {
            pipe_temp_mid_.set_internal(true);
        } else {
            pipe_temp_mid_.set_internal(false);
            pipe_temp_mid_.publish_state(pipe_temp_mid);
        }
    }

    void update() {
        ESP_LOGD(TAG, "update");

        bool had_error = false;
        while (UARTDevice::available() > 0) {
            if (!UARTDevice::read_byte(&recv_buf_[recv_buf_len_])) {
                break;
            }
            last_recv_millis_ = millis();
            recv_buf_len_++;
            if (recv_buf_len_ == MsgLen) {
                process_message(recv_buf_, &had_error);
                recv_buf_len_ = 0;
            }
        }

        // If we did not receive the message we sent last time, try to send it again next time.
        // Ignore this when we're initializing because the unit then immediately responds by
        // sending a lot of messages and this introduces a delay.
        if (pending_send_ != PendingSendKind::None && !is_initializing_) {
            ESP_LOGE(TAG, "did not receive message we just sent");
            switch (pending_send_) {
                case PendingSendKind::Status:
                    pending_status_change_ = true;
                    break;
                case PendingSendKind::TypeA:
                    pending_type_a_settings_change_ = true;
                    break;
                case PendingSendKind::TypeB:
                    pending_type_b_settings_change_ = true;
                    break;
                case PendingSendKind::None:
                    ESP_LOGE(TAG, "unreachable");
                    break;
            }
            pending_send_ = PendingSendKind::None;
            return;
        }

        if (recv_buf_len_ > 0) {
            if (millis() - last_recv_millis_ > 15 * 1000) {
                ESP_LOGE(TAG, "discarding incomplete data %s",
                         format_hex_pretty(recv_buf_, recv_buf_len_).c_str());
                recv_buf_len_ = 0;
            }
            return;
        }

        if (had_error) {
            return;
        }

        uint32_t millis_now = millis();

        // Handle sleep timer.
        if (sleep_timer_target_millis_.has_value()) {
            int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis_now);
            if (diff <= 0) {
                ESP_LOGD(TAG, "Turning off for sleep timer");
                sleep_timer_target_millis_.reset();
                active_reservation_= false;
                ignore_sleep_timer_callback_ = true;
                sleep_timer_.publish_state(0);
                ignore_sleep_timer_callback_ = false;
                this->mode = climate::CLIMATE_MODE_OFF;
                pending_status_change_ = true;
                publish_state();
            } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
                if (sleep_timer_.state != *minutes) {
                    ignore_sleep_timer_callback_ = true;
                    sleep_timer_.publish_state(*minutes);
                    ignore_sleep_timer_callback_ = false;
                }
            }
        }

        if (slave_ && is_initializing_) {
            ESP_LOGD(TAG, "Not sending, waiting for other controller or unit to send first");
            return;
        }

        // Make sure the RX pin is idle for at least 500 ms to avoid collisions on the bus as much
        // as possible. If there is still a collision, we'll likely both start sending at
        // approximately the same time and the message will hopefully be corrupt (and ignored)
        // anyway. Else the pending_send_/send_buf_ mechanism should catch it and we try again.
        //
        // Note: using digital_read is *much* better for this than using UARTDevice because that
        // interface has significant delays. It has to wait for a full byte to arrive and this
        // takes about 9-10 ms with our slow baud rate. There are also various buffers and
        // timeouts before incoming bytes reach us.
        //
        // 500 ms might be overkill, but the device usually sends the same message twice with a
        // short delay (about 200 ms?) between them so let's not send there either to avoid
        // collisions.
        auto check_can_send = [&]() -> bool {
            while (true) {
                if (UARTDevice::available() > 0 || !rx_pin_.digital_read()) {
                    ESP_LOGD(TAG, "line busy, not sending yet");
                    return false;
                }
                if (millis() - millis_now > 500) {
                    return true;
                }
                delay(5);
            }
        };

        if (pending_type_a_settings_change_) {
            if (check_can_send()) {
                send_type_a_settings_message();
            }
            return;
        }
        if (pending_type_b_settings_change_) {
            if (check_can_send()) {
                send_type_b_settings_message(/* timed = */ false);
            }
            return;
        }
        // Send a status message if there is a pending change.
        // Additionally, queue a Type A message after sending the status message because some
        // units set the vane position to the default setting after changing swing mode or
        // operation mode.
        if (pending_status_change_) {
            if (check_can_send()) {
                send_status_message();
                pending_type_a_settings_change_ = true;
            }
            return;
        }
        // Send an AB message every 10 minutes to request pipe temperature values.
        if (!slave_ && millis_now - last_sent_recv_type_b_millis_ > 10 * 60 * 1000) {
            if (check_can_send()) {
                send_type_b_settings_message(/* timed = */ true);
            }
            return;
        }
        // Send a status message every 20 seconds.
        // Slave controllers only send this if needed.
        if (!slave_ && millis_now - last_sent_status_millis_ > 20 * 1000) {
            if (check_can_send()) {
                send_status_message();
            }
            return;
        }
    }
};

} // namespace esphome::lg_controller
