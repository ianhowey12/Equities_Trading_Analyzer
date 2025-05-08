#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"

// base addresses for file reading
char address0[] = "C:\\Daily Stock Data\\ .csv";
char address1[] = "C:\\Daily Stock Data\\  .csv";
char address2[] = "C:\\Daily Stock Data\\   .csv";
char address3[] = "C:\\Daily Stock Data\\    .csv";
char address4[] = "C:\\Daily Stock Data\\     .csv";
char addressBaseLength = 20;

// read data from a csv file into text
char readCSV(int fileIndex, char* text) {

	if (fileIndex < 0 || fileIndex >= 100000) {
		printf("Couldn't read file with index %i because out of range [0, 99999].\n", fileIndex);
		exit(1);
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
					address4[b + 3] = ((fileIndex / 10) % 10) + 48;
					address4[b + 4] = (fileIndex % 10) + 48;
					fileAddress = address4;
				}
			}
		}
	}

	fopen_s(&fp, fileAddress, "rb");

	if (fp == NULL) {
		printf("Couldn't read file %s, index %i\n\n", fileAddress, fileIndex);
		exit(1);
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
