
#include "widgets/weatherWidget.h"
#include "icons.h"

#include <config.h>

WeatherWidget::WeatherWidget(ScreenManager &manager) : Widget(manager) {
    m_mode = MODE_HIGHS;
}

WeatherWidget::~WeatherWidget() {
}

void WeatherWidget::changeMode() {
    m_mode++;
    if (m_mode > MODE_LOWS) {
        m_mode = MODE_HIGHS;
    }
    draw(true);
}

void WeatherWidget::setup() {
    m_lastUpdate = millis() - m_updateDelay + 1000;
    m_time = GlobalTime::getInstance();
}

void WeatherWidget::draw(bool force) {
    m_time->updateTime();
    int clockStamp = getClockStamp();
    if (clockStamp != m_clockStamp || force) {
        displayClock(0, TFT_WHITE, TFT_BLACK);
        m_clockStamp = clockStamp;
    }

    // Weather, displays a clock, city & text weather discription, weather icon, temp, 3 day forecast
    if (force || model.isChanged()) {
        weatherText(1, TFT_WHITE, TFT_BLACK);
        drawWeatherIcon(model.getCurrentIcon(), 2, 0, 0, 1);
        singleWeatherDeg(3, TFT_WHITE, TFT_BLACK);
        threeDayWeather(4);
        model.setChangedStatus(false);
    }
}

void WeatherWidget::update(bool force) {
    if (force || m_lastUpdate == 0 || (millis() - m_lastUpdate) >= m_updateDelay) {
        setBusy(true);
        if (force) {
            int retry = 0;
            while (!getWeatherData() && retry++ < MAX_RETRIES)
                ;
        } else {
            getWeatherData();
        }
        setBusy(false);
        m_lastUpdate = millis();
    }
}

bool WeatherWidget::getWeatherData() {
    HTTPClient http;
    http.begin(httpRequestAddress);
    int httpCode = http.GET();
    if (httpCode > 0) { 
        // Check for the returning code
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, http.getString());
        http.end();

        if (!error) {
            model.setCityName(doc["resolvedAddress"].as<String>());
            model.setCurrentTemperature(doc["currentConditions"]["temp"].as<float>());
            model.setCurrentText(doc["days"][0]["description"].as<String>());

            model.setCurrentIcon(doc["currentConditions"]["icon"].as<String>());
            model.setTodayHigh(doc["days"][0]["tempmax"].as<float>());
            model.setTodayLow(doc["days"][0]["tempmin"].as<float>());
            for (int i = 0; i < 3; i++) {
                model.setDayIcon(i, doc["days"][i + 1]["icon"].as<String>());
                model.setDayHigh(i, doc["days"][i + 1]["tempmax"].as<float>());
                model.setDayLow(i, doc["days"][i + 1]["tempmin"].as<float>());
            }
        } else {
            // Handle JSON deserialization error
            switch (error.code()) {
                case DeserializationError::Ok:
                    Serial.print(F("Deserialization succeeded"));
                    break;
                case DeserializationError::InvalidInput:
                    Serial.print(F("Invalid input!"));
                    break;
                case DeserializationError::NoMemory:
                    Serial.print(F("Not enough memory"));
                    break;
                default:
                    Serial.print(F("Deserialization failed"));
                    break;
            }

            return false;
        }
    } else {
        // Handle HTTP request error
        Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
    return true;
}

void WeatherWidget::displayClock(int displayIndex, uint32_t background, uint32_t color) {
    const int clockY = 94;
    const int dayOfWeekY = 160;
    const int dateY = 197;

    m_manager.selectScreen(displayIndex);

    TFT_eSPI &display = m_manager.getDisplay();

    display.fillScreen(background);
    display.setTextColor(color);
    display.setTextSize(1);
    display.setTextDatum(MC_DATUM);
#ifdef WEATHER_UNITS_METRIC
    display.drawString(String(m_time->getDay()) + " " + m_time->getMonthName(), centre, dateY, 4);
#else
    display.drawString(m_time->getMonthName() + " " + String(m_time->getDay()), centre, dateY, 4);
#endif
    display.setTextSize(2);
    display.drawString(m_time->getWeekday(), centre, dayOfWeekY, 4);

    display.setTextSize(1);
    display.setTextDatum(MR_DATUM);
    display.drawString(m_time->getHourPadded(), centre - 5, clockY, 8);
    display.setTextDatum(MC_DATUM);
    display.drawString(":", centre, clockY, 8);
    display.setTextDatum(ML_DATUM);
    display.drawString(m_time->getMinutePadded(), centre + 5, clockY, 8);
}

// This will write an image to the screen when called from a hex array. Pass in:
// Screen #, X, Y coords, Bye Array To Pass, the sizeof that array, scale of the image(1= full size, then multiples of 2 to scale down)
// getting the byte array size is very annoying as its computed on compile so you cant do it dynamicly.
void WeatherWidget::showJPG(int displayIndex, int x, int y, const byte jpgData[], int jpgDataSize, int scale) {
    m_manager.selectScreen(displayIndex);

    TJpgDec.setJpgScale(scale);
    uint16_t w = 0, h = 0;
    TJpgDec.getJpgSize(&w, &h, jpgData, jpgDataSize);
    TJpgDec.drawJpg(x, y, jpgData, jpgDataSize);
}

// This takes the text output form the weatehr API and maps it to arespective icon/byte aarray, then displays it,
void WeatherWidget::drawWeatherIcon(String condition, int displayIndex, int x, int y, int scale) {
    const byte *icon = NULL;
    int size = 0;
    if (condition == "partly-cloudy-night") {
        icon = moonCloud_start;
        size = moonCloud_end - moonCloud_start;
    } else if (condition == "partly-cloudy-day") {
        icon = sunClouds_start;
        size = sunClouds_end - sunClouds_start;
    } else if (condition == "clear-day") {
        icon = sun_start;
        size = sun_end - sun_start;
    } else if (condition == "clear-night") {
        icon = moon_start;
        size = moon_end - moon_start;
    } else if (condition == "snow") {
        icon = snow_start;
        size = snow_end - snow_start;
    } else if (condition == "rain") {
        icon = rain_start;
        size = rain_end - rain_start;
    } else if (condition == "fog" || condition == "wind" || condition == "cloudy") {
        icon = clouds_start;
        size = clouds_end - clouds_start;
    } else {
        Serial.println("unknown weather icon:" + condition);
    }

    if (icon != NULL && size > 0) {
        showJPG(displayIndex, x, y, icon, size, scale);
    }
}

// This displays the current weather temp on a single screen. Pass in display number, background color, text color
// doesnt round deg, just removes all text after the decimil, should probably be fixed
void WeatherWidget::singleWeatherDeg(int displayIndex, uint32_t backgroundColor, uint32_t textColor) {
    m_manager.selectScreen(displayIndex);

    TFT_eSPI &display = m_manager.getDisplay();
    display.fillScreen(backgroundColor);

    drawDegrees(model.getCurrentTemperature(0), centre, 108, 8, 1, 15, 8, textColor, backgroundColor);

    display.fillRect(0, 170, 240, 70, TFT_BLACK);

    display.fillRect(centre - 1, 170, 2, 240, TFT_WHITE);

    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("high", 80, 190, 4);
    display.drawString("low", 160, 190, 4);
    drawDegrees(model.getTodayHigh(0), 80, 216, 4, 1, 4, 2, TFT_WHITE, TFT_BLACK);
    drawDegrees(model.getTodayLow(0), 160, 216, 4, 1, 4, 2, TFT_WHITE, TFT_BLACK);
}

// This displays the users current city and the text desctiption of the weather. Pass in display number, background color, text color
void WeatherWidget::weatherText(int displayIndex, int16_t b, int16_t t) {
    m_manager.selectScreen(displayIndex);
    TFT_eSPI &display = m_manager.getDisplay();
    //=== TEXT OVERFLOW ============================
    // This takes a given string a and breaks it down in max x character long strings ensuring not to break it only at a space.
    // Given the small width of the screens this will porbablly be needed to this project again so making sure to outline it
    // clearly as this should liekly eventually be turned into a fucntion. Before use the array size should be made to be dynamic.
    // In this case its used for the weather text description

    String message = model.getCurrentText() + " ";
    String messageArr[4];
    int variableRangeS = 0;
    int variableRangeE = 18;
    for (int i = 0; i < 4; i++) {
        while (message.substring(variableRangeE - 1, variableRangeE) != " ") {
            variableRangeE--;
        }
        messageArr[i] = message.substring(variableRangeS, variableRangeE);
        variableRangeS = variableRangeE;
        variableRangeE = variableRangeS + 18;
    }
    //=== OVERFLOW END ==============================

    display.fillScreen(b);
    display.setTextColor(t);
    display.setTextSize(2);
    display.setTextDatum(MC_DATUM);
    String cityName = model.getCityName();
    cityName.remove(cityName.indexOf(",", 0));
    display.drawString(cityName, centre, 84, 4);
    display.setTextSize(1);
    display.setTextFont(4);

    auto y = 118;
    for (auto i = 0; i < 4; i++) {
        display.drawString(messageArr[i], centre, y);
        y += 21;
    }
}

// This displays the next 3 days weather forecast
void WeatherWidget::threeDayWeather(int displayIndex) {
    const int days = 3;
    const int columnSize = 75;
    const int highLowY = 199;

    m_manager.selectScreen(displayIndex);
    TFT_eSPI &display = m_manager.getDisplay();

    display.setTextDatum(MC_DATUM);
    display.fillScreen(TFT_WHITE);
    display.setTextSize(1);

    display.fillRect(0, 170, 240, 70, TFT_BLACK);

    display.setTextColor(TFT_WHITE);
    display.drawString(m_mode == MODE_HIGHS ? "highs" : "lows", centre, highLowY, 4);
    
    int temperatureFontId = 6;  // 48px 0-9 only
    // Look up all the temperatures, and if any of them are more than 2 digits, we need
    // to scale down the font -- or it won't look right on the screen.
    String temps[days];
    for (auto i = 0; i < days; i++) {
        temps[i] = m_mode == MODE_HIGHS ? model.getDayHigh(i, 0) : model.getDayLow(i, 0);
        if (temps[i].length() > 2) {
            // We've got a nutty 3-digit temperature, scale down
            temperatureFontId = 4;  // 26px 0-9; if there were a 36 or 32, I'd use it
        }
    }

    display.setTextColor(TFT_BLACK);
    for (auto i = 0; i < days; i++) {
        // TODO: only works for 3 days
        const int x = (centre - columnSize) + i * columnSize;

        drawWeatherIcon(model.getDayIcon(i), displayIndex, x - 30, 40, 4);
        drawDegrees(temps[i], x, 122, temperatureFontId, 1, 7, 4, TFT_BLACK, TFT_WHITE);

        String shortDayName = dayStr(weekday(m_time->getUnixEpoch() + (86400 * (i + 1))));
        shortDayName.remove(3);
        display.drawString(shortDayName, x, 154, 4);
    }
}

int WeatherWidget::drawDegrees(String number, int x, int y, uint8_t font, uint8_t size, uint8_t outerRadius, uint8_t innerRadius, int16_t textColor, int16_t backgroundColor) {
    TFT_eSPI &display = m_manager.getDisplay();

    display.setTextColor(textColor);
    display.setTextFont(font);
    display.setTextSize(size);
    display.setTextDatum(MC_DATUM);

    int16_t textWidth = display.textWidth(number);
    int16_t fontHeight = display.fontHeight(font);
    int offset = ceil(fontHeight * 0.15);
    int circleX = textWidth / 2 + x + offset;
    int circleY = y - fontHeight / 2 + floor(fontHeight / 10);

    display.drawString(number, x, y, font);
    display.fillCircle(circleX, circleY, outerRadius, textColor);
    display.fillCircle(circleX, circleY, innerRadius, backgroundColor);

    return textWidth + offset;
}

int WeatherWidget::getClockStamp() {
    return m_time->getHour() * 60 + m_time->getMinute();
}

String WeatherWidget::getName() {
    return "Weather";
}