#include "PigeonData.h"

PigeonData::PigeonData(const std::string& path): m_path(path){
    if(!ReadConfig())
        throw std::string("Error while reading config file");
}


bool PigeonData::ReadConfig(){

    Json::Value root;
    Json::CharReaderBuilder builder;

    if(m_path.empty())
        return false;
     
    std::ifstream file(m_path);  
    
    if(!file)
        return false;

    
    bool parsed = Json::parseFromStream(builder, file, &root, nullptr);
    
    if(!parsed)
        return false;

    for (const auto& key : root.getMemberNames()) {
        m_data[key] = root[key];
    }
    return true;
}