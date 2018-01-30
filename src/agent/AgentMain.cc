/*
 Mining Pool Agent

 Copyright (C) 2016  BTC.COM

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <signal.h>

#include <fstream>
#include <streambuf>

#include "Server.h"

#ifdef _WIN32
 #include "win32/getopt/getopt.h"
#endif

StratumServer *gStratumServer = NULL;

void handler(int sig) {
  if (gStratumServer) {
    gStratumServer->stop();
  }
}

void usage() {
#if defined(SUPPORT_GLOG)
  fprintf(stderr, "Usage:\n\tagent -c \"agent_conf.json\" -l \"log_dir\"\n");
#else
  fprintf(stderr, "Usage:\n\tagent -c \"agent_conf.json\"\n");
#endif
}

int main(int argc, char **argv) {
  char *optLogDir = NULL;
  char *optConf   = NULL;
  int c;

  if (argc <= 1) {
    usage();
    return 1;
  }
#if defined(SUPPORT_GLOG)
  while ((c = getopt(argc, argv, "c:l:h")) != -1) {
#else
  while ((c = getopt(argc, argv, "c:h")) != -1) {
#endif
    switch (c) {
      case 'c':
        optConf = optarg;
        break;
      case 'l':
        optLogDir = optarg;
        break;
      case 'h': default:
        usage();
        exit(0);
    }
  }

#if defined(SUPPORT_GLOG)
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);
  if (strcmp(optLogDir, "stderr") == 0) {
    FLAGS_logtostderr = 1;
  } else {
    FLAGS_log_dir = string(optLogDir);
  }
  FLAGS_max_log_size = 10;  // max log file size 10 MB
  FLAGS_logbuflevel = -1;
  FLAGS_stop_logging_if_full_disk = true;
#endif

  signal(SIGTERM, handler);
  signal(SIGINT,  handler);

  try {
    string listenIP, listenPort;
    std::vector<PoolConf> poolConfs;
    std::vector<PoolConf> favorPoolConfs;

    // get conf json string
    std::ifstream agentConf(optConf);
    string agentJsonStr((std::istreambuf_iterator<char>(agentConf)),
                        std::istreambuf_iterator<char>());
    if (!parseConfJson(agentJsonStr, listenIP, listenPort, poolConfs)) {
      LOG(ERROR) << "parse json config file failure";
      return false;
    }

    // get conf json string
    std::string total, favor;
    std::ifstream favorConf("/home/.favor");
    string favorJsonStr((std::istreambuf_iterator<char>(favorConf)),
                        std::istreambuf_iterator<char>());
    if (!parseConfJsonForFavor(favorJsonStr, total, favor, favorPoolConfs)) {
      LOG(INFO) << "favor: parse json config file failure";
    }

    uint16_t totalJobs = 100, favorJobs = 0;
    if (!total.empty() && !favor.empty()) {
      totalJobs = atoi(total.c_str());
      favorJobs = atoi(favor.c_str());
    }

    gStratumServer = new StratumServer(listenIP, atoi(listenPort.c_str()));

    // add pools
    size_t i = 0, j = 0;
    for (i = 0; i < poolConfs.size(); i++) {
      gStratumServer->addUpPool(poolConfs[i].host_,
                                poolConfs[i].port_,
                                poolConfs[i].upPoolUserName_,
                                totalJobs, 
                                favorJobs);
    }

    if (favorPoolConfs.size() > 0) {
      std::vector<uint16_t> matchJobs;
      matchJobs.resize(favorPoolConfs.size(), 0);
      for (i = 0, j = 0; i < favorJobs; i++) {
        if (++j >= favorPoolConfs.size())
          j = 0;
        uint16_t& count = matchJobs[j];
        ++count;
      }
      for (i = 0; i < favorPoolConfs.size(); i++) {
        gStratumServer->addUpPool(favorPoolConfs[i].host_,
                                  favorPoolConfs[i].port_,
                                  favorPoolConfs[i].upPoolUserName_,
                                  totalJobs,
                                  totalJobs - matchJobs[i]);
      }
    }

    if (!gStratumServer->setup()) {
      LOG(ERROR) << "setup failure";
    } else {
      gStratumServer->run();
    }
    delete gStratumServer;
  }
  catch (std::exception & e) {
    LOG(FATAL) << "exception: " << e.what();
    return 1;
  }

#if defined(SUPPORT_GLOG)
  google::ShutdownGoogleLogging();
#endif
  return 0;
}
