/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #include "ucx_numa.h"
 #include "common/nixl_log.h"

 #include <fstream>
 #include <sstream>
 #include <string>
 #include <vector>
 #include <dirent.h>
 #include <sched.h>
 #include <pthread.h>

 namespace nixl::ucx::numa {

 int getDeviceNumaNode(const std::string &device_name) {
     // Try InfiniBand sysfs path first
     std::string path = "/sys/class/infiniband/" + device_name + "/device/numa_node";
     std::ifstream ifs(path);
     if (!ifs.is_open()) {
         // Try network device sysfs path as fallback
         path = "/sys/class/net/" + device_name + "/device/numa_node";
         ifs.open(path);
     }

     if (!ifs.is_open()) {
         NIXL_DEBUG << "Could not open NUMA node file for device " << device_name
                    << ", defaulting to NUMA node 0";
         return 0;
     }

     int numa_node = -1;
     ifs >> numa_node;
     if (numa_node < 0) {
         NIXL_DEBUG << "Device " << device_name << " reports NUMA node " << numa_node
                    << ", defaulting to 0";
         return 0;
     }

     NIXL_DEBUG << "Device " << device_name << " is on NUMA node " << numa_node;
     return numa_node;
 }

 std::vector<int> getNumaNodeCpus(int numa_node) {
     std::vector<int> cpus;
     std::string path = "/sys/devices/system/node/node" + std::to_string(numa_node) + "/cpulist";
     std::ifstream ifs(path);
     if (!ifs.is_open()) {
         NIXL_WARN << "Could not open CPU list for NUMA node " << numa_node;
         return cpus;
     }

     std::string cpulist;
     std::getline(ifs, cpulist);

     // Parse cpulist format: "0-3,8-11" or "0,1,2,3"
     std::stringstream ss(cpulist);
     std::string range;
     while (std::getline(ss, range, ',')) {
         size_t dash = range.find('-');
         if (dash != std::string::npos) {
             int start = std::stoi(range.substr(0, dash));
             int end = std::stoi(range.substr(dash + 1));
             for (int i = start; i <= end; i++) {
                 cpus.push_back(i);
             }
         } else {
             cpus.push_back(std::stoi(range));
         }
     }

     return cpus;
 }

 int bindThreadToNumaNode(int numa_node) {
     std::vector<int> cpus = getNumaNodeCpus(numa_node);
     if (cpus.empty()) {
         NIXL_WARN << "No CPUs found for NUMA node " << numa_node
                   << ", skipping thread binding";
         return -1;
     }

     cpu_set_t cpuset;
     CPU_ZERO(&cpuset);
     for (int cpu : cpus) {
         CPU_SET(cpu, &cpuset);
     }

     int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
     if (ret != 0) {
         NIXL_WARN << "Failed to set thread affinity to NUMA node " << numa_node
                   << ": " << strerror(ret);
         return ret;
     }

     NIXL_DEBUG << "Bound thread to NUMA node " << numa_node
                << " (CPUs: " << cpus.front() << "-" << cpus.back() << ")";
     return 0;
 }

 bool isNumaAvailable() {
     std::string path = "/sys/devices/system/node/node0";
     std::ifstream ifs(path + "/cpulist");
     return ifs.is_open();
 }

 int getNumaNodeCount() {
     int count = 0;
     DIR *dir = opendir("/sys/devices/system/node");
     if (!dir) {
         return 1;
     }

     struct dirent *entry;
     while ((entry = readdir(dir)) != nullptr) {
         std::string name = entry->d_name;
         if (name.substr(0, 4) == "node" && name.size() > 4 &&
             std::isdigit(name[4])) {
             count++;
         }
     }
     closedir(dir);
     return count > 0 ? count : 1;
 }

 int getCurrentThreadNumaNode() {
     cpu_set_t cpuset;
     CPU_ZERO(&cpuset);

     int ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
     if (ret != 0) {
         return -1;
     }

     // Find the first CPU in the set and determine its NUMA node
     int num_numa = getNumaNodeCount();
     for (int node = 0; node < num_numa; node++) {
         std::vector<int> node_cpus = getNumaNodeCpus(node);
         for (int cpu : node_cpus) {
             if (CPU_ISSET(cpu, &cpuset)) {
                 return node;
             }
         }
     }

     return -1;
 }

 } // namespace nixl::ucx::numa
