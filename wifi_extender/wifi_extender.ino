/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Create a WiFi access point and provide a web server on it.
  allow change of extenders name
  allow change of connected network

*/

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

AsyncWebServer server(80);
DynamicJsonDocument Config(2048);




/* Set these to your desired credentials. */

#if LWIP_FEATURES && !LWIP_IPV6

#define HAVE_NETDUMP 0

#ifndef STASSID
#define STASSID "Anurella"
#define STAPSK  "G15dislove."
#endif

#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <LwipDhcpServer.h>

#define NAPT 1000
#define NAPT_PORT 10

#if HAVE_NETDUMP

#include <NetDump.h>

void dump(int netif_idx, const char* data, size_t len, int out, int success) {
  (void)success;
  Serial.print(out ? F("out ") : F(" in "));
  Serial.printf("%d ", netif_idx);

  // optional filter example: if (netDump_is_ARP(data))
  {
    netDump(Serial, data, len);
    //netDumpHex(Serial, data, len);
  }
}
#endif


class wifi {

  public:

  JsonObject obj = Config.as<JsonObject>();

    // just added this so i can see the files in the file system
    void listDir(const char * dirname) {
      Serial.printf("Listing directory: %s\n", dirname);

      Dir root = LittleFS.openDir(dirname);

      while (root.next()) {
        File file = root.openFile("r");
        Serial.print("  FILE: ");
        Serial.print(root.fileName());
        Serial.print("  SIZE: ");
        Serial.print(file.size());
        file.close();

      }
    }

    void create_server() {

      server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {

        // scan for networks and get ssid
        String network_html = "";


        // link to code https://github.com/me-no-dev/ESPAsyncWebServer/issues/85#issuecomment-258603281
        // there is a problem with scanning whil using espasync that is why the code below is not just WiFi.scan()
        int n = WiFi.scanComplete();
        if (n == -2) {
          WiFi.scanNetworks(true);
        } else if (n) {
          for (int i = 0; i < n; ++i) {
            String router = WiFi.SSID(i);
            Serial.println(router);
            network_html += "<input type=\"radio\" id=\"html\" name=\"ssid\" value=" + router + " required ><label for=\"html\">" + router + "</label><br>";

          }
          WiFi.scanDelete();
          if (WiFi.scanComplete() == -2) {
            WiFi.scanNetworks(true);
          }
        }
        //        for (int i = 0; i < n; i++) {
        //          String router = WiFi.SSID(i);
        //          Serial.println(router);
        //          network_html += "<input type=\"radio\" id=\"html\" name=\"ssid\" value=" + router + " required ><label for=\"html\">" + router + "</label><br>";
        //        }

        String html = "<!DOCTYPE html><html>";
        html += "<body>";
        html += "<h1>Pius Electronics Extender Config page</h1>";
        html += "<p>networks found </p>";
        html += "<form action=\"/credentials\">";
        html += "<p>Please select a WiFi network:</p>" + network_html;
        html += "<input type=\"password\" id=\"pass\" name=\"pass\" value=\"password\" required ><label for=\"pass\">password</label><br><br>";
        html += "<input type=\"text\" id=\"ap\" name=\"ap\" value=\"PiusElectronics Extender\" required ><label for=\"ap\">A.P name:</label><br>";
        html += "<input type=\"submit\" value=\"Submit\">";
        html += "</form></body></html>";

        request->send(200, "text/html", html);
      });

      // Send a GET request to <IP>/get?message=<message>
      server.on("/credentials", HTTP_GET, [] (AsyncWebServerRequest * request) {
        String param = "ssid";
        
        if (request->hasParam(param)) {
          String ssid = request->getParam(param)->value();
          Config["ssid"] = ssid;
          Serial.println(ssid);
        } else {
          Serial.println("No " + param + " sent");
        }

        param = "pass";
        if (request->hasParam(param)) {
          String pass = request->getParam(param)->value();
          Config["pass"] = pass;
          Serial.println(pass);
        } else {
          Serial.println("No " + param + " sent");
        }

        param = "ap";
        if (request->hasParam(param)) {
          String ap = request->getParam(param)->value();
          Config["ap"] = ap;
          Serial.println(ap);
        } else {
          Serial.println("No " + param + " sent");
        }
        String output;
        serializeJson(Config, output);
        Serial.print(output);

        String path = "/config.json";

        Serial.printf("Writing file: %s\n", path);

        File file = LittleFS.open(path, "w");
        if (!file) {
          Serial.println("Failed to open file for writing");
          return;
        }
        if (file.print(output)) {
          Serial.println("File written");
        } else {
          Serial.println("Write failed");
        }
        file.close();

      });



    }


    String* get_credentials() {
      String path = "/config.json";
      String credentials = "";
    
      Serial.print("reading file ");
      Serial.println(path);

      File file = LittleFS.open(path, "r");
      if (!file) {
        Serial.println("Failed to open file for reading");
        return {};
      }

      Serial.print("Read from file: ");
      while (file.available()) {
        credentials += file.readString();

      }
      Serial.println(credentials);
      deserializeJson(Config,credentials);
      file.close();
     
        String credential[3]= {Config["ssid"],Config["pass"],Config["ap"]};
      return credential;
    }




};

wifi my_wifi;

const char *ssid = STASSID;
const char *password = STAPSK;






void setup() {
  delay(1000);

  Serial.begin(115200);

  Serial.println();


  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  Serial.printf("\n\nNAPT Range extender\n");
  Serial.printf("Heap on start: %d\n", ESP.getFreeHeap());

#if HAVE_NETDUMP
  phy_capture = dump;
#endif

  // first, connect to STA so we can get a proper local DNS server
  Serial.println(my_wifi.get_credentials()[0]);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.printf("\nSTA: %s (dns: %s / %s)\n",
                WiFi.localIP().toString().c_str(),
                WiFi.dnsIP(0).toString().c_str(),
                WiFi.dnsIP(1).toString().c_str());

  // give DNS servers to AP side
  dhcpSoftAP.dhcps_set_dns(0, WiFi.dnsIP(0));
  dhcpSoftAP.dhcps_set_dns(1, WiFi.dnsIP(1));

  WiFi.softAPConfig(  // enable AP, with android-compatible google domain
    IPAddress(172, 217, 28, 254),
    IPAddress(172, 217, 28, 254),
    IPAddress(255, 255, 255, 0));
  WiFi.softAP(STASSID "extender", STAPSK);
  Serial.printf("AP: %s\n", WiFi.softAPIP().toString().c_str());

  Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
  err_t ret = ip_napt_init(NAPT, NAPT_PORT);
  Serial.printf("ip_napt_init(%d,%d): ret=%d (OK=%d)\n", NAPT, NAPT_PORT, (int)ret, (int)ERR_OK);
  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
    Serial.printf("ip_napt_enable_no(SOFTAP_IF): ret=%d (OK=%d)\n", (int)ret, (int)ERR_OK);
    if (ret == ERR_OK) {
      Serial.printf("WiFi Network '%s' with same password is now NATed behind '%s'\n", STASSID "extender", STASSID);
    }
  }
  Serial.printf("Heap after napt init: %d\n", ESP.getFreeHeap());
  if (ret != ERR_OK) {
    Serial.printf("NAPT initialization failed\n");
  }

}

#else

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nNAPT not supported in this configuration\n");
  my_wifi.create_server();
  server.begin();
  Serial.println("HTTP server started");

}

#endif

void loop() {
}