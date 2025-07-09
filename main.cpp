#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <unordered_map>

using namespace std;
namespace fs = std::filesystem;

vector<char> symbolData;

int numFiles = 0;
int numFilesComplete = 0;

// data used to read CSV files
vector<string> filesToRead;
string fileAddress = "";
string fileLine = "";
int fileLineIndex = 0;
int fileLineLength = 0;
int fileLineSpot = 0;
int fileLineSpot1 = 0;

// date indices that have not been filled
#define DUMMY_INT INT_MIN
// empty prices, entries[] and exits[] values
#define DUMMY_DOUBLE (double)INT_MIN

string columnCodes = "";

#define maxNumSymbols 20000 // with about 12 arrays of size maxNumSymbols * maxNumDates, this product can be at most ~10000000 (resulting in 960M bytes)
#define maxNumDates 1400

double open[maxNumSymbols][maxNumDates];
double high[maxNumSymbols][maxNumDates];
double low[maxNumSymbols][maxNumDates];
double close[maxNumSymbols][maxNumDates];
double volume[maxNumSymbols][maxNumDates];
double gap[maxNumSymbols][maxNumDates];
double change[maxNumSymbols][maxNumDates];
double totalGap[maxNumDates];
double totalChange[maxNumDates];
double avgGap[maxNumDates];
double avgChange[maxNumDates];


// number of possible values for the variables when minutewise backtesting
#define numTrialsFirst 10
#define numTrialsSecond 10
#define numTrialsThird 1

const int numTrialsTotal = numTrialsFirst * numTrialsSecond * numTrialsThird;

// trade entry price and time for each strategy, dummy when not in trade
double entryPrices[maxNumSymbols][numTrialsTotal];
int entryDaysSinceOpen[maxNumSymbols][numTrialsTotal];

// file parsing info
long long numBarsTotal = 0;
int numBars[maxNumSymbols];
int currentSymbolIndex = 0;
string currentSymbol = "";
long long currentDateTime = 0;
int currentDate = 0;
int currentDateIndex = 0;
int currentTime = 0;
int currentTimeIndex = 0;
vector<double> currentOHLCV = {0.0,0.0,0.0,0.0,0.0};


// date to index and back mapping
int numDates = 0;
unordered_map<int, int> date_index;
unordered_map<int, int> index_date;

// symbol to index and back mapping (all symbols in dataset)
int numSymbols = 0;
unordered_map<string, int> symbol_index;
unordered_map<int, string> index_symbol;

// number of symbols found in each date of the dataset
int numBarsByDateIndex[maxNumDates];
// values for each day and symbol used to determine trade entry and exit values
unordered_map<string, int> symbol_index_day[maxNumDates];
unordered_map<int, string> index_symbol_day[maxNumDates];

// values for each day and symbol (only ones with all times within that day and symbol filled) used to determine trade entry and exit values
unordered_map<string, int> symbol_index_sorted[maxNumDates];
unordered_map<int, string> index_symbol_sorted[maxNumDates];

// number of bars in each date AFTER FILLING PRICE HOLES
int numAvailableSymbolsDate[maxNumDates];

int firstDateAll = 0;
int lastDateAll = 0;

// first and last date index recorded for each symbol's data
vector<int> firstDate(maxNumSymbols, DUMMY_INT);
vector<int> lastDate(maxNumSymbols, DUMMY_INT);
vector<double> lastPrice(maxNumSymbols, DUMMY_DOUBLE);

// input (entries) and output (exits) data of trades
vector<vector<double>> entries;
vector<vector<double>> exits;

// global backtesting settings
int btStartingDateIndex = 0;
int btEndingDateIndex = 0;
int btMinSymbolRankDay = 0;
int btMaxSymbolRankDay = 9;
long long dwbtMinPreviousVolume = 0;
long long dwbtMaxPreviousVolume = 0;
int btPreviousVolumeLookBackLength = 0;
int mwbtEarliestTimeToTrade = 930;
int mwbtLatestTimeToTrade = 1559;
bool mwbtUseTradingTimes = false;
double btLeverage = 1.0;
bool btDisregardFilters = false;
double btMinOutlier = 0.0;
double btMaxOutlier = 0.0;
bool btIgnoreOutliers = true;
bool btPrintOutliers = true;
bool btPrintEntries = true;
bool btPrintAllResults = true;
bool btPrintDetailedResults = true;
bool btPrintLoading = true;
int btPrintLoadingInterval = 1;
int btLoadingBarWidth = 60;
bool btPrintSummary = true;

// outcome stats with respect to # of symbols traded each day
double btResult = 0.0;

// backtesting stats for different numbers of symbols traded per day
vector<int> btTrades;
vector<int> btWins;
vector<int> btLosses;
vector<int> btTies;
vector<double> btWon;
vector<double> btLost;
vector<double> btBalance;
vector<double> btVar;
vector<double> btSD;
vector<int> btNumOutliers;


long long power(int b, int e) {
    if (e < 0) return 0;
    if (e == 0) return 1;
    long long n = b;
    for (int i = 1; i < e; i++) {
        n *= (long long)b;
    }
    return n;
}

bool isNumeric(char c) {
    return (c >= '0' && c <= '9');
}

bool isAlpha(char c) {
    return (c >= '0' && c <= '9');
}

string dateToString(int d) {
    if(d < 20000000 || d > 20291231){
        cerr << "Date " << d << " is out of the range 20000000 to 20291231.\n\n";
        exit(1);
    }
    return to_string(d);
}

// parse a ticker symbol within a CSV file line
string parseSymbol(){
    string ticker = "";
    fileLineSpot1 = fileLineSpot;
    while (fileLine[fileLineSpot1] != ',') {
        fileLineSpot1++;
        if(fileLineSpot1 >= fileLineLength){
            break;
        }
    }
    if(fileLineSpot1 - fileLineSpot < 1 || fileLineSpot1 - fileLineSpot > 10){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a ticker symbol of length " << fileLineSpot1 - fileLineSpot << ", but the length must be from 1 through 10.\n\n";
        exit(1);
    }
    return fileLine.substr(fileLineSpot, fileLineSpot1 - fileLineSpot);
}

// parse a time within a CSV file line
int parseTime(){
    
    if(fileLineSpot + 4 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 5-character time value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 4]) && fileLine[fileLineSpot + 2] == ':')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct time format (HH:MM) for the moment value.\n\n";
        exit(1);
    }

    // time
    int time = (fileLine[fileLineSpot] - '0') * 1000 + (fileLine[fileLineSpot + 1] - '0') * 100 + (fileLine[fileLineSpot + 2] - '0') * 10 + (fileLine[fileLineSpot + 3] - '0');
    if (time < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time before 00:00.\n\n";
        exit(1);
    }
    if (time >= 2400) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time after 23:59.\n\n";
        exit(1);
    }
    
    return time;
}

// parse a date within a CSV file line
int parseDate(){
    
    if(fileLineSpot + 9 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 10-character date value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 2]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 5]) && isNumeric(fileLine[fileLineSpot + 6]) && isNumeric(fileLine[fileLineSpot + 8]) && isNumeric(fileLine[fileLineSpot + 9]) && fileLine[fileLineSpot + 4] == '-' && fileLine[fileLineSpot + 7] == '-')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct date format (YYYY-MM-DD) for the moment value.\n\n";
        exit(1);
    }

    // date
    int date = (fileLine[fileLineSpot + 2] - '0') * 100000 + (fileLine[fileLineSpot + 3] - '0') * 10000 + (fileLine[fileLineSpot + 5] - '0') * 1000 + (fileLine[fileLineSpot + 6] - '0') * 100 + (fileLine[fileLineSpot + 8] - '0') * 10 + (fileLine[fileLineSpot + 9] - '0');
    if (date < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date before 2000/01/01.\n\n";
        exit(1);
    }
    if (date >= 11160) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date after 2030/01/01.\n\n";
        exit(1);
    }
    
    return date;
}

// parse a combined date and time within a CSV file line
long long parseMoment(){
    
    if(fileLineSpot + 15 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 16-character moment value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 2]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 5]) && isNumeric(fileLine[fileLineSpot + 6]) && isNumeric(fileLine[fileLineSpot + 8]) && isNumeric(fileLine[fileLineSpot + 9]) && isNumeric(fileLine[fileLineSpot + 11]) && isNumeric(fileLine[fileLineSpot + 12]) && isNumeric(fileLine[fileLineSpot + 14]) && isNumeric(fileLine[fileLineSpot + 15]) && fileLine[fileLineSpot + 4] == '-' && fileLine[fileLineSpot + 7] == '-')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct date and time format (YYYY-MM-DD HH:MM:SS or YYYY-MM-DD HH:MM) for the moment value.\n\n";
        exit(1);
    }
    

    // date
    int date = (fileLine[fileLineSpot + 2] - '0') * 100000 + (fileLine[fileLineSpot + 3] - '0') * 10000 + (fileLine[fileLineSpot + 5] - '0') * 1000 + (fileLine[fileLineSpot + 6] - '0') * 100 + (fileLine[fileLineSpot + 8] - '0') * 10 + (fileLine[fileLineSpot + 9] - '0');
    if (date < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date before 2000/01/01.\n\n";
        exit(1);
    }
    if (date >= 11160) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date after 2030/01/01.\n\n";
        exit(1);
    }


    // time
    int time = (fileLine[fileLineSpot + 11] - '0') * 1000 + (fileLine[fileLineSpot + 12] - '0') * 100 + (fileLine[fileLineSpot + 14] - '0') * 10 + (fileLine[fileLineSpot + 15] - '0');
    if (time < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time before 00:00.\n\n";
        exit(1);
    }
    if (time >= 2400) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time after 23:59.\n\n";
        exit(1);
    }

    return date * 10000 + time;
}

// parse a Unix timestamp within a CSV file line
long long parseUnix(){

    while(fileLine[fileLineSpot1] != ','){
        if (!(isNumeric(fileLine[fileLineSpot1]))) {
            cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. It must contain only digits.\n\n";
            exit(1);
        }

        fileLineSpot1++;
        if (fileLineSpot1 >= fileLineLength) {
            break;
        }
    }

    if(fileLineSpot1 - fileLineSpot < 18 || fileLineSpot1 - fileLineSpot > 19){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. The value must be 18 or 19 digits long.\n\n";
        exit(1);
    }

    long long unix = 0;

    for(int i=fileLineSpot;i<fileLineSpot1-9;i++){
        unix *= 10;
        unix += fileLine[i] - '0';
    }
    
    if(unix < 946702800 || unix >= 1893474000){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. The value (excluding the last 9 digits) must be between 946702800 (Jan 1, 2000, 12:00 AM EST) and 1893474000 (Jan 1, 2030, 12:00 AM EST). It is " << unix << ".\n\n";
        exit(1);
    }

    /* Central Time:  vector<int> yearStarts = {946706400, 978328800, 1009864800, 1041400800, 1072936800, 1104559200, 1136095200, 1167631200, 1199167200, 1230789600, 1262325600,
    1293861600, 1325397600, 1357020000, 1388556000, 1420092000, 1451628000, 1483250400, 1514786400, 1546322400, 1577858400, 1609480800, 1641016800, 1672552800,
    1704088800, 1735711200, 1767247200, 1798783200, 1830319200, 1861941600, 1893477600};
    */
    /* vector<int> yearStarts = {946702800, 978325200, 1009861200, 1041397200, 1072933200, 1104555600, 1136091600, 1167627600, 1199163600, 1230786000, 1262322000,
    1293858000, 1325394000, 1357016400, 1388552400, 1420088400, 1451624400, 1483246800, 1514782800, 1546318800, 1577854800, 1609477200, 1641013200, 1672549200,
    1704085200, 1735707600, 1767243600, 1798779600, 1830315600, 1861938000, 1893474000};
    */
    // Above are the true Unix timestamps for the year starts, but due to Daylight Saving Time shifting the start of each
    // day by 1H for 9M/year, we use the below starts which result in accurate time of day calculations for those 9M.
    // It's better to have the day division (as calculated in this program) either actually be at
    // either 00:00 or 01:00 than either 23:00 or 00:00 as the latter misidentifies the day depending on the time of year.
    // It's also better to be correct 9M/12 than 3M/12.
    vector<int> yearStarts = {946699200, 978321600, 1009857600, 1041393600, 1072929600, 1104552000, 1136088000, 1167624000, 1199160000, 1230782400, 1262318400,
    1293854400, 1325390400, 1357012800, 1388548800, 1420084800, 1451620800, 1483243200, 1514779200, 1546315200, 1577851200, 1609473600, 1641009600, 1672545600,
    1704081600, 1735704000, 1767240000, 1798776000, 1830312000, 1861934400, 1893470400};
    
    vector<int> monthDays = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

    long long dateTime = 20000000;
    int yearStart = 946699200;
    
    // convert to date and time (year, month, day are 0-indexed) example output: 202503160930
    int year = 0;
    for(int i=1;i<=30;i++){
        if(yearStarts[i] > unix){
            year = i - 1;
            yearStart = yearStarts[year];
            dateTime = 20000000 + year * 10000;
            break;
        }
    }
    
    int dayOfYear = (unix - yearStart) / 86400;
    int month = 0;
    int dayOfMonth = 0;
    bool leapDay = 0;
    if(year % 4 == 0){ // if it's a leap year (100-year rule does not matter because 2000 is a leap year and 1900/2100 aren't here)
        if(dayOfYear == 59){
            month = 2;
            dateTime += 229;
            dayOfMonth = 29;
            leapDay = 1;
        }else{
            if(dayOfYear > 59){
                dayOfYear--;
            }
        }
    }

    if(!leapDay){
        for(month = 1; month <= 12; month++){
            if(monthDays[month] > dayOfYear){
                dayOfMonth = dayOfYear - monthDays[month - 1] + 1;
                dateTime += month * 100 + dayOfMonth;
                break;
            }
        }
    }

    int time = ((unix - 14400) / 60) % 1440;

    time = (time / 60) * 100 + (time % 60);

    dateTime = dateTime * 10000 + time;
    
    return dateTime;
}

// parse an OHLCV value within a CSV file line
double parseOHLCV(char ohlcvCode){
    
    while (fileLine[fileLineSpot1] != ',' && fileLine[fileLineSpot1] != '.') {
        fileLineSpot1++;
        if (fileLineSpot1 >= fileLineLength) {
            break;
        }
    }
    double num = 0.0;
    // get digits before .
    for (int j = fileLineSpot; j < fileLineSpot1; j++) {
        num += (double)((fileLine[j] - '0') * (int)power(10, fileLineSpot1 - j - 1));
    }
    if (ohlcvCode == 'V') {
        return num;
    }
    
    // get all digits after .
    if (fileLine[fileLineSpot1] == '.') {
        double power = 0.1;
        while(1){
            fileLineSpot1++;
            if(fileLineSpot1 >= fileLineLength) {
            return num;}
            if(!isNumeric(fileLine[fileLineSpot1])){
            return num;}
            num += (double)(fileLine[fileLineSpot1] - '0') * power;
            power *= 0.1;
        }
    }

    return num;
}

void checkColumnCodes(){
    int numS = 0, numM = 0, numD = 0, numT = 0, numO = 0, numH = 0, numL = 0, numC = 0, numV = 0;
    int n = size(columnCodes);
    if(n < 3 || n > 20){
        cerr << "The length of the CSV column code must be from 3 to 20 characters long. It is " << n << "characters long.\n\n";
        exit(1);
    }
    for(int i=0;i<n;i++){
        switch(columnCodes[i]){
            case 'S':
                numS++;
                break;
            case 'M':
            case 'U':
                numM++;
                break;
            case 'D':
                numD++;
                break;
            case 'T':
                numT++;
                break;
            case 'O':
                numO++;
                break;
            case 'H':
                numH++;
                break;
            case 'L':
                numL++;
                break;
            case 'C':
                numC++;
                break;
            case 'V':
                numV++;
                break;
            case '-':
                break;
            cerr << "The CSV column code may only contain the characters \"MDTSOHLCV-\".\n\n";
            exit(1);
        }
    }

    if(numS != 1){
        cerr << "The CSV column code must contain exactly one symbol value (S).\n\n";
        exit(1);
    }
    if(numM > 1 || numD > 1 || numT > 1){
        cerr << "The CSV column code must contain no more than one moment, Unix time, time, or date value (M/U/T/D).\n\n";
        exit(1);
    }
    if((numM != 1 && (numD != 1 || numT != 1)) || (numM == 1 && (numD == 1 || numT == 1))){
        cerr << "The CSV column code must contain exactly one moment or Unix time value, or one time and one date value (M/U/T/D).\n\n";
        exit(1);
    }
    if(numO != 1){
        cerr << "The CSV column code must contain exactly one open value (O).\n\n";
        exit(1);
    }
    if(numH != 1){
        cerr << "The CSV column code must contain exactly one high value (H).\n\n";
        exit(1);
    }
    if(numL != 1){
        cerr << "The CSV column code must contain exactly one low value (L).\n\n";
        exit(1);
    }
    if(numC != 1){
        cerr << "The CSV column code must contain exactly one close value (C).\n\n";
        exit(1);
    }
    if(numV != 1){
        cerr << "The CSV column code must contain exactly one volume value (V).\n\n";
        exit(1);
    }
}

void updateAfterBar(){
    // update the last price for this symbol with the last close recorded
    lastPrice[currentSymbolIndex] = currentOHLCV[3];

    // update the previous datetime recorded for this symbol
    lastDate[currentSymbolIndex] = currentDate;
        
    numBarsTotal++;
    numBars[currentSymbolIndex]++;
}

// helper function for reading dates
void addNewDate(){
    if (date_index.find(currentDate) != date_index.end()) {
        currentDateIndex = date_index.at(currentDate);
    }else{
        if(firstDateAll == DUMMY_INT){
            firstDateAll = currentDate;
        }
        lastDateAll = currentDate;
        
        currentDateIndex = numDates;
        date_index[currentDate] = numDates;
        index_date[numDates] = currentDate;
        numDates++;
        if(numDates > maxNumDates){
            cerr << "The number of dates was greater than the maximum number of dates allowed, " << maxNumDates << ". Increase maxNumDates.\n\n";
            exit(1);
        }
    }
}

// read one line
void readLine(int numValuesPerLine){
    fileLineIndex++;
    fileLineSpot = 0;
    fileLineSpot1 = 0;
    fileLineLength = fileLine.length();

    if (fileLineLength < 20) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " with a length of " << fileLineLength << "characters was incorrectly shorter than 20 characters long. Make sure it includes a date and time, OHLCV values, and a ticker symbol.\n\n";
        exit(1);
    }

    // reset currentTimeIndex so that it can be used to break line or keep reading
    currentTimeIndex = 0;

    // reset the data so we can tell if all price data has been filled on this line
    currentOHLCV[0] = DUMMY_DOUBLE;
    currentOHLCV[1] = DUMMY_DOUBLE;
    currentOHLCV[2] = DUMMY_DOUBLE;
    currentOHLCV[3] = DUMMY_DOUBLE;
    currentOHLCV[4] = DUMMY_DOUBLE;

    // get every value in the line, then afterwards organize the values
    for(int i=0;i<numValuesPerLine;i++){
    fileLineSpot1 = fileLineSpot;

    // get this value
    switch(columnCodes[i]){
        case 'S':{
        currentSymbol = parseSymbol();
        if (symbol_index.find(currentSymbol) != symbol_index.end()) {
            currentSymbolIndex = symbol_index.at(currentSymbol);
            }else{
                currentSymbolIndex = numSymbols;
                symbol_index[currentSymbol] = numSymbols;
                index_symbol[numSymbols] = currentSymbol;
                numSymbols++;
                if(numSymbols > maxNumSymbols){
                    cerr << "The number of symbols was greater than the maximum number of symbols allowed, " << maxNumSymbols << ". Increase maxNumSymbols.\n\n";
                    exit(1);
                }
            }
            break;
        }
        case 'M':{
            currentDateTime = parseMoment();
            currentDate = currentDateTime / 10000;
            addNewDate();
            break;
        }
        case 'U':{
            currentDateTime = parseUnix();
            currentDate = currentDateTime / 10000;
            addNewDate();
            break;
        }
        case 'D':{
            currentDate = parseDate();
            currentDateTime = currentDate * 10000 + currentTime;
            addNewDate();
            break;
        }
        case 'T':{
            currentTime = parseTime();
            currentDateTime = currentDate * 10000 + currentTime;
            break;
        }
        case 'O':{
            currentOHLCV[0] = parseOHLCV('O');
            break;
        }
        case 'H':{
            currentOHLCV[1] = parseOHLCV('H');
            break;
        }
        case 'L':{
            currentOHLCV[2] = parseOHLCV('L');
            break;
        }
        case 'C':{
            currentOHLCV[3] = parseOHLCV('C');
            break;
        }
        case 'V':{
            currentOHLCV[4] = parseOHLCV('V');
            break;
        }
    }

    // move to next comma if there is a next comma
    if(i < numValuesPerLine - 1){
        while (fileLine[fileLineSpot] != ',') {
            fileLineSpot++;
            if (fileLineSpot >= fileLineLength) {
                cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended without a comma after value " << i << ". According to the columnCodes you entered, there should be " << numValuesPerLine << " values and " << numValuesPerLine - 1 << " commas.\n\n";
                exit(1);
            }
        }
    }
    // move past it
    fileLineSpot++;
    }

    // if this isn't the first bar recorded of this symbol EVER (if it is, ascend/descend does not matter as there is no previous datetime to compare to)
    if(numBars[currentSymbolIndex] > 0){

        // handle out of ascending order exception
        if (currentDate <= lastDate[currentSymbolIndex]) {
            cerr << "Consecutively parsed bars " << lastDate[currentSymbolIndex] << " and " << currentDate << " of symbol " << index_symbol.at(currentSymbolIndex) << " were not recorded in ascending order as previous bars of this symbol and/or other symbols were.\n\n";
            exit(1);
        }
    }

    // record all data values for this symbol and date IF they have all been found
    for(int i=0;i<5;i++){
        if(currentOHLCV[i] == DUMMY_DOUBLE){
            cerr << "Price value on line  " << numBarsTotal << " (0-indexed) of all files, bar " << numBars[currentSymbolIndex] << " (0-indexed) of symbol " << index_symbol.at(currentSymbolIndex) << " in file " << fileAddress << " did not contain every needed price value (OHLCV).\n\n";
            exit(1);
        }
    }

    open[currentSymbolIndex][currentDateIndex] = currentOHLCV[0];
    high[currentSymbolIndex][currentDateIndex] = currentOHLCV[1];
    low[currentSymbolIndex][currentDateIndex] = currentOHLCV[2];
    close[currentSymbolIndex][currentDateIndex] = currentOHLCV[3];
    volume[currentSymbolIndex][currentDateIndex] = currentOHLCV[4];

    if(firstDate[currentSymbolIndex] == DUMMY_INT){
        firstDate[currentSymbolIndex] = currentDate;
    }
    lastDate[currentSymbolIndex] = currentDate;

    // update the map of symbols on this day if a new symbol is found
    int currentSymbolIndexDay = 0;
    if (symbol_index_day[currentDateIndex].find(currentSymbol) != symbol_index_day[currentDateIndex].end()) {
        currentSymbolIndexDay = symbol_index_day[currentDateIndex].at(currentSymbol);
    }else{
        currentSymbolIndexDay = numBarsByDateIndex[currentDateIndex];
        symbol_index_day[currentDateIndex][currentSymbol] = currentSymbolIndexDay;
        index_symbol_day[currentDateIndex][currentSymbolIndexDay] = currentSymbol;
        numBarsByDateIndex[currentDateIndex]++;
    }

    updateAfterBar();
}

// read one file
void readFile(){
    int numValuesPerLine = size(columnCodes);

    ifstream f(fileAddress);

    if (!f.is_open()) {
        cerr << "File " << fileAddress << " was found but could not be opened.\n\n";
        exit(1);
    }

    fileLine = "";

    getline(f, fileLine);

    fileLineIndex = -1;
    while (getline(f, fileLine)) {
        readLine(numValuesPerLine);
    }

    f.close();
}

void readAllFiles(string path) {
    cout << "Reading files in folder " << path << "... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    numFiles = 0;
    numFilesComplete = 0;
    bool readCustomFiles = numFiles > 0;
    if(readCustomFiles) numFiles = size(filesToRead);
    int numFilesTotal = 0;
    
    // get the number of files
    for (const auto& entry : fs::directory_iterator(path)){
        if(!readCustomFiles){
            numFiles++;
        }
        numFilesTotal++;
    }

    // read all files
    for (const auto& entry : fs::directory_iterator(path)){
        // print the loading bar or number of files complete depending on settings
        if(btPrintLoading){
            cout << "[";
            for(int i=1;i<=btLoadingBarWidth;i++){
                double pct = (double)numFilesComplete / (double)numFiles;
                if(pct >= (double)i / (double)btLoadingBarWidth){
                    cout << "=";
                }else{
                    cout << " ";
                }
            }
            double pct = (double)numFilesComplete / (double)numFiles;
            cout << "] " << (int)(pct * 10000.0) / 100 << "." << ((int)(pct * 10000.0) / 10) % 10 << (int)(pct * 10000.0) % 10 << "% (" << numFilesComplete << "/" << numFiles << ")\r";
            cout.flush();
        }else{
            if(numFilesComplete % btPrintLoadingInterval == 0){
                cout << numFilesComplete << "/" << numFiles << " files complete.\n";
            }
        }

        fileAddress = entry.path().string();
        if(readCustomFiles){
            for(int i=0;i<numFiles;i++){
                if(filesToRead[i] == fileAddress){
                    readFile();
                    break;
                }
            }
        }else{
            readFile();
        }
        numFilesComplete++;
    }

    if(btPrintLoading){
        cout << "[";
        for(int i=1;i<=btLoadingBarWidth;i++){
            cout << "=";
        }
        cout << "] 100.00% (" << numFiles << "/" << numFiles << ")\n";
    }

    cout << "Finished reading " << numFiles << " of " << numFilesTotal << " files in this folder.\n\n";
}

// get the indices within the sorted arrays corresponding to the user-provided starting and ending dates
void getDateIndices(int ds, int de) {
	if (ds >= lastDateAll) ds = lastDateAll;
	if (ds <= firstDateAll) ds = firstDateAll;
	if (de >= lastDateAll) de = lastDateAll;
	if (de <= firstDateAll) de = firstDateAll;
	while (date_index.find(ds) == date_index.end()){
		ds++;
	}
	while (date_index.find(de) == date_index.end()) {
		de++;
	}
	btStartingDateIndex = date_index.at(ds);
	btEndingDateIndex = date_index.at(de);
	if (btStartingDateIndex < 0) {
		printf("Starting date index was less than 0.\n");
		exit(1);
	}
	if (btStartingDateIndex >= numDates) {
		printf("Starting date index was greater than the number of dates present.\n");
		exit(1);
	}
	if (btEndingDateIndex < 0) {
		printf("Ending date index was less than 0.\n");
		exit(1);
	}
	if (btEndingDateIndex >= numDates) {
		printf("Ending date index was greater than the number of dates present.\n");
		exit(1);
	}
}

void setup() {
	int i = 0, j = 0;

    numDates = 0;
    numSymbols = 0;

	currentSymbol = -1;
	
    for (j = 0; j < maxNumDates; j++) {
        numBarsByDateIndex[j] = 0;
        totalChange[j] = 0.0;
        totalGap[j] = 0.0;
        avgChange[j] = 0.0;
        avgGap[j] = 0.0;
        numAvailableSymbolsDate[j] = 0;
    }

    numBarsTotal = 0;
	for (i = 0; i < maxNumSymbols; i++) {
        numBars[i] = 0;

		for (j = 0; j < maxNumDates; j++) {
            open[i][j] = DUMMY_DOUBLE;
            close[i][j] = DUMMY_DOUBLE;
            high[i][j] = DUMMY_DOUBLE;
            low[i][j] = DUMMY_DOUBLE;
            volume[i][j] = 0;
            change[i][j] = 0.0;
            gap[i][j] = 0.0;
		}
	}
}

void swap(double* a, int n, int s) {
	double pl = a[s];
	a[s] = a[n - 1 - s];
	a[n - 1 - s] = pl;
}

/* positive/negative/zero sort algorithm (no faster than regular bucket sort algorithm) (I didn't add in the loading bar to this code)
// sort a pair of arrays from highest to lowest: sort in[] while ordering out[] so that in[i] still corresponds to out[i]
void sortData(bool ascending) {
	
	double pl = 0.0;
	int i = 0, j = 0, k = 0, l = 0;
    entries.clear();
    exits.clear();

	// fill the arrays that determine trade entries and exits (depends on strategy)
    // this can be anything involving gaps, changes, individual ticker price movements, mas, etc.
	for (i = 0; i < numDates; i++) {
        vector<double> en;
        vector<double> ex;
		for (j = 0; j < numSymbols; j++) {
            if(open[j][i] == DUMMY_DOUBLE) continue;
            if(high[j][i] == DUMMY_DOUBLE) continue;
            if(low[j][i] == DUMMY_DOUBLE) continue;
            if(close[j][i] == DUMMY_DOUBLE) continue;
            if(volume[j][i] == DUMMY_DOUBLE) continue;
            
            // if any data has been recorded on this symbol and date
            en.push_back(gap[j][i]);
            ex.push_back(change[j][i]);
		}
        entries.push_back(en);
        exits.push_back(ex);
	}
    
    bool swap = 0;

    // sort the data within every day of points[]
    double temp = 0.0;
    string temps = "";
    int numBuckets = 200000;
    vector<vector<double>> negBucketsEntries(numBuckets, vector<double>(0, 0.0));
    vector<vector<double>> negBucketsExits(numBuckets, vector<double>(0, 0.0));
    vector<vector<string>> negBucketsSymbols(numBuckets, vector<string>(0, ""));
    vector<vector<double>> posBucketsEntries(numBuckets, vector<double>(0, 0.0));
    vector<vector<double>> posBucketsExits(numBuckets, vector<double>(0, 0.0));
    vector<vector<string>> posBucketsSymbols(numBuckets, vector<string>(0, ""));
    vector<double> zeroBucketsEntries(0, 0.0);
    vector<double> zeroBucketsExits(0, 0.0);
    vector<string> zeroBucketsSymbols(0, "");
    double maxVal = (double)INT_MIN;
    double minVal = (double)INT_MAX;

    for (i = 0; i < numDates; i++) {

        int numPoints = size(entries[i]);
        if (numPoints == 0) continue;
        
        // debug in case the sort takes too long
        cout << numPoints << " ";
        
        // get the range of values to initialize the buckets
        for (j = 0; j < numPoints; j++) {

            if (entries[i][j] < minVal) minVal = entries[i][j];
            if (entries[i][j] > maxVal) maxVal = entries[i][j];
        }
        // clear all buckets from previous use
        for (j = 0; j < numBuckets; j++) {
            negBucketsEntries[j].clear();
            negBucketsExits[j].clear();
            negBucketsSymbols[j].clear();
            posBucketsEntries[j].clear();
            posBucketsExits[j].clear();
            posBucketsSymbols[j].clear();
        }
            zeroBucketsEntries.clear();
            zeroBucketsExits.clear();
            zeroBucketsSymbols.clear();
        // if all values are the same, no need to sort (buckets wouldn't work anyway)
        if(maxVal == 0 && minVal == 0){
            continue;
        }
        int index = 0;
        // fill the buckets with the values to be sorted
        for (j = 0; j < numPoints; j++) {
            double num = entries[i][j];
            if(num > 0.0){
                index = (int)((double)(numBuckets-1) * num / maxVal);
                posBucketsEntries[index].push_back(entries[i][j]);
                posBucketsExits[index].push_back(exits[i][j]);
                posBucketsSymbols[index].push_back(index_symbol_sorted[i][j]);
            }else{
                if(num < 0.0){
                    index = (int)((double)(numBuckets-1) * num / minVal);
                    negBucketsEntries[index].push_back(entries[i][j]);
                    negBucketsExits[index].push_back(exits[i][j]);
                    negBucketsSymbols[index].push_back(index_symbol_sorted[i][j]);
                }else{
                    zeroBucketsEntries.push_back(entries[i][j]);
                    zeroBucketsExits.push_back(exits[i][j]);
                    zeroBucketsSymbols.push_back(index_symbol_sorted[i][j]);
                }
            }
        }
        // iterate over the buckets
        for (j = 0; j < numBuckets; j++) {
            // sort them using exchange sort
            for (k = 0; k < size(posBucketsEntries[j]); k++) {
                for (l = k + 1; l < size(posBucketsEntries[j]); l++) {
                    swap = ascending && posBucketsEntries[j][k] > posBucketsEntries[j][l] || !ascending && posBucketsEntries[j][k] < posBucketsEntries[j][l];
                    if (swap) {
                        temp = posBucketsEntries[j][k];
                        posBucketsEntries[j][k] = posBucketsEntries[j][l];
                        posBucketsEntries[j][l] = temp;

                        temp = posBucketsExits[j][k];
                        posBucketsExits[j][k] = posBucketsExits[j][l];
                        posBucketsExits[j][l] = temp;

                        temps = posBucketsSymbols[j][k];
                        posBucketsSymbols[j][k] = posBucketsSymbols[j][l];
                        posBucketsSymbols[j][l] = temps;

                    }
                }
            }
        }
        // iterate over the buckets
        for (j = 0; j < numBuckets; j++) {
            // sort them using exchange sort
            for (k = 0; k < size(negBucketsEntries[j]); k++) {
                for (l = k + 1; l < size(negBucketsEntries[j]); l++) {
                    swap = ascending && negBucketsEntries[j][k] > negBucketsEntries[j][l] || !ascending && negBucketsEntries[j][k] < negBucketsEntries[j][l];
                    if (swap) {
                        temp = negBucketsEntries[j][k];
                        negBucketsEntries[j][k] = negBucketsEntries[j][l];
                        negBucketsEntries[j][l] = temp;

                        temp = negBucketsExits[j][k];
                        negBucketsExits[j][k] = negBucketsExits[j][l];
                        negBucketsExits[j][l] = temp;

                        temps = negBucketsSymbols[j][k];
                        negBucketsSymbols[j][k] = negBucketsSymbols[j][l];
                        negBucketsSymbols[j][l] = temps;

                    }
                }
            }
        }

        // concatenate the buckets for all data types
        index = 0;
        for (j = 0; j < numBuckets; j++) {
            for (k = 0; k < size(negBucketsEntries[j]); k++) {
                entries[i][index] = negBucketsEntries[j][k];
                exits[i][index] = negBucketsExits[j][k];
                index_symbol_sorted[i][index] = negBucketsSymbols[j][k];
                symbol_index_sorted[i][negBucketsSymbols[j][k]] = index;
                index++;
            }
        }
            for (k = 0; k < size(zeroBucketsEntries); k++) {
                entries[i][index] = zeroBucketsEntries[k];
                exits[i][index] = zeroBucketsExits[k];
                index_symbol_sorted[i][index] = zeroBucketsSymbols[k];
                symbol_index_sorted[i][zeroBucketsSymbols[k]] = index;
                index++;
            }
        for (j = 0; j < numBuckets; j++) {
            for (k = 0; k < size(posBucketsEntries[j]); k++) {
                entries[i][index] = posBucketsEntries[j][k];
                exits[i][index] = posBucketsExits[j][k];
                index_symbol_sorted[i][index] = posBucketsSymbols[j][k];
                symbol_index_sorted[i][posBucketsSymbols[j][k]] = index;
                index++;
            }
        }
        if (index != numPoints) {
            cerr << "Some data was unexpectedly lost while sorting values within a given date. Number of bucket values was " << index << " and number of points for this date was " << numPoints << ".\n\n";
            exit(1);
        }

        // assign symbols to indices since we have only sorted indices to symbols on this day
        //for(j=0;j<numPoints;j++){
        //    symbol_index_day[i][index_symbol_sorted[i][j]] = j;       // don't know why I wrote this
        //}
    }
} */

// sort a pair of arrays from highest to lowest: sort in[] while ordering out[] so that in[i] still corresponds to out[i]
void sortData(bool ascending) {
	
	double pl = 0.0;
	int i = 0, j = 0, k = 0, l = 0;
    entries.clear();
    exits.clear();

	// fill the arrays that determine trade entries and exits (depends on strategy)
    // this can be anything involving gaps, changes, individual ticker price movements, mas, etc.
	for (i = 0; i < numDates; i++) {
        vector<double> en;
        vector<double> ex;
		for (j = 0; j < numSymbols; j++) {
            if(open[j][i] == DUMMY_DOUBLE) continue;
            if(high[j][i] == DUMMY_DOUBLE) continue;
            if(low[j][i] == DUMMY_DOUBLE) continue;
            if(close[j][i] == DUMMY_DOUBLE) continue;
            if(volume[j][i] == DUMMY_DOUBLE) continue;
            
            // if any data has been recorded on this symbol and date
            en.push_back(gap[j][i]);
            ex.push_back(change[j][i]);
            index_symbol_sorted[i][j] = index_symbol.at(j);
            symbol_index_sorted[i][index_symbol.at(j)] = j;
		}
        entries.push_back(en);
        exits.push_back(ex);
	}
    
    bool swap = 0;

    // sort the data within every day of points[]
    double temp = 0.0;
    string temps = "";
    int numBuckets = 500000;
    vector<vector<double>> bucketsEntries(numBuckets, vector<double>(0, 0.0));
    vector<vector<double>> bucketsExits(numBuckets, vector<double>(0, 0.0));
    vector<vector<string>> bucketsSymbols(numBuckets, vector<string>(0, ""));
    double maxVal = (double)INT_MIN;
    double minVal = (double)INT_MAX;

    for (i = 0; i < numDates; i++) {

        // print the loading bar or number of files complete depending on settings
        if(btPrintLoading){
            cout << "[";
            for(int j=1;j<=btLoadingBarWidth;j++){
                double pct = (double)i / (double)numDates;
                if(pct >= (double)j / (double)btLoadingBarWidth){
                    cout << "=";
                }else{
                    cout << " ";
                }
            }
            double pct = (double)i / (double)numDates;
            cout << "] " << (int)(pct * 10000.0) / 100 << "." << ((int)(pct * 10000.0) / 10) % 10 << (int)(pct * 10000.0) % 10 << "% (" << i << "/" << numDates << ")\r";
            cout.flush();
        }else{
            if(i % btPrintLoadingInterval == 0){
                cout << i << "/" << numDates << " dates sorted.\n";
            }
        }

        int numPoints = size(entries[i]);
        if (numPoints == 0) continue;
        
        // get the range of values to initialize the buckets
        for (j = 0; j < numPoints; j++) {

            if (entries[i][j] < minVal) minVal = entries[i][j];
            if (entries[i][j] > maxVal) maxVal = entries[i][j] * 1.0001;
        }
        // clear all buckets from previous use
        for (j = 0; j < numBuckets; j++) {
            bucketsEntries[j].clear();
            bucketsExits[j].clear();
            bucketsSymbols[j].clear();
        }
        double range = maxVal - minVal;
        // if all values are the same, no need to sort (buckets wouldn't work anyway)
        if(range == 0){
            continue;
        }
        int index = 0;
        // fill the buckets with the values to be sorted
        for (j = 0; j < numPoints; j++) {

            index = (int)((double)numBuckets * (entries[i][j] - minVal) / range);
            bucketsEntries[index].push_back(entries[i][j]);
            bucketsExits[index].push_back(exits[i][j]);
            bucketsSymbols[index].push_back(index_symbol_sorted[i][j]);
        }
        // iterate over the buckets
        for (j = 0; j < numBuckets; j++) {
            // sort them using exchange sort
            for (k = 0; k < size(bucketsEntries[j]); k++) {
                for (l = k + 1; l < size(bucketsEntries[j]); l++) {
                    swap = ascending && bucketsEntries[j][k] > bucketsEntries[j][l] || !ascending && bucketsEntries[j][k] < bucketsEntries[j][l];
                    if (swap) {
                        temp = bucketsEntries[j][k];
                        bucketsEntries[j][k] = bucketsEntries[j][l];
                        bucketsEntries[j][l] = temp;

                        temp = bucketsExits[j][k];
                        bucketsExits[j][k] = bucketsExits[j][l];
                        bucketsExits[j][l] = temp;

                        temps = bucketsSymbols[j][k];
                        bucketsSymbols[j][k] = bucketsSymbols[j][l];
                        bucketsSymbols[j][l] = temps;

                    }
                }
            }
        }

        // concatenate the buckets for all data types
        index = 0;
        for (j = 0; j < numBuckets; j++) {
            for (k = 0; k < size(bucketsEntries[j]); k++) {
                entries[i][index] = bucketsEntries[j][k];
                exits[i][index] = bucketsExits[j][k];
                index_symbol_sorted[i][index] = bucketsSymbols[j][k];
                symbol_index_sorted[i][bucketsSymbols[j][k]] = index;
                index++;
            }
        }
        if (index != numPoints) {
            cerr << "Some data was unexpectedly lost while sorting values within a given date. Number of bucket values was " << index << " and number of points for this date was " << numPoints << ".\n\n";
            exit(1);
        }

        // assign symbols to indices since we have only sorted indices to symbols on this day
        //for(j=0;j<numPoints;j++){
        //    symbol_index_day[i][index_symbol_sorted[i][j]] = j;       // don't know why I wrote this
        //}
    }

    if(btPrintLoading){
        cout << "[";
        for(int i=1;i<=btLoadingBarWidth;i++){
            cout << "=";
        }
        cout << "] 100.00% (" << numDates << "/" << numDates << ")\n";
    }
}


// get every data measurement
void getStats() {
	int i = 0, j = 0, d = 0, n = 0;

	// get changes and gaps by both symbols and dates simply for abstraction for use in the trading strategy during backtesting
	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numDates; j++) {

            if(open[i][j] == DUMMY_DOUBLE || high[i][j] == DUMMY_DOUBLE || low[i][j] == DUMMY_DOUBLE || close[i][j] == DUMMY_DOUBLE || volume[i][j] == DUMMY_DOUBLE){
                continue;
            }

            change[i][j] = 0.0;
			if (open[i][j] != 0.0) change[i][j] = (close[i][j] - open[i][j]) / open[i][j];

            gap[i][j] = 0.0;
            if(j > 0){
			    if (close[i][j - 1] != 0.0) gap[i][j] = (open[i][j] - close[i][j - 1]) / close[i][j - 1];
            }

			totalChange[j] += change[i][j];
			totalGap[j] += gap[i][j];
            
            numAvailableSymbolsDate[j]++;
		}
	}

	// find the average changes and gaps on each date
	for (i = 0; i < numDates; i++) {

		avgChange[i] = 0.0;
		avgGap[i] = 0.0;
		if (numBarsByDateIndex[i] != 0) {
            // this assumes that numBarsByDateIndex has the correct number of bars, which it should
			avgChange[i] = totalChange[i] / (double)numBarsByDateIndex[i];
			avgGap[i] = totalGap[i] / (double)numBarsByDateIndex[i];
		}
	}
}

// print the basic stats for the whole dataset
void testBacktesting() {
	int wins = 0;
	int losses = 0;
	double won = 0.0;
	double lost = 0.0;
	int ties = 0;
	int total = 0;
	for (int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numDates; j++) {
            if(close[i][j] == DUMMY_DOUBLE) continue;
            if(open[i][j] == DUMMY_DOUBLE) continue;
            
            if (close[i][j] > open[i][j]) {
                wins++;
                won += close[i][j] - open[i][j];
            }
            if (close[i][j] < open[i][j]) {
                losses++;
                lost += close[i][j] - open[i][j];
            }
            if (close[i][j] == open[i][j]) {
                ties++;
            }
            total++;
		}
	}
	printf("Basic stat backtesting: Wins: %i, Losses: %i, Won: %f, Lost: %f, Ties: %i, Total: %i\n\n", wins, losses, won, lost, ties, total);
}

// print outlying bar values
void findOutliers() {
	double n = 0.0;
	for(int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numDates; j++) {
            if(close[i][j] == DUMMY_DOUBLE) continue;
            if(open[i][j] == DUMMY_DOUBLE) continue;
                
            n = 0.0;
            if(j > 0){
                if(close[i][j - 1] != 0.0) n = (open[i][j] - close[i][j - 1]) / close[i][j - 1];
            }
            if (n >= btMaxOutlier || n <= btMinOutlier) { printf("Symbol %i with ticker %s at bar %i has an outlying daily gap of %.2f.\n", i, index_symbol.at(i), j, n);}
            n = 0.0;
            if(open[i][j] != 0.0) n = (close[i][j] - open[i][j]) / open[i][j];
            if (n >= btMaxOutlier || n <= btMinOutlier) { printf("Symbol %i with ticker %s at bar %i has an outlying daily change of %.2f.\n", i, index_symbol.at(i), j, n); }

            // print the first outlying close price and ticker symbol of the stocks with at least one daily close greater than $10000
            // note: in barchart files, greater than $100000s have all been removed except BRK.A
            //if (closeRaw[i][j] >= 1000000) { printf("Symbol %i with ticker %s at bar %i has an outlying close of $%.2f.\n", i, index_symbol.at(i), j, (double)closeRaw[i][j] / 100.0); break; }
        }
	}
	printf("\n");
}

// print the average values for every date with at least 1 bar
void printAverages(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = btStartingDateIndex; i <= btEndingDateIndex; i++) {
		d = index_date[i];
		if (numBarsByDateIndex[i] > 0) {
			printf("Date: %i, Number of Bars: %i, Average Change: %f, Average Gap: %f\n", d, numBarsByDateIndex[i], avgChange[i], avgGap[i]);
		}
	}
	printf("\n");
}

// print the bar price values for every date
void printNumDateBars(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = btStartingDateIndex; i <= btEndingDateIndex; i++) {
		d = index_date[i];
		if (numBarsByDateIndex[i] > 0) {
			printf("Date: %i, Number of Bars: %i\n", d, numBarsByDateIndex[i]);
		}
	}
	printf("\n");
}

// print the number of bars for every symbol
void printNumSymbolBars(int start, int end) {
	if (start < 0) start = 0;
	if (start >= numSymbols) start = numSymbols - 1;
	if (end < 0) end = 0;
	if (end >= numSymbols) end = numSymbols - 1;

	for (int i = start; i <= end; i++) {
		printf("Symbol Index: %i, Ticker %s, Number of Bars: %i\n", i, index_symbol[i], numBars[i]);
	}
	printf("\n");
}

// print the bar data for every date
void printAllDateBars(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = 0; i < numSymbols; i++) {
        for (int j = btStartingDateIndex; j <= btEndingDateIndex; j++) {
            if(open[i][j] == DUMMY_DOUBLE) continue;
            if(high[i][j] == DUMMY_DOUBLE) continue;
            if(low[i][j] == DUMMY_DOUBLE) continue;
            if(close[i][j] == DUMMY_DOUBLE) continue;
            if(volume[i][j] == DUMMY_DOUBLE) continue;

            d = index_date.at(j);
        
			cout << "Date " << d << ", Symbol " << i << " (" << index_symbol.at(i) << "): OHLCV: " << open[i][j] << ", " << high[i][j] << ", " << low[i][j] << ", " << close[i][j] << ", " << volume[i][j] << "\n";

		}
	}
	printf("Number of symbols: %i\n\n", numSymbols);
}

// print the bar price values for every symbol
void printAllSymbolBars(int symbolStart, int symbolEnd, int dateStart, int dateEnd) {

	if (symbolStart < 0) symbolStart = 0;
	if (symbolStart >= numSymbols) symbolStart = numSymbols - 1;
	if (symbolEnd < 0) symbolEnd = 0;
	if (symbolEnd >= numSymbols) symbolEnd = numSymbols - 1;

	getDateIndices(dateStart, dateEnd);

	for (int i = symbolStart; i <= symbolEnd; i++) {
		cout << "Symbol " << i << " - Ticker " << index_symbol.at(i) << "\n";
		for (int j = btStartingDateIndex; j <= btEndingDateIndex; j++) {
            if(open[i][j] == DUMMY_DOUBLE) continue;
            if(high[i][j] == DUMMY_DOUBLE) continue;
            if(low[i][j] == DUMMY_DOUBLE) continue;
            if(close[i][j] == DUMMY_DOUBLE) continue;
            if(volume[i][j] == DUMMY_DOUBLE) continue;

            int d = index_date.at(j);
            double change = 0.0;
            if(open[i][j] != 0.0) change = (close[i][j] - open[i][j]) / open[i][j];
            double gap = 0.0;
            if(j > 0){
                if(close[i][j - 1] != 0.0) gap = (open[i][j] - close[i][j - 1]) / close[i][j - 1];
            }
			printf("Bar %i (%i): %i %i %i %i, Change: %.2f%%, Gap: %.2f%%\n", j, d, open[i][j], high[i][j], low[i][j], close[i][j], change, gap);
		}
		printf("\n");
	}
	printf("Number of symbols: %i\n\n", numSymbols);
}

// create date/index converter, initialize and fill data storage by date
void setupDateData() {
	int i = 0, j = 0, k = 0;

	// fill holes in data
	for (i = 0; i < numSymbols; i++) {

        // find first filled value. if there is none, exit
        bool flag = 1;
        for(j = 0; j < numDates; j++){
            if(open[i][j] != DUMMY_DOUBLE || high[i][j] != DUMMY_DOUBLE || low[i][j] != DUMMY_DOUBLE || close[i][j] != DUMMY_DOUBLE || volume[i][j] != DUMMY_DOUBLE){
                flag = 0; break;
            }
        }
        if(flag){
            cerr << "No data points were filled after reading every file.\n";
            exit(1);
        }

		while(j > 0) {
			// copy the first value left to fill in dates before the beginning of this symbol's history
			open[i][j - 1] = open[i][j];
			high[i][j - 1] = high[i][j];
			low[i][j - 1] = low[i][j];
			close[i][j - 1] = close[i][j];
			volume[i][j - 1] = volume[i][j];
            j--;
		}
		for (j = 1; j < numDates; j++) {
			// copy next values right to fill in holes and dates after symbol history
			if (close[i][j] == DUMMY_DOUBLE) {
				open[i][j] = open[i][j - 1];
				high[i][j] = high[i][j - 1];
				low[i][j] = low[i][j - 1];
				close[i][j] = close[i][j - 1];
				volume[i][j] = volume[i][j - 1];
			}
		}
	}
}

// setup the file reader, read, reverse (if daily bars are sorted in descending date order), and interpret all the data
char gatherData(string path) {

	cout << "Setting up file reader... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";

	setup();

	cout << "Reading data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";

	readAllFiles(path);

	setupDateData();

	cout << "Getting important statistics... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";

	getStats();

    cout << "Sorting the date data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    
	sortData(true);

	cout << "Finished gathering data.\n\n";
	
	return 1;
}

// backtest the chosen strategy
void backtest(int dateStart, int dateEnd, char skipIfNotEnoughBars, char printResults) {

	if (btLeverage <= 0.0 || btLeverage > 100.0) {
		printf("Backtesting leverage must be greater than 0 and at most 100.\n\n");
		exit(1);
	}
	if (dateStart > dateEnd) {
		printf("Starting date must be less than or equal to ending date.\n\n");
		exit(1);
	}

	getDateIndices(dateStart, dateEnd);

	if (printResults) {
		cout << "Running the backtest from " << dateStart << " (adjusted to " << index_date.at(btStartingDateIndex) << " with index " << btStartingDateIndex << ") to " << dateEnd << " (adjusted to " << index_date.at(btEndingDateIndex) << " with index " << btEndingDateIndex << ")... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
	}

	double n = 0.0;
	double ln = 0.0;

	int barEnd = 0;

	int numDatesTraded = 0;
	int numDatesObserved = 0;
	int totalNumSymbols = 0;

    for(int i = 0; i <= btMaxSymbolRankDay - btMinSymbolRankDay; i++){
        btTrades.push_back(0);
        btWins.push_back(0);
        btLosses.push_back(0);
        btTies.push_back(0);
        btWon.push_back(0.0);
        btLost.push_back(0.0);
        btBalance.push_back(1.0);
        btVar.push_back(0.0);
        btSD.push_back(0.0);
        btNumOutliers.push_back(0);
    }

	for (int i = btStartingDateIndex; i <= btEndingDateIndex; i++) {

		int x = numAvailableSymbolsDate[i];
		double e = 0.0;

		if (x < 1) continue;

		numDatesObserved++;
		totalNumSymbols += x;

		int starting = btMinSymbolRankDay;
		int ending = btMaxSymbolRankDay;
		// negative values mean count from the end backwards
		if (starting < 0) {
			starting = x + starting;
		}
		if (ending < 0) {
			ending = x + ending;
		}

		if (skipIfNotEnoughBars && (starting < 0 || starting >= x || ending < 0 || ending >= x)) continue;

		// handle out of bounds starting and ending values
		if (starting < 0) starting = 0;
		if (starting >= x) starting = x - 1;
		if (ending < 0) ending = 0;
		if (ending >= x) ending = x - 1;

        if(btPrintEntries){
            cout << "Date " << index_date.at(i) << ": ";
        }

		// trade on selected bars this day
		for (int j = 0; j <= ending - starting; j++) {
            int symbol = j + starting;
			e = exits[i][symbol];

            if(e <= btMinOutlier || e >= btMaxOutlier){
                btNumOutliers[j]++;
                if(btPrintOutliers){
                    cout << e << "* ";
                }
                if(btIgnoreOutliers) continue;
            }

			// calculate what to multiply account by as a result of this trade
			n = (100.0 + e) / 100.0;
			ln = (100.0 + btLeverage * e) / 100.0;

			// evaluate outcome
            btTrades[j]++;
			if (e > 0.0) {
				btWins[j]++;
				btWon[j] += e;
			}
			else {
				if (e < 0.0) {
					btLosses[j]++;
					btLost[j] += e;
				}
				else {
					btTies[j]++;
				}
			}
			btBalance[j] *= ln;
			btVar[j] += e * e / 10000.0;

            if(btPrintEntries){
                if(btPrintDetailedResults){
                    cout << index_date.at(i) << "_" << index_symbol_sorted[i].at(symbol) << "_" << entries[i][symbol] << "_" << exits[i][symbol] << " ";
                }else{
                    cout << e << " ";
                }
            }
		}

        if(btPrintEntries){
            cout << "\n";
        }
		
		numDatesTraded++;
	}

	if (printResults) {
        int r = size(btBalance);
        string output = "";
		output += "Finished the backtest from " + to_string(dateStart) + " (adjusted to " + to_string(index_date.at(btStartingDateIndex)) + " with index " + to_string(btStartingDateIndex) + ") to " + to_string(dateEnd) + " (adjusted to " + to_string(index_date.at(btEndingDateIndex)) + " with index " + to_string(btEndingDateIndex) + ").\n\n";
        output += "SUMMARY:\n\nNumber of Dates Considered For Trading: ";
        output += to_string(numDatesObserved) + "\nNumber of Dates Traded: ";
        output += to_string(numDatesTraded) + "\nNumber of Trades Completed: ";
        for(int i=0;i<r;i++){
            output += to_string(btTrades[i]) + " ";
        }
        output += "\nNumber of Trades Per Day Traded: ";
        for(int i=0;i<r;i++){
            output += to_string((double)btTrades[i] / (double)numDatesTraded) + " ";
        }
        output += "\nNumber of Symbols Available Per Day Traded: ";
        output += to_string((double)totalNumSymbols / (double)numDatesTraded);
        output += "\nLeverage Used: ";
        output += to_string(btLeverage);
        output += "\nFinal Balance: ";
        for(int i=0;i<r;i++){
            output += to_string(btBalance[i]) + " ";
        }
        output += "\nWins: ";
        for(int i=0;i<r;i++){
            output += to_string(btWins[i]) + " ";
        }
        output += "\nTies: ";
        for(int i=0;i<r;i++){
            output += to_string(btTies[i]) + " ";
        }
        output += "\nLosses: ";
        for(int i=0;i<r;i++){
            output += to_string(btLosses[i]) + " ";
        }
        output += "\nWin Rate: ";
        for(int i=0;i<r;i++){
            output += to_string((double)btWins[i] / (double)btTrades[i]) + " ";
        }
        output += "\nTie Rate: ";
        for(int i=0;i<r;i++){
            output += to_string((double)btTies[i] / (double)btTrades[i]) + " ";
        }
        output += "\nLoss Rate: ";
        for(int i=0;i<r;i++){
            output += to_string((double)btLosses[i] / (double)btTrades[i]) + " ";
        }
        output += "\nTotal of Wins: ";
        for(int i=0;i<r;i++){
            output += to_string(btWon[i]) + " ";
        }
        output += "\nTotal of Losses: ";
        for(int i=0;i<r;i++){
            output += to_string(btLost[i]) + " ";
        }
        output += "\nProfit Factor: ";
        for(int i=0;i<r;i++){
            output += to_string(-1.0 * btWon[i] / btLost[i]) + " ";
        }
        output += "\nAverage Win Result: ";
        for(int i=0;i<r;i++){
            output += to_string(btWon[i] / (double)btWins[i]) + " ";
        }
        output += "\nAverage Loss Result: ";
        for(int i=0;i<r;i++){
            output += to_string(btLost[i] / (double)btLosses[i]) + " ";
        }
        output += "\nAverage Trade Result: ";
        for(int i=0;i<r;i++){
            output += to_string((btWon[i] + btLost[i]) / (double)btTrades[i]) + " ";
        }
        output += "\nSum of Trade Results: ";
        for(int i=0;i<r;i++){
            output += to_string(btWon[i] + btLost[i]) + " ";
        }
        output += "\nAverage Squared Deviation (Variance): ";
        for(int i=0;i<r;i++){
            output += to_string((double)btVar[i] / (double)btTrades[i]) + " ";
        }
        output += "\nLeveraged Account Gain Sharpe Ratio: ";
        for(int i=0;i<r;i++){
            output += to_string((btBalance[i] - 1.0) / sqrt((double)btVar[i] / (double)btTrades[i])) + " ";
        }
        output += "\nTotal Trade Gain Sharpe Ratio: ";
        for(int i=0;i<r;i++){
            output += to_string((btWon[i] + btLost[i]) / sqrt((double)btVar[i] / (double)btTrades[i])) + " ";
        }
        cout << output;
	}
}

void backtestAndPrintHistogram(int dateStart, int dateEnd, char skipIfNotEnoughBars, double minLeverage, double maxLeverage, int chartHeight) {
	double maxResult = -1.0 * pow(2.0, 80.0), minResult = pow(2.0, 80.0);
    int worstRank = 0, bestRank = 0;
    int r = size(btBalance);
	double* bars = (double*)calloc(r, sizeof(double));

	backtest(dateStart, dateEnd, skipIfNotEnoughBars, 0);

	for (int i = 0; i < r; i++) {
		if (btBalance[i] < minResult) {
			minResult = btBalance[i];
			worstRank = i + btMinSymbolRankDay;
		}
		if (btBalance[i] > maxResult){
			maxResult = btBalance[i];
			bestRank = i + btMinSymbolRankDay;
		}
		bars[i] = btBalance[i];
	}
	for (int i = 0; i < chartHeight; i++) {
		printf("|");
		for (int j = 0; j < r; j++) {
			if ((bars[j] - minResult) / (maxResult - minResult) >= (double)(chartHeight - i) / (double)chartHeight) {
				printf("#");
			}
			else {
				printf(" ");
			}
		}
		printf("|\n");
	}
	printf("\nShowing balance outcome results from date %i to date %i .\n\n\n", dateStart, dateEnd);
	printf("\nMinimum Result: %f at rank %f, Maximum Result: %f at rank %f.\n\n\n", minResult, worstRank, maxResult, bestRank);
}

int main(void) {
    setup();

    string readPath = "C:\\Minute Stock Data\\";
    columnCodes = "SVOCHLU-";

    // set backtesting settings:
    // int btStartingDateIndex, int btEndingDateIndex, int btMinSymbolRankDay, int btMaxSymbolRankDay,
    // double btLeverage, bool btDisregardFilters, double btMinOutlier, double btMaxOutlier, bool btIgnoreOutliers,
    // bool btPrintOutliers, bool btPrintEntries, bool btPrintAllResults, bool btPrintDetailedResults,
    // bool btPrintLoading, bool btPrintLoadingInterval, int btLoadingBarWidth, bool btPrintSummary
    btStartingDateIndex = 0;
    btEndingDateIndex = -1;
    btMinSymbolRankDay = 0;
    btMaxSymbolRankDay = 3;
    btLeverage = 1.0;
    btMinOutlier = -0.5;
    btMaxOutlier = 1.0;
    btIgnoreOutliers = false;
    btPrintOutliers = true;
    btPrintEntries = true;
    btPrintAllResults = true;
    btPrintDetailedResults = true;
    btPrintLoading = false;
    btPrintLoadingInterval = 5;
    btLoadingBarWidth = 60;
    btPrintSummary = true;

	if (!gatherData("C:\\Daily Stock Data\\")) {
		return -1;
	}

	//printAllSymbolBars(0, 3, 20240102, 20240106);
	//printAllDateBars(20240102, 20240106);

	//printAverages(20240101, 20241231);

	//printNumSymbolBars(0, 10000);
	//printNumDateBars(20240101, 20241231);

	//findOutliers();

	//testBacktesting();
	
	backtest(20100101, 20260101, 0, 1);

	//backtestAndPrintHistogram(20100101, 20260101, 1, 0.01, 1.0, 100, 20);

	return 0;
}
