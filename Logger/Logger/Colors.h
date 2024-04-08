#pragma once

#define RST  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define FRED(x) KRED x RST
#define FGRN(x) KGRN x RST
#define FYEL(x) KYEL x RST
#define FBLU(x) KBLU x RST
#define FMAG(x) KMAG x RST
#define FCYN(x) KCYN x RST
#define FWHT(x) KWHT x RST


#define BGRED  "\x1B[41m" 
#define BGGRN  "\x1B[42m" 
#define BGYEL  "\x1B[43m" 
#define BGBLU  "\x1B[44m" 
#define BGMAG  "\x1B[45m" 
#define BGCYN  "\x1B[46m" 
#define BGWHT  "\x1B[47m" 
#define BGBLK  "\x1B[40m" 

inline std::string Color(const std::string& x, const std::string& textColor, const std::string& bgColor) {
    return textColor + bgColor + x + RST;
}

inline std::string Bold(const std::string& x) { return "\x1B[1m" + x + RST; }