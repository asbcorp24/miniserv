#ifndef USER_DEVICE_SERVER_H_
#define USER_DEVICE_SERVER_H_

#include <WebServer_ESP32_W5500.h>

unsigned long   lastConnectionTime = 0;         // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 3000L; // delay between updates, in milliseconds


IPAddress server_FSW26 (192, 168, 2, 150);
uint16_t port_FSW26 = 5025;

IPAddress server_MSO_X_4054A (192, 168, 2, 30);
uint16_t port_MSO_X_4054A = 5025;

IPAddress server_PRF7100L (192, 168, 2, 111);
uint16_t port_PRF7100L = 2268;

IPAddress server_MX_2SP4T_0018 (192, 168, 2, 40);
uint16_t port_MX_2SP4T_0018 = 5000;

IPAddress server_MX_2SP6T_0018 (192, 168, 2, 42);
uint16_t port_MX_2SP6T_0018 = 5000;


class DeviceRemote
{  
        public: 

	        DeviceRemote();
                ~DeviceRemote();              

                // void Send();
                // void Read();
                // void SendRead();
                String send_read_IDN(String send_mess);
};



#endif //USER_DEVICE_SERVER_H_