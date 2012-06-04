//============================================================================
// Name        : SerializationHelper.h
// Copyright   : DataSoft Corporation 2011-2012
//	Nova is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   Nova is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with Nova.  If not, see <http://www.gnu.org/licenses/>.
// Description : Helper functions for serialization
//============================================================================/*

#ifndef SERIALIZATIONHELPER_H_
#define SERIALIZATIONHELPER_H_

#include "sys/types.h"

#include <string>
#include <exception>

namespace Nova
{

class serializationException : public std::exception {
	  virtual const char* what() const throw()
	  {
	    return "Error when serializing or deserializing";
	  }
};

/* Serializes (simple memcpy) a chunk of data with checking on buffer bounds
 *    buf             : Pointer to buffer location where serialized data should go
 *    offset          : Offset from the buffer (will be incremented by SerializeChunk)
 *    dataToSerialize : Pointer to data to serialize
 *    size            : Size of the data to serialize
 *   maxBufferSize   : Max size of the buffer, throw exception if serialize goes past this
 */
bool SerializeChunk(u_char* buf, uint32_t* offset, char* dataToSerialize, uint32_t size, uint32_t maxBufferSize);
bool DeserializeChunk(u_char* buf, uint32_t* offset, char* deserializeTo, uint32_t size, uint32_t maxBufferSize);


/* Serializes a hash map
 *    buf             : Pointer to buffer location where serialized data should go
 *    offset          : Offset from the buffer (will be incremented by SerializeChunk)
 *    dataToSerialize : Reference to the hash map
 *    nullValue       : Any map entries with this value will be skipped (eg, 0 for bins that store a count)
 *   maxBufferSize    : Max size of the buffer, throw exception if serialize goes past this
 */
template <typename TableType, typename KeyType, typename ValueType>
inline void SerializeHashTable(u_char* buf, uint32_t* offset, TableType& dataToSerialize, KeyType nullValue, uint32_t maxBufferSize)
{
	typename TableType::iterator it = dataToSerialize.begin();
	typename TableType::iterator last = dataToSerialize.end();

	uint32_t count = 0;
	while (it != last)
	{
		if (it->first != nullValue)
		{
			count++;
		}
		it++;
	}

	it = dataToSerialize.begin();

	//The size of the Table
	SerializeChunk(buf, offset, (char*)&count, sizeof count, maxBufferSize);

	while (it != last)
	{
		if (it->first != nullValue)
		{
			SerializeChunk(buf, offset, (char*)&it->first, sizeof it->first, maxBufferSize);
			SerializeChunk(buf, offset, (char*)&it->second, sizeof it->second, maxBufferSize);
		}
		it++;
	}
}

template <typename TableType, typename KeyType, typename ValueType>
inline void DeserializeHashTable(u_char* buf, uint32_t* offset, TableType& deserializeTo, uint32_t maxBufferSize)
{
	uint32_t tableSize = 0;
	ValueType value;
	KeyType key;

	DeserializeChunk(buf, offset, (char*)&tableSize, sizeof tableSize, maxBufferSize);

	for(uint32_t i = 0; i < tableSize;)
	{
		DeserializeChunk(buf, offset, (char*)&key, sizeof key, maxBufferSize);
		DeserializeChunk(buf, offset, (char*)&value, sizeof value, maxBufferSize);

		deserializeTo[key] = value;
		i++;
	}
}

} /* namespace Nova */
#endif /* SERIALIZATIONHELPER_H_ */
