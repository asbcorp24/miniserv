#pragma once
#include "Arduino.h"
class Binary
{
public:
	Binary();
	~Binary();
	float ReadFloat();
	short ReadShort();
	int ReadInt();
	ulong ReadUlong();
	uint16_t ReadUShort();
	uint16_t ReadUShortBE();
	unsigned long long ReadUll();
	byte ReadByte();
	String ReadString();
	void WriteFloat(float value);
	void WriteShort(short value);
	void WriteUshort(ushort value);
	void WriteUshortBE(ushort value);
	void WriteInt(int value);
	void WriteUlong(ulong value);
	void WriteUll(unsigned long long value);
	void WriteByte(byte value);
	void WriteString(String value);
	byte* Buffer;
	int BufferPosition, Lenght;
	String ULLtoStr(unsigned long long ULL);
	void Flush(bool ResetPos);
	union FloatBts
	{
		float Value;
		byte Bytes[4];
	};
	union ShortBts
	{
		short Value;
		byte Bytes[2];
	};
	union UShortBts
	{
		ushort Value;
		byte Bytes[2];
	};
	union IntBts
	{
		int Value;
		byte Bytes[4];
	};
	union UlongBts
	{
		ulong Value;
		byte Bytes[8];
	};
	union UllBts
	{
		unsigned long long Value;
		byte Bytes[8];
	};
};

