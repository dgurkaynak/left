// uncomment next line to use HSPI for EPD (and VSPI for SD), e.g. with Waveshare ESP32 Driver Board
#define USE_HSPI_FOR_EPD

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <time.h>
#include <Fonts/FreeMono9pt7b.h>
#include "fonts/helvetica-neue-thin-32pt7b.h"
#include "credentials.h"

// select the display class (only one), matching the kind of display panel
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW

// select the display driver class (only one) for your  panel
#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7 // GDEY075T7  800x480, UC8179 (GD7965), (FPC-C001 20.08.20)

// somehow there should be an easier way to do this
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)

#if defined(ESP32)
#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
#if IS_GxEPD2_BW(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
#elif IS_GxEPD2_3C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
#elif IS_GxEPD2_7C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
#endif
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25));
#endif

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

const unsigned long SIX_HOURS_IN_MS = 6UL * 60UL * 60UL * 1000UL; // 6 hours in milliseconds
unsigned long lastRefreshTime = 0;                                // Track last refresh time
bool firstRefreshDone = false;                                    // Track if we've done the first midnight refresh

void setup()
{
  Serial.begin(115200);

  // I don't why, but without this delay, we don't see the initial logging messages?
  delay(1000);
  Serial.println("");
  Serial.println("Hello bitches...");

  // *** special handling for Waveshare ESP32 Driver board *** //
  // ********************************************************* //
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif
  // *** end of special handling for Waveshare ESP32 Driver board *** //
  // **************************************************************** //

  connectToWiFiAndSyncTimeAndDisconnectWifi();

  display.init(115200);
  // first update should be full refresh

  // drawYearProgress();
  drawDaysLeftInYear();
  // drawWeeksLeftInLife();

  lastRefreshTime = millis(); // Initialize last refresh time
  delay(1000);

  display.powerOff();
  Serial.println("setup done");
}

void loop()
{
  unsigned long currentTime = millis();
  struct tm timeinfo;

  // Get current time
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    delay(1000);
    return;
  }

  // If we haven't done the first refresh yet, wait until midnight
  if (!firstRefreshDone)
  {
    // Calculate time until midnight
    int secondsUntilMidnight = (24 - timeinfo.tm_hour) * 3600 - timeinfo.tm_min * 60 - timeinfo.tm_sec;
    unsigned long msUntilMidnight = secondsUntilMidnight * 1000UL;

    if (currentTime - lastRefreshTime >= msUntilMidnight)
    {
      // It's midnight, do the first refresh
      Serial.println("First refresh at midnight...");
      connectToWiFiAndSyncTimeAndDisconnectWifi();
      display.init(115200);
      drawDaysLeftInYear();
      display.powerOff();
      lastRefreshTime = currentTime;
      firstRefreshDone = true;
    }
  }
  else
  {
    // After first refresh, refresh every 24 hours
    if (currentTime - lastRefreshTime >= 24UL * 60UL * 60UL * 1000UL)
    {
      Serial.println("24 hours passed, refreshing display...");
      connectToWiFiAndSyncTimeAndDisconnectWifi();
      display.init(115200);
      drawDaysLeftInYear();
      display.powerOff();
      lastRefreshTime = currentTime;
    }
  }

  // Add a small delay to prevent too frequent checks
  delay(1000);
}

void drawYearProgress()
{
  // Get the current date
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }

  // How many days in the current year?
  int daysInYear = 365;
  int currentYear = timeinfo.tm_year + 1900; // tm_year is years since 1900

  // Check if it's a leap year
  // Leap year if divisible by 4 and not divisible by 100, unless also divisible by 400
  if ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0))
  {
    daysInYear = 366;
  }

  // Calculate current day of year (1-based)
  int currentDayOfYear = timeinfo.tm_yday + 1;

  int MARGIN_LEFT = 8;
  int MARGIN_TOP = 36;
  int CIRCLE_RADIUS = 10;
  int CIRCLE_HORIZONTAL_SPACING = 5;
  int CIRCLE_VERTICAL_SPACING = 5;

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);

  // Draw the circles (days of the year)
  int currentX = MARGIN_LEFT + CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING;
  int currentY = MARGIN_TOP + CIRCLE_RADIUS + CIRCLE_VERTICAL_SPACING;
  int circlesPerRow = (display.width() - 2 * CIRCLE_RADIUS) / (2 * CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING);

  for (int day = 0; day < daysInYear; day++)
  {
    // Draw circle for this day
    if (day + 1 < currentDayOfYear)
    {
      // Past day - filled circle
      display.fillCircle(currentX, currentY, CIRCLE_RADIUS, GxEPD_BLACK);
    }
    else
    {
      // Future day - outline circle
      display.drawCircle(currentX, currentY, CIRCLE_RADIUS, GxEPD_BLACK);
    }

    // Move to next position
    currentX += 2 * CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING;

    // If we've reached the end of the row, move to next row
    if (currentX + CIRCLE_RADIUS > display.width() - CIRCLE_HORIZONTAL_SPACING)
    {
      currentX = MARGIN_LEFT + CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING;
      currentY += 2 * CIRCLE_RADIUS + CIRCLE_VERTICAL_SPACING;
    }
  }

  int MARGIN_BOTTOM = 15;
  int MARGIN_RIGHT = 8;

  // Draw the year text
  display.setFont(&HelveticaNeueThin32pt7b);
  display.setTextColor(GxEPD_BLACK);
  char yearStr[5];
  sprintf(yearStr, "%d", currentYear);
  display.setCursor(MARGIN_LEFT + CIRCLE_HORIZONTAL_SPACING + 2, display.height() - MARGIN_BOTTOM);
  display.print(yearStr);

  // Draw the completion percentage and "completed" text
  display.setFont(&HelveticaNeueThin32pt7b);
  float completionPercentage = (float)(currentDayOfYear - 1) / daysInYear * 100;
  char percentageStr[20];
  sprintf(percentageStr, "%d%% completed", (int)completionPercentage);

  // Calculate text width for right alignment of percentage text
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(percentageStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(display.width() - tbw - MARGIN_RIGHT - CIRCLE_HORIZONTAL_SPACING - 5, display.height() - MARGIN_BOTTOM);
  display.print(percentageStr);

  // Draw the refresh timestamp with different font at the top center
  display.setFont(&FreeMono9pt7b);
  char refreshStr[50];
  sprintf(refreshStr, "Refreshed at %02d/%02d/%d %02d:%02d",
          timeinfo.tm_mday,
          timeinfo.tm_mon + 1,     // tm_mon is 0-based, so add 1 for human-readable month
          timeinfo.tm_year + 1900, // tm_year is years since 1900
          timeinfo.tm_hour, timeinfo.tm_min);

  // Calculate text width for center alignment of refresh text
  int16_t rtbx, rtby;
  uint16_t rtbw, rtbh;
  display.getTextBounds(refreshStr, 0, 0, &rtbx, &rtby, &rtbw, &rtbh);
  uint16_t refreshX = (display.width() - rtbw) / 2;
  display.setCursor(refreshX, 20);
  display.print(refreshStr);

  // Draw a horizontal line below refresh timestamp
  display.drawLine(MARGIN_LEFT + CIRCLE_HORIZONTAL_SPACING, 30, display.width() - MARGIN_RIGHT - CIRCLE_HORIZONTAL_SPACING - 3, 30, GxEPD_BLACK);

  display.nextPage();
  Serial.println("drawYearProgress done");
}

void drawDaysLeftInYear()
{
  // Get the current date
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }

  int currentYear = timeinfo.tm_year + 1900; // tm_year is years since 1900
  int currentMonth = timeinfo.tm_mon;        // 0-based month
  int currentDay = timeinfo.tm_mday;         // 1-based day

  // Days in each month
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  // Adjust February for leap years
  if ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0))
  {
    daysInMonth[1] = 29;
  }

  int MARGIN_LEFT = 8;
  int MARGIN_TOP = 36;
  int MARGIN_RIGHT = 8;
  int MARGIN_BOTTOM = 15;
  int CIRCLE_RADIUS = 10; // Slightly smaller to fit 12 rows
  int CIRCLE_HORIZONTAL_SPACING = 4;
  int CIRCLE_VERTICAL_SPACING = 4;

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);

  // Draw month names and their days
  const char *monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  // Calculate starting X position for circles (after month name)
  int monthNameWidth = strlen(monthNames[0]) * 6; // Approximate width of month name
  int startX = MARGIN_LEFT + monthNameWidth + 33; // Add some padding after month name

  // Draw day labels at the top
  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);

  // Draw labels for 10, 20, 30
  for (int labelDay = 10; labelDay <= 30; labelDay += 10)
  {
    int xPos = startX + (labelDay - 1) * (2 * CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING);
    char labelStr[3];
    sprintf(labelStr, "%d", labelDay);
    display.setCursor(xPos - 10, 20); // Center the label above the circles
    display.print(labelStr);
  }

  // Draw current day label
  char currentDayStr[3];
  sprintf(currentDayStr, "%d", currentDay);
  int currentDayX = startX + (currentDay - 1) * (2 * CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING);
  display.setCursor(currentDayX - 10, 20);
  display.print(currentDayStr);

  // Draw a horizontal line below the labels
  display.drawLine(MARGIN_LEFT, 30, display.width() - MARGIN_RIGHT - 3, 30, GxEPD_BLACK);

  int currentY = MARGIN_TOP;
  for (int month = 0; month < 12; month++)
  {
    // Draw month name
    display.setFont(&FreeMono9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_LEFT, currentY + CIRCLE_RADIUS + 4);
    display.print(monthNames[month]);

    // Calculate starting X position for circles (after month name)
    int monthNameWidth = strlen(monthNames[month]) * 6; // Approximate width of month name
    int currentX = MARGIN_LEFT + monthNameWidth + 33;   // Add some padding after month name

    // Draw circles for this month
    for (int day = 0; day < daysInMonth[month]; day++)
    {
      // Determine if this day is past, current, or future
      bool isPast = (month < currentMonth) ||
                    (month == currentMonth && day + 1 < currentDay);
      bool isToday = (month == currentMonth && day + 1 == currentDay);

      // Draw circle for this day
      if (isPast)
      {
        // Past day - filled circle
        display.fillCircle(currentX, currentY + CIRCLE_RADIUS, CIRCLE_RADIUS, GxEPD_BLACK);
      }
      else
      {
        // Future day - outline circle
        display.drawCircle(currentX, currentY + CIRCLE_RADIUS, CIRCLE_RADIUS, GxEPD_BLACK);
      }

      // Draw X inside the circle for today's date
      if (isToday)
      {
        int x = currentX;
        int y = currentY + CIRCLE_RADIUS;

        // Draw a filled black circle
        display.fillCircle(x, y, CIRCLE_RADIUS, GxEPD_BLACK);
        // Draw a slightly smaller white circle on top to create a thick border
        display.fillCircle(x, y, CIRCLE_RADIUS - 6, GxEPD_WHITE);
      }

      // Move to next position
      currentX += 2 * CIRCLE_RADIUS + CIRCLE_HORIZONTAL_SPACING;
    }

    // Move to next row
    currentY += 2 * CIRCLE_RADIUS + CIRCLE_VERTICAL_SPACING + 4; // Add some extra spacing between rows
  }

  // Draw the year text
  display.setFont(&HelveticaNeueThin32pt7b);
  display.setTextColor(GxEPD_BLACK);
  char yearStr[5];
  sprintf(yearStr, "%d", currentYear);
  display.setCursor(MARGIN_LEFT, display.height() - MARGIN_BOTTOM);
  display.print(yearStr);

  // Calculate remaining days in the year (including today)
  int daysInYear = 365;
  if ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0))
  {
    daysInYear = 366;
  }
  int remainingDays = daysInYear - timeinfo.tm_yday; // tm_yday is 0-based, so we don't need to subtract 1
  char daysStr[20];
  sprintf(daysStr, "%d days left", remainingDays);

  // Calculate text width for right alignment of days text
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(daysStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(display.width() - tbw - MARGIN_RIGHT, display.height() - MARGIN_BOTTOM);
  display.print(daysStr);

  // Draw a horizontal line below refresh timestamp
  display.drawLine(MARGIN_LEFT, 30, display.width() - MARGIN_RIGHT - 3, 30, GxEPD_BLACK);

  display.nextPage();
  Serial.println("drawDaysLeftInYear done");
}

void drawWeeksLeftInLife()
{
  // Get the current date
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }

  // Calculate birth date in time_t format
  struct tm birth = {0};
  birth.tm_year = 89; // 1989
  birth.tm_mon = 3;   // April (0-based)
  birth.tm_mday = 12; // 12th
  time_t birth_time = mktime(&birth);

  // Get current time
  time_t now;
  time(&now);

  // Calculate weeks lived and remaining
  double weeks_lived = difftime(now, birth_time) / (7.0 * 24.0 * 60.0 * 60.0);
  int weeks_remaining = 4000 - (int)weeks_lived;

  int MARGIN_LEFT = 8;
  int MARGIN_TOP = 8;
  int RECT_SIZE = 6;
  int RECT_HORIZONTAL_SPACING = 2;
  int RECT_VERTICAL_SPACING = 2;

  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);

  // Draw the rectangles (weeks of life)
  int currentX = MARGIN_LEFT + RECT_HORIZONTAL_SPACING;
  int currentY = MARGIN_TOP + RECT_VERTICAL_SPACING;
  int rectsPerRow = (display.width() - 2 * MARGIN_LEFT) / (RECT_SIZE + RECT_HORIZONTAL_SPACING);

  for (int week = 0; week < 4000; week++) // TODO: As we increase this number, the view will be more fucked up (we can say "pale", to be more polite/specific)
  {
    // Draw rectangle for this week
    if (week < (int)weeks_lived)
    {
      // Past week - filled rectangle
      display.fillRect(currentX, currentY, RECT_SIZE, RECT_SIZE, GxEPD_BLACK);
    }
    else
    {
      // Future week - outline rectangle
      // display.drawRect(currentX, currentY, RECT_SIZE, RECT_SIZE, GxEPD_BLACK); // TODO: If we use this, it's going to be super fucked up
      display.fillRect(currentX + 2, currentY + 2, RECT_SIZE - 4, RECT_SIZE - 4, GxEPD_BLACK);
    }

    // Move to next position
    currentX += RECT_SIZE + RECT_HORIZONTAL_SPACING;

    // If we've reached the end of the row, move to next row
    if (currentX + RECT_SIZE > display.width() - MARGIN_LEFT)
    {
      currentX = MARGIN_LEFT + RECT_HORIZONTAL_SPACING;
      currentY += RECT_SIZE + RECT_VERTICAL_SPACING;
    }
  }

  int MARGIN_BOTTOM = 15;
  int MARGIN_RIGHT = 8;

  // Draw the remaining weeks text
  display.setFont(&HelveticaNeueThin32pt7b);
  display.setTextColor(GxEPD_BLACK);
  char weeksStr[20];
  sprintf(weeksStr, "%d weeks left", weeks_remaining);

  // Calculate text width for right alignment of weeks text
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(weeksStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(display.width() - tbw - MARGIN_RIGHT - RECT_HORIZONTAL_SPACING - 5, display.height() - MARGIN_BOTTOM);
  display.print(weeksStr);

  display.nextPage();
  Serial.println("drawWeeksLeftInLife done");
}

void connectToWiFiAndSyncTimeAndDisconnectWifi()
{
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Init and get the time
  Serial.println("Initializing time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Sometimes I get `Failed to obtain time` error. That's the reason for this delay.
  delay(500);

  // Print the time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  // disconnect WiFi as it's no longer needed
  Serial.println("Disconnecting WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
