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

const char *ssid = "WIFI_SSID_HERE";
const char *password = "WIFI_PASSWORD_HERE";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

const unsigned long SIX_HOURS_IN_MS = 6UL * 60UL * 60UL * 1000UL;  // 6 hours in milliseconds
unsigned long lastRefreshTime = 0;  // Track last refresh time

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

  drawYearProgressScene();
  drawYearDaysLeftScene();
  lastRefreshTime = millis();  // Initialize last refresh time
  delay(1000);

  display.powerOff();
  Serial.println("setup done");
}

void loop()
{
  unsigned long currentTime = millis();
  
  // Check if 6 hours have passed since last refresh
  if (currentTime - lastRefreshTime >= SIX_HOURS_IN_MS) {
    Serial.println("6 hours passed, refreshing display...");
    
    // Connect to WiFi and sync time before refresh
    connectToWiFiAndSyncTimeAndDisconnectWifi();
    
    // Wake up the display
    display.init(115200);
    
    // Update the display
    drawYearProgressScene();
    
    // Power off the display to save energy
    display.powerOff();
    
    // Update last refresh time
    lastRefreshTime = currentTime;
    
    Serial.println("Display refresh complete");
  }
  
  // Add a small delay to prevent too frequent checks
  delay(1000);
}

void drawYearProgressScene()
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
          timeinfo.tm_mon + 1,  // tm_mon is 0-based, so add 1 for human-readable month
          timeinfo.tm_year + 1900,  // tm_year is years since 1900
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
  Serial.println("drawYearProgressScene done");
}


void drawYearDaysLeftScene()
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

  // Calculate remaining days with decimal precision
  float remainingDays = daysInYear - currentDayOfYear + 1.0;  // Add 1.0 to include current day
  float remainingDaysWithDecimal = remainingDays + (24.0 - timeinfo.tm_hour) / 24.0;  // Add decimal part based on current hour

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

  // Draw the remaining days text
  display.setFont(&HelveticaNeueThin32pt7b);
  char daysStr[20];
  sprintf(daysStr, "%d days left", (int)remainingDaysWithDecimal);

  // Calculate text width for right alignment of days text
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(daysStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(display.width() - tbw - MARGIN_RIGHT - CIRCLE_HORIZONTAL_SPACING - 5, display.height() - MARGIN_BOTTOM);
  display.print(daysStr);

  // Draw the refresh timestamp with different font at the top center
  display.setFont(&FreeMono9pt7b);
  char refreshStr[50];
  sprintf(refreshStr, "Refreshed at %02d/%02d/%d %02d:%02d",
          timeinfo.tm_mday,
          timeinfo.tm_mon + 1,  // tm_mon is 0-based, so add 1 for human-readable month
          timeinfo.tm_year + 1900,  // tm_year is years since 1900
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
  Serial.println("drawYearDaysLeftScene done");
}

void connectToWiFiAndSyncTimeAndDisconnectWifi()
{
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
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
