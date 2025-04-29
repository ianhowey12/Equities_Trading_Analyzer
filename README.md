# Equities_Trading_Analyzer
Automated trading strategy backtesting and statistical analysis of daily equities price data.

Includes functions for examining and analyzing daily historical financial data (values, averages, data point quantities), finding outliers, and backtesting customizable strategies based on daily gaps and changes in market prices.

Provides statistics such as winrate, account balance change, variance, standard deviation, and Sharpe ratio in backtesting.

This program requires local files to be stored within "C:\Daily Stock Data\" on your PC's drive and numbered increasing from 0.

Example: "C:\Daily Stock Data\356.csv".

Change the parameter numFiles to the number of files within that folder.

All historical data files must be stored in CSV format in the order "symbol,year-month-day,open,high,low,close,change,%change,volume" on each line.

Ticker symbols must contain only capital letters and periods and can be up to five characters long.

The first and last lines of each file are assumed to not contain data.
