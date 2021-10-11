#include <Arduino.h>
namespace LEDService
{
    void beginPixels();
    void beginServer();
    void listen();
    String getHomepage();
    String getHTTPOK();
    String getHTTPNotOK();
    void fancy();
    void showError();
}
