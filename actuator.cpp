// export DISPLAY=:0
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// ndn-congestion-alt-topo-plugin.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

/*
# architecture.txt

#                                                               0 /------\
#                                                          +----->|  t1  |
#                                                         /       \------/
#                                                        /        
#    /------\ 0                                         /       0 /------\
#    |  u1  |<--+                                      /      +-->|  f1  |
#    \------/    \                                    /      /    \------/
#                 \                                  |      /     
#                  \  1  v0                         2v  3  /      
#                   +->/------\                 /------\<-+       
#                     2|  cs  |<===============>|  hs  |4         
#                   +->\------/4               0\------/<-+       
#                  /                                 ^5    \      
#                 /                                  |      \     
#    /------\ 0  /                                    \      \  0 /------\
#    |  u2  |<--+                                      \      +-->|  t2  |
#    \------/                                           \         \------/
#                          "All consumer-router and"     \        
#                          "router-producer links are"    \    0 /------\
#                          "10Mbps"                        +---->|  f2  |
#                                                                \------/
#                                                                
#    "Numbers near nodes denote face IDs. Face ID is assigned based on the order of link"
#    "definitions in the topology file"
*/


void installConsumer(Ptr<Node> consumerNode, std::string prefix , Ptr<Node> producerNode, float start, float end){

  /////////////////////////////////////////////////////////////////////////////////
  // install consumer app on consumer node to request data from producer //
  /////////////////////////////////////////////////////////////////////////////////


  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetAttribute("Frequency", StringValue("10")); // 100 interests a second

  consumerHelper.SetPrefix(prefix);
  ApplicationContainer consumer = consumerHelper.Install(consumerNode);
  consumer.Start(Seconds(start));
  consumer.Stop(Seconds(end));
}

void installProducer(Ptr<Node> producerNode, std::string prefix, int freshness){

    ///////////////////////////////////////////////
    // install producer app on producer node p_i //
    ///////////////////////////////////////////////

    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds (freshness)));

    // install producer that will satisfy Interests in /dst1 namespace
    producerHelper.SetPrefix(prefix);
    ApplicationContainer producer = producerHelper.Install(producerNode);
}

void transferPackets(Ptr<Node> consumerNode, std::string prefix , Ptr<Node> producerNode, int freshness, float start, float end){
  installProducer(producerNode, prefix, freshness);
  installConsumer(consumerNode, prefix, producerNode, start, end); 
}


int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 1);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/actuator.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Freshness::Lru", "MaxSize",
                               "0"); // ! Attention ! If set to 0, then MaxSize is infinite
  // ndnHelper.setCsSize(100);
  ndnHelper.InstallAll();

  // Set BestRoute strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

  // Getting containers for the consumer/producer
  Ptr<Node> users[2] = {Names::Find<Node>("u1"), Names::Find<Node>("u2")};
  Ptr<Node> sensors[2] = {Names::Find<Node>("t1"), Names::Find<Node>("t2")};
  Ptr<Node> actuators[2] = {Names::Find<Node>("f1"), Names::Find<Node>("f2")};
   

  if (users[0] == 0 || users[1] == 0 || 
  sensors[0] == 0 || sensors[1] == 0 || 
  actuators[0] == 0 || actuators[1] == 0) 
  {
    NS_FATAL_ERROR("Error in topology: one nodes u1, u2, t1, f1, t2, or f2 is missing");
  }


  float i=0.0;

  transferPackets(users[0], "data/room1", sensors[0], 5, i, i+2);
  transferPackets(actuators[0], "action/room1", users[0], 5, i+3, i+4);

  i = 5.0;


  transferPackets(users[1], "data/room2", sensors[1], 5, i, i+2);
  transferPackets(actuators[1], "action/room2", users[1], 5, i+3, i+4);



  // Manually configure FIB routes
  ndn::FibHelper::AddRoute("u1", "/data/room1", "n1", 1); // link to n1
  ndn::FibHelper::AddRoute("u2", "/data/room2", "n1", 1); // link to n1

  ndn::FibHelper::AddRoute("n1", "/data", "n2", 1);  // link to n2

  ndn::FibHelper::AddRoute("n2", "/data/room1", "t1", 1); // link to t1
  ndn::FibHelper::AddRoute("n2", "/data/room2", "t2", 1); // link to t2

  ndn::FibHelper::AddRoute("f1", "/action/room1", "n2", 1); // link to f1
  ndn::FibHelper::AddRoute("f2", "/action/room2", "n2", 1); // link to f2
  
  ndn::FibHelper::AddRoute("n2", "/action", "n1", 1);  // link to n2
  
  ndn::FibHelper::AddRoute("n1", "/action/room1", "u1", 1); // link to n1
  ndn::FibHelper::AddRoute("n1", "/action/room2", "u2", 1); // link to n1

  

  // Schedule simulation time and run the simulation
  Simulator::Stop(Seconds(20.0));
  ndn::CsTracer::InstallAll("cs-trace.txt", Seconds(0.2));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
