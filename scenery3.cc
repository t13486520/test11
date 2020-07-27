#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/nstime.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("SceneryNetwork");


double totalTime = 10.000000;
double starttime = 0.000000;
double duration = totalTime - starttime;
uint32_t port = 9;//端口号
double totalThroughput = 0.0;
uint32_t staNodes = 20;
uint32_t packetSize =20;
int maxBytes = 20480;//发送的最大Byte
uint32_t dataRate =54*1000*1000;//速率
Time delay_Sum = Seconds(0.0);

void experiment ();


int main(int argc, char *argv[])
{

	CommandLine cmd;
	cmd.Parse (argc, argv);

	Time::SetResolution (Time::NS);//设置时间最小单位为纳秒

	experiment ();
	cout << endl;
	return 0;
}

void experiment ()
{

	NodeContainer nodes;
	nodes.Create(staNodes);
	NodeContainer apNode;
	apNode.Create(1);

	Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (100));//DCF信道接入机制
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  	phy.SetChannel (channel.Create ());

	// 设置WifiMac并在节点中安装NetDevice
  	WifiHelper wifi;
  	wifi.SetStandard (WIFI_PHY_STANDARD_80211a);		
  	wifi.SetRemoteStationManager ("ns3::AarfWifiManager");//用于WiFi速率控制
	
	//设置MAC层属性
	WifiMacHelper mac;
	Ssid ssid = Ssid("ns-3-ssid");//设置ssid属性
	mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    	NetDeviceContainer staDevices = wifi.Install (phy, mac, nodes);//安装STA节点
    	mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    	NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);//安装AP节点

	//为AP节点设置移动模型
	MobilityHelper apMobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (0.0, 0.0, 0.0));
	apMobility.SetPositionAllocator (positionAlloc);
	apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobility.Install (apNode);

	MobilityHelper mobility;
	mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
								  "X", StringValue("0.0"), 
								  "Y", StringValue("0.0"), 
								  "rho", StringValue("50.0"));
	mobility.Install (nodes);

	//使节点具备传输层的基本功能
	InternetStackHelper stack;
	stack.Install(nodes);
	stack.Install(apNode);
						
	Ipv4AddressHelper address;
  	address.SetBase ("10.0.0.0", "255.255.255.0");
  	Ipv4InterfaceContainer apInterface;
  	apInterface = address.Assign (apDevice);
  	Ipv4InterfaceContainer interfaces;
  	interfaces = address.Assign (staDevices);


	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

		PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
		ApplicationContainer sinkApp = sinkHelper.Install (apNode);
		Ptr<PacketSink> sink = StaticCast<PacketSink> (sinkApp.Get (0));
		sinkApp.Start (Seconds (0.0));
		sinkApp.Stop (Seconds (10.0));

		OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), port)));
		server.SetAttribute ("PacketSize", UintegerValue (packetSize));
		server.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
		server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));//以固定码率发送分组
		server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));//停止发送分组,0表示一直发
		for (uint32_t j=0; j<staNodes; j++)
  		{
		server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
		ApplicationContainer serverApp;
		serverApp = server.Install (nodes.Get (j));
		serverApp.Start (Seconds (0.0));
		serverApp.Stop (Seconds (10.0));
		}

	phy.EnablePcapAll("scenery1");
	AsciiTraceHelper ascii;
    	phy.EnableAsciiAll (ascii.CreateFileStream ("scenery1.tr"));

  	FlowMonitorHelper flowmon;
  	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  	Simulator::Stop (Seconds (totalTime));

	AnimationInterface anim("scenery1.xml");

  	Simulator::Run ();

  	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	if(!classifier)
	{
		NS_LOG_UNCOND("DynamicCast failure");
	}
  	FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  	for (map<FlowId, FlowMonitor::FlowStats>::iterator i = stats.begin (); i != stats.end (); ++i)//map容器
    	{
      		if (i->first > staNodes)
        	{
          		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          		cout << "Flow " << i->first - staNodes << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          		cout << "  Tx Packets: " << i->second.txPackets << "\n";
          		cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          		cout << "  TxOffered:  " << i->second.txBytes * 8.0  / 1000 / 1000  << " Mbps\n";
          		cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          		cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  	totalThroughput = totalThroughput+i->second.rxBytes*8.0/duration/1000/1000;
          		cout << "  Throughput: " << i->second.rxBytes * 8.0/duration  / 1000 / 1000  << " Mbps\n";
        	}
    	}
		cout << "Total throughput:" << totalThroughput << " Mbps";

  	Simulator::Destroy ();

}
