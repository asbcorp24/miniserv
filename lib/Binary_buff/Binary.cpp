#include "Binary.h"
#include "Arduino.h"
byte* Buffer;
int BufferPosition = 0, Lenght = 0;;
byte stringLenght;
String readedString;
Binary::FloatBts FlBtsEx = Binary::FloatBts();
Binary::ShortBts ShortBtsEx = Binary::ShortBts();
Binary::IntBts IntBtsEx = Binary::IntBts();
Binary::UlongBts UlongBtsEx = Binary::UlongBts();
Binary::UShortBts UShortBtsEx = Binary::UShortBts();
Binary::UllBts UllBtsEx = Binary::UllBts();

Binary::Binary()
{
}

Binary::~Binary()
{
}
void Binary::Flush(bool ResetPos)
{
	Lenght = BufferPosition;
	if (ResetPos)
	{
		BufferPosition = 0;
	}
}
uint16_t Binary::ReadUShort()
{
	for (size_t i = 0; i < 2; i++)
	{
		UShortBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return UShortBtsEx.Value;
}
uint16_t Binary::ReadUShortBE()
{
	BufferPosition += 1;
	for (size_t i = 0; i < 2; i++)
	{
		UShortBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition--;
	}
	BufferPosition += 3;
	return UShortBtsEx.Value;
}
String Binary::ULLtoStr(unsigned long long ULL)
{
	char str[20];
	sprintf(str, "%llu", ULL);
	return (String)str;
}
float Binary::ReadFloat()
{
	for (size_t i = 0; i < 4; i++)
	{
		FlBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return FlBtsEx.Value;
}
short Binary::ReadShort()
{
	for (size_t i = 0; i < 2; i++)
	{
		ShortBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return ShortBtsEx.Value;
}
int Binary::ReadInt()
{
	for (size_t i = 0; i < 4; i++)
	{
		IntBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return IntBtsEx.Value;
}
ulong Binary::ReadUlong()
{
	for (size_t i = 0; i < 8; i++)
	{
		UlongBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return UlongBtsEx.Value;
}
unsigned long long Binary::ReadUll()
{
	for (size_t i = 0; i < 8; i++)
	{
		UllBtsEx.Bytes[i] = Buffer[BufferPosition];
		BufferPosition++;
	}
	return UllBtsEx.Value;
}
byte Binary::ReadByte()
{
	BufferPosition++;
	return Buffer[BufferPosition - 1];
}
String Binary::ReadString()
{
	readedString = "";
	stringLenght = ReadShort();
	for (size_t i = 0; i < stringLenght; i++)
	{
		readedString += (char)Buffer[BufferPosition];
		BufferPosition++;
	}
	return readedString;

}
void Binary::WriteFloat(float value)
{
	FlBtsEx.Value = value;
	for (size_t i = 0; i < 4; i++)
	{
		Buffer[BufferPosition] = FlBtsEx.Bytes[i];
		BufferPosition++;
	}
}
void Binary::WriteShort(short value)
{
	ShortBtsEx.Value = value;
	for (size_t i = 0; i < 2; i++)
	{
		Buffer[BufferPosition] = ShortBtsEx.Bytes[i];
		BufferPosition++;
	}

}
void Binary::WriteUshort(ushort value)
{
	UShortBtsEx.Value = value;
	for (size_t i = 0; i < 2; i++)
	{
		Buffer[BufferPosition] = UShortBtsEx.Bytes[i];
		BufferPosition++;
	}
}
void Binary::WriteUshortBE(ushort value)
{
	UShortBtsEx.Value = value;
	Buffer[BufferPosition] = UShortBtsEx.Bytes[1];
	BufferPosition++;
	Buffer[BufferPosition] = UShortBtsEx.Bytes[0];
	BufferPosition++;
}
void Binary::WriteInt(int value)
{
	IntBtsEx.Value = value;
	for (size_t i = 0; i < 4; i++)
	{
		Buffer[BufferPosition] = IntBtsEx.Bytes[i];
		BufferPosition++;
	}

}
void Binary::WriteUlong(ulong value)
{
	UlongBtsEx.Value = value;
	for (size_t i = 0; i < 8; i++)
	{
		Buffer[BufferPosition] = UlongBtsEx.Bytes[i];
		BufferPosition++;
	}
}
void Binary::WriteUll(unsigned long long value)
{
	UllBtsEx.Value = value;
	for (size_t i = 0; i < 8; i++)
	{
		Buffer[BufferPosition] = UllBtsEx.Bytes[i];
		BufferPosition++;
	}
}
void Binary::WriteByte(byte value)
{
	Buffer[BufferPosition] = value;
	BufferPosition++;
}
void Binary::WriteString(String value)
{
	WriteUshort((ushort)value.length());
	for (size_t i = 0; i < value.length(); i++)
	{
		Buffer[BufferPosition] = (byte)value[i];
		BufferPosition++;
	}
}
