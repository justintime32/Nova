// ============================================================================
// Name        : SuspectTable.h
// Copyright   : DataSoft Corporation 2011-2012
// 	Nova is free software: you can redistribute it and/or modify
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
//   along with Nova.  If not, see <http:// www.gnu.org/licenses/>.
// Description : Wrapper class for the SuspectHashMap object used to maintain a
// 		list of suspects.
// ============================================================================/*

#ifndef SUSPECTTABLE_H_
#define SUSPECTTABLE_H_

#include <arpa/inet.h>
#include <vector>

#include "Suspect.h"

typedef google::dense_hash_map<uint64_t, Nova::Suspect *, std::tr1::hash<uint64_t>, eqkey > SuspectHashTable;
typedef google::dense_hash_map<uint64_t, pthread_rwlock_t *, std::tr1::hash<uint64_t>, eqkey> SuspectLockTable;

namespace Nova {

enum SuspectTableRet : int32_t
{
	KEY_INVALID = -2, //The key cannot be associated with a recognized suspect
	SUSPECT_CHECKED_OUT = -1, //If the suspect is checked out by another thread,
	SUCCESS = 0, //The call succeeded
	SUSPECT_NOT_CHECKED_OUT = 1, //The suspect isn't checked out, only returned by CheckIn()
	SUSPECT_EXISTS = 2, //If the suspect already exists, only returned by AddNewSuspect()
	IS_HOSTILE = -3, //Returned by GetHostility if the suspect is hostile
	IS_BENIGN = 3	//Returned by GetHostility if the suspect is benign
};

class SuspectTable
{

public:

	// Default Constructor for SuspectTable
	SuspectTable();

	// Default Deconstructor for SuspectTable
	~SuspectTable();

	// Adds the Suspect pointed to in 'suspect' into the table using suspect->GetIPAddress() as the key;
	// 		suspect: pointer to the Suspect you wish to add
	// Returns (0) on Success, and (2) if the suspect exists;
	SuspectTableRet AddNewSuspect(Suspect * suspect);

	// Adds the Suspect pointed to in 'suspect' into the table using the source of the packet as the key;
	// 		packet: copy of the packet you whish to create a suspect from
	// Returns (0) on Success, and (2) if the suspect exists;
	SuspectTableRet AddNewSuspect(Packet packet);

	//XXX
	SuspectTableRet AddEvidenceToSuspect(in_addr_t key, Packet packet);

	// Copies the suspect pointed to in 'suspect', into the table at location key
	// 		suspect: pointer to the Suspect you wish to copy in
	// Returns (0) on Success, (-1) if the Suspect is checked out by someone else
	// and (1) if the Suspect is not checked out
	// Note:  This function blocks until it can acquire a write lock on the suspect
	// IP address must be the same as key checked out with
	SuspectTableRet CheckIn(Suspect * suspect);

	// Copies out a suspect and marks the suspect so that it cannot be written or deleted
	// 		key: uint64_t of the Suspect
	// Returns empty Suspect on failure.
	// Note: A 'Checked Out' Suspect cannot be modified until is has been replaced by the suspect 'Checked In'
	// 		However the suspect can continue to be read. It is similar to having a read lock.
	Suspect CheckOut(in_addr_t ipAddr);

	// Lookup and get an Asynchronous copy of the Suspect
	// 		key: uint64_t of the Suspect
	// Returns an empty suspect on failure
	// Note: To modify or lock a suspect use CheckOut();
	// Note: This is the same as GetSuspectStatus except it copies the feature set object which can grow very large.
	Suspect GetSuspect(in_addr_t ipAddr);

	// Lookup and get an Asynchronous copy of the Suspect
	// 		key: uint64_t of the Suspect
	// Returns an empty suspect on failure
	// Note: To modify or lock a suspect use CheckOut();
	// Note: This is the same as GetSuspect except is does not copy the feature set object which can grow very large.
	Suspect GetSuspectStatus(in_addr_t ipAddr);

	// Erases a suspect from the table if it is not locked
	// 		key: uint64_t of the Suspect
	// Returns 0 on success.
	SuspectTableRet Erase(in_addr_t ipAddr);

	// Iterates over the Suspect Table and returns a std::vector containing each Hostile Suspect's uint64_t
	// Returns a std::vector of hostile suspect in_addr_t ipAddrs
	std::vector<uint64_t> GetHostileSuspectKeys();

	// Iterates over the Suspect Table and returns a std::vector containing each Benign Suspect's uint64_t
	// Returns a std::vector of benign suspect in_addr_t ipAddrs
	std::vector<uint64_t> GetBenignSuspectKeys();

	// Looks at the hostility of the suspect at key
	// 		key: uint64_t of the Suspect
	// Returns (3) for Benign, (-3) for Hostile, and (-2) if the key is invalid
	SuspectTableRet GetHostility(in_addr_t ipAddr);

	// Get the size of the Suspect Table
	// Returns the size of the Table
	uint Size();

	// Resizes the table to the given size.
	//		size: the number of bins to use
	// Note: Choosing an initial size that covers normal usage can improve performance.
	void Resize(uint size);

	// Clears the Suspect Table of all entries
	//		blockUntilDone: if this value is set to true the function will block until the table can be safely cleared
	// Returns (0) on Success, and (-1) if the table cannot be cleared safely
	SuspectTableRet Clear(bool blockUntilDone = true);

	// Checks the validity of the key
	//		key: The uint64_t of the Suspect
	// Returns true if there is a suspect associated with the given key, false otherwise
	bool IsValidKey(in_addr_t ipAddr);

	// Saves suspectTable to a human readable text file
	void SaveSuspectsToFile(std::string filename);

	Suspect m_emptySuspect;

//protected:

	// Google Dense Hashmap used for constant time key lookups
	SuspectHashTable m_table;

	// Lock used to maintain concurrency between threads
	pthread_rwlock_t m_lock;

	std::vector<uint64_t> m_keys;

private:

	uint64_t m_empty_key;

	uint64_t m_deleted_key;

};

}

#endif /* SUSPECTTABLE_H_ */
