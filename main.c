#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define numFiles 487

#define numCharsPerFile 5000000

char text[numCharsPerFile];
int textSpot;

#include "main.h"

// low values to denote holes in rectangular arrays
int DUMMY_INT = 0;
double DUMMY_DOUBLE = 0.0;

// info for file reading
int lastNewlinePos = 0;
int nextNewlinePos = 0;
char* bar = NULL;
int barLength = 0;
int bs = 0;

// maximum number of symbols, for memory allocation
#define maxNumSymbols 6000
// number of symbols
int numSymbols = 0;

// number of bars for each symbol
int* numBarsBySymbol;

// number of bars in each date
int* numBarsByDate;
int* numAvailableSymbolsDate;

// number of date formats in range from Jan 1, 2000 to Dec 31, 2029
#define rangeDates 300000
// largest number of dates possible
#define maxNumDates 5000
// number of dates encompassed by all bars in the dataset
int numDates = 0;

// number of bars per date, for all possible date formats
int dateIsPresent[rangeDates];

// date/index conversion
int dateToIndex[rangeDates];
int indexToDate[rangeDates];

// info for file data interpreting
int currentSymbol = 0;
int currentBar = 0;
char endOfText = 0;
char currentTicker[5] = { ' ', ' ', ' ', ' ', ' ' };
int currentDate = 0;
int currentNumber[7] = { 0,0,0,0,0,0,0 };

// raw symbol price data ordered by symbol
char** tickerRaw;
int** dateRaw;
int** openRaw;
int** highRaw;
int** lowRaw;
int** closeRaw;
int** volumeRaw;
double** pctChangeRaw;
double** pctGapRaw;
double** stdChangeRaw;
double** stdGapRaw;

// raw symbol price data ordered by date
int** open;
int** high;
int** low;
int** close;
int** volume;
double** pctChange;
double** pctGap;
double** stdChange;
double** stdGap;

// first and last date recorded for each symbol's data
int* firstDate;
int* lastDate;

// sum of all changes, sum of gaps
double* totalChangeDate;
double* totalGapDate;

// average of all changes and gaps in each date
double* avChangeDate;
double* avGapDate;

// changes and gaps (original and standardized) sorted from highest to lowest
double** sortedPctChange;
double** sortedPctGap;
double** sortedStdChange;
double** sortedStdGap;

// input (entries) and output (exits) data of trades
double** entries;
double** exits;

int dateIndexStart = 0;
int dateIndexEnd = 0;

// get the indices within the sorted arrays corresponding to the user-provided starting and ending dates
void getDateIndices(int dateStart, int dateEnd) {
	dateStart -= 20000000;
	dateEnd -= 20000000;
	int ds = dateStart;
	int de = dateEnd;
	if (dateStart >= indexToDate[numDates - 1]) ds = indexToDate[numDates - 1];
	if (dateStart <= indexToDate[0]) ds = indexToDate[0];
	if (dateEnd >= indexToDate[numDates - 1]) de = indexToDate[numDates - 1];
	if (dateEnd <= indexToDate[0]) de = indexToDate[0];
	while (dateToIndex[dateStart] == -1) {
		dateStart++;
		if (dateStart >= numDates) {
			printf("Starting date %i was too large.\n", dateStart + 20000000);
			exit(1);
		}
	}
	while (dateToIndex[dateEnd] == -1) {
		dateEnd--;
		if (dateEnd < 0) {
			printf("Ending date %i was too small.\n", dateEnd + 20000000);
			exit(1);
		}
	}
	dateIndexStart = dateToIndex[dateStart];
	dateIndexEnd = dateToIndex[dateEnd];
	if (dateIndexStart == -1) {
		printf("Starting date index was not found.\n");
		exit(1);
	}
	if (dateIndexStart < 0) {
		printf("Starting date index was less than 0.\n");
		exit(1);
	}
	if (dateIndexStart >= numDates) {
		printf("Starting date index was greater than the number of dates present.\n");
		exit(1);
	}
	if (dateIndexEnd == -1) {
		printf("Ending date index was not found.\n");
		exit(1);
	}
	if (dateIndexEnd < 0) {
		printf("Ending date index was less than 0.\n");
		exit(1);
	}
	if (dateIndexEnd >= numDates) {
		printf("Ending date index was greater than the number of dates present.\n");
		exit(1);
	}
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

	//currentDate = (bar[bst] - 48) * 10000000 + (bar[bst + 1] - 48) * 1000000 + (bar[bst + 2] - 48) * 100000 + (bar[bst + 3] - 48) * 10000 + (bar[bst + 5] - 48) * 1000 + (bar[bst + 6] - 48) * 100 + (bar[bst + 8] - 48) * 10 + (bar[bst + 9] - 48);
	currentDate = (bar[bst + 2] - 48) * 100000 + (bar[bst + 3] - 48) * 10000 + (bar[bst + 5] - 48) * 1000 + (bar[bst + 6] - 48) * 100 + (bar[bst + 8] - 48) * 10 + (bar[bst + 9] - 48);
	dateIsPresent[currentDate]++;
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
			numBarsBySymbol[currentSymbol] = currentBar;
		}
		currentBar = 0;
		currentSymbol++;
		for (int i = 0; i < 5; i++) {
			tickerRaw[currentSymbol][i] = currentTicker[i];
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
	if (getNextNumber(6, -2)) {
		return 1;
	}

	// set the data that was just read for this symbol and bar
	dateRaw[currentSymbol][currentBar] = currentDate;
	openRaw[currentSymbol][currentBar] = currentNumber[0];
	highRaw[currentSymbol][currentBar] = currentNumber[1];
	lowRaw[currentSymbol][currentBar] = currentNumber[2];
	closeRaw[currentSymbol][currentBar] = currentNumber[3];
	volumeRaw[currentSymbol][currentBar] = currentNumber[6];

	// update first and last bar within this symbol for by date data arranging later
	if (currentDate < firstDate[currentSymbol]) firstDate[currentSymbol] = currentDate;
	if (currentDate > lastDate[currentSymbol]) lastDate[currentSymbol] = currentDate;

	currentBar++;
	return 0;
}

// read and parse every text line individually
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
	int i = 0, j = 0;

	for (i = 0; i < numCharsPerFile; i++) {
		text[i] = 0;
	}

	for (i = 0; i < rangeDates; i++) {
		dateIsPresent[i] = 0;
	}

	numBarsBySymbol = (int*)calloc(maxNumSymbols, sizeof(int));

	firstDate = (int*)calloc(maxNumSymbols, sizeof(int));
	lastDate = (int*)calloc(maxNumSymbols, sizeof(int));

	currentSymbol = -1;
	currentBar = 0;

	tickerRaw = (char**)calloc(maxNumSymbols, sizeof(char*));

	dateRaw = (int**)calloc(maxNumSymbols, sizeof(int*));

	openRaw = (int**)calloc(maxNumSymbols, sizeof(int*));
	highRaw = (int**)calloc(maxNumSymbols, sizeof(int*));
	lowRaw = (int**)calloc(maxNumSymbols, sizeof(int*));
	closeRaw = (int**)calloc(maxNumSymbols, sizeof(int*));
	volumeRaw = (int**)calloc(maxNumSymbols, sizeof(int*));

	pctChangeRaw = (double**)calloc(maxNumSymbols, sizeof(double*));
	pctGapRaw = (double**)calloc(maxNumSymbols, sizeof(double*));

	stdChangeRaw = (double**)calloc(maxNumSymbols, sizeof(double*));
	stdGapRaw = (double**)calloc(maxNumSymbols, sizeof(double*));

	for (i = 0; i < maxNumSymbols; i++) {
		tickerRaw[i] = (char*)calloc(5, sizeof(char));
		for (int j = 0; j < 5; j++) {
			tickerRaw[i][j] = ' ';
		}

		dateRaw[i] = (int*)calloc(maxNumDates, sizeof(int));

		openRaw[i] = (int*)calloc(maxNumDates, sizeof(int));
		highRaw[i] = (int*)calloc(maxNumDates, sizeof(int));
		lowRaw[i] = (int*)calloc(maxNumDates, sizeof(int));
		closeRaw[i] = (int*)calloc(maxNumDates, sizeof(int));
		volumeRaw[i] = (int*)calloc(maxNumDates, sizeof(int));

		pctChangeRaw[i] = (double*)calloc(maxNumDates, sizeof(double));
		pctGapRaw[i] = (double*)calloc(maxNumDates, sizeof(double));

		stdChangeRaw[i] = (double*)calloc(maxNumDates, sizeof(double));
		stdGapRaw[i] = (double*)calloc(maxNumDates, sizeof(double));

		for (j = 0; j < maxNumDates; j++) {
			pctChangeRaw[i][j] = DUMMY_DOUBLE;
			pctGapRaw[i][j] = DUMMY_DOUBLE;
			stdChangeRaw[i][j] = DUMMY_DOUBLE;
			stdGapRaw[i][j] = DUMMY_DOUBLE;
		}
	}
}

void swap(int* a, int n, int s) {
	int pl = a[s];
	a[s] = a[n - 1 - s];
	a[n - 1 - s] = pl;
}

void reverseData() {
	for (int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numBarsBySymbol[i] / 2; j++) {
			swap(dateRaw[i], numBarsBySymbol[i], j);
			swap(openRaw[i], numBarsBySymbol[i], j);
			swap(highRaw[i], numBarsBySymbol[i], j);
			swap(lowRaw[i], numBarsBySymbol[i], j);
			swap(closeRaw[i], numBarsBySymbol[i], j);
			swap(volumeRaw[i], numBarsBySymbol[i], j);
			swap(pctChangeRaw[i], numBarsBySymbol[i], j);
			swap(pctGapRaw[i], numBarsBySymbol[i], j);
			swap(stdChangeRaw[i], numBarsBySymbol[i], j);
			swap(stdGapRaw[i], numBarsBySymbol[i], j);
		}
	}
}

// sort a pair of arrays from highest to lowest: sort in[] while ordering out[] so that in[i] still corresponds to out[i]
void sort(double** in, double** out, int exitOffset) {
	if (exitOffset < 0 || exitOffset > 10) {
		printf("Trade exit data offset must be between 0 and 10.\n");
		exit(1);
	}

	double pl = 0.0;
	entries = (double**)calloc(numSymbols, sizeof(double));
	exits = (double**)calloc(numSymbols, sizeof(double));
	int i = 0, j = 0, k = 0;
	int limit = numDates - exitOffset;

	// fill the arrays that determine trade entries and exits
	for (i = 0; i < numSymbols; i++) {
		entries[i] = (double*)calloc(numDates, sizeof(double));
		for (j = 0; j < numDates; j++) {
			entries[i][j] = in[i][j];
		}
		exits[i] = (double*)calloc(numDates, sizeof(double));
		for (j = 0; j < limit; j++) {
			exits[i][j] = out[i][j + exitOffset];
		}
		for (j = limit; j < numDates; j++) {
			exits[i][j] = out[i][numDates - 1];
		}
	}

	// sort entries and exits
	for (i = 0; i < numDates; i++) {

		for (j = 0; j < numBarsByDate[i]; j++) {
			for (k = j + 1; k < numBarsByDate[i]; k++) {
				if (entries[j][i] < entries[k][i]) {
					pl = entries[j][i];
					entries[j][i] = entries[k][i];
					entries[k][i] = pl;

					pl = exits[j][i];
					exits[j][i] = exits[k][i];
					exits[k][i] = pl;
				}
			}
		}
	}
}

// get every data measurement
void getStats(int minVolume, int maxVolume) {
	int i = 0, j = 0, d = 0, n = 0;

	totalChangeDate = (double*)calloc(numDates, sizeof(double));
	totalGapDate = (double*)calloc(numDates, sizeof(double));
	numAvailableSymbolsDate = (int*)calloc(numDates, sizeof(int));

	avChangeDate = (double*)calloc(numDates, sizeof(double));
	avGapDate = (double*)calloc(numDates, sizeof(double));

	// get changes and gaps by both symbols and dates
	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numBarsBySymbol[i]; j++) {

			d = dateToIndex[dateRaw[i][j]];

			totalChangeDate[d] += pctChangeRaw[i][j];
			totalGapDate[d] += pctGapRaw[i][j];

			if (openRaw[i][j] > 0) {
				pctChangeRaw[i][j] = 100.0 * (double)(closeRaw[i][j] - openRaw[i][j]) / (double)openRaw[i][j];
				pctChange[i][d] = pctChangeRaw[i][j];
			}
			else {
				pctChangeRaw[i][j] = 0.0;
				pctChange[i][d] = 0.0;
			}

			if (j == 0) {
				pctGapRaw[i][j] = 0.0;
				pctGap[i][d] = 0.0;
			}
			else {
				if (closeRaw[i][j - 1] > 0) {
					pctGapRaw[i][j] = 100.0 * (double)(openRaw[i][j] - closeRaw[i][j - 1]) / (double)closeRaw[i][j - 1];
					pctGap[i][d] = pctChangeRaw[i][d];
				}
				else {
					pctGapRaw[i][j] = 0.0;
					pctGap[i][d] = 0.0;
				}
			}
		}
	}

	// initialize the date arrays which will be filled in sorted order
	sortedPctChange = (double**)calloc(numSymbols, sizeof(double));
	sortedPctGap = (double**)calloc(numSymbols, sizeof(double));
	sortedStdChange = (double**)calloc(numSymbols, sizeof(double));
	sortedStdGap = (double**)calloc(numSymbols, sizeof(double));

	for (i = 0; i < numSymbols; i++) {
		sortedPctChange[i] = (double*)calloc(numBarsBySymbol[i], sizeof(double));
		sortedPctGap[i] = (double*)calloc(numBarsBySymbol[i], sizeof(double));
		sortedStdChange[i] = (double*)calloc(numBarsBySymbol[i], sizeof(double));
		sortedStdGap[i] = (double*)calloc(numBarsBySymbol[i], sizeof(double));
	}

	// find the average changes and gaps on each date
	for (i = 0; i < numDates; i++) {
		if (numBarsByDate[i] > 0) {
			avChangeDate[i] = totalChangeDate[i] / (double)numBarsByDate[i];
			avGapDate[i] = totalGapDate[i] / (double)numBarsByDate[i];
		}
		else {
			avChangeDate[i] = 0.0;
			avGapDate[i] = 0.0;
		}
	}

	// for each bar within each symbol
	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numBarsBySymbol[i]; j++) {

			d = dateToIndex[dateRaw[i][j]];
			printf("%i %i %i\n", i, j, d);

			// get the standardized change and gap values for each bar by subtracting the average change and gap values on the bar's date from the bar's change and gap values
			stdChangeRaw[i][j] = pctChangeRaw[i][j] - avChangeDate[d];
			stdGapRaw[i][j] = pctGapRaw[i][j] - avGapDate[d];

			if (numAvailableSymbolsDate[d] >= numBarsByDate[d]) {
				printf("ERROR: Tried to store more bars (limit: %i) on date %i (index %i) than accepted.\n", numBarsByDate[d], dateRaw[i][j] + 20000000, d);
				exit(1);
			}

			// skip empty bars
			if (pctChangeRaw[i][j] == DUMMY_DOUBLE) continue;
			if (pctGapRaw[i][j] == DUMMY_DOUBLE) continue;
			if (stdChangeRaw[i][j] == DUMMY_DOUBLE) continue;
			if (stdGapRaw[i][j] == DUMMY_DOUBLE) continue;

			// place the bar's change and gap among the bars with the same date in change and gap arrays (in sorted order), if the previous day's volume was big enough
			int previousVolume = 0;
			if (j > 0) {
				previousVolume = volumeRaw[i][j - 1];
			}
			if (previousVolume >= minVolume && previousVolume <= maxVolume) {

				n = numAvailableSymbolsDate[d];

				if (n >= numBarsBySymbol[i]) {
					printf("Number of symbols detected on date %i (index %i) was %i and exceeded the maximum allocated of %i.\n", dateRaw[i][j] + 20000000, d, n, numBarsBySymbol[i]);
					exit(1);
				}
				sortedPctChange[n][d] = pctChangeRaw[i][j];
				sortedPctGap[n][d] = pctGapRaw[i][j];
				sortedStdChange[n][d] = stdChangeRaw[i][j];
				sortedStdGap[n][d] = stdGapRaw[i][j];

				// set the number of symbols with high enough volume on this day
				numAvailableSymbolsDate[d]++;
			}
		}
	}
}

// backtest the chosen strategy
double backtest(int dateStart, int dateEnd, double leverage, int startingBarRank, int endingBarRank, char skipIfNotEnoughBars, char printResults) {

	if (leverage <= 0.0 || leverage > 100.0) {
		printf("Backtesting leverage must be greater than 0 and at most 100.\n\n");
		exit(1);
	}
	if (dateStart > dateEnd) {
		printf("Starting date must be less than or equal to ending date.\n\n");
		exit(1);
	}

	if (printResults) {
		printf("Running the backtest from %i to %i... (This may take a few seconds to minutes depending on the amount of data present.)\n\n", dateStart, dateEnd);
	}

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
	double avWin = 0.0;
	double avLoss = 0.0;

	double n = 0.0;
	double ln = 0.0;

	int barEnd = 0;

	int numDatesTraded = 0;
	int numDatesObserved = 0;
	int totalNumSymbols = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = dateIndexStart; i <= dateIndexEnd; i++) {

		int x = numAvailableSymbolsDate[i];
		double e = 0;

		if (x < 1) continue;

		numDatesObserved++;
		totalNumSymbols += x;

		int starting = startingBarRank;
		int ending = endingBarRank;
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

		// trade on selected bars this day
		for (int j = starting; j <= ending; j++) {
			e = exits[j][i];

			// calculate what to multiply account by as a result of this trade
			n = (100.0 + e) / 100.0;
			ln = (100.0 + leverage * e) / 100.0;

			// evaluate outcome
			if (n > 1.0) {
				wins++;
				avWin += e;
			}
			else {
				if (n < 1.0) {
					losses++;
					avLoss += e;
				}
				else {
					ties++;
				}
			}
			balance *= ln;
			numTrades++;
			totalGain += e;
			avSquaredDeviation += e * e / 10000.0;
		}
		
		numDatesTraded++;
	}

	winrate = 100.0 * (double)wins / (double)numTrades;
	tierate = 100.0 * (double)ties / (double)numTrades;
	lossrate = 100.0 * (double)losses / (double)numTrades;
	avSquaredDeviation = avSquaredDeviation / (double)numTrades;
	double lSR = (balance - 1.0) / sqrt(avSquaredDeviation);
	double tSR = 0.01 * totalGain / sqrt(avSquaredDeviation);
	double avTrade = totalGain / (double)numTrades;

	if (printResults) {
		printf("\n\nANALYSIS:\n\n");
		printf("Number of Dates Considered For Trading: %i\n", numDatesObserved);
		printf("Number of Dates Traded: %i\n", numDatesTraded);
		printf("Number of Trades Completed: %i\n", numTrades);
		printf("Average Number of Trades Per Day Traded: %.4f\n", (double)numTrades / (double)numDatesTraded);
		printf("Average Number of Symbols Available Per Day Traded: %.4f\n", (double)totalNumSymbols / (double)numDatesTraded);
		printf("Leverage Used: %.6f\n", leverage);
		printf("Account Balance: %.4f\n", balance);
		printf("Wins: %i\n", wins);
		printf("Ties: %i\n", ties);
		printf("Losses: %i\n", losses);
		printf("Win Rate: %.4f%%\n", winrate);
		printf("Tie Rate: %.4f%%\n", tierate);
		printf("Loss Rate: %.4f%%\n", lossrate);
		printf("Total of Wins: %.6f%%\n", avWin);
		printf("Total of Losses: %.6f%%\n", avLoss);
		printf("Average Win Result: %.6f%%\n", avWin / (double)wins);
		printf("Average Loss Result: %.6f%%\n", avLoss / (double)losses);
		printf("Average Trade Result: %.6f%%\n", totalGain / (double)numTrades);
		printf("Sum of Trade Results: %.6f%%\n", totalGain);
		printf("Average Squared Deviation (Variance): %.6f%%\n", avSquaredDeviation * 100.0);
		printf("Standard Deviation: %.6f%%\n", sqrt(avSquaredDeviation) * 100.0);
		printf("Leveraged Account Gain Sharpe Ratio: %.6f\n", lSR);
		printf("Total Trade Gain Sharpe Ratio: %.6f\n\n\n", tSR);
	}
	return balance;
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
		for (int j = 1; j < numBarsBySymbol[i]; j++) {
			if (closeRaw[i][j] > openRaw[i][j]) {
				wins++;
				won += closeRaw[i][j] - openRaw[i][j];
			}
			if (closeRaw[i][j] < openRaw[i][j]) {
				losses++;
				lost += closeRaw[i][j] - openRaw[i][j];
			}
			if (closeRaw[i][j] == openRaw[i][j]) {
				ties++;
			}
			total++;
		}
	}
	printf("Basic stat backtesting: Wins: %i, Losses: %i, Won: %f, Lost: %f, Ties: %i, Total: %i\n\n", wins, losses, won, lost, ties, total);
}

// print outlying bar values
void findOutliers() {
	double n = 0;
	for(int i = 0; i < numSymbols; i++) {
		for (int j = 0; j < numBarsBySymbol[i]; j++) {
			n = sortedPctGap[i][j];
			if (n >= 100.00 || n <= -100.00) { printf("Symbol %i with ticker %s at bar %i has an outlying daily gap of %.2f percent.\n", i, tickerRaw[i], j, sortedPctGap[i][j]);}
			n = sortedPctChange[i][j];
			if (n >= 100.00 || n <= -100.00) { printf("Symbol %i with ticker %s at bar %i has an outlying daily change of %.2f percent.\n", i, tickerRaw[i], j, sortedPctChange[i][j]); }

			// print the first outlying close price and ticker symbol of the stocks with at least one daily close greater than $10000
			// note: greater than $100000s have all been removed except BRK.A
			if (closeRaw[i][j] >= 1000000) { printf("Symbol %i with ticker %s at bar %i has an outlying close of $%.2f.\n", i, tickerRaw[i], j, (double)closeRaw[i][j] / 100.0); break; }
		}
	}
	printf("\n");
}

// print the average values for every date with at least 1 bar
void printAverages(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = dateIndexStart; i <= dateIndexEnd; i++) {
		d = indexToDate[i];
		if (numBarsByDate[d] > 0) {
			printf("Date: %i, Number of Bars: %i, Average Change: %f, Average Gap: %f\n", d + 20000000, numBarsByDate[i], avChangeDate[i], avGapDate[i]);
		}
	}
	printf("\n");
}

// print the bar price values for every date
void printNumDateBars(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = dateIndexStart; i <= dateIndexEnd; i++) {
		d = indexToDate[i];
		if (numBarsByDate[d] > 0) {
			printf("Date: %i, Number of Bars: %i\n", d + 20000000, numBarsByDate[i]);
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
		printf("Symbol Index: %i, Ticker %s, Number of Bars: %i\n", i, tickerRaw[i], numBarsBySymbol[i]);
	}
	printf("\n");
}

// print the bar changes and gaps for every date
void printAllDateBars(int dateStart, int dateEnd) {
	int d = 0;

	getDateIndices(dateStart, dateEnd);

	for (int i = dateIndexStart; i <= dateIndexEnd; i++) {
		d = indexToDate[i];
		if (numBarsByDate[d] > 0) {
			for (int j = 0; j < numBarsByDate[i]; j++) {
				printf("Date %i, Bar %i: Change: %.2f%%, Gap: %.2f%%, Standardized Change: %.2f%%, Standardized Gap: %.2f%%\n", d + 20000000, j, sortedPctChange[d][j], sortedPctGap[d][j], sortedStdChange[d][j], sortedStdGap[d][j]);
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
		for (int j = 0; j < numBarsBySymbol[i]; j++) {
			if (dateRaw[i][j] >= startingDate && dateRaw[i][j] <= endingDate && pctChangeRaw[i][j] > DUMMY_DOUBLE && pctGapRaw[i][j] > DUMMY_DOUBLE) {
				printf("Bar %i (%i): %i %i %i %i, Change: %.2f%%, Gap: %.2f%%\n", j, dateRaw[i][j], openRaw[i][j], highRaw[i][j], lowRaw[i][j], closeRaw[i][j], pctChangeRaw[i][j], pctGapRaw[i][j]);
			}
		}
		printf("\n");
	}
	printf("Number of symbols: %i\n\n", numSymbols);
}

// create date/index converter, initialize and fill data storage by date
void setupDateData() {
	int i = 0, j = 0, k = 0;

	// create date/index converter
	int spot = 0;
	for (i = 0; i < rangeDates; i++) {
		if (dateIsPresent[i]) {
			dateToIndex[i] = spot;
			indexToDate[spot] = i;
			spot++;
		}
	}
	numDates = spot;

	numBarsByDate = (int*)calloc(numDates, sizeof(int));
	for (i = 0; i < rangeDates; i++) {
		if (dateIsPresent[i]) {
			numBarsByDate[dateToIndex[i]] = dateIsPresent[i];
		}
	}

	open = (int**)calloc(numSymbols, sizeof(int*));
	high = (int**)calloc(numSymbols, sizeof(int*));
	low = (int**)calloc(numSymbols, sizeof(int*));
	close = (int**)calloc(numSymbols, sizeof(int*));
	volume = (int**)calloc(numSymbols, sizeof(int*));

	pctChange = (double**)calloc(numSymbols, sizeof(double*));
	pctGap = (double**)calloc(numSymbols, sizeof(double*));

	stdChange = (double**)calloc(numSymbols, sizeof(double*));
	stdGap = (double**)calloc(numSymbols, sizeof(double*));

	for (i = 0; i < numSymbols; i++) {

		open[i] = (int*)calloc(numDates, sizeof(int));
		high[i] = (int*)calloc(numDates, sizeof(int));
		low[i] = (int*)calloc(numDates, sizeof(int));
		close[i] = (int*)calloc(numDates, sizeof(int));
		volume[i] = (int*)calloc(numDates, sizeof(int));

		pctChange[i] = (double*)calloc(numDates, sizeof(double));
		pctGap[i] = (double*)calloc(numDates, sizeof(double));

		stdChange[i] = (double*)calloc(numDates, sizeof(double));
		stdGap[i] = (double*)calloc(numDates, sizeof(double));
	}

	// fill arrays using raw arrays filled previously
	for (i = 0; i < numSymbols; i++) {
		for (j = 0; j < numBarsBySymbol[i]; j++) {
			
			int d = dateToIndex[dateRaw[i][j]];
			open[i][d] = openRaw[i][j];
			high[i][d] = highRaw[i][j];
			low[i][d] = lowRaw[i][j];
			close[i][d] = closeRaw[i][j];
			volume[i][d] = volumeRaw[i][j];
			pctChange[i][d] = pctChangeRaw[i][j];
			pctGap[i][d] = pctGapRaw[i][j];
			stdChange[i][d] = stdChangeRaw[i][j];
			stdGap[i][d] = stdGapRaw[i][j];
		}

		// fill holes in new arrays
		for (j = firstDate[i] - 1; j >= 0; j--) {
			// copy the first value left to fill in dates before the beginning of this symbol's history
			open[i][j] = open[i][j + 1];
			high[i][j] = high[i][j + 1];
			low[i][j] = low[i][j + 1];
			close[i][j] = close[i][j + 1];
			volume[i][j] = volume[i][j + 1];
			pctChange[i][j] = DUMMY_DOUBLE;
			pctGap[i][j] = DUMMY_DOUBLE;
			stdChange[i][j] = DUMMY_DOUBLE;
			stdGap[i][j] = DUMMY_DOUBLE;
		}
		for (j = firstDate[i] + 1; j < numDates; j++) {
			// copy next values right to fill in holes and dates after symbol history
			if (close[i][j] == 0) {
				open[i][j] = open[i][j - 1];
				high[i][j] = high[i][j - 1];
				low[i][j] = low[i][j - 1];
				close[i][j] = close[i][j - 1];
				volume[i][j] = volume[i][j - 1];
				pctChange[i][j] = DUMMY_DOUBLE;
				pctGap[i][j] = DUMMY_DOUBLE;
				stdChange[i][j] = DUMMY_DOUBLE;
				stdGap[i][j] = DUMMY_DOUBLE;
			}
		}
	}
}

// setup the file reader, read, reverse (if daily bars are sorted in descending date order), and interpret all the data
char gatherData(char reverse, int minVolume, int maxVolume) {

	printf("Setting up file reader... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	setup();

	printf("Reading data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	for (int i = 0; i < numFiles; i++) {
		if (!readCSV(i, text)) { return 0; }
		getDataFromText();
	}

	if (reverse) {
		printf("Reversing data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");
		reverseData();
	}

	setupDateData();

	printf("Getting important statistics... (This may take a few seconds to minutes depending on the amount of data present.)\n\n");

	getStats(minVolume, maxVolume);

	printf("Finished gathering data.\n\n");
	
	return 1;
}

void printBarChart(int dateStart, int dateEnd, int startingBarRank, int endingBarRank, char skipIfNotEnoughBars, double minLeverage, double maxLeverage, int numChartBars, int chartHeight) {
	double leverage = 0.0, result = 1.0, maxResult = -1.0 * pow(2.0, 80.0), minResult = pow(2.0, 80.0), worstLeverage = 0.0, bestLeverage = 0.0;
	double* bars = (double*)calloc(numChartBars, sizeof(double));
	for (int i = 0; i < numChartBars; i++) {
		leverage = ((double)i / (double)(numChartBars - 1)) * (maxLeverage - minLeverage) + minLeverage;
		result = backtest(dateStart, dateEnd, leverage, startingBarRank, endingBarRank, skipIfNotEnoughBars, 0);
		if (result < minResult) {
			minResult = result;
			worstLeverage = leverage;
		}
		if (result > maxResult){
			maxResult = result;
			bestLeverage = leverage;
		}
		bars[i] = result;
	}
	for (int i = 0; i < chartHeight; i++) {
		printf("|");
		for (int j = 0; j < numChartBars; j++) {
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
	printf("\nMinimum Result: %f at leverage %f, Maximum Result: %f at leverage %f.\n\n\n", minResult, worstLeverage, maxResult, bestLeverage);
}

int main(void) {

	DUMMY_INT = INT_MIN;
	DUMMY_DOUBLE = (double)INT_MIN;

	if (!gatherData(1, 4000, 20000)) {
		return -1;
	}

	// set and sort only the input and outcome pointer arrays so that in[i] and out[i] are the same symbol on the same day
	sort(sortedPctGap, sortedPctGap, 1);



	//printAllSymbolBars(0, 3, 20240102, 20240106);
	//printAllDateBars(20240102, 20240106);

	//printAverages(20240101, 20241231);

	//printNumSymbolBars(0, 10000);
	//printNumDateBars(20240101, 20241231);

	//findOutliers();

	//testBacktesting();
	
	backtest(20240101, 20260101, 0.2, -1, -1, 1, 1);

	printBarChart(20240101, 20260101, -1, -1, 1, 0.01, 1.0, 100, 20);

	return 0;
}
