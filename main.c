#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define numFiles 487

#define numCharsPerFile 5000000

char text[numCharsPerFile];
int textSpot;

// maximum number of symbols, for memory allocation
#define maxNumSymbols 6000
// number of symbols
int numSymbols = 0;
// maximum number of bars a symbol can have, for memory allocation
#define maxNumBars 6000
// number of bars for each symbol
int* numBars;

// info for file reading
int lastNewlinePos = 0;
int nextNewlinePos = 0;
char* bar = NULL;
int barLength = 0;
int bs = 0;

// info for file data interpreting
int currentSymbol = 0;
int currentBar = 0;
char endOfText = 0;
char currentTicker[5] = { ' ', ' ', ' ', ' ', ' ' };
int currentDate = 0;
int currentNumber[7] = { 0,0,0,0,0,0,0 };

// raw symbol price data
char** ticker;
int** date;
int** open;
int** high;
int** low;
int** close;
int** volume;

// number of total possible dates - covers range from Jan 1, 2000 to Dec 31, 2029
#define numDates 300000

// percentage daily changes and gaps for each bar of each symbol
double** pctChange;
double** pctGap;

// sum of all changes, sum of gaps
double* totalChangeDate;
double* totalGapDate;
// number of bars in each date
int* numBarsDate;

// average of all changes and gaps in each date
double* avChangeDate;
double* avGapDate;

// standardized (subtract average) change and gap of each bar
double** stdChange;
double** stdGap;

// changes and gaps (original and standardized) sorted from highest to lowest
double** sortedPctChange;
double** sortedPctGap;
double** sortedStdChange;
double** sortedStdGap;

// base addresses for file reading
char address0[] = "C:\\Daily Stock Data\\ .csv";
char address1[] = "C:\\Daily Stock Data\\  .csv";
char address2[] = "C:\\Daily Stock Data\\   .csv";
char address3[] = "C:\\Daily Stock Data\\    .csv";
char address4[] = "C:\\Daily Stock Data\\     .csv";
char addressBaseLength = 20;

// read data from a csv file into text
char readCSV(int fileIndex) {

	if (fileIndex < 0 || fileIndex >= 100000) {
		printf("Couldn't read file with index %i because out of range [0, 99999].\n", fileIndex);
		return 0;
	}

	FILE* fp;
	
	char* fileAddress = NULL;

	int b = addressBaseLength;

	if (fileIndex < 10) {
		address0[b] = fileIndex + 48;
		fileAddress = address0;
	}
	else {
		if (fileIndex < 100) {
			address1[b] = (fileIndex / 10) + 48;
			address1[b + 1] = (fileIndex % 10) + 48;
			fileAddress = address1;
		}
		else {
			if (fileIndex < 1000) {
				address2[b] = (fileIndex / 100) + 48;
				address2[b + 1] = ((fileIndex / 10) % 10) + 48;
				address2[b + 2] = (fileIndex % 10) + 48;
				fileAddress = address2;
			}
			else {
				if (fileIndex < 10000) {
					address3[b] = (fileIndex / 1000) + 48;
					address3[b + 1] = ((fileIndex / 100) % 10) + 48;
					address3[b + 2] = ((fileIndex / 10) % 10) + 48;
					address3[b + 3] = (fileIndex % 10) + 48;
					fileAddress = address3;
				}
				else {
					address4[b] = (fileIndex / 10000) + 48;
					address4[b + 1] = ((fileIndex / 1000) % 10) + 48;
					address4[b + 2] = ((fileIndex / 100) % 10) + 48;
					address4[b + 3] = ((fileIndex / 10 ) % 10) + 48;
					address4[b + 4] = (fileIndex % 10) + 48;
					fileAddress = address4;
				}
			}
		}
	}

	fopen_s(&fp, fileAddress, "rb");

	if (fp == NULL) {
		printf("Couldn't read file %s, index %i\n\n", fileAddress, fileIndex);
	}
	if (fp != NULL) {
		fread(text, sizeof(char), numCharsPerFile, fp);
		fclose(fp);

		if (text[numCharsPerFile - 1] != 0) {
			printf("ERROR: Text file went out of bounds. Increase numCharsPerFile.\n\n");
			exit(1);
		}
	}
	return 1;
}

int power(int b, int e) {
	if (e < 0) {
		return 0;
	}
	if (e == 0) {
		return 1;
	}
	int n = b;
	for (int i = 1; i < e; i++) {
		n *= b;
	}
	return n;
}

char isNumber(char n) {
	if (n >= '0' && n <= '9') {
		return 1;
	}
	return 0;
}

char isLetter(char n) {
	if (n >= 'A' && n <= 'Z') {
		return 1;
	}
	return 0;
}

// find the next decimal number within the current bar
char getNextNumber(char index, char pow) {
	int i = 0;

	// finding the next number by finding the comma before it
	while (bar[bs - 1] != ',') {
		// bs is first digit
		bs++;
		if (bs >= barLength) {
			printf("ERROR: Number exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
			exit(1);
		}
	}

	while (!(isNumber(bar[bs]))) {
		// bs is first digit
		bs++;
		if (bs >= barLength) {
			printf("ERROR: Number exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
			exit(1);
		}
	}

	int bs1 = bs + 1;
	while (isNumber(bar[bs1])) {
		// bs1 is first punctuation
		bs1++;
		if (bs1 >= barLength) {
			printf("ERROR: Number exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
			exit(1);
		}
	}

	if (bar[bs1] == '.') {
		int bs2 = bs1 + 1;
		while (isNumber(bar[bs2])) {
			// bs2 is second punctuation
			bs2++;
			if (bs2 >= barLength) {
				printf("ERROR: Number exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
				exit(1);
			}
		}

		currentNumber[index] = 0;
		for (i = 0; i < bs1 - bs; i++) {
			currentNumber[index] += (bar[bs + i] - 48) * power(10, bs1 - bs - 1 - i + pow);
		}
		for (i = 0; i < bs2 - bs1 - 1; i++) {
			currentNumber[index] += (bar[bs1 + i + 1] - 48) * power(10, 1 - i);
		}
		bs = bs2;
	}
	else {
		currentNumber[index] = 0;
		for (i = 0; i < bs1 - bs; i++) {
			currentNumber[index] += (bar[bs + i] - 48) * power(10, bs1 - bs - 1 - i + pow);
		}
	}

	return 0;
}

// find the next date within the current bar
char getNextDate() {
	int bst = bs;
	while (!(isNumber(bar[bst]) && isNumber(bar[bst + 1]) && isNumber(bar[bst + 2]) && isNumber(bar[bst + 3]) && bar[bst + 4] == '-' && isNumber(bar[bst + 5]) && isNumber(bar[bst + 6]) && bar[bst + 7] == '-' && isNumber(bar[bst + 8]) && isNumber(bar[bst + 9]))) {
		bst++;
		if (bst + 9 >= barLength) {
			printf("ERROR: Date exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
			exit(1);
		}
	}

	currentDate = (bar[bst] - 48) * 10000000 + (bar[bst + 1] - 48) * 1000000 + (bar[bst + 2] - 48) * 100000 + (bar[bst + 3] - 48) * 10000 + (bar[bst + 5] - 48) * 1000 + (bar[bst + 6] - 48) * 100 + (bar[bst + 8] - 48) * 10 + (bar[bst + 9] - 48);
	bs = bst + 10;
	return 0;
}

// find the next ticker symbol within the current bar
char getNextSymbol() {

	if (bs + 4 >= barLength) {
		printf("ERROR: Ticker symbol exceeded bar length while reading data. Bar %i of symbol %i (both 0-indexed).\n\n", currentBar, currentSymbol);
		exit(1);
	}

	// get the new ticker from the first 1-5 characters (numbers and .) in the bar line
	char newTicker[5] = { bar[bs],' ',' ',' ',' ' };
	bs++;
	if (isLetter(bar[bs]) || bar[bs] == '.') {
		newTicker[1] = bar[bs];
		bs++;
		if (isLetter(bar[bs]) || bar[bs] == '.') {
			newTicker[2] = bar[bs];
			bs++;
			if (isLetter(bar[bs]) || bar[bs] == '.') {
				newTicker[3] = bar[bs];
				bs++;
				if (isLetter(bar[bs]) || bar[bs] == '.') {
					newTicker[4] = bar[bs];
					bs++;
				}
			}
		}
	}

	// compare 5 letters in symbols
	char identical = 1;
	for (int i = 0; i < 5; i++) {
		if (currentTicker[i] != newTicker[i]) {
			identical = 0;
		}
	}

	// if ticker changed: set the ticker symbol, document the number of bars in the previous symbol, reset to first bar, increment the symbol index
	// currentSymbol starts at -1 and gets incremented to 0 at the start of the first symbol
	if (!identical) {
		for (int i = 0; i < 5; i++) {
			currentTicker[i] = newTicker[i];
		}
		if (currentSymbol >= 0) {
			numBars[currentSymbol] = currentBar;
		}
		currentBar = 0;
		currentSymbol++;
		for (int i = 0; i < 5; i++) {
			ticker[currentSymbol][i] = currentTicker[i];
		}
	}
	return 0;
}

// initialize the next bar within text
void initBar() {
	lastNewlinePos = nextNewlinePos + 1;
	nextNewlinePos = lastNewlinePos;
	while (text[nextNewlinePos] != '\n') {
		nextNewlinePos++;
		if (nextNewlinePos >= numCharsPerFile) {
			printf("Reached last bar, bar %i\n", currentBar);
			return;
		}
	}

	barLength = nextNewlinePos - lastNewlinePos + 1;
	bar = (char*)calloc(barLength, sizeof(char));
	if (bar != NULL) {
		for (int i = 0; i < barLength; i++) {
			bar[i] = text[lastNewlinePos + i];
		}
	}

	bs = 0;
}

// parse and interpret the next bar within text
char getNextBar() {
	initBar();

	// if on last line of file containing no data, skip it
	if (bar[0] == '\"') {
		return 1;
	}

	if (getNextSymbol()) {
		return 1;
	}
	if (getNextDate()) {
		return 1;
	}
	if (getNextNumber(0, 2)) {
		return 1;
	}
	if (getNextNumber(1, 2)) {
		return 1;
	}
	if (getNextNumber(2, 2)) {
		return 1;
	}
	if (getNextNumber(3, 2)) {
		return 1;
	}
	if (getNextNumber(4, 0)) {
		return 1;
	}
	if (getNextNumber(5, 0)) {
		return 1;
	}
	if (getNextNumber(6, 0)) {
		return 1;
	}

	date[currentSymbol][currentBar] = currentDate;

	open[currentSymbol][currentBar] = currentNumber[0];
	high[currentSymbol][currentBar] = currentNumber[1];
	low[currentSymbol][currentBar] = currentNumber[2];
	close[currentSymbol][currentBar] = currentNumber[3];
	volume[currentSymbol][currentBar] = currentNumber[6];

	currentBar++;
	return 0;
}

void getDataFromText() {

	currentBar = 0;

	textSpot = 0;
	while (text[textSpot] != '\n') {
		textSpot++;
	}

	nextNewlinePos = textSpot;

	// reading every bar from text
	while (1) {

		if (getNextBar()) {
			break;
		}
	}

	numSymbols = currentSymbol + 1;
}

void setup() {
	int i = 0;

	for (i = 0; i < numCharsPerFile; i++) {
		text[i] = 0;
	}

	numBars = (int*)calloc(maxNumSymbols, sizeof(int));

	currentSymbol = -1;
	currentBar = 0;

	ticker = (char**)calloc(maxNumSymbols, sizeof(char*));

	date = (int**)calloc(maxNumSymbols, sizeof(int*));

	open = (int**)calloc(maxNumSymbols, sizeof(int*));
	high = (int**)calloc(maxNumSymbols, sizeof(int*));
	low = (int**)calloc(maxNumSymbols, sizeof(int*));
	close = (int**)calloc(maxNumSymbols, sizeof(int*));
	volume = (int**)calloc(maxNumSymbols, sizeof(int*));

	pctChange = (double**)calloc(maxNumSymbols, sizeof(double*));
	pctGap = (double**)calloc(maxNumSymbols, sizeof(double*));

	stdChange = (double**)calloc(maxNumSymbols, sizeof(double*));
	stdGap = (double**)calloc(maxNumSymbols, sizeof(double*));

	for (int i = 0; i < maxNumSymbols; i++) {
		ticker[i] = (char*)calloc(5, sizeof(char));
		for (int j = 0; j < 5; j++) {
			ticker[i][j] = ' ';
		}

		date[i] = (int*)calloc(maxNumBars, sizeof(int));

		open[i] = (int*)calloc(maxNumBars, sizeof(int));
		high[i] = (int*)calloc(maxNumBars, sizeof(int));
		low[i] = (int*)calloc(maxNumBars, sizeof(int));
		close[i] = (int*)calloc(maxNumBars, sizeof(int));
		volume[i] = (int*)calloc(maxNumBars, sizeof(int));

		pctChange[i] = (double*)calloc(maxNumBars, sizeof(double));
		pctGap[i] = (double*)calloc(maxNumBars, sizeof(double));

		stdChange[i] = (double*)calloc(maxNumBars, sizeof(double));
		stdGap[i] = (double*)calloc(maxNumBars, sizeof(double));
	}
}

void swap(int* a, int n, int s) {
	int pl = a[s];
	a[s] = a[n - 1 - s];
	a[n - 1 - s] = pl;
}

void reverseData() {

	for (int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numBars[i] / 2; j++) {
			swap(date[i], numBars[i], j);
			swap(open[i], numBars[i], j);
			swap(high[i], numBars[i], j);
			swap(low[i], numBars[i], j);
			swap(close[i], numBars[i], j);
			swap(volume[i], numBars[i], j);
		}
	}
}

int dateToIndex(int d) {
	return (d - 20000000);
}

// sort a pair of arrays from highest to lowest: sort in[] so that in[i] still corresponds to out[i]
void sort(double* in, double* out, int n) {
	double pl = 0.0;

	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			if (in[i] < in[j]) {
				pl = in[i];
				in[i] = in[j];
				in[j] = pl;

				pl = out[i];
				out[i] = out[j];
				out[j] = pl;
			}
		}
	}
}

// get every data measurement from changes, gaps, and dates
void getStats() {
	int i = 0; int j = 0;
	int d = 0;

	totalChangeDate = (double*)calloc(numDates, sizeof(double));
	totalGapDate = (double*)calloc(numDates, sizeof(double));
	numBarsDate = (int*)calloc(numDates, sizeof(int));

	avChangeDate = (double*)calloc(numDates, sizeof(double));
	avGapDate = (double*)calloc(numDates, sizeof(double));

	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numBars[i]; j++) {
			if (open[i][j] > 0) {
				pctChange[i][j] = 100.0 * (double)(close[i][j] - open[i][j]) / (double)open[i][j];
			}
			else {
				pctChange[i][j] = 0.0;
			}

			if (j == 0) {
				pctGap[i][j] = 0.0;
			}
			else {
				if (close[i][j - 1] > 0) {
					pctGap[i][j] = 100.0 * (double)(open[i][j] - close[i][j - 1]) / (double)close[i][j - 1];
				}
				else {
					pctGap[i][j] = 0.0;
				}
			}

			d = dateToIndex(date[i][j]);

			numBarsDate[d]++;
			totalChangeDate[d] += pctChange[i][j];
			totalGapDate[d] += pctGap[i][j];
		}
	}

	// initialize the date arrays which will be filled in sorted order
	sortedPctChange = (double**)calloc(numDates, sizeof(double));
	sortedPctGap = (double**)calloc(numDates, sizeof(double));
	sortedStdChange = (double**)calloc(numDates, sizeof(double));
	sortedStdGap = (double**)calloc(numDates, sizeof(double));

	for (i = 0; i < numDates; i++) {
		sortedPctChange[i] = (double*)calloc(numBarsDate[i], sizeof(double));
		sortedPctGap[i] = (double*)calloc(numBarsDate[i], sizeof(double));
		sortedStdChange[i] = (double*)calloc(numBarsDate[i], sizeof(double));
		sortedStdGap[i] = (double*)calloc(numBarsDate[i], sizeof(double));
	}

	// find the average changes and gaps on each date
	for (i = 0; i < numDates; i++) {
		if (numBarsDate[i] > 0) {
			avChangeDate[i] = totalChangeDate[i] / (double)numBarsDate[i];
			avGapDate[i] = totalGapDate[i] / (double)numBarsDate[i];
		}
		else {
			avChangeDate[i] = 0.0;
			avGapDate[i] = 0.0;
		}
	}

	int* countWithinDate = (int*)calloc(numDates, sizeof(int));

	// for each bar within each symbol
	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numBars[i]; j++) {
			d = dateToIndex(date[i][j]);

			// get the standardized change and gap values for each bar by subtracting the average change and gap values on the bar's date from the bar's change and gap values
			stdChange[i][j] = pctChange[i][j] - avChangeDate[d];
			stdGap[i][j] = pctGap[i][j] - avGapDate[d];

			// place the bar's change and gap among the bars with the same date in change and gap arrays (in sorted order)
			if (countWithinDate[d] >= numBarsDate[d]) {
				printf("ERROR: Tried to store more bars on date %i (index %i) than accepted.\n", date[i][j], d);
			}
			sortedPctChange[d][countWithinDate[d]] = pctChange[i][j];
			sortedPctGap[d][countWithinDate[d]] = pctGap[i][j];
			sortedStdChange[d][countWithinDate[d]] = stdChange[i][j];
			sortedStdGap[d][countWithinDate[d]] = stdGap[i][j];

			countWithinDate[d]++;

		}
	}

	// sort only the input and outcome pointer arrays so that in[i] and out[i] are the same symbol on the same day
	for (i = 0; i < numDates; i++) {
		sort(sortedPctGap[i], sortedPctChange[i], numBarsDate[i]);
	}
}

// backtest the chosen strategy
void backtest(int dateStart, int dateEnd, double leverage, int startingBarRank, int endingBarRank, char skipIfNotEnoughBars) {

	if (leverage <= 0.0 || leverage > 1.0) {
		printf("Backtesting leverage must be greater than 0 and at most 1.\n\n");
	}

	printf("Running the backtest from %i to %i... (This may take a few seconds to minutes depending on the amount of data present.)\n\n", dateStart, dateEnd);

	double balance = 1.0;
	int numTrades = 0;
	int wins = 0;
	int ties = 0;
	int losses = 0;
	double winrate = 0.0;
	double tierate = 0.0;
	double lossrate = 0.0;
	double totalGain = 0.0;
	double avSquaredDeviation = 0.0;

	double n = 0.0;
	double ln = 0.0;

	int barEnd = 0;

	int numDatesTraded = 0;

	int ds = dateToIndex(dateStart);
	int de = dateToIndex(dateEnd);

	if (ds < 0) ds = 0;
	if (de > numDates) de = numDates;

	for (int i = ds; i <= de; i++) {

		int x = numBarsDate[i];

		if (skipIfNotEnoughBars && x < endingBarRank - startingBarRank + 1) { continue; }

		int starting = startingBarRank;
		int ending = endingBarRank;
		if (starting < 0) {
			starting = x + starting;
		}
		if (ending < 0) {
			ending = x + ending;
		}
		if (starting < 0) starting = 0;
		if (starting >= x) starting = x - 1;
		if (ending < 0) ending = 0;
		if (ending >= x) ending = x - 1;

		// trade on 10 largest gap bars this day
		for (int j = starting; j <= ending; j++) {

			// calculate what to multiply account by as a result of this trade
			n = (100.0 + sortedPctChange[i][j]) / 100.0;
			ln = (100.0 + leverage * sortedPctChange[i][j]) / 100.0;

			// evaluate outcome
			if (n > 1.0) {
				wins++;
			}
			else {
				if (n < 1.0) {
					losses++;
				}
				else {
					ties++;
				}
			}
			balance *= ln;
			numTrades++;
			totalGain += sortedPctChange[i][j];
			avSquaredDeviation += sortedPctChange[i][j] * sortedPctChange[i][j] / 10000.0;
		}
		
		numDatesTraded++;
	}

	winrate = 100.0 * (double)wins / (double)numTrades;
	tierate = 100.0 * (double)ties / (double)numTrades;
	lossrate = 100.0 * (double)losses / (double)numTrades;
	avSquaredDeviation = avSquaredDeviation / (double)numTrades;
	double lSR = (balance - 1.0) / sqrt(avSquaredDeviation);
	double tSR = 0.01 * totalGain / sqrt(avSquaredDeviation);
	double avGain = totalGain / (double)numTrades;
	
	printf("\n\nANALYSIS:\n\n");
	printf("Number of Dates Traded: %i\n", numDatesTraded);
	printf("Number of Trades Completed: %i\n", numTrades);
	printf("Average Number of Trades Per Day: %.4f\n", (double)numTrades / (double)numDatesTraded);
	printf("Leverage Used: %.6f\n", leverage);
	printf("Account Balance: %.4f\n", balance);
	printf("Wins: %i\n", wins);
	printf("Ties: %i\n", ties);
	printf("Losses: %i\n", losses);
	printf("Win Rate: %.4f%%\n", winrate);
	printf("Tie Rate: %.4f%%\n", tierate);
	printf("Loss Rate: %.4f%%\n", lossrate);
	printf("Sum of Trade Results: %.6f%%\n", totalGain);
	printf("Average Trade Result: %.6f%%\n", avGain);
	printf("Average Squared Deviation (Variance): %.6f%%\n", avSquaredDeviation * 100.0);
	printf("Standard Deviation: %.6f%%\n", sqrt(avSquaredDeviation) * 100.0);
	printf("Leveraged Account Gain Sharpe Ratio: %.6f\n", lSR);
	printf("Total Trade Gain Sharpe Ratio: %.6f\n", tSR);
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
		for (int j = 1; j < numBars[i]; j++) {
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
	for(int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numBars[i]; j++) {
			if (pctGap[i][j] >= 100.00 || pctGap[i][j] <= -100.00) { printf("Symbol %i with ticker %s at bar %i has an outlying daily gap of %.2f percent.\n", i, ticker[i], j, pctGap[i][j]);}
			if (pctChange[i][j] >= 100.00 || pctChange[i][j] <= -100.00) { printf("Symbol %i with ticker %s at bar %i has an outlying daily change of %.2f percent.\n", i, ticker[i], j, pctChange[i][j]); }

			// print the first outlying close price and ticker symbol of the stocks with at least one daily close greater than $10000
			// note: greater than $100000s have all been removed except BRK.A
			if (close[i][j] >= 1000000) { printf("Symbol %i with ticker %s at bar %i has an outlying close of $%.2f.\n", i, ticker[i], j, (double)close[i][j] / 100.0); break; }
		}
	}
	printf("\n");
}

// print the average values for every date with at least 1 bar
void printAverages(int startingDate, int endingDate) {
	int d = 0;

	if (startingDate < 20000000) startingDate = 20000000;
	if (startingDate >= 20300000) startingDate = 20299999;
	if(endingDate < 20000000) endingDate = 20000000;
	if (endingDate >= 20300000) endingDate = 20299999;

	for (int i = startingDate; i <= endingDate; i++) {
		d = dateToIndex(i);
		if (numBarsDate[d] > 0) {
			printf("Date: %i, Number of Bars: %i, Average Change: %f, Average Gap: %f\n", i, numBarsDate[d], avChangeDate[d], avGapDate[d]);
		}
	}
	printf("\n");
}

// print the bar price values for every date
void printNumDateBars(int startingDate, int endingDate) {
	int d = 0;

	if (startingDate < 20000000) startingDate = 20000000;
	if (startingDate >= 20300000) startingDate = 20299999;
	if (endingDate < 20000000) endingDate = 20000000;
	if (endingDate >= 20300000) endingDate = 20299999;

	for (int i = startingDate; i <= endingDate; i++) {
		d = dateToIndex(i);
		if (numBarsDate[d] > 0) {
			printf("Date: %i, Number of Bars: %i\n", i, numBarsDate[d]);
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
		printf("Symbol Index: %i, Ticker %s, Number of Bars: %i\n", i, ticker[i], numBars[i]);
	}
	printf("\n");
}

// print the bar changes and gaps for every date
void printAllDateBars(int startingDate, int endingDate) {
	int d = 0;

	if (startingDate < 20000000) startingDate = 20000000;
	if (startingDate >= 20300000) startingDate = 20299999;
	if (endingDate < 20000000) endingDate = 20000000;
	if (endingDate >= 20300000) endingDate = 20299999;

	for (int i = startingDate; i <= endingDate; i++) {
		d = dateToIndex(i);
		if (numBarsDate[d] > 0) {
			for (int j = 0; j < numBarsDate[d]; j++) {
				printf("Date %i, Bar %i: Change: %.2f%%, Gap: %.2f%%, Standardized Change: %.2f%%, Standardized Gap: %.2f%%\n", i, j, sortedPctChange[d][j], sortedPctGap[d][j], sortedStdChange[d][j], sortedStdGap[d][j]);
			}
		}
	}
	printf("Number of symbols: %i\n\n", numSymbols);
}

// print the bar price values for every symbol
void printAllSymbolBars(int startingSymbol, int endingSymbol, int startingDate, int endingDate) {

	if (startingSymbol < 0) startingSymbol = 0;
	if (startingSymbol >= numSymbols) startingSymbol = numSymbols - 1;
	if (endingSymbol < 0) endingSymbol = 0;
	if (endingSymbol >= numSymbols) endingSymbol = numSymbols - 1;

	for (int i = startingSymbol; i <= endingSymbol; i++) {
		printf("Symbol %i\n", i);
		for (int j = 0; j < numBars[i]; j++) {
			if (date[i][j] >= startingDate && date[i][j] <= endingDate) {
				printf("Bar %i (%i): %i %i %i %i, Change: %.2f%%, Gap: %.2f%%\n", j, date[i][j], open[i][j], high[i][j], low[i][j], close[i][j], pctChange[i][j], pctGap[i][j]);
			}
		}
		printf("\n");
	}
	printf("Number of symbols: %i\n\n", numSymbols);
}

// setup the file reader, read, reverse (if daily bars are sorted in descending date order), and interpret all the data
char gatherData(char reverse) {

	printf("Setting up file reader... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	setup();

	printf("Reading data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	for (int i = 0; i < numFiles; i++) {
		if (!readCSV(i)) { return 0; }
		getDataFromText();
	}

	if (reverse) {
		printf("Reversing data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

		reverseData();
	}

	printf("Getting important statistics... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	getStats();

	printf("Finished gathering data.\n\n");
	
	return 1;
}

int main(void) {

	if (!gatherData(1)) {
		return -1;
	}

	//printAllSymbolBars(0, 3, 20240102, 20240106);
	//printAllDateBars(20240102, 20240106);

	//printAverages(20240101, 20241231);

	//printNumSymbolBars(0, 10000);
	//printNumDateBars(20240101, 20241231);

	//findOutliers();

	//testBacktesting();

	backtest(20100101, 20260101, 0.01, -2, -1, 1);

	return 0;
}
