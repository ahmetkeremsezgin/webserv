#include "Config.hpp"

Config::Config() {
    servers.resize(1);

    servers[0].interface = "localhost";
    servers[0].port = 9090;
    servers[0].max_byte = 1000;
    
    servers[0].errorPages.resize(2);
    servers[0].errorPages[0].code = 404;
    servers[0].errorPages[0].path = "/tmp/www/error-pages/404.html";
    servers[0].errorPages[1].code = 400;
    servers[0].errorPages[1].path = "/tmp/www/error-pages/400.html";

    servers[0].locations.resize(2);
    servers[0].locations[0].allowedMethods.resize(2);
    servers[0].locations[0].url = "/ana-sayfa";
    servers[0].locations[0].allowedMethods[0] = "GET";
    servers[0].locations[0].autoindex = true;
    servers[0].locations[0].index_path = "index.html";
    servers[0].locations[0].max_byte = 100;
    servers[0].locations[0].path = "/tmp/www/ana-sayfa";
    servers[0].locations[0].redirect = "";
    servers[0].locations[0].upload = false;
    servers[0].locations[0].upload_path = "/tmp/www/uploads";

    servers[0].locations[1].allowedMethods.resize(3);
    servers[0].locations[1].url = "/galeri";
    servers[0].locations[1].allowedMethods[0] = "GET";
    servers[0].locations[1].allowedMethods[1] = "POST";
    servers[0].locations[1].allowedMethods[2] = "DELETE";
    servers[0].locations[1].autoindex = false;
    servers[0].locations[1].index_path = "galeri.html";
    servers[0].locations[1].max_byte = 10000;
    servers[0].locations[1].path = "/tmp/www/galeri";
    servers[0].locations[1].redirect = "";
    servers[0].locations[1].upload = true;
    servers[0].locations[1].upload_path = "/tmp/www/uploads";

    servers[0].locations.resize(3); 
    
    servers[0].locations[2].url = "/uploads";                   
    servers[0].locations[2].path = "/tmp/www/uploads";          
    
    servers[0].locations[2].allowedMethods.resize(2);           
    servers[0].locations[2].allowedMethods[0] = "GET";          
    servers[0].locations[2].allowedMethods[1] = "DELETE";       
    
    servers[0].locations[2].autoindex = true;                   
    servers[0].locations[2].index_path = "";
    servers[0].locations[2].upload = false;                     
}