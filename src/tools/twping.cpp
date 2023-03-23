#include <base/logger.h>
#include <base/system.h>

#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>

static CNetClient g_NetOp; // main

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);

	secure_random_init();
	log_set_global_logger_default();

	net_init();
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_ALL;
	g_NetOp.Open(BindAddr);

	if(argc != 2)
	{
		log_error("twping", "usage: %s server[:port] (default port: 8303)", argv[0]);
		return 1;
	}

	NETADDR Addr;
	if(net_host_lookup(argv[1], &Addr, NETTYPE_ALL))
	{
		log_error("twping", "host lookup failed");
		return 1;
	}

	if(Addr.port == 0)
		Addr.port = 8303;

	unsigned char aBuffer[sizeof(SERVERBROWSE_GETINFO) + 1];
	CNetChunk Packet;

	mem_copy(aBuffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));

	int CurToken = rand() % 256;
	aBuffer[sizeof(SERVERBROWSE_GETINFO)] = CurToken;

	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(aBuffer);
	Packet.m_pData = aBuffer;

	g_NetOp.Send(&Packet);

	int64_t startTime = time_get();

	net_socket_read_wait(g_NetOp.m_Socket, 1000000);

	g_NetOp.Update();

	while(g_NetOp.Recv(&Packet))
	{
		if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_INFO) && mem_comp(Packet.m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
		{
			// we got ze info
			CUnpacker Up;

			Up.Reset((unsigned char *)Packet.m_pData + sizeof(SERVERBROWSE_INFO), Packet.m_DataSize - sizeof(SERVERBROWSE_INFO));
			int Token = str_toint(Up.GetString());
			if(Token != CurToken)
				continue;

			int64_t endTime = time_get();
			log_info("twping", "%g ms", (double)(endTime - startTime) / time_freq() * 1000);
		}
	}

	return 0;
}
