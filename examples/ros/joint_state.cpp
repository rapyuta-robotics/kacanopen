/*
 * Copyright (c) 2015-2016, Thomas Keh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bridge.h"
#include "logger.h"
#include "joint_state_publisher.h"
#include "joint_state_subscriber.h"
 
#include <thread>
#include <chrono>
#include <memory>

// #define BUSNAME ... // set by CMake
// #define BAUDRATE ... // set by CMake

int main(int argc, char* argv[]) {

	PRINT("This example publishes and subscribes JointState messages for each connected CiA 402 device.");

	kaco::Master master;
	bool success = master.start(BUSNAME, BAUDRATE);

	if (!success) {
		ERROR("Starting master failed.");
		return EXIT_FAILURE;
	}

	std::this_thread::sleep_for(std::chrono::seconds(1));
	while (master.num_devices()<1) {
		ERROR("No devices found. Waiting.");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// Create bridge
	ros::init(argc, argv, "canopen_bridge");
	kaco::Bridge bridge;

	bool found = false;
	for (size_t i=0; i<master.num_devices(); ++i) {
		
		kaco::Device& device = master.get_device(i);
		device.start();

		if (device.get_device_profile_number()==402) {

			found = true;
			PRINT("Found CiA 402 device with node ID "<<device.get_node_id());

			if (!device.load_dictionary_from_library()) {
				ERROR("No suitable EDS file found for this device.");
				return EXIT_FAILURE;
			}

			device.load_operations();
			device.load_constants();

			DUMP(device.get_entry("manufacturer_device_name"));

			PRINT("Enable operation");
			device.execute("enable_operation");

			PRINT("Set position mode");
			device.set_entry("modes_of_operation", device.get_constant("profile_position_mode"));

			// TODO: target_position should be mapped to a PDO

			auto jspub = std::make_shared<kaco::JointStatePublisher>(device, 0, 350000);
			bridge.add_publisher(jspub);

			auto jssub = std::make_shared<kaco::JointStateSubscriber>(device, 0, 350000);
			bridge.add_subscriber(jssub);

		}

	}

	if (!found) {
		ERROR("This example is intended for use with a CiA 402 device but I can't find one.");
		return EXIT_FAILURE;
	}

	// run ROS loop and publish everything repeatedly with 100 Hz
	bridge.run(100);

}
