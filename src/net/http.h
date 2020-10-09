#ifndef _http_H_
#define _http_H_

#include <ESPAsyncWebServer.h>
#include <functional>

class HTTPServer {
 public:
  HTTPServer(std::function<void()> reset_device);
  ~HTTPServer() { delete server; }
  void enable() { server->begin(); }
  void handle_not_found(AsyncWebServerRequest* request);
  void handle_config(AsyncWebServerRequest* request);
  void handle_device_reset(AsyncWebServerRequest* request);
  void handle_device_restart(AsyncWebServerRequest* request);
  void handle_info(AsyncWebServerRequest* request);
  void add_http_handler(String relativeUri, WebRequestMethodComposite method, std::function<void(AsyncWebServerRequest*)> handler)
  {
    if(this->server != NULL)
    {
      server->on(relativeUri.c_str(), method, handler);
    }
  }

 private:
  AsyncWebServer* server;
  std::function<void()> reset_device;
  void handle_config_list(AsyncWebServerRequest* request);
};

#endif
