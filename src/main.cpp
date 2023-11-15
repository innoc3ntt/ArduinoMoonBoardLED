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

int state = 0;                 // Variable to store the current state of the problem string parser
String problemstring = "";     // Variable to store the current problem string
bool useadditionalled = false; // Variable to store the additional LED setting

void setup()
{
  Serial.begin(9600);

  std::map<char, std::pair<RgbColor, String>> holdcolors = {
      {'S', std::make_pair(green, " (green)")},
      {'R', std::make_pair(blue, " (blue)")},
      {'L', std::make_pair(violet, " (violet)")},
      {'M', std::make_pair(pink, " (pink)")},
      {'F', std::make_pair(cyan, " (cyan)")},
      {'E', std::make_pair(red, " (red)")}};

  Serial.println(holdcolors['S'].second);

  if (!bleSerial.beginAndSetupBLE("MoonBoard A"))
  { // Initialize BLE UART and check if it is successful
    // This should never happen as it means that the BLE setup failed and the program cannot run!
    while (true)
    {
      Serial.println("BLE setup failed!");
      delay(1000);
    }
  }

  strip.Begin(); // Initialize LED strip
  strip.Show();  // Good practice to call Show() in order to clear all LEDs

  // Test LEDs by cycling through the colors and then turning the LEDs off again

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

  // Wait for the MoonBoard App to connect
  while (!bleSerial)
    ;
}

void loop()
{
  // Read messages from the BLE UART
  bleSerial.poll();

  // Check if message parts are available on the BLE UART
  while (bleSerial.available() > 0)
  {
    char c = bleSerial.read();

    // State 0: wait for configuration instructions (sent only if V2 option is enabled in app) or beginning of problem string
    if (state == 0)
    {
      switch (c)
      {
      case '~':
        state = 1; // Switch to state 1 (read configuration)
        break;

      case '1':
        state = 2; // Switch to state 2 (read problem string)
        break;

      default:
        break;
      }
    }

    // State 1: read configuration option (sent only if V2 option is enabled in app)
    if (state == 1)
    {
      if (c == 'D')
      {
        useadditionalled = true;
        state = 2; // Switch to state 2 (read problem string
        Serial.println("Additional LEDs enabled");
        continue;
      }
      if (c == 'l')
      {
        state = 2; // Switch to state 2 (read problem string)
        continue;
      }
    }

    // State 2: wait for the second part of the beginning of a new problem string (# after the lower-case L)
    if (state == 2)
    {
      if (c == '#')
      {
        state = 3; // Switch to state 3
        continue;
      }
    }

    // State 3: store hold descriptions in problem string
    if (state == 3)
    {
      if (c == '#')
      {            // problem string ends with #
        state = 4; // Switch to state 4 (start parsing and show LEDs)
        continue;
      }
      problemstring.concat(c); // add current character to problem string
    }
  }

  // State 4: complete problem string received, start parsing
  if (state == 4)
  {
    std::map<char, std::pair<RgbColor, String>> holdcolors = {
        {'S', std::make_pair(green, " (green)")},
        {'R', std::make_pair(blue, " (blue)")},
        {'L', std::make_pair(violet, " (violet)")},
        {'M', std::make_pair(pink, " (pink)")},
        {'F', std::make_pair(cyan, " (cyan)")},
        {'E', std::make_pair(red, " (red)")}};

    strip.ClearTo(black); // Turn off all LEDs in LED string
    Serial.println("\n---------");
    Serial.print("Problem string: ");
    Serial.println(problemstring);
    Serial.println("");

    String problemstringstore = problemstring; // store copy of problem string

    if (useadditionalled)
    { // only render additional LEDs in first loop
      Serial.println("Additional LEDs:");
      int pos = 0;
      while (pos >= 0)
      {
        pos = problemstring.indexOf(','); // Hold descriptions are separated by a comma (,)

        String hold = pos > 0 ? problemstring.substring(0, pos) : problemstring;

        char holdtype = hold.charAt(0);             // Hold descriptions consist of a hold type (S, P, E) ...
        int holdnumber = hold.substring(1).toInt(); // ... and a hold number
        int lednumber = ledmapping[holdnumber];
        int additionallednumber = lednumber + additionalledmapping[holdnumber];

        if (additionalledmapping[holdnumber] != 0)
        {
          Serial.print(holdtype);
          Serial.print(holdnumber);
          Serial.print(" --> ");
          Serial.print(additionallednumber);

          // Right, left, match, start, or foot hold
          if (holdtype != 'E')
          {
            strip.SetPixelColor(additionallednumber, yellow);
            Serial.println(" (yellow)");
          }
          // Finish holds don't get an additional LED!
        }

        problemstring = problemstring.substring(pos + 1, problemstring.length()); // Remove processed hold from string
      }
      Serial.println("");
      problemstring = problemstringstore; // Restore problem string for rendering normal hold LEDs
    }

    Serial.println("Problem LEDs:");
    int pos = 0;
    while (pos >= 0)
    {                                   // render all normal LEDs (possibly overriding additional LEDs)
      pos = problemstring.indexOf(','); // Hold descriptions are separated by a comma (,)

      // String hold;
      // if (pos > 0)
      // {                                         // Still holds left in the problem string
      //   hold = problemstring.substring(0, pos); // Extract one hold description
      // }
      // else
      // { // Last hold in the problem string
      //   hold = problemstring;
      // }

      String hold = pos > 0 ? problemstring.substring(0, pos) : problemstring; // Extract one hold description

      char holdtype = hold.charAt(0);             // Hold descriptions consist of a hold type (S, P, E) ...
      int holdnumber = hold.substring(1).toInt(); // ... and a hold number
      int lednumber = ledmapping[holdnumber];

      Serial.print(holdtype);
      Serial.print(holdnumber);
      Serial.print(" --> ");
      Serial.print(lednumber);

      strip.SetPixelColor(lednumber, holdcolors[holdtype].first);
      Serial.print(holdcolors[holdtype].second);

      // if (holdtype == 'S')
      // { // Start hold
      //   strip.SetPixelColor(lednumber, green);
      //   Serial.println(" (green)");
      // }
      // if (holdtype == 'R')
      // { // Right hold
      //   strip.SetPixelColor(lednumber, blue);
      //   Serial.println(" (blue)");
      // }
      // if (holdtype == 'L')
      // { // Left hold
      //   strip.SetPixelColor(lednumber, violet);
      //   Serial.println(" (violet)");
      // }
      // if (holdtype == 'M')
      // { // Match hold
      //   strip.SetPixelColor(lednumber, pink);
      //   Serial.println(" (pink)");
      // }
      // if (holdtype == 'F')
      // { // Foot hold
      //   strip.SetPixelColor(lednumber, cyan);
      //   Serial.println(" (cyan)");
      // }
      // if (holdtype == 'E')
      // { // End hold
      //   strip.SetPixelColor(lednumber, red);
      //   Serial.println(" (red)");
      // }

      problemstring = problemstring.substring(pos + 1, problemstring.length()); // Remove processed hold from string
    }

    strip.Show();             // Light up all hold (and additional) LEDs
    problemstring = "";       // Reset problem string
    useadditionalled = false; // Reset additional LED option
    state = 0;                // Switch to state 0 (wait for new problem string or configuration)
    Serial.println("---------\n");
  }
}
