/*
 *   ibeo_scala.cpp - ROS implementation of the Ibeo ScaLa driver.
 *   Copyright (C) 2017 AutonomouStuff, Co.
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *   USA
 */

#include <ibeo_scala_ros_msg_handler.h>
#include <network_interface/network_interface.h>

//C++ Includes
#include <unordered_map>

//PCL Includes
#include <pcl_ros/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>

using namespace AS;
using namespace AS::Network;
using namespace AS::Drivers::Ibeo;
using namespace AS::Drivers::IbeoScala;

TCPInterface tcp_interface;
std::unordered_map<unsigned short, ros::Publisher> pub_list;
IbeoScalaRosMsgHandler handler;

int main(int argc, char **argv)
{
  std::string ip_address = "192.168.1.52";
	int port = 12002;
	std::string frame_id = "ibeo_scala";
	bool is_fusion = false;
  bool publish_raw = false;
  unsigned char *msg_buf;
  unsigned char *orig_msg_buf; //Used for deallocation.
  size_t bytes_read;
  int buf_size = IBEO_PAYLOAD_SIZE;
  std::vector<unsigned char> grand_buffer;
  std::vector<std::vector<unsigned char>> messages;

	// ROS initialization
	ros::init(argc, argv, "ibeo_scala");
	ros::NodeHandle n;
	ros::NodeHandle priv("~");
	ros::Rate loop_rate(1000.0);
	bool exit = false;

  //Wait for time to be valid.
  while (ros::Time::now().nsec == 0);

	if (priv.getParam("ip_address", ip_address))
	{ 
		ROS_INFO("Ibeo ScaLa - Got ip_address: %s", ip_address.c_str());

		if (ip_address == "" )
		{
		 ROS_ERROR("Ibeo ScaLa - IP Address Invalid");
		 exit = true;
		}
	}

	if (priv.getParam("port", port))
	{ 
		ROS_INFO("Ibeo ScaLa - Got port: %d", port);

		if (port < 0)
		{
		 ROS_ERROR("Ibeo ScaLa - Port Invalid");
		 exit = true;
		}
	}
  
	if (priv.getParam("is_fusion", is_fusion))
	{ 
		ROS_INFO("Ibeo ScaLa - Is Fusion ECU: %s", (is_fusion)? "true" : "false");
	}

	if (priv.getParam("sensor_frame_id", frame_id))
	{
		ROS_INFO("Ibeo ScaLa - Got sensor frame ID: %s", frame_id.c_str());
	}

  if (priv.getParam("publish_raw_data", publish_raw))
  {
    ROS_INFO("Ibeo ScaLa - Publish raw data: %s", publish_raw ? "true" : "false");
  }

	if(exit)
    return 0;

  ros::Publisher eth_tx_pub = n.advertise<network_interface::TCPFrame>("tcp_tx", 10);
  ros::Publisher pointcloud_pub = n.advertise<pcl::PointCloud <pcl::PointXYZL> >("as_tx/point_cloud", 1);
  ros::Publisher object_markers_pub = n.advertise<visualization_msgs::MarkerArray>("as_tx/objects", 1);
  ros::Publisher object_contour_points_pub = n.advertise<visualization_msgs::Marker>("as_tx/object_contour_points", 1);

  ros::Publisher scan_2202_pub, scan_2205_pub, scan_2208_pub,
                 object_2225_pub, object_2270_pub, object_2271_pub, object_2280_pub,
                 camera_image_pub, vehicle_state_2805_pub, vehicle_state_2806_pub,
                 vehicle_state_2807_pub, device_status_pub;

  ROS_DEBUG("Ibeo ScaLa - Ethernet connection to ScaLa established successfully.");

  if (is_fusion)
  {

    scan_2205_pub = n.advertise<ibeo_msgs::ScanData2205>("parsed_tx/scan_data_2205", 1);
    object_2225_pub = n.advertise<ibeo_msgs::ObjectData2225>("parsed_tx/object_data_2225", 1);
    object_2280_pub = n.advertise<ibeo_msgs::ObjectData2280>("parsed_tx/object_data_2280", 1);
    camera_image_pub = n.advertise<ibeo_msgs::CameraImage>("parsed_tx/camera_image", 1);
    vehicle_state_2806_pub = n.advertise<ibeo_msgs::HostVehicleState2806>("parsed_tx/host_vehicle_state_2806", 1);
    vehicle_state_2807_pub = n.advertise<ibeo_msgs::HostVehicleState2807>("parsed_tx/host_vehicle_state_2807", 1);

    pub_list.insert(std::make_pair(ScanData2205::DATA_TYPE, scan_2205_pub));
    pub_list.insert(std::make_pair(ObjectData2225::DATA_TYPE, object_2225_pub));
    pub_list.insert(std::make_pair(ObjectData2280::DATA_TYPE, object_2280_pub));
    pub_list.insert(std::make_pair(CameraImage::DATA_TYPE, camera_image_pub));
    pub_list.insert(std::make_pair(HostVehicleState2806::DATA_TYPE, vehicle_state_2806_pub));
    pub_list.insert(std::make_pair(HostVehicleState2807::DATA_TYPE, vehicle_state_2807_pub));
  }
  else
  {
    scan_2202_pub = n.advertise<ibeo_msgs::ScanData2202>("parsed_tx/scan_data_2202", 1);
    scan_2208_pub = n.advertise<ibeo_msgs::ScanData2208>("parsed_tx/scan_data_2208", 1);
    object_2270_pub = n.advertise<ibeo_msgs::ObjectData2270>("parsed_tx/object_data_2270", 1);
    object_2271_pub = n.advertise<ibeo_msgs::ObjectData2271>("parsed_tx/object_data_2271", 1);
    vehicle_state_2805_pub = n.advertise<ibeo_msgs::HostVehicleState2805>("parsed_tx/host_vehicle_state_2805", 1);

    pub_list.insert(std::make_pair(ScanData2202::DATA_TYPE, scan_2202_pub));
    pub_list.insert(std::make_pair(ScanData2208::DATA_TYPE, scan_2208_pub));
    pub_list.insert(std::make_pair(ObjectData2270::DATA_TYPE, object_2270_pub));
    pub_list.insert(std::make_pair(ObjectData2271::DATA_TYPE, object_2271_pub));
    pub_list.insert(std::make_pair(HostVehicleState2805::DATA_TYPE, vehicle_state_2805_pub));
  }

  device_status_pub = n.advertise<ibeo_msgs::DeviceStatus>("parsed_tx/device_status", 1);

  pub_list.insert(std::make_pair(DeviceStatus::DATA_TYPE, device_status_pub));

  return_statuses status;
  bool fusion_filter_sent = false;

  while (ros::ok())
  {
    if (!tcp_interface.is_open())
    {
      if (is_fusion)
        fusion_filter_sent = false;

      return_statuses status = tcp_interface.open(ip_address.c_str(), port);

      if (status != OK)
        ROS_DEBUG("Ibeo ScaLa - TCP Interface connected at %s:%d",ip_address.c_str(),port);

      ros::Duration(1.0).sleep();
    }
    else
    {
      if (is_fusion && fusion_filter_sent == false)
      {
        ROS_DEBUG("Ibeo ScaLa - Sending filter message to fusion ECU");

        CommandSetFilter cmd;
        cmd.encode();

        status = tcp_interface.write(cmd.encoded_data.data(), cmd.encoded_data.size());
        
        if(status == OK)
        {
          ROS_DEBUG("Ibeo ScaLa - Wrote command to set filter.");
          fusion_filter_sent = true;
        }
        else
        {
          ROS_ERROR("Ibeo ScaLa - Failed to write set filter command.");
        }

        ros::Duration(0.5).sleep();
      }
      else
      {
        //ROS_DEBUG("Ibeo ScaLa - Setup complete. Starting loop.");

        buf_size = IBEO_PAYLOAD_SIZE;
        std::unique_ptr<unsigned char[]> msg_buf(new unsigned char[buf_size + 1]);

        status = tcp_interface.read(msg_buf.get(), buf_size, bytes_read); //Read a (big) chunk.
        
        if (status != OK && status != NO_MESSAGES_RECEIVED)
        {
          ROS_WARN("Ibeo ScaLa - Failed to read from socket: %d - %s", status, return_status_desc(status).c_str());
        }
        else if (status == OK)
        {
          buf_size = bytes_read;
          grand_buffer.insert(grand_buffer.end(), msg_buf.get(), msg_buf.get() + bytes_read);
      
          int first_mw = 0;
          //ROS_DEBUG("Finished reading %d bytes of data. Total buffer size is %d.",bytes_read, grand_buffer.size());

          while (true)
          {
            first_mw = find_magic_word(grand_buffer.data(), grand_buffer.size(), MAGIC_WORD);
            
            if(first_mw == -1) // no magic word found. move along.
            {
               break;
            }
            else  // magic word found. pull out message from grand buffer and add it to the message list.
            {
              //ROS_DEBUG("Size before removing unused bytes: %lu", grand_buffer.size());
              //ROS_DEBUG("Location of MW: %i", first_mw);

              if (first_mw > 0)
                grand_buffer.erase(grand_buffer.begin(), grand_buffer.begin() + first_mw); // Unusable data in beginning of buffer.

              // From here on, the detected Magic Word should be at the beginning of the grand_buffer.

              //ROS_DEBUG("Size before reading message: %lu", grand_buffer.size());

              IbeoDataHeader header;
              std::vector<unsigned char> msg;

              header.parse(grand_buffer.data());

              //ROS_DEBUG("Calculated size of message: %u", IBEO_HEADER_SIZE + header.message_size);

              if (grand_buffer.size() < IBEO_HEADER_SIZE + header.message_size)
                break; // Incomplete message left in grand buffer. Wait for next read.

              msg.insert(msg.end(), grand_buffer.begin(), grand_buffer.begin() + IBEO_HEADER_SIZE + header.message_size);
              //ROS_DEBUG("Size of copied message: %lu", msg.size());
              messages.push_back(msg);
              grand_buffer.erase(grand_buffer.begin(), grand_buffer.begin() + IBEO_HEADER_SIZE + header.message_size);

              //ROS_DEBUG("Size after reading message: %lu", grand_buffer.size());
            }

            if (!ros::ok())
              break;
          }
        }

        if (!messages.empty())
        {
          //Found at least one message, let's parse them.
          for(unsigned int i = 0; i < messages.size(); i++)
          {
            //ROS_DEBUG("Ibeo ScaLa - Parsing message %u of %lu.", i, messages.size());

            if (publish_raw)
            {
              network_interface::TCPFrame raw_frame;
              raw_frame.address = ip_address;
              raw_frame.port = port;
              raw_frame.size = messages[i].size();
              raw_frame.data.insert(raw_frame.data.begin(), messages[i].begin(), messages[i].end());
              raw_frame.header.frame_id = frame_id;
              raw_frame.header.stamp = ros::Time::now();

              eth_tx_pub.publish(raw_frame);
            }

            //ROS_DEBUG("Ibeo ScaLa - Size of message: %lu.", messages[i].size());

            IbeoDataHeader ibeo_header;
            ibeo_header.parse(messages[i].data());

            //ROS_DEBUG("Ibeo ScaLa - Got message type: 0x%x", ibeo_header.data_type_id);

            auto class_parser = IbeoTxMessage::make_message(ibeo_header.data_type_id); //Instantiate a parser class of the correct type.
            auto pub = pub_list.find(ibeo_header.data_type_id); //Look up the message handler for this type.

            //Only parse message types we know how to handle.
            if (class_parser != NULL && pub != pub_list.end())
            {
              class_parser->parse(messages[i].data()); //Parse the raw data into the class.
              handler.fillAndPublish(ibeo_header.data_type_id, frame_id, pub->second, class_parser); //Create a new message of the correct type and publish it.

              if (class_parser->has_scan_points)
              {
                pcl::PointCloud<pcl::PointXYZL> pcl_cloud;
                pcl_cloud.header.frame_id = frame_id;
                //pcl_cloud.header.stamp = ibeo_header.time;
                pcl_conversions::toPCL(ros::Time::now(), pcl_cloud.header.stamp);
                std::vector<Point3DL> scan_points = class_parser->get_scan_points();
                handler.fillPointcloud(scan_points, pcl_cloud);
                pointcloud_pub.publish(pcl_cloud);
              }

              if (class_parser->has_contour_points)
              {
                visualization_msgs::Marker marker;
                marker.header.frame_id = frame_id;
                marker.header.stamp = ros::Time::now();
                std::vector<Point3D> contour_points = class_parser->get_contour_points();

                if( contour_points.size() > 0 )
                {
                  handler.fillContourPoints(contour_points, marker, frame_id);
                  object_contour_points_pub.publish(marker);
                }
              }

              if (class_parser->has_objects)
              {
                std::vector<IbeoObject> objects = class_parser->get_objects();
                visualization_msgs::MarkerArray marker_array;
                handler.fillMarkerArray(objects, marker_array, frame_id);

                for( visualization_msgs::Marker m : marker_array.markers )
                {
                  m.header.frame_id = frame_id;
                }

                object_markers_pub.publish(marker_array);
              }
            }
          }

          messages.clear();
        } // If we have messages to parse
      } // If not fusion or filter message already sent
    } // If interface is open

    loop_rate.sleep();
    //ros::spinOnce(); // No callbacks yet - no reason to spin.
  } // ROS Loop

  status = tcp_interface.close();

  if (status != OK)
    ROS_ERROR("Ibeo ScaLa - Connection to ScaLa was not properly closed: %d - %s", status, return_status_desc(status).c_str());

  return 0;
}
