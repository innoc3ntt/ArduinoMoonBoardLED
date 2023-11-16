#include <HardwareBLESerial.h>
#include <NeoPixelBus.h>
#include <algorithm>
#include <config.h>
#include <map>
#include <vector>

HardwareBLESerial &bleSerial = HardwareBLESerial::getInstance();

#ifdef GRB
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
#else
NeoPixelBus<NeoRgbFeature, NeoWs2812xMethod> strip(PixelCount, PixelPin);
#endif

RgbColor red(brightness, 0, 0);
RgbColor green(0, brightness, 0);
RgbColor blue(0, 0, brightness);
RgbColor yellow(additionalledbrightness, additionalledbrightness, 0);
RgbColor cyan(0, brightness, brightness);
RgbColor pink(brightness, 0, brightness / 2);
RgbColor violet(brightness / 2, 0, brightness);
RgbColor black(0);

std::map<char, std::pair<RgbColor, String>> hold_colors = {
    {'S', std::make_pair(green, " (green)")},
    {'R', std::make_pair(blue, " (blue)")},
    {'L', std::make_pair(violet, " (violet)")},
    {'M', std::make_pair(pink, " (pink)")},
    {'F', std::make_pair(cyan, " (cyan)")},
    {'E', std::make_pair(red, " (red)")}};

int state = 0;                    // Variable to store the current state of the problem string parser
String problem_string = "";       // Variable to store the current problem string
bool use_additional_leds = false; // Variable to store the additional LED setting

void startupSequence()
{ // Test LEDs by cycling through the colors and then turning the LEDs off again
  std::vector<RgbColor> colors = {green, blue, yellow, cyan, pink, violet, red};

  std::for_each(colors.begin(), colors.end(), [&](RgbColor color)
                {
      strip.SetPixelColor(0, color);
    for (int i = 0; i < PixelCount; i++)
    {
      strip.ShiftRight(1);
      strip.Show();
      delay(10);
    } });

  strip.ClearTo(black);
  strip.Show();
}

void setup()
{
  Serial.begin(9600);

  while (!bleSerial.beginAndSetupBLE("MoonBoard A"))
  { // Initialize BLE UART and check if it is successful
    Serial.println("BLE setup failed!");
    delay(1000);
  }

  strip.Begin(); // Initialize LED strip
  strip.Show();  // Good practice to call Show() in order to clear all LEDs

  startupSequence();

  // Wait for the MoonBoard App to connect
  while (!bleSerial)
    ;
}

void parseProblemString(String problem_string)
{
  int pos = 0;
  char buffer[32];
  while (pos >= 0)
  {
    pos = problem_string.indexOf(','); // Hold descriptions are separated by a comma (,)

    String hold = pos > 0 ? problem_string.substring(0, pos) : problem_string;

    char holdtype = hold.charAt(0);             // Hold descriptions consist of a hold type (S, P, E) ...
    int holdnumber = hold.substring(1).toInt(); // ... and a hold number
    int lednumber = ledmapping[holdnumber];

    Serial.print(holdtype);
    snprintf(buffer, sizeof(buffer), " %c %d  --> ", holdtype, holdnumber);

    if (use_additional_leds)
    {
      int additionallednumber = lednumber + additionalledmapping[holdnumber];
      if (additionalledmapping[holdnumber] != 0)
      {
        Serial.print(additionallednumber);

        // Right, left, match, start, or foot hold
        if (holdtype != 'E')
        {
          strip.SetPixelColor(additionallednumber, yellow);
          Serial.println(" (yellow)");
        }
      }
    }

    if (!use_additional_leds)
    {
      Serial.print(lednumber);
      strip.SetPixelColor(lednumber, hold_colors[holdtype].first);
      Serial.println(hold_colors[holdtype].second);
    }
    problem_string = problem_string.substring(pos + 1, problem_string.length()); // Remove processed hold from string
  }
}

void handleState4()
{
  strip.ClearTo(black); // Turn off all LEDs in LED string
  Serial.println("\n---------");
  Serial.print("Problem string: ");
  Serial.println(problem_string);
  Serial.println("");

  if (use_additional_leds)
  { // only render additional LEDs in first loop
    Serial.println("Additional LEDs:");
    parseProblemString(problem_string);
    Serial.println("");
  }

  Serial.println("Problem LEDs:");
  parseProblemString(problem_string);

  strip.Show();                // Light up all hold (and additional) LEDs
  use_additional_leds = false; // Reset additional LED option
  state = 0;                   // Switch to state 0 (wait for new problem string or configuration)
  Serial.println("---------\n");
}

void loop()
{
  // Read messages from the BLE UART
  bleSerial.poll();

  // Check if message parts are available on the BLE UART
  while (bleSerial.available() > 0)
  {
    char c = bleSerial.read();

    switch (state)
    {
    case 0:
      // State 0: wait for configuration instructions (sent only if V2 option is enabled in app) or beginning of problem string
      switch (c)
      {
      case '~':
        state = 1; // Switch to state 1 (read configuration)
        continue;

      case 'l':
        state = 2; // Switch to state 2 (read problem string)
        continue;
      }

    case 1:
      switch (c)
      {
      case 'D':
        state = 2;
        use_additional_leds = true;
        Serial.println("Additional LEDs enabled");
        continue;

      case 'l':
        state = 2;
        continue;
      }

    case 2:
      if (c == '#')
      {
        state = 3; // Switch to state 3
        continue;
      }

    case 3:
      // problem string ends with #
      if (c == '#')
      {
        state = 4;
        continue;
      }

      problem_string.concat(c); //
      break;

    case 4:
      handleState4();
    }
  }
}