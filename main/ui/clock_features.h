#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>

#define ALARM_NS "alarm"
#define ALARM_KEY_ENABLED "enabled"
#define ALARM_KEY_HOUR "hour"
#define ALARM_KEY_MINUTE "minute"
#define ALARM_KEY_REPEAT "repeat"

#define CHIME_NS "chime"
#define CHIME_KEY_ENABLED "enabled"
#define CHIME_KEY_START_HOUR "start_hour"
#define CHIME_KEY_END_HOUR "end_hour"
#define CHIME_KEY_DAYS "days"

enum AlarmRepeat {
    ALARM_REPEAT_ONCE = 0,
    ALARM_REPEAT_DAILY = 1,
    ALARM_REPEAT_WEEKDAYS = 2,
};

enum ChimeDays {
    CHIME_EVERY_DAY = 0,
    CHIME_WEEKDAYS_ONLY = 1,
};

inline bool IsWeekday(int tm_wday) {
    return tm_wday >= 1 && tm_wday <= 5;
}

class ClockFeatures {
public:
    static void Initialize();
    static void CheckAlarm();
    static void CheckChime();

    static void InvalidateNvsCache() { cache_valid_ = false; }

    static bool IsAlarmEnabled();
    static void GetAlarmTime(int& hour, int& minute);
    static int GetAlarmRepeat();
    static void SetAlarmEnabled(bool enabled);
    static void SetAlarmTime(int hour, int minute);
    static void SetAlarmRepeat(int repeat);

    static bool IsChimeEnabled();
    static void GetChimeRange(int& start_hour, int& end_hour);
    static int GetChimeDays();
    static void SetChimeEnabled(bool enabled);
    static void SetChimeRange(int start_hour, int end_hour);
    static void SetChimeDays(int days);

private:
    static void LoadCache();
    static std::mutex cache_mutex_;
    static std::atomic<bool> cache_valid_;
    static bool cached_alarm_enabled_;
    static int cached_alarm_hour_;
    static int cached_alarm_minute_;
    static int cached_alarm_repeat_;
    static bool cached_chime_enabled_;
    static int cached_chime_start_hour_;
    static int cached_chime_end_hour_;
    static int cached_chime_days_;
};
