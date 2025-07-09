# Equities_Trading_Analyzer
Automated trading strategy backtesting and statistical analysis of daily equities price data.

Includes functions for examining and analyzing daily historical financial data (values, averages, data point quantities), finding outliers, and backtesting customizable strategies based on daily gaps and changes in market prices.

Provides statistics such as winrate, account balance change, variance, standard deviation, and Sharpe ratio in backtesting.

Backtesting and histogram display allow minimum and maximum daily volume requirements. You can also customize the range of dates involved, the account leverage, and the number of symbols to trade each day. Changing btMinSymbolRankDay and btMaxSymbolRankDay changes which symbols are traded on each day. They are ranked in order of their customizable entry condition. Other backtesting settings can be changed within main().

This program requires local CSV files to be stored within "C:\Daily Stock Data\" on your PC's drive.

The first line of each file is assumed to not contain data.

The dates of the price data need to be indicated within each line and need to increase as the line numbers increase and as the file names increase in lexicographical order.

Change the parameter numFiles to the number of files within that folder.

Change columnCodes to indicate the format of each line.
