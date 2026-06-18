#include "clock_features.h"
#include "settings.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <ctime>
#include <vector>

#define TAG "ClockFeatures"

static int last_chime_hour_ = -1;
static time_t last_alarm_epoch_min_ = 0;

std::mutex ClockFeatures::cache_mutex_;
std::atomic<bool> ClockFeatures::cache_valid_ = false;
bool ClockFeatures::cached_alarm_enabled_ = false;
int ClockFeatures::cached_alarm_hour_ = 8;
int ClockFeatures::cached_alarm_minute_ = 0;
int ClockFeatures::cached_alarm_repeat_ = ALARM_REPEAT_DAILY;
bool ClockFeatures::cached_chime_enabled_ = true;
int ClockFeatures::cached_chime_start_hour_ = 0;
int ClockFeatures::cached_chime_end_hour_ = 24;
int ClockFeatures::cached_chime_days_ = CHIME_EVERY_DAY;

void ClockFeatures::LoadCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (cache_valid_) return;
    Settings alarm_settings(ALARM_NS);
    cached_alarm_enabled_ = alarm_settings.GetBool(ALARM_KEY_ENABLED, false);
    cached_alarm_hour_ = alarm_settings.GetInt(ALARM_KEY_HOUR, 8);
    cached_alarm_minute_ = alarm_settings.GetInt(ALARM_KEY_MINUTE, 0);
    cached_alarm_repeat_ = alarm_settings.GetInt(ALARM_KEY_REPEAT, ALARM_REPEAT_DAILY);
    Settings chime_settings(CHIME_NS);
    cached_chime_enabled_ = chime_settings.GetBool(CHIME_KEY_ENABLED, true);
    cached_chime_start_hour_ = chime_settings.GetInt(CHIME_KEY_START_HOUR, 0);
    cached_chime_end_hour_ = chime_settings.GetInt(CHIME_KEY_END_HOUR, 24);
    cached_chime_days_ = chime_settings.GetInt(CHIME_KEY_DAYS, CHIME_EVERY_DAY);
    cache_valid_ = true;
}

void ClockFeatures::Initialize() {
    Settings alarm_settings(ALARM_NS, true);
    if (!alarm_settings.GetBool("init", false)) {
        alarm_settings.SetBool("init", true);
        alarm_settings.SetBool(ALARM_KEY_ENABLED, false);
        alarm_settings.SetInt(ALARM_KEY_HOUR, 8);
        alarm_settings.SetInt(ALARM_KEY_MINUTE, 0);
        alarm_settings.SetInt(ALARM_KEY_REPEAT, ALARM_REPEAT_DAILY);
    }

    Settings chime_settings(CHIME_NS, true);
    if (!chime_settings.GetBool("init", false)) {
        chime_settings.SetBool("init", true);
        chime_settings.SetBool(CHIME_KEY_ENABLED, true);
        chime_settings.SetInt(CHIME_KEY_START_HOUR, 0);
        chime_settings.SetInt(CHIME_KEY_END_HOUR, 24);
        chime_settings.SetInt(CHIME_KEY_DAYS, CHIME_EVERY_DAY);
    }

    LoadCache();
}

void ClockFeatures::CheckAlarm() {
    if (!cache_valid_) LoadCache();
    if (!cached_alarm_enabled_) return;

    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateIdle) return;

    auto tm = Application::GetCachedTm();
    if (tm.tm_year < (2025 - 1900)) return;

    if (tm.tm_hour == cached_alarm_hour_ && tm.tm_min == cached_alarm_minute_ && tm.tm_sec < 2) {
        time_t alarm_epoch_min = Application::GetCachedTime() / 60;
        if (alarm_epoch_min == last_alarm_epoch_min_) return;
        last_alarm_epoch_min_ = alarm_epoch_min;
        if (cached_alarm_repeat_ == ALARM_REPEAT_WEEKDAYS && !IsWeekday(tm.tm_wday)) return;

        app.Alert(Lang::Strings::WARNING, "Alarm!", "neutral", Lang::Sounds::OGG_POPUP);

        if (cached_alarm_repeat_ == ALARM_REPEAT_ONCE) {
            Settings rw_settings(ALARM_NS, true);
            rw_settings.SetBool(ALARM_KEY_ENABLED, false);
            cached_alarm_enabled_ = false;
        }
    }
}

void ClockFeatures::CheckChime() {
    if (!cache_valid_) LoadCache();
    if (!cached_chime_enabled_) return;

    auto tm = Application::GetCachedTm();
    if (tm.tm_year < (2025 - 1900)) return;

    if (tm.tm_min != 0) {
        last_chime_hour_ = -1;
        return;
    }

    if (tm.tm_hour == last_chime_hour_) return;

    if (tm.tm_hour < cached_chime_start_hour_ || tm.tm_hour >= cached_chime_end_hour_) return;

    if (cached_chime_days_ == CHIME_WEEKDAYS_ONLY && !IsWeekday(tm.tm_wday)) return;

    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateIdle) return;

    ESP_LOGI(TAG, "Chiming hour %d", tm.tm_hour);
    last_chime_hour_ = tm.tm_hour;

    static const std::string_view digit_sounds[] = {
        Lang::Sounds::OGG_0, Lang::Sounds::OGG_1, Lang::Sounds::OGG_2,
        Lang::Sounds::OGG_3, Lang::Sounds::OGG_4, Lang::Sounds::OGG_5,
        Lang::Sounds::OGG_6, Lang::Sounds::OGG_7, Lang::Sounds::OGG_8,
        Lang::Sounds::OGG_9
    };
    int tens = tm.tm_hour / 10;
    int ones = tm.tm_hour % 10;
    std::vector<std::string_view> chime_sounds;
    if (tens > 0) chime_sounds.push_back(digit_sounds[tens]);
    chime_sounds.push_back(digit_sounds[ones]);
    app.PlayCombinedSound(chime_sounds);

    char time_msg[32];
    strftime(time_msg, sizeof(time_msg), "It's %H:%M", &tm);
    app.Alert(Lang::Strings::WARNING, time_msg, "clock", {});
}

bool ClockFeatures::IsAlarmEnabled() {
    if (!cache_valid_) LoadCache();
    return cached_alarm_enabled_;
}

void ClockFeatures::GetAlarmTime(int& hour, int& minute) {
    if (!cache_valid_) LoadCache();
    hour = cached_alarm_hour_;
    minute = cached_alarm_minute_;
}

int ClockFeatures::GetAlarmRepeat() {
    if (!cache_valid_) LoadCache();
    return cached_alarm_repeat_;
}

void ClockFeatures::SetAlarmEnabled(bool enabled) {
    Settings settings(ALARM_NS, true);
    settings.SetBool(ALARM_KEY_ENABLED, enabled);
    cached_alarm_enabled_ = enabled;
}

void ClockFeatures::SetAlarmTime(int hour, int minute) {
    Settings settings(ALARM_NS, true);
    settings.SetInt(ALARM_KEY_HOUR, hour);
    settings.SetInt(ALARM_KEY_MINUTE, minute);
    cached_alarm_hour_ = hour;
    cached_alarm_minute_ = minute;
}

void ClockFeatures::SetAlarmRepeat(int repeat) {
    Settings settings(ALARM_NS, true);
    settings.SetInt(ALARM_KEY_REPEAT, repeat);
    cached_alarm_repeat_ = repeat;
}

bool ClockFeatures::IsChimeEnabled() {
    if (!cache_valid_) LoadCache();
    return cached_chime_enabled_;
}

void ClockFeatures::GetChimeRange(int& start_hour, int& end_hour) {
    if (!cache_valid_) LoadCache();
    start_hour = cached_chime_start_hour_;
    end_hour = cached_chime_end_hour_;
}

int ClockFeatures::GetChimeDays() {
    if (!cache_valid_) LoadCache();
    return cached_chime_days_;
}

void ClockFeatures::SetChimeEnabled(bool enabled) {
    Settings settings(CHIME_NS, true);
    settings.SetBool(CHIME_KEY_ENABLED, enabled);
    cached_chime_enabled_ = enabled;
}

void ClockFeatures::SetChimeRange(int start_hour, int end_hour) {
    Settings settings(CHIME_NS, true);
    settings.SetInt(CHIME_KEY_START_HOUR, start_hour);
    settings.SetInt(CHIME_KEY_END_HOUR, end_hour);
    cached_chime_start_hour_ = start_hour;
    cached_chime_end_hour_ = end_hour;
}

void ClockFeatures::SetChimeDays(int days) {
    Settings settings(CHIME_NS, true);
    settings.SetInt(CHIME_KEY_DAYS, days);
    cached_chime_days_ = days;
}
