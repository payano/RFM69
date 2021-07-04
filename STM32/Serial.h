#pragma once

/* Printing is not implemented yet... */

class SerialDebug {
private:
	void *huart;
public:
	SerialDebug(void *huart);
	void print(int val, int type);
	void print(int val);
	void print(char val);
	void print(const char *val);
	void print(float val);

	void println();
	void println(const char *val);
	void println(const char *val, int type);
	void println(int val, int type);
	void println(float val);

	void setTimeout(int val);
	int readBytesUntil(const char, const char*, int len);
};



