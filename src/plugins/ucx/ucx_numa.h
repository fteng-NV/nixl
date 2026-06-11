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
 #ifndef NIXL_SRC_PLUGINS_UCX_UCX_NUMA_H
 #define NIXL_SRC_PLUGINS_UCX_UCX_NUMA_H

 #include <string>
 #include <vector>

 /**
  * NUMA-aware utilities for UCX backend thread pinning
  *
  * This module provides functionality to:
  * 1. Discover the NUMA node of a network device
  * 2. Bind worker threads to the same NUMA node as the NIC
  * 3. Optimize memory access patterns for RDMA operations
  */

 namespace nixl::ucx::numa {

 /**
  * Get the NUMA node ID for a given network device
  *
  * @param device_name The network device name (e.g., "mlx5_0")
  * @return The NUMA node ID (0-based), or 0 if not found or NUMA not available
  *
  * This function reads from /sys/class/infiniband/{device_name}/device/numa_node
  * to determine which NUMA node the NIC is attached to.
  */
 int getDeviceNumaNode(const std::string &device_name);

 /**
  * Bind the current thread to a specific NUMA node
  *
  * @param numa_node The NUMA node ID to bind to
  * @return 0 on success, non-zero on failure
  *
  * This function uses libnuma to:
  * 1. Get all CPUs in the specified NUMA node
  * 2. Set thread affinity to those CPUs using pthread_setaffinity_np()
  *
  * If NUMA is not available on the system, this function logs a warning
  * and returns 0 (no-op).
  */
 int bindThreadToNumaNode(int numa_node);

 /**
  * Get all CPUs in a NUMA node
  *
  * @param numa_node The NUMA node ID
  * @return A vector of CPU IDs in the specified NUMA node
  *
  * Returns an empty vector if NUMA is not available or the node is invalid.
  */
 std::vector<int> getNumaNodeCpus(int numa_node);

 /**
  * Check if NUMA is available on the system
  *
  * @return true if NUMA is available, false otherwise
  */
 bool isNumaAvailable();

 /**
  * Get the number of configured NUMA nodes
  *
  * @return The number of NUMA nodes, or 1 if NUMA is not available
  */
 int getNumaNodeCount();

 /**
  * Get the NUMA node of the current thread
  *
  * @return The NUMA node ID, or -1 if it cannot be determined
  *
  * This function checks the current thread's CPU affinity and maps
  * the first CPU in the affinity set to its NUMA node.
  */
 int getCurrentThreadNumaNode();

 } // namespace nixl::ucx::numa

 #endif // NIXL_SRC_PLUGINS_UCX_UCX_NUMA_H