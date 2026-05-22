#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "Config.hpp"

class Client {
private:
    int         _fd;                // İstemcinin soket dosya tanımlayıcısı
    Server      _serverConfig;      // İstemcinin bağlandığı portun Server ayarları
    
    std::string _requestBuffer;     // Soketten okunan ham verinin biriktiği yer
    std::string _responseBuffer;    // Sokete yazılacak (gönderilecek) nihai yanıt
    
    bool        _isRequestReady;    // İstek tamamen okundu mu? (\r\n\r\n geldi mi?)
    bool        _isResponseReady;   // Yanıt oluşturuldu ve gönderilmeye hazır mı?

public:
    Client(int fd, const Server& config);
    ~Client();

    // Getter'lar
    int getFd() const;
    bool isRequestReady() const;
    bool isResponseReady() const;

    // İşlem fonksiyonları
    bool readData();                // Server.cpp'deki read() burada çağrılacak
    bool sendData();                // Server.cpp'deki send() burada çağrılacak
    
    void processRequest();          // HandleClient mantığının geleceği yer
};

#endif