#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cmath>

using namespace ns3;
// IPS gateway packet blocking model
Ptr<RateErrorModel> ipsDropModel;
// Unknown node IPS
Ptr<RateErrorModel> unknownDropModel;
uint32_t unknownBlockedPackets = 0;
// ==========================================
// IPS MONITORING VARIABLES
// ==========================================

// Registered sensor traffic counters
uint32_t sensorPacketCount[5] = {0, 0, 0, 0, 0};

// IPS blocking status for each sensor
bool sensorBlocked[5] = {false, false, false, false, false};

// IPS statistics
uint32_t totalBlockedPackets = 0;

// Monitoring thresholds (packets per second)
const double WARNING_THRESHOLD = 150.0;
const double CRITICAL_THRESHOLD = 1050.0;
// ==========================================
// MONITOR PACKETS FROM EACH SENSOR
// ==========================================

void MonitorSensorPacket(uint32_t sensorId,
                         Ptr<const Packet> packet)
{
    if (sensorId >= 5)
    {
        return;
    }

    sensorPacketCount[sensorId]++;
}
// ==========================================
// CHECK SENSOR TRAFFIC EVERY SECOND
// ==========================================

void CheckSensorTraffic()
{
    std::cout << "\nTime: "
              << Simulator::Now().GetSeconds()
              << " seconds\n";

    for (uint32_t i = 0; i < 5; i++)
    {
        double packetRate = sensorPacketCount[i];

        std::cout << "Sensor " << i + 1
                  << " Rate: " << packetRate
                  << " packets/sec";

        if (packetRate > CRITICAL_THRESHOLD)
{
    std::cout << " | CRITICAL ALERT";

    if (!sensorBlocked[i])
    {
        sensorBlocked[i] = true;

        if (i == 0 && ipsDropModel)
        {
            ipsDropModel->SetAttribute(
                "ErrorRate",
                DoubleValue(1.0));

            std::cout << "\nIPS ACTIVATED"
                      << "\nBlocked Source : Sensor 1"
                      << "\nLocation       : IoT Gateway"
                      << "\nAction         : DROP PACKETS\n";
        }
   }
}
        else if (packetRate > WARNING_THRESHOLD)
        {
            std::cout << " | WARNING";
        }
        else
        {
            std::cout << " | NORMAL";
        }

        std::cout << std::endl;

        // Reset counter for the next second
        sensorPacketCount[i] = 0;
    }

    // Schedule the next monitoring check
    if (Simulator::Now().GetSeconds() < 12.0)
    {
        Simulator::Schedule(
            Seconds(1.0),
            &CheckSensorTraffic);
    }
}
// Gateway IPS packet filter


// Count packets dropped at the gateway
void CountIpsDrop(Ptr<const Packet> packet)
{
    totalBlockedPackets++;
}
// Count packets blocked from unknown node
void CountUnknownDrop(Ptr<const Packet> packet)
{
    unknownBlockedPackets++;
}
int main(int argc, char *argv[])
{
    double extraRateKbps = 800.0;
    CommandLine cmd(__FILE__);
    cmd.AddValue("extraRateKbps", "Extra Sensor 1 traffic in Kbps, 0-800", extraRateKbps);
    cmd.Parse(argc, argv);
    if (!std::isfinite(extraRateKbps) || extraRateKbps < 0.0 || extraRateKbps > 800.0) {
        std::cerr << "Invalid extraRateKbps (allowed: 0-800)\n";
        return 1;
    }

    // =====================================================
    // SMART TRAFFIC IoT - FINAL IDS
    // =====================================================

    NodeContainer sensors;
    sensors.Create(5);

    NodeContainer gateway;
    gateway.Create(1);

    NodeContainer server;
    server.Create(1);

    // Create one additional node to represent
    // an unregistered/unknown node.
    NodeContainer unknownNode;
    unknownNode.Create(1);

    InternetStackHelper internet;
    internet.Install(sensors);
    internet.Install(gateway);
    internet.Install(server);
    internet.Install(unknownNode);

    // =====================================================
    // SENSOR -> GATEWAY CONNECTIONS
    // =====================================================

    PointToPointHelper sensorLink;

    sensorLink.SetDeviceAttribute(
        "DataRate",
        StringValue("5Mbps"));

    sensorLink.SetChannelAttribute(
        "Delay",
        StringValue("2ms"));

    Ipv4AddressHelper address;

    for (uint32_t i = 0; i < 5; i++)
    {
        NodeContainer pair(
            sensors.Get(i),
            gateway.Get(0));

        NetDeviceContainer devices =
            sensorLink.Install(pair);
// Install IPS filter on Sensor 1's gateway interface
if (i == 0)
{
    Ptr<PointToPointNetDevice> gatewayDevice =
        DynamicCast<PointToPointNetDevice>(
            devices.Get(1));

    ipsDropModel = CreateObject<RateErrorModel>();

    ipsDropModel->SetAttribute(
        "ErrorUnit",
        EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    ipsDropModel->SetAttribute(
        "ErrorRate",
        DoubleValue(0.0));

    gatewayDevice->SetReceiveErrorModel(ipsDropModel);

    gatewayDevice->TraceConnectWithoutContext(
        "PhyRxDrop",
        MakeCallback(&CountIpsDrop));
}
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";

        address.SetBase(
            subnet.str().c_str(),
            "255.255.255.0");

        address.Assign(devices);
    }

    // =====================================================
    // GATEWAY -> SERVER
    // =====================================================

    PointToPointHelper gatewayLink;

    gatewayLink.SetDeviceAttribute(
        "DataRate",
        StringValue("5Mbps"));

    gatewayLink.SetChannelAttribute(
        "Delay",
        StringValue("2ms"));

    NetDeviceContainer gatewayServer =
        gatewayLink.Install(
            gateway.Get(0),
            server.Get(0));

    address.SetBase(
        "10.1.10.0",
        "255.255.255.0");

    address.Assign(gatewayServer);

    // =====================================================
    // UNKNOWN NODE -> GATEWAY
    // Unregistered node traffic is blocked at the gateway.
    // =====================================================

    NetDeviceContainer unknownGateway =
        gatewayLink.Install(
            unknownNode.Get(0),
            gateway.Get(0));
     // Install IPS filter at gateway for unknown node
Ptr<PointToPointNetDevice> unknownGatewayDevice =
    DynamicCast<PointToPointNetDevice>(
        unknownGateway.Get(1));

unknownDropModel = CreateObject<RateErrorModel>();

unknownDropModel->SetAttribute(
    "ErrorUnit",
    EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

unknownDropModel->SetAttribute(
    "ErrorRate",
    DoubleValue(1.0));

unknownGatewayDevice->SetReceiveErrorModel(unknownDropModel);

unknownGatewayDevice->TraceConnectWithoutContext(
    "PhyRxDrop",
    MakeCallback(&CountUnknownDrop));
    address.SetBase(
        "10.1.20.0",
        "255.255.255.0");

    address.Assign(unknownGateway);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // =====================================================
    // SERVER
    // =====================================================

    uint16_t port = 9000;

    UdpServerHelper serverHelper(port);

    ApplicationContainer serverApp =
        serverHelper.Install(server.Get(0));

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(13.0));

    // =====================================================
    // NORMAL SENSOR TRAFFIC
    // =====================================================

    for (uint32_t i = 0; i < 5; i++)
    {
        OnOffHelper traffic(
            "ns3::UdpSocketFactory",
            InetSocketAddress(
                server.Get(0)
                    ->GetObject<Ipv4>()
                    ->GetAddress(1, 0)
                    .GetLocal(),
                port));

        traffic.SetAttribute(
            "DataRate",
            DataRateValue(
                DataRate("80Kbps")));

        traffic.SetAttribute(
            "PacketSize",
            UintegerValue(100));

        traffic.SetAttribute(
            "OnTime",
            StringValue(
                "ns3::ConstantRandomVariable[Constant=1]"));

        traffic.SetAttribute(
            "OffTime",
            StringValue(
                "ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer app =
            traffic.Install(sensors.Get(i));
Ptr<OnOffApplication> sensorApp =
    DynamicCast<OnOffApplication>(app.Get(0));

sensorApp->TraceConnectWithoutContext(
    "Tx",
    MakeBoundCallback(&MonitorSensorPacket, i));

        app.Start(Seconds(2.0));
        app.Stop(Seconds(12.0));
    }

    // =====================================================
    // HIGH TRAFFIC FROM SENSOR 1
    // =====================================================

    OnOffHelper highTraffic(
        "ns3::UdpSocketFactory",
        InetSocketAddress(
            server.Get(0)
                ->GetObject<Ipv4>()
                ->GetAddress(1, 0)
                .GetLocal(),
            port));

    highTraffic.SetAttribute(
        "DataRate",
        DataRateValue(
            DataRate(static_cast<uint64_t>(extraRateKbps * 1000.0))));

    highTraffic.SetAttribute(
        "PacketSize",
        UintegerValue(100));

    highTraffic.SetAttribute(
        "OnTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=1]"));

    highTraffic.SetAttribute(
        "OffTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=0]"));

    if (extraRateKbps > 0.0) {
    ApplicationContainer highTrafficApp =
        highTraffic.Install(sensors.Get(0));
     Ptr<OnOffApplication> highApp =
    DynamicCast<OnOffApplication>(
        highTrafficApp.Get(0));

highApp->TraceConnectWithoutContext(
    "Tx",
    MakeBoundCallback(&MonitorSensorPacket, 0));
    highTrafficApp.Start(Seconds(2.0));
    highTrafficApp.Stop(Seconds(12.0));
    }

    // =====================================================
    // UNKNOWN NODE TRAFFIC
    // =====================================================

    OnOffHelper unknownTraffic(
        "ns3::UdpSocketFactory",
        InetSocketAddress(
            server.Get(0)
                ->GetObject<Ipv4>()
                ->GetAddress(1, 0)
                .GetLocal(),
            port));

    unknownTraffic.SetAttribute(
        "DataRate",
        DataRateValue(
            DataRate("50Kbps")));

    unknownTraffic.SetAttribute(
        "PacketSize",
        UintegerValue(100));

    unknownTraffic.SetAttribute(
        "OnTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=1]"));

    unknownTraffic.SetAttribute(
        "OffTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer unknownApp =
        unknownTraffic.Install(
            unknownNode.Get(0));

    unknownApp.Start(Seconds(3.0));
    unknownApp.Stop(Seconds(8.0));

    // =====================================================
    // FLOW MONITOR
    // =====================================================

    FlowMonitorHelper flowmon;

    Ptr<FlowMonitor> monitor =
        flowmon.InstallAll();
    Simulator::Schedule(
    Seconds(3.0),
    &CheckSensorTraffic);
    Simulator::Stop(Seconds(13.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    FlowMonitor::FlowStatsContainer stats =
        monitor->GetFlowStats();
// Verify successful delivery from each sensor
std::cout << "\n===== IPS SENSOR DELIVERY REPORT =====\n";

Ptr<Ipv4FlowClassifier> deliveryClassifier =
    DynamicCast<Ipv4FlowClassifier>(
        flowmon.GetClassifier());

for (uint32_t i = 0; i < 5; i++)
{
    uint64_t sent = 0;
    uint64_t received = 0;

    std::string sensorIP =
        "10.1." + std::to_string(i + 1) + ".1";

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple =
            deliveryClassifier->FindFlow(flow.first);

        if (tuple.sourceAddress ==
            Ipv4Address(sensorIP.c_str()))
        {
            sent += flow.second.txPackets;
            received += flow.second.rxPackets;
        }
    }

    std::cout << "Sensor " << i + 1
              << " | Sent: " << sent
              << " | Received: " << received;

    if (received > 0)
    {
        std::cout << " | DELIVERY CONFIRMED";
    }
    else
    {
        std::cout << " | NO DELIVERY";
    }

    std::cout << std::endl;
}

    // ==========================================
// IDENTIFY HIGH TRAFFIC SENSOR
// ==========================================

Ptr<Ipv4FlowClassifier> classifier =
    DynamicCast<Ipv4FlowClassifier>(
        flowmon.GetClassifier());

uint64_t sensorPackets[5] = {0, 0, 0, 0, 0};

// Collect packet counts for each sensor
for (const auto &flow : stats)
{
    Ipv4FlowClassifier::FiveTuple tuple =
        classifier->FindFlow(flow.first);

    for (uint32_t i = 0; i < 5; i++)
    {
        std::string sensorIP =
            "10.1." + std::to_string(i + 1) + ".1";

        if (tuple.sourceAddress == Ipv4Address(sensorIP.c_str()))
        {
            sensorPackets[i] += flow.second.txPackets;
        }
    }
}

std::cout << "\n===== SENSOR TRAFFIC MONITOR =====\n";

for (uint32_t i = 0; i < 5; i++)
{
    double packetRate = sensorPackets[i] / 10.0;

    std::cout << "Sensor " << i + 1
              << " | Packets: " << sensorPackets[i]
              << " | Rate: " << packetRate
              << " packets/sec";

    if (packetRate > 1050)
{
    std::cout << " | Severity: CRITICAL";
    std::cout << " | IDS ALERT";
}
else if (packetRate > 150)
{
    std::cout << " | Severity: WARNING";
    std::cout << " | IDS ALERT";
}
else
{
    std::cout << " | Severity: NORMAL";
}

    std::cout << std::endl;
}

// Total packet statistics
uint64_t totalTxPackets = 0;
uint64_t totalRxPackets = 0;

// Communication policy monitoring
uint64_t unauthorizedPackets = 0;

Ptr<Ipv4FlowClassifier> policyClassifier =
    DynamicCast<Ipv4FlowClassifier>(
        flowmon.GetClassifier());

for (auto const &flow : stats)
{
    totalTxPackets += flow.second.txPackets;
    totalRxPackets += flow.second.rxPackets;

    Ipv4FlowClassifier::FiveTuple tuple =
        policyClassifier->FindFlow(flow.first);

    bool registeredSensor = false;

    for (uint32_t i = 0; i < 5; i++)
    {
        std::string sensorIP =
            "10.1." + std::to_string(i + 1) + ".1";

        if (tuple.sourceAddress ==
            Ipv4Address(sensorIP.c_str()))
        {
            registeredSensor = true;
            break;
        }
    }

    bool correctDestination =
        (tuple.destinationAddress ==
         Ipv4Address("10.1.10.2"));

    if (!registeredSensor || !correctDestination)
    {
        unauthorizedPackets += flow.second.txPackets;
    }
}    

    uint64_t lostPackets =
        totalTxPackets - totalRxPackets;

    double packetLoss = 0.0;

    if (totalTxPackets > 0)
    {
        packetLoss =
            ((double)lostPackets /
             totalTxPackets) * 100.0;
    }

    double pdr = 0.0;

    if (totalTxPackets > 0)
    {
        pdr =
            ((double)totalRxPackets /
             totalTxPackets) * 100.0;
    }

    // =====================================================
    // IDS CHECK 1 - UNKNOWN NODE
    // =====================================================

    uint32_t unknownId =
        unknownNode.Get(0)->GetId();

    bool registered =
        false;

    for (uint32_t i = 0; i < 5; i++)
    {
        if (unknownId ==
            sensors.Get(i)->GetId())
        {
            registered = true;
        }
    }

    bool unknownNodeDetected =
        !registered;

    // =====================================================
    // IDS CHECK 2 - COMMUNICATION POLICY
    // =====================================================

    // Allowed architecture:
    //
    // Sensor -> Gateway -> Server
    //
    // Unregistered source -> Server is not allowed.

bool policyViolation =
    (unauthorizedPackets > 0 ||
     unknownBlockedPackets > 0);      

    // =====================================================
    // IDS CHECK 3 - HIGH TRAFFIC
    // =====================================================

    // Use the measured per-sensor rate decision, not aggregate packet count.
    bool highTrafficDetected = sensorBlocked[0];

    // =====================================================
    // FINAL IDS REPORT
    // =====================================================

    std::cout
        << "\n====================================\n";

    std::cout
        << "       SMART TRAFFIC IoT IDS\n";

    std::cout
        << "====================================\n";

    // SOURCE CHECK

    std::cout
        << "\n[1] SOURCE ID CHECK\n";

    if (unknownNodeDetected)
    {
        std::cout
            << "Status : ALERT\n";

        std::cout
            << "Reason : Unknown node detected\n";
    }
    else
    {
        std::cout
            << "Status : NORMAL\n";
    }

    // POLICY CHECK

    std::cout
        << "\n[2] COMMUNICATION POLICY CHECK\n";

    std::cout << "Unauthorized flow packets observed : "
              << unauthorizedPackets << "\n";
    std::cout << "Unknown-node packets blocked at gateway : "
              << unknownBlockedPackets << "\n";

    if (policyViolation)
    {
        std::cout
            << "Status : ALERT\n";

        std::cout
            << "Reason : Unauthorized communication attempt\n";
    }
    else
    {
        std::cout
            << "Status : NORMAL\n";
    }

    // TRAFFIC CHECK

    std::cout
        << "\n[3] TRAFFIC BEHAVIOUR CHECK\n";

    std::cout
        << "Packets Sent : "
        << totalTxPackets << "\n";

    std::cout
        << "Packets Received : "
        << totalRxPackets << "\n";

    std::cout
        << "Packet Loss : "
        << packetLoss << " %\n";

    std::cout
        << "PDR : "
        << pdr << " %\n";

    if (highTrafficDetected)
    {
        std::cout
            << "Status : ALERT\n";

        std::cout
            << "Reason : High traffic detected\n";
    }
    else
    {
        std::cout
            << "Status : NORMAL\n";
    }

    // =====================================================
    // OVERALL IDS STATUS
    // =====================================================

    bool anyAlert =
        unknownNodeDetected ||
        policyViolation ||
        highTrafficDetected;

    std::cout
        << "\n====================================\n";

    if (anyAlert)
    {
        std::cout
            << "IDS STATUS : ALERT\n";

        std::cout
            << "Suspicious behaviour detected.\n";
    }
    else
    {
        std::cout
            << "IDS STATUS : NORMAL\n";

        std::cout
            << "No suspicious behaviour detected.\n";
    }

    std::cout
        << "====================================\n";
std::cout << "\n====================================\n";
std::cout << "       IPS FINAL REPORT\n";
std::cout << "====================================\n";

std::cout << "Blocked Source : Sensor 1\n";

std::cout << "Packets Dropped by Gateway IPS : "
          << totalBlockedPackets << "\n";

std::cout << "IPS Status : "
          << (sensorBlocked[0] ? "ACTIVE" : "INACTIVE") << "\n";
std::cout << "\n===== UNKNOWN NODE IPS REPORT =====\n";

std::cout << "Node Status : UNREGISTERED\n";

std::cout << "Gateway Action : BLOCK\n";

std::cout << "Unknown Node Packets Blocked : "
          << unknownBlockedPackets << "\n";    
double totalDelaySeconds = 0.0;
uint64_t delayReceivedPackets = 0;
uint64_t receivedBytes = 0;
for (const auto &flow : stats) {
    totalDelaySeconds += flow.second.delaySum.GetSeconds();
    delayReceivedPackets += flow.second.rxPackets;
    receivedBytes += flow.second.rxBytes;
}
const double averageDelayMs = delayReceivedPackets ? (totalDelaySeconds * 1000.0 / delayReceivedPackets) : 0.0;
const double throughputMbps = receivedBytes * 8.0 / (10.0 * 1000000.0);
// Export this completed NS-3 run as machine-readable dashboard data.
// File is written in the directory where ./ns3 is launched.
uint64_t deliveredBySensor[5] = {0, 0, 0, 0, 0};
for (const auto &flow : stats)
{
    Ipv4FlowClassifier::FiveTuple tuple =
        policyClassifier->FindFlow(flow.first);
    for (uint32_t i = 0; i < 5; ++i)
    {
        std::string ip = "10.1." + std::to_string(i + 1) + ".1";
        if (tuple.sourceAddress == Ipv4Address(ip.c_str()))
        {
            deliveredBySensor[i] += flow.second.rxPackets;
        }
    }
}
std::ofstream dashboardFile("dashboard-results.json");
if (dashboardFile.is_open())
{
    dashboardFile << std::fixed << std::setprecision(5);
    dashboardFile << "{\n"
                  << "  \"source\": \"NS-3 completed simulation\",\n"
                  << "  \"extraRateKbps\": " << extraRateKbps << ",\n"
                  << "  \"averageDelayMs\": " << averageDelayMs << ",\n"
                  << "  \"throughputMbps\": " << throughputMbps << ",\n"
                  << "  \"packetsSent\": " << totalTxPackets << ",\n"
                  << "  \"packetsReceived\": " << totalRxPackets << ",\n"
                  << "  \"packetsLost\": " << lostPackets << ",\n"
                  << "  \"pdr\": " << pdr << ",\n"
                  << "  \"packetLossPercent\": " << packetLoss << ",\n"
                  << "  \"sensorBlockedPackets\": " << totalBlockedPackets << ",\n"
                  << "  \"unknownBlockedPackets\": " << unknownBlockedPackets << ",\n"
                  << "  \"unauthorizedFlowPackets\": " << unauthorizedPackets << ",\n"
                  << "  \"unknownNodeDetected\": " << (unknownNodeDetected ? "true" : "false") << ",\n"
                  << "  \"policyViolation\": " << (policyViolation ? "true" : "false") << ",\n"
                  << "  \"highTrafficDetected\": " << (highTrafficDetected ? "true" : "false") << ",\n"
                  << "  \"ipsActive\": " << (sensorBlocked[0] ? "true" : "false") << ",\n"
                  << "  \"sensors\": [\n";
    for (uint32_t i = 0; i < 5; ++i)
    {
        dashboardFile << "    {\"id\": " << (i + 1)
                      << ", \"sent\": " << sensorPackets[i]
                      << ", \"received\": " << deliveredBySensor[i]
                      << ", \"ratePps\": " << (sensorPackets[i] / 10.0)
                      << ", \"blocked\": " << (sensorBlocked[i] ? "true" : "false")
                      << "}" << (i < 4 ? "," : "") << "\n";
    }
    dashboardFile << "  ]\n}\n";
    dashboardFile.close();
    std::cout << "\nDashboard JSON saved: dashboard-results.json\n";
}
else
{
    std::cerr << "WARNING: Could not write dashboard-results.json\n";
}

Simulator::Destroy();

    return 0;
}
