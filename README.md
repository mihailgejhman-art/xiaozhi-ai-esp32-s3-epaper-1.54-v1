# Xiaozhi AI — ESP32-S3 ePaper 1.54" v1 (English)

Форк [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.2.6, адаптированный для платы **Waveshare ESP32-S3 ePaper 1.54" v1** с английским языком интерфейса.

## Особенности

- **Плата:** Waveshare ESP32-S3 ePaper 1.54" v1 (4MB Flash, 2MB PSRAM, Quad SPI)
- **Язык:** Английский (конфигурация `CONFIG_LANGUAGE_EN_US=y`)
- **Погода:** Сервис wttr.in (город настраивается в `idf.py menuconfig`)
- **Дисплей:** e-paper 1.54", LVGL

## Изменения относительно оригинала

- Добавлена конфигурация `esp32-s3-epaper-1.54-v1` с `sdkconfig_append` для v1-платы
- Включён английский язык (`CONFIG_LANGUAGE_EN_US=y`)
- Настроен Quad SPI для PSRAM (`CONFIG_SPIRAM_MODE_QUAD=y`)
- Раздел 4MB (`partitions/v2/4m.csv`)

## Сборка

```bash
# Установка ESP-IDF v5.5.1
# Клонирование репозитория
git clone https://github.com/mihailgejhman-art/xiaozhi-ai-esp32-s3-epaper-1.54-v1.git
cd xiaozhi-ai-esp32-s3-epaper-1.54-v1

# Настройка города погоды (опционально)
idf.py menuconfig  # Xiaozhi Assistant > Weather Display > City name

# Сборка и прошивка
python scripts/release.py waveshare/esp32-s3-epaper-1.54 --name esp32-s3-epaper-1.54-v1
esptool --port COM5 --baud 921600 write_flash 0x0 build/merged-binary.bin
```

## Прошивка

Готовый бинарник: `releases/v2.2.6_waveshare-esp32-s3-epaper-1.54-v1.zip`
