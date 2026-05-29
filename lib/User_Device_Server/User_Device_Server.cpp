#include <WebServer_ESP32_W5500.h>
#include "User_Device_Server.h"

WiFiClient client_cpp;



DeviceRemote::DeviceRemote()
{
  _NOP();
}

DeviceRemote::~DeviceRemote()
{
  _NOP();
}


// void DeviceRemote::Read()
// {

// }

// void DeviceRemote::Send()
// {
        
// }

// void DeviceRemote::SendRead()
// {
        
// }


// IPAddress server_MX_2SP4T_0018 (192, 168, 2, 40);
// uint16_t port_MX_2SP4T_0018 = 5000;

// extern unsigned long   lastConnectionTime = 0;         // last time you connected to the server, in milliseconds
// extern const unsigned long postingInterval = 3000L; // delay between updates, in milliseconds
// extern WiFiClient client;


String DeviceRemote::send_read_IDN(String send_mess)
{
  String str;

  Serial.begin(115200);
  Serial.print("Тест старт\n\t");

  //if (client.connect(server_MX_2SP4T_0018, port_MX_2SP4T_0018))
  if (client_cpp.connect(server_MX_2SP4T_0018, port_MX_2SP4T_0018))
  {
    Serial.print("Тест старт внутри\n\t");
    client_cpp.print(send_mess);
    str = client_cpp.readString();
    Serial.print(str);
    Serial.print(str);
    Serial.print(str);
    lastConnectionTime = millis();
    client_cpp.stop();
  }
  else
  {    
    Serial.print(F("Connection failed \n\t"));
    str = "Error read";
  }
  return str;
}





// String send_read_MX(String send_mess)
// {
//   String str;
//   Serial.print("Тест старт\n\t");

//    if (client.connect(server_MX_2SP4T_0018, port_MX_2SP4T_0018))
//   {
//     Serial.print("Тест старт внутри\n\t");
//     client.print(send_mess);
//     str = client.readString();
//     Serial.print(str);
//     Serial.print(str);
//     Serial.print(str);
//     lastConnectionTime = millis();
//     client.stop();
//   }
//   else
//   {    
//     Serial.print(F("Connection failed \n\t"));
//     str = "Error read";
//   }
//   return str;
// }

 DeviceRemote DEV;