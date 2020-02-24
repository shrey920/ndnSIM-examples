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
#    |  c1  |<--+                                      /      +-->|  h1  |
#    \------/    \                                    /      /    \------/
#                 \                                  |      /     
#                  \  1  v0                         2v  3  /      
#                   +->/------\                 /------\<-+       
#                     2|  hs  |<===============>|  cs  |4         
#                   +->\------/4               0\------/<-+       
#                  /                                 ^5    \      
#                 /                                  |      \     
#    /------\ 0  /                                    \      \  0 /------\
#    |  c2  |<--+                                      \      +-->|  t2  |
#    \------/                                           \         \------/
#                          "All consumer-router and"     \        
#                          "router-producer links are"    \    0 /------\
#                          "10Mbps"                        +---->|  h2  |
#                                                                \------/
#                                                                
#    "Numbers near nodes denote face IDs. Face ID is assigned based on the order of link"
#    "definitions in the topology file"
*/


void installConsumer(Ptr<Node> consumerNode, std::string room , Ptr<Node> producerNode, int start, int end){

  std::string prefix = "/data/" + room + "/" + Names::FindName(producerNode);

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetAttribute("Frequency", StringValue("10")); // 100 interests a second

  consumerHelper.SetPrefix(prefix);
  ApplicationContainer consumer = consumerHelper.Install(consumerNode);
  consumer.Start(Seconds(start));
  consumer.Stop(Seconds(end));
}


int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 1);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/scenario2.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize",
                               "1"); // ! Attention ! If set to 0, then MaxSize is infinite
  ndnHelper.InstallAll();

  // Set BestRoute strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

  // Getting containers for the consumer/producer
  Ptr<Node> consumers[2] = {Names::Find<Node>("c1"), Names::Find<Node>("c2")};
  Ptr<Node> producers[4] = {Names::Find<Node>("t1"), Names::Find<Node>("h1"),
                            Names::Find<Node>("t2"), Names::Find<Node>("h2")};

  if (consumers[0] == 0 || consumers[1] == 0 || 
  producers[0] == 0 || producers[1] == 0 || producers[2] == 0 || producers[3] == 0) {
    NS_FATAL_ERROR("Error in topology: one nodes c1, c2, t1, h1, t2, or h2 is missing");
  }

  for (int i = 0; i < 4; i++) {

    std::string room;
    if(i<2){
      room = "room1";
    }
    else{
      room = "room2";
    }

    std::string prefix = "/data/" + room + "/" + Names::FindName(producers[i]);

    
    ///////////////////////////////////////////////
    // install producer app on producer node p_i //
    ///////////////////////////////////////////////

    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));

    // install producer that will satisfy Interests in /dst1 namespace
    producerHelper.SetPrefix(prefix);
    ApplicationContainer producer = producerHelper.Install(producers[i]);
    // when Start/Stop time is not specified, the application is running throughout the simulation
  }



  /////////////////////////////////////////////////////////////////////////////////
  // install consumer app on consumer node to request data from producer //
  /////////////////////////////////////////////////////////////////////////////////

  for (int j=0; j<5; j++){
    int i = j*10;
    installConsumer(consumers[0], "room1", producers[0], i, i+5);
    installConsumer(consumers[1], "room1", producers[1], i, i+5);

    installConsumer(consumers[0], "room2", producers[2], i+5, i+10);
    installConsumer(consumers[1], "room2", producers[3], i+5, i+10);

  }




  // Manually configure FIB routes
  // ndn::FibHelper::AddRoute("c1", "/data", "n1", 1); // link to n1
  ndn::FibHelper::AddRoute("c2", "/data", "n1", 1); // link to n1
  ndn::FibHelper::AddRoute("c1", "/data", "n3", 2); // link to n3
  // ndn::FibHelper::AddRoute("c2", "/data", "n3", 2); // link to n3

  ndn::FibHelper::AddRoute("n1", "/data", "n2", 1);  // link to n2
  ndn::FibHelper::AddRoute("n1", "/data", "n3", 1);  // link to n3

  ndn::FibHelper::AddRoute("n3", "/data", "n2", 1);  // link to n2

  ndn::FibHelper::AddRoute("n2", "/data/room1/t1", "t1", 1); // link to t1
  ndn::FibHelper::AddRoute("n2", "/data/room1/h1", "h1", 1); // link to h1
  ndn::FibHelper::AddRoute("n2", "/data/room2/t2", "t2", 1); // link to t2
  ndn::FibHelper::AddRoute("n2", "/data/room2/h2", "h2", 1); // link to h2

  // Schedule simulation time and run the simulation
  Simulator::Stop(Seconds(20.0));
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
