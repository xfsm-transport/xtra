#include "xfsm_conf.h"

using namespace ns3;


/**********************************/
/*       Various parameters       */
/*                                */
/*  To vary them, move them to    */
/*  the code for cmd-parsing      */
/**********************************/

#define TEB_ACTIVE 1
#define DOSACK true

#define SIMDURATION 0.3
#define STARTDELAY 0.0

// 1446 yields packets of 1500 bytes.
// 1448 is the max for default pointtopoint links, yielding packets of size 1502

// !!! NOTE: this number is hardcoded in my seq plotting script
//#define TCPMSS 946

#define TCPMSS 1446
#define PACKETSIZE (TCPMSS+54)
// this is in segments. 3 is a normal default, or 10 for Linux, not 1 like in ns-3
#define TCPINITCWND 3

// in units of TCPMSS !  e.g. 3 = 3 TCP packets
#define APPBLOCKSIZE 30
// number of app blocks to send in total
#define APPNUMBLOCKS 100000000
#define APPRATE "1Gbps"

// packets to drop; to drop none, make this empty
// Index from 0; the sender sends SYN, then ACK, then data packets
// So data packets begin at number 2
// E.g. to drop the first and last from IW10: 2, 11
const int PACKETS_TO_DROP[] = {};



// BDP: 800kbps * 120ms = 800000/8 * 0.12 = 12000 byte.
// packets are 1000 byte each, so that's 12 packets.
// if we give the queue a length of 12, cwnd should reach 24 packets (24000 bytes).


// access links have 1Gbps, 1ms, so the total OWD becomes 2ms + BOTTLENECK_DELAY
// ... and the RTT is 4ms + 2*BOTTLENECK_DELAY
//#define BOTTLENECK_CAPACITY "1Mbps"
//#define BOTTLENECK_CAPACITY "0.8Mbps"
#define BOTTLENECK_CAPACITY "10Mbps"
//#define BOTTLENECK_DELAY "3ms"
#define BOTTLENECK_DELAY "10ms"

// #define USE_HELPER
#define QLEN 14
//#define QLEN 12


/**********************************/
/*      MyApp (from fifth.cc)     */
/*                                */
/*  This can be useful to emulate */
/*  app-limited senders (perhaps) */
/**********************************/

class MyApp : public Application
{
    public:
    
    MyApp ();
    virtual ~MyApp();
    
    void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
    
    private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);
    
    void ScheduleTx (void);
    void SendPacket (void);
    
    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint32_t        m_packetSize;
    uint32_t        m_nPackets;
    DataRate        m_dataRate;
    EventId         m_sendEvent;
    bool            m_running;
    uint32_t        m_packetsSent;
};

MyApp::MyApp ()
: m_socket (0),
m_peer (),
m_packetSize (0),
m_nPackets (0),
m_dataRate (0),
m_sendEvent (),
m_running (false),
m_packetsSent (0)
{
}

MyApp::~MyApp()
{
    m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    Simulator::Schedule(Seconds(STARTDELAY), &MyApp::SendPacket, this);
}

void
MyApp::StopApplication (void)
{
    m_running = false;
    
    if (m_sendEvent.IsRunning ())
    {
        Simulator::Cancel (m_sendEvent);
    }
    
    if (m_socket)
    {
        m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    
    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
    if (m_running)
    {
        Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
        m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}


/**********************************/
/*      CALLBACKS (for tracing    */
/**********************************/
Ptr< Node > node0;


static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    // this writes it to stderr...
    //    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd <<  "\t" << (newCwnd/TCPMSS) << std::endl;
}

/*static void
QlenChange (Ptr<OutputStreamWrapper> stream, uint32_t oldQ, uint32_t newQ)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newQ << std::endl;
}
*/


/*static void changeNode0LinkDelay (int delay)
{
    std::cout << "*** CHANGING BOTTLENECK DELAY TO " << delay << "ms at " << Simulator::Now().GetSeconds() << std::endl;
    uint32_t channelid = node0->GetDevice(0)->GetChannel()->GetId();
    Config::Set("/ChannelList/" + std::to_string(channelid) + "/$ns3::PointToPointChannel/Delay", StringValue(std::to_string(delay) + "ms"));
}
*/

/****************************************************/
/****************************************************/
/****************************************************/

int main (int argc, char *argv[])
{
    //int tcpSegmentSize = 1000-54;
    //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    //    Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (1500));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (TCPMSS));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (TCPINITCWND));
    Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue (Time("0ms")));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    //Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (DOSACK));
    //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpReno"));
    //  Config::SetDefault ("ns3::RttEstimator::MinRTO", TimeValue (MilliSeconds (500)));
    //    Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (QLEN) );
    
    //Config::SetDefault ("ns3::TcpSocketBase::TebSocket", UintegerValue (TEB_ACTIVE));
    Config::SetDefault ("ns3::TcpSocketBase::Verbose", UintegerValue (0));

    uint32_t    nLeaf = 1; // If non-zero, number of both left and right
    // add more parameters here, e.g. replacing the defines above, to vary them
    CommandLine cmd;
    cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
    cmd.Parse (argc,argv);
    
    /****************************************************/
    // Create topology
    /****************************************************/
    PointToPointHelper pointToPointRouter;

    pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue (BOTTLENECK_CAPACITY));
    pointToPointRouter.SetChannelAttribute ("Delay", StringValue (BOTTLENECK_DELAY));
    pointToPointRouter.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(QLEN));

    PointToPointHelper pointToPointLeaf;
    //    pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("1Gbps"));
    pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("1Gbps"));
    pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.1ms"));
    

    PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf,
                                  nLeaf, pointToPointLeaf,
                                  pointToPointRouter);

    // Install Stack
    InternetStackHelper stack;
    d.InstallStack (stack);
    
    // Assign IP Addresses
    d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"), // left
                           Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"), // right
                           Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));    // router

    // NOTE: this does more than just assign addresses - I followed the
    // recommendation at:
    // https://stackoverflow.com/questions/38951115/tcp-congestion-window-graph-ns-3
    // and disabled the tc installation that also happens here.
    
    node0 = d.GetLeft(0);
    
    // finish off the address assignment + static routing job
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    
    /****************************************************/
    // Initialize TCP sources and sinks
    /****************************************************/
    
    uint16_t sinkPort = 8080;
    Address sinkAddress (InetSocketAddress (d.GetRightIpv4Address (0), sinkPort));
    PacketSinkHelper sinkA ("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkAppA = sinkA.Install (d.GetRight (0));

    Ptr<Socket> ns3TcpSinkSocket = Socket::CreateSocket (d.GetRight (0), TcpSocketFactory::GetTypeId ());
    
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (d.GetLeft (0), TcpSocketFactory::GetTypeId ());

    ns3TcpSocket->SetXfsm(configure_xfsm());
    

    /****************************************************/
    /* End TCP XFSM configuration (sender side)         */
    /****************************************************/

    Ptr<MyApp> app = CreateObject<MyApp> ();
  
    app->Setup (ns3TcpSocket, sinkAddress, 1000, 2000000, DataRate (APPRATE));

    // When we add, it also starts?! Strange enough...
    d.GetLeft (0)->AddApplication (app);
   
    /****************************************************/
    // Initialize interference: errors, bg traffic
    /****************************************************/
    
    // the list of packeonets to drop, numbering them from 0
    std::list<uint32_t> lostPackets;
    // not dropping any right now
    int numDrops = sizeof(PACKETS_TO_DROP)/sizeof(int);
    if(numDrops>0) {
        for(int i=0; i<numDrops; i++)
        lostPackets.push_back(PACKETS_TO_DROP[i]);
    }
    //lostPackets.push_back(5);
    
    //Ptr<ReceiveListErrorModel> pem = CreateObject<ReceiveListErrorModel> ();
    //pem->SetList (lostPackets);
    //d.GetRight (0)->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
    
    // ---------------------------------------------------------------------
    

    AsciiTraceHelper asciiTraceHelper;
    
    /****************************************************/
    // Tracing
    /****************************************************/
    
    // cwnd
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream ("cwnd.txt");
    ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, cwndStream));
   
    pointToPointLeaf.EnablePcap ("tcpsender", d.GetLeft (0)->GetId (), 0);
    pointToPointLeaf.EnablePcap ("tcpreceiver", d.GetRight (0)->GetId (), 0);
    if(nLeaf>1) {
        pointToPointLeaf.EnablePcap ("bgsender", d.GetLeft (1)->GetId (), 0);
        pointToPointLeaf.EnablePcap ("bgreceiver", d.GetRight (1)->GetId (), 0);
    }
    
    // anim
    d.BoundingBox (1, 1, 100, 100);
    AnimationInterface anim ("anim.xml");
    anim.EnablePacketMetadata (); // Optional
    anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (20)); // Optional

   
    app->SetStartTime (Seconds (0.));
    app->SetStopTime (Seconds (SIMDURATION));
         
    sinkAppA.Start (Seconds (0.));
    sinkAppA.Stop (Seconds (SIMDURATION));
    
    Simulator::Stop (Seconds (SIMDURATION));
    
//    Time::SetResolution(ns3::Time::PS);
//    Time::SetResolution(ns3::Time::MS);

    Simulator::Run ();
    
    
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkAppA.Get (0));
    std::cout << "Total Bytes Received from sender: " << sink1->GetTotalRx () << std::endl;
    //    std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
    std::cout << "Simulation finished." << std::endl;
    Simulator::Destroy ();
    return 0;
}
