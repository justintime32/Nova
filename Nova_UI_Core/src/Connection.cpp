//============================================================================
// Name        : Connection.cpp
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
// Description : Manages connections out to Novad, initializes and closes them
//============================================================================

#include "messaging/messages/ControlMessage.h"
#include "messaging/messages/ErrorMessage.h"
#include "messaging/MessageManager.h"
#include "Commands.h"
#include "NovaUtil.h"
#include "Logger.h"
#include "Config.h"
#include "Lock.h"

#include <string>
#include <cerrno>
#include <sys/un.h>
#include <sys/socket.h>
#include "event.h"
#include "event2/thread.h"

using namespace std;
using namespace Nova;
//Socket communication variables
int IPCSocketFD = -1;

struct event_base *libeventBase = NULL;
struct bufferevent *bufferevent = NULL;
pthread_t eventDispatchThread;

namespace Nova
{

void *EventDispatcherThread(void *arg)
{
	if(event_base_dispatch(libeventBase) != 0)
	{
		LOG(WARNING, "Message loop ended uncleanly.", "");
	}
	return NULL;
}

bool ConnectToNovad()
{
	if(IsNovadUp(false))
	{
		return true;
	}

	DisconnectFromNovad();

	//Create new base
	if(libeventBase == NULL)
	{
		evthread_use_pthreads();
		libeventBase = event_base_new();
	}

	//Builds the key path
	string key = Config::Inst()->GetPathHome();
	key += "/keys";
	key += NOVAD_LISTEN_FILENAME;

	//Builds the address
	struct sockaddr_un novadAddress;
	novadAddress.sun_family = AF_UNIX;
	memset(novadAddress.sun_path, '\0', sizeof(novadAddress.sun_path));
	strncpy(novadAddress.sun_path, key.c_str(), sizeof(novadAddress.sun_path));

	bufferevent = bufferevent_socket_new(libeventBase, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	if(bufferevent == NULL)
	{
		LOG(ERROR, "Unable to create a socket to Nova", "");
		DisconnectFromNovad();
		return false;
	}

	bufferevent_setcb(bufferevent, MessageManager::MessageDispatcher, NULL, MessageManager::ErrorDispatcher, NULL);

	bufferevent_enable(bufferevent, EV_READ);

	if(bufferevent_socket_connect(bufferevent, (struct sockaddr *)&novadAddress, sizeof(novadAddress)) < 0)
	{
		bufferevent = NULL;
		LOG(DEBUG, "Unable to connect to NOVAD: "+string(strerror(errno))+".", "");
		return false;
	}

	IPCSocketFD = bufferevent_getfd(bufferevent);
	if(IPCSocketFD == -1)
	{
		LOG(DEBUG, "Unable to connect to NOVAD: "+string(strerror(errno))+".", "");
		bufferevent_free(bufferevent);
		bufferevent = NULL;
		return false;
	}

	evutil_make_socket_nonblocking(IPCSocketFD);

	MessageManager::Instance().DeleteEndpoint(IPCSocketFD);
	MessageManager::Instance().StartSocket(IPCSocketFD);

	pthread_create(&eventDispatchThread, NULL, EventDispatcherThread, NULL);

	//Send a connection request
	Ticket ticket = MessageManager::Instance().StartConversation(IPCSocketFD);

	ControlMessage connectRequest(CONTROL_CONNECT_REQUEST);
	if(!MessageManager::Instance().WriteMessage(ticket, &connectRequest))
	{
		LOG(ERROR, "Could not send CONTROL_CONNECT_REQUEST to NOVAD", "");
		DisconnectFromNovad();
		return false;
	}

	Message *reply = MessageManager::Instance().ReadMessage(ticket);
	if(reply->m_messageType == ERROR_MESSAGE && ((ErrorMessage*)reply)->m_errorType == ERROR_TIMEOUT)
	{
		LOG(ERROR, "Timeout error when waiting for message reply", "");
		delete reply;
		return false;
	}

	if(reply->m_messageType != CONTROL_MESSAGE)
	{
		stringstream s;
		s << "Expected message type of CONTROL_MESSAGE but got " << reply->m_messageType;
		LOG(ERROR, s.str(), "");

		reply->DeleteContents();
		delete reply;
		DisconnectFromNovad();
		return false;
	}
	ControlMessage *connectionReply = (ControlMessage*)reply;
	if(connectionReply->m_controlType != CONTROL_CONNECT_REPLY)
	{
		stringstream s;
		s << "Expected control type of CONTROL_CONNECT_REPLY but got " <<connectionReply->m_controlType;
		LOG(ERROR, s.str(), "");

		reply->DeleteContents();
		delete reply;
		DisconnectFromNovad();
		return false;
	}
	bool replySuccess = connectionReply->m_success;
	delete connectionReply;

	return replySuccess;
}

void DisconnectFromNovad()
{
	//Close out any possibly remaining socket artifacts
	if(libeventBase != NULL)
	{
		if(eventDispatchThread != 0)
		{
			if(event_base_loopexit(libeventBase, NULL) == -1)
			{
				LOG(WARNING, "Unable to exit event loop", "");
			}

			pthread_join(eventDispatchThread, NULL);
			eventDispatchThread = 0;
		}
	}
	if(bufferevent != NULL)
	{
		bufferevent_free(bufferevent);
		bufferevent = NULL;
	}

	IPCSocketFD = -1;
}

bool TryWaitConnectToNovad(int timeout_ms)
{
	if(ConnectToNovad())
	{
		return true;
	}
	else
	{
		//usleep takes in microsecond argument. Convert to milliseconds
		usleep(timeout_ms *1000);
		return ConnectToNovad();
	}
}

bool CloseNovadConnection()
{
	Ticket ticket = MessageManager::Instance().StartConversation(IPCSocketFD);

	if(IPCSocketFD == -1)
	{
		return true;
	}

	bool success = true;

	ControlMessage disconnectNotice(CONTROL_DISCONNECT_NOTICE);
	if(!MessageManager::Instance().WriteMessage(ticket, &disconnectNotice))
	{
		success = false;
	}

	Message *reply = MessageManager::Instance().ReadMessage(ticket);
	if(reply->m_messageType == ERROR_MESSAGE && ((ErrorMessage*)reply)->m_errorType == ERROR_TIMEOUT)
	{
		LOG(ERROR, "Timeout error when waiting for message reply", "");
		reply->DeleteContents();
		delete reply;
		success = false;
	}
	else if(reply->m_messageType != CONTROL_MESSAGE)
	{
		reply->DeleteContents();
		delete reply;
		success = false;
	}
	else
	{
		ControlMessage *connectionReply = (ControlMessage*)reply;
		if(connectionReply->m_controlType != CONTROL_DISCONNECT_ACK)
		{
			connectionReply->DeleteContents();
			delete connectionReply;
			success = false;
		}
	}

	DisconnectFromNovad();
	LOG(DEBUG, "Call to CloseNovadConnection complete", "");
	return success;
}

}
