#pragma once

#include <jsoncpp/json/json.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>


class PigeonData{


public:

    PigeonData(const std::string& path);
    bool ReadConfig();

    std::unordered_map<std::string, Json::Value> GetData() const{
        return m_data;
    }

private:

    std::unordered_map<std::string, Json::Value> m_data = {};
    std::string m_path = "";

};




